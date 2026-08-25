/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to manage MIC connections
*/

#include <stdlib.h>
#include "kymera_mic_if.h"
#include "kymera_aec.h"
#include "kymera_mic_resampler.h"
#include "kymera_splitter.h"
#include "kymera_config.h"
#include "kymera_setup.h"
#include "kymera_leakthrough.h"
#include <logging.h>

#define CVC_FRAME_IN_US (7500)
#define MIN_SAMPLE_RATE_IN_KHZ (16)
#define MAX_SAMPLE_RATE_IN_KHZ (32)
#define MAX_CVC_FRAME_SIZE ((CVC_FRAME_IN_US * MAX_SAMPLE_RATE_IN_KHZ) / 1000)

#define MIC_PATH_TRANSFORM_SIZE (1024)
#define AEC_PATH_TRANSFORM_SIZE ((MAX_CVC_FRAME_SIZE / 2) + MIC_PATH_TRANSFORM_SIZE)

/*! Registration array for all available users */
typedef struct
{
    unsigned nr_entries;
    const mic_registry_per_user_t* *entry;
} mic_registry_t;

typedef struct
{
    uint8 num_of_mics;
    /* List of mic sinks */
    Sink *mic_sinks;
    /* List of mic sources */
    Source *mic_sources;
} mic_connections_internal_t;

typedef void (* SourceFunction) (Source *array, unsigned length_of_array);

static splitter_config_t splitter_config =
{
    .transform_size_in_words = AEC_PATH_TRANSFORM_SIZE,
    .data_format = operator_data_format_pcm,
    .cap_id_splitter = capability_id_splitter,
};

static const struct
{
    mic_users_t mic_users;
    unsigned leakthrough_enabled:1;
    aec_usecase_t aec_usecase;
} aec_usecase_map[] =
{
    {mic_user_va, FALSE, aec_usecase_default},
    {mic_user_sco, FALSE, aec_usecase_default},
    {(mic_user_sco | mic_user_aanc), FALSE, aec_usecase_default},
    {mic_user_le_voice, FALSE, aec_usecase_default},
    {(mic_user_le_voice | mic_user_aanc), FALSE, aec_usecase_default},
    {mic_user_usb_voice, FALSE, aec_usecase_default},
    {(mic_user_usb_voice | mic_user_aanc), FALSE, aec_usecase_default},
    {mic_user_aanc, FALSE, aec_usecase_default},
    {(mic_user_va | mic_user_aanc), FALSE, aec_usecase_default},
    {mic_user_fit_test, FALSE, aec_usecase_default},
    {(mic_user_fit_test | mic_user_aanc), FALSE, aec_usecase_default},
    {mic_user_le_mic, FALSE, aec_usecase_default},

    {mic_user_none, TRUE, aec_usecase_enable_leakthrough},
    {mic_user_va, TRUE, aec_usecase_enable_leakthrough},
    {mic_user_sco, TRUE, aec_usecase_enable_leakthrough},
    {mic_user_le_mic, TRUE, aec_usecase_enable_leakthrough},
};

static struct
{
    mic_registry_t registry;
    splitter_handle_t splitter;
    uint32 mic_sample_rate;
    mic_users_t current_users;
    mic_users_t stream_map[MAX_NUM_OF_CONCURRENT_MIC_USERS];
    mic_users_t wake_states;
    unsigned leakthrough_enabled:1;
    unsigned chains_are_awake:1;
    unsigned aec_ref_offset:1;
    Source mic_sources[MAX_NUM_OF_CONCURRENT_MICS];
    uint8  use_count[MAX_SUPPORTED_MICROPHONES];
} state =
{
    .registry = {0, NULL},
    .splitter = NULL,
    .mic_sample_rate = 0,
    .current_users = mic_user_none,
    .stream_map = {mic_user_none},
    .wake_states = mic_user_all_mask,
    .leakthrough_enabled = FALSE,
    .chains_are_awake = TRUE,
    .aec_ref_offset = 0,
    .mic_sources = {NULL},
    .use_count = { 0 },
};

static void kymera_AddMandatoryMicsFromUser(mic_users_t user, uint16 *mic_ids, Sink *mic_sinks, uint8 *num_of_mics);
static void kymera_AddMandatoryMicsFromAllUsers(uint16 *mic_ids, Sink *mic_sinks, uint8 *num_of_mics);
static void kymera_AddMicsToOrderedList(const uint16 *current_mic_ids, const Sink *current_mic_sinks, uint8 current_num_of_mics,
                                        uint16 *ordered_mic_ids, Sink *ordered_mic_sinks, uint8 *ordered_num_of_mics);
static void kymera_AddToOrderedListOfMics(uint16 *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint16 new_mic_id, Sink new_mic_sink);
static void kymera_UserGetConnectionParameters(mic_users_t user, mic_connect_params_t *mic_params, uint16 *mic_ids, Sink *mic_sinks, Sink *aec_ref_sink);
static bool kymera_IsAnyUserNonInterruptible(mic_users_t exclude_user);

static bool kymera_IsMicConcurrencyEnabled(void)
{
    return Kymera_GetChainConfigs()->chain_mic_resampler_config != NULL;
}

static const aec_usecase_t * kymera_GetAecRefUseCase(mic_users_t users)
{
    unsigned i;
    // Leakthrough is managed via a boolean
    users &= ~mic_user_leakthrough;
    DEBUG_LOG("kymera_GetAecRefUseCase: user: enum:mic_user_t:%d leakthrough %d", users, state.leakthrough_enabled);

    for(i = 0; i < ARRAY_DIM(aec_usecase_map); i++)
    {
        if ((aec_usecase_map[i].mic_users == users) &&
            (aec_usecase_map[i].leakthrough_enabled == state.leakthrough_enabled))
        {
            DEBUG_LOG("kymera_GetAecRefUseCase: enum:aec_usecase_t:%d", aec_usecase_map[i].aec_usecase);
            return &aec_usecase_map[i].aec_usecase;
        }
    }

    return NULL;
}

static void kymera_SetAecRefUseCase(mic_users_t users)
{
    aec_usecase_t aec_usecase = aec_usecase_default;
    const aec_usecase_t *config = kymera_GetAecRefUseCase(users);

    if (config)
    {
        aec_usecase = *config;
    }
    Kymera_SetAecUseCase(aec_usecase);
}

static const mic_registry_per_user_t * kymera_GetRegistryEntry(mic_users_t user)
{
    unsigned i;
    for(i = 0; i < state.registry.nr_entries; i++)
    {
        if (state.registry.entry[i]->user == user)
        {
            return state.registry.entry[i];
        }
    }
    DEBUG_LOG_ERROR("kymera_GetRegistryEntry: User not found");
    Panic();
    return NULL;
}

static uint8 kymera_GetStreamIndex(mic_users_t user)
{
    uint8 stream_index;

    for(stream_index = 0; stream_index < ARRAY_DIM(state.stream_map); stream_index++)
    {
        if (state.stream_map[stream_index] == user)
        {
            return stream_index;
        }
    }

    DEBUG_LOG_ERROR("kymera_GetStreamIndex: No stream entry for user enum:mic_users_t:%d", user);
    Panic();
    return 0;
}

static void kymera_ReplaceEntryInStreamMap(mic_users_t old_entry, mic_users_t new_entry)
{
    uint8 stream_index;

    for(stream_index = 0; stream_index < ARRAY_DIM(state.stream_map); stream_index++)
    {
        if (state.stream_map[stream_index] == old_entry)
        {
            state.stream_map[stream_index] = new_entry;
            return;
        }
    }

    DEBUG_LOG_ERROR("kymera_ReplaceEntryInStreamMap: Couldn't find entry enum:mic_users_t:%d", old_entry);
    Panic();
}

static bool kymera_IsCurrentUser(mic_users_t user)
{
    return (state.current_users & user) != 0;
}

static void kymera_AddToCurrentUsers(mic_users_t user)
{
    state.current_users |= user;
}

static void kymera_RemoveFromCurrentUsers(mic_users_t user)
{
    state.current_users &= ~user;
}

static uint8 kymera_GetNrOfCurrentUsers(void)
{
    uint8 users_count = 0;
    mic_users_t users = state.current_users  & ~mic_user_leakthrough;
    while(users != mic_user_none)
    {
        if ((users & 1) != 0)
        {
            users_count ++;
        }
        users = users >> 1;
    }
    return users_count;
}

static void kymera_AddMicUser(mic_users_t user)
{
    if(!kymera_IsCurrentUser(user))
    {
        kymera_AddToCurrentUsers(user);
        kymera_SetAecRefUseCase(state.current_users);
        kymera_ReplaceEntryInStreamMap(mic_user_none, user);
    }
}

static void kymera_RemoveMicUser(mic_users_t user)
{
    PanicFalse(kymera_IsCurrentUser(user));
    kymera_RemoveFromCurrentUsers(user);
    kymera_SetAecRefUseCase(state.current_users);
    kymera_ReplaceEntryInStreamMap(user, mic_user_none);
}

static bool kymera_DoesMicTypeMatchSelected(uint16 mic_id, mic_type_t selected_mic_type)
{
    audio_mic_params *mic_config;

    mic_config = Microphones_GetMicrophoneConfig(mic_id);
    PanicFalse(mic_config);
    if(mic_config->type & selected_mic_type)
    {
        return TRUE;
    }
    return FALSE;
}

static void kymera_UnsynchroniseMics(uint8 num_of_mics, const uint16 *mic_ids, const Source *mic_sources)
{
    if (kymera_GetNrOfCurrentUsers() == 0)
    {
        for(unsigned i = 0; i < num_of_mics; i++)
        {
            if(kymera_DoesMicTypeMatchSelected(mic_ids[i], (mic_type_analog | mic_type_digital)))
            {
                SourceSynchronise(mic_sources[i], NULL);
                DEBUG_LOG("kymera_UnsynchroniseMics: source 0x%x", mic_sources[i]);
            }
        }
    }
}

static void kymera_SynchroniseMics(uint8 num_of_mics, const uint16 *mic_ids, const Source *mic_sources)
{
    int8 sync_cnt = -1;
    for(unsigned i = 0; i < num_of_mics; i++)
    {
        /* Synchronisation is only needed for mics that are connected to aec_ref */
        if(kymera_DoesMicTypeMatchSelected(mic_ids[i], (mic_type_analog | mic_type_digital)))
        {
            if(sync_cnt >= 0)
            {
                SourceSynchronise(mic_sources[sync_cnt], mic_sources[i]);
                DEBUG_LOG("kymera_SynchroniseMics: source 0x%x with source 0x%x", mic_sources[sync_cnt], mic_sources[i]);
            }
            sync_cnt = i;
        }
    }
}

static mic_users_t kymera_ExtractUserFromRegistry(uint8 user_index)
{
    return state.registry.entry[user_index]->user;
}

static mic_user_state_t kymera_ExtractStateFromRegistry(uint8 user_index)
{
    return *state.registry.entry[user_index]->mic_user_state;
}

static void kymera_CollectMicsFromMultipleUsers(mic_users_t users,
                                                uint16 *combined_mic_ids, Sink *combined_mic_sinks,
                                                uint8 *combined_num_of_mics, uint32 *combined_sample_rate)
{
    mic_connect_params_t current_mic_params = { 0 };
    uint16 current_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
    Sink current_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    current_mic_params.connections.mic_ids = current_mic_ids;
    current_mic_params.connections.mic_sinks = current_mic_sinks;

    Sink empty_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};

    DEBUG_LOG("kymera_CollectMicsFromMultipleUsers: users: 0x%x", users);
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if ((users & current_user) != 0)
        {
            Sink aec_ref_sink;
            kymera_UserGetConnectionParameters(current_user, &current_mic_params, current_mic_ids, current_mic_sinks, &aec_ref_sink);
            *combined_sample_rate = MAX(*combined_sample_rate, MAX(current_mic_params.sample_rate, (MIN_SAMPLE_RATE_IN_KHZ * 1000)));
            /* Add to combined array with sink NULL */
            kymera_AddMicsToOrderedList(current_mic_ids, empty_mic_sinks, current_mic_params.connections.num_of_mics,
                                        combined_mic_ids, combined_mic_sinks, combined_num_of_mics);
        }
    }
}

static bool kymera_IsConnectingUserNonInterruptible(mic_users_t user)
{
    bool is_non_interruptible = FALSE;

    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if(current_user == user)
        {
            mic_user_state_t current_state = kymera_ExtractStateFromRegistry(user_index);
            if (current_state == mic_user_state_non_interruptible)
            {
                is_non_interruptible = TRUE;
            }
        }
    }
    return is_non_interruptible;
}

static bool kymera_ExtractAecRefUsageFromUser(mic_users_t user)
{
    const mic_registry_per_user_t * reg_entry;
    bool use_aec_ref = FALSE;

    reg_entry = kymera_GetRegistryEntry(user);
    if(reg_entry->use_aec_ref != NULL)
    {
        use_aec_ref = *reg_entry->use_aec_ref;
    }
    return use_aec_ref;
}

static bool kymera_CheckAecRefDueToNonInterruptibleUsers(mic_users_t new_user)
{
    if(kymera_IsAnyUserNonInterruptible(mic_user_none) || kymera_IsConnectingUserNonInterruptible(new_user))
    {
        for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
        {
            mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
            if(kymera_ExtractAecRefUsageFromUser(current_user))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static bool kymera_CheckAecRefFromUsers(mic_users_t users)
{
    for(mic_users_t user_mask = 1; user_mask < mic_user_all_mask; user_mask = user_mask << 1)
    {
        if((user_mask & users) != 0)
        {
            if(kymera_ExtractAecRefUsageFromUser(user_mask))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static bool kymera_IsAecRefRequired(mic_users_t existing_users, mic_users_t new_user, mic_users_t reconnect_users)
{
    bool aec_ref_required = FALSE;

    if(kymera_CheckAecRefDueToNonInterruptibleUsers(new_user))
    {
        DEBUG_LOG("kymera_IsAecRefRequired: Required due to non-interruptible user");
        aec_ref_required = TRUE;
    }
    else
    {
        if (kymera_CheckAecRefFromUsers(existing_users | new_user | reconnect_users))
        {
            DEBUG_LOG("kymera_IsAecRefRequired: Required due existing users");
            aec_ref_required = TRUE;
        }
    }
    return aec_ref_required;
}

static bool kymera_IsAecRefConfigChangeRequired(mic_users_t new_user)
{
    if(kymera_GetNrOfCurrentUsers() > 0)
    {
        if((state.aec_ref_offset == 0) &&
           kymera_IsAecRefRequired(state.current_users & ~mic_user_leakthrough, new_user, mic_user_none))
        {
            DEBUG_LOG("kymera_IsAecRefConfigChangeRequired: TRUE");
            return TRUE;
        }
    }
    return FALSE;
}

static void kymera_DefineUsageOfAecRef(mic_users_t reconnect_users)
{
    if(kymera_IsAecRefRequired(state.current_users & ~mic_user_leakthrough, mic_user_none, reconnect_users))
    {
        state.aec_ref_offset = 1;
    }
    else
    {
        state.aec_ref_offset = 0;
    }
    DEBUG_LOG("kymera_DefineUsageOfAecRef: aec_ref_offset = %d", state.aec_ref_offset);
}

static bool kymera_AreMicsInUse(uint8 num_of_mics, const uint16 *mic_ids)
{
    for(unsigned i = 0; i < num_of_mics; i++)
    {
        if (state.use_count[mic_ids[i]] == 0)
        {
            DEBUG_LOG("kymera_AreMicsInUse: New mic_id %d is not in use", mic_ids[i]);
            return FALSE;
        }
    }
    return TRUE;
}

static bool kymera_CheckMandatoryMicsIfNewUserIsNonInterruptible(mic_users_t new_user)
{
    if (kymera_IsConnectingUserNonInterruptible(new_user))
    {
        mic_connect_params_t combined_mic_params = { 0 };
        uint16 combined_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
        Sink combined_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
        combined_mic_params.connections.mic_ids = combined_mic_ids;
        combined_mic_params.connections.mic_sinks = combined_mic_sinks;

        kymera_AddMandatoryMicsFromAllUsers(combined_mic_ids, combined_mic_sinks, &combined_mic_params.connections.num_of_mics);

        if (!kymera_AreMicsInUse(combined_mic_params.connections.num_of_mics, combined_mic_params.connections.mic_ids))
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void kymera_TurnOnMics(uint32 sample_rate, uint8 num_of_mics, const uint16 *mic_ids, Source *mic_sources)
{
    unsigned i;
    bool create_mics = (kymera_AreMicsInUse(num_of_mics, mic_ids) == FALSE);

    if ((create_mics) || (state.mic_sample_rate < (MIN_SAMPLE_RATE_IN_KHZ * 1000)))
    {
        state.mic_sample_rate = sample_rate;
    }

    for(i = 0; i < num_of_mics; i++)
    {
        // Simply increases number of users if mic already created
        Microphones_SetMicRate(mic_ids[i], state.mic_sample_rate, non_exclusive_user);
        mic_sources[i] = Microphones_TurnOnMicrophone(mic_ids[i], non_exclusive_user);
        state.use_count[mic_ids[i]]++;
    }

    /* When TurnOnMics is called, the connecting user is not yet added to current_users. */
    if (kymera_GetNrOfCurrentUsers() == 0)
    {
        kymera_UnsynchroniseMics(num_of_mics, mic_ids, mic_sources);
        kymera_SynchroniseMics(num_of_mics, mic_ids, mic_sources);
    }
}

static void kymera_TurnOffMics(uint8 num_of_mics, const uint16 *mic_ids)
{
    unsigned i;

    for(i = 0; i < num_of_mics; i++)
    {
        PanicFalse(state.use_count[mic_ids[i]]>0);
        state.use_count[mic_ids[i]]--;
        Microphones_TurnOffMicrophone(mic_ids[i], non_exclusive_user);
    }
}

static void kymera_PopulateAecConnectParams(mic_connections_internal_t *mics,
                                            Sink aec_ref_sink, aec_connect_audio_input_t *aec_params)
{
    aec_params->reference_output = aec_ref_sink;

    switch (mics->num_of_mics)
    {
        case 4:
            aec_params->mic_input_4 = mics->mic_sources[3];
            aec_params->mic_output_4 = mics->mic_sinks[3];
        // Fallthrough
        case 3:
            aec_params->mic_input_3 = mics->mic_sources[2];
            aec_params->mic_output_3 = mics->mic_sinks[2];
        // Fallthrough
        case 2:
            aec_params->mic_input_2 = mics->mic_sources[1];
            aec_params->mic_output_2 = mics->mic_sinks[1];
        // Fallthrough
        case 1:
            aec_params->mic_input_1 = mics->mic_sources[0];
            aec_params->mic_output_1 = mics->mic_sinks[0];
            break;
        default:
            DEBUG_LOG_ERROR("kymera_PopulateAecConnectParams: Unsupported number of mics = %d", mics->num_of_mics);
            Panic();
    }
}

static void kymera_PopulateAecConfig(uint32 sample_rate, aec_audio_config_t *aec_config)
{
    aec_config->mic_sample_rate = sample_rate;
}

static void kymera_ConnectUserDirectlyToAec(mic_connections_internal_t *mics, Sink aec_ref_sink)
{
    aec_connect_audio_input_t connect_params = {0};
    aec_audio_config_t config = {0};

    kymera_PopulateAecConnectParams(mics, aec_ref_sink, &connect_params);
    kymera_PopulateAecConfig(state.mic_sample_rate, &config);
    Kymera_ConnectAudioInputToAec(&connect_params, &config);
}

static void kymera_ConnectUserToConcurrencyChain(uint8 stream_index, uint8 num_of_mics, const Sink *mic_sinks,
                                                 uint32 sample_rate, Sink aec_ref_sink)
{
    unsigned i;
    bool use_resampler = (sample_rate != state.mic_sample_rate);
    Sink local_sinks[1 + MAX_NUM_OF_CONCURRENT_MICS] = {0};

    if (use_resampler)
    {
        Kymera_MicResamplerCreate(stream_index, state.mic_sample_rate, sample_rate);
        if (aec_ref_sink)
        {
            PanicNull(StreamConnect(Kymera_MicResamplerGetAecOutput(stream_index), aec_ref_sink));
            aec_ref_sink = Kymera_MicResamplerGetAecInput(stream_index);
        }
        for(i = 0; i < num_of_mics; i++)
        {
            if (mic_sinks[i])
            {
                PanicNull(StreamConnect(Kymera_MicResamplerGetMicOutput(stream_index, i), mic_sinks[i]));
                local_sinks[i + state.aec_ref_offset] = Kymera_MicResamplerGetMicInput(stream_index, i);
            }
        }
    }
    else
    {
        memcpy(&local_sinks[state.aec_ref_offset], mic_sinks, num_of_mics * sizeof(local_sinks[0]));
    }

    if(state.aec_ref_offset != 0)
    {
        local_sinks[0] = aec_ref_sink;  /* will be overwritten in case no aec_ref is needed */
    }

    DEBUG_LOG("kymera_ConnectUserToConcurrencyChain");
    Kymera_SplitterConnectToOutputStream(&state.splitter, stream_index, local_sinks);

    if (use_resampler)
    {
        Kymera_MicResamplerStart(stream_index);
    }
}

static void kymera_ConnectSplitterChainToAec(uint8 num_of_mics, Source *mic_sources)
{
    mic_connections_internal_t mics = {0};
    Sink mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    mics.mic_sinks = mic_sinks;
    mics.mic_sources = mic_sources;
    mics.num_of_mics = num_of_mics;
    Sink aec_sink = NULL;

    if(state.aec_ref_offset != 0)
    {
        aec_sink = Kymera_SplitterGetInput(&state.splitter, 0);
    }

    for(unsigned i = 0; i < mics.num_of_mics; i++)
    {
        mic_sinks[i] = Kymera_SplitterGetInput(&state.splitter, i + state.aec_ref_offset);
    }

    if (Kymera_AecIsMicPathInputConnected())
    {
        // All mics should have already been connected to AEC reference
        aec_mic_path_output_t connect_params = {0};
        connect_params.aec_reference = aec_sink;
        connect_params.num_of_mics = mics.num_of_mics;
        connect_params.mics = mics.mic_sinks;
        Kymera_ConnectToAecMicPathOutput(&connect_params);
    }
    else
    {
        aec_connect_audio_input_t connect_params = {0};
        aec_audio_config_t config = {0};
        kymera_PopulateAecConfig(state.mic_sample_rate, &config);
        kymera_PopulateAecConnectParams(&mics, aec_sink, &connect_params);
        Kymera_ConnectAudioInputToAec(&connect_params, &config);
    }
}

static void kymera_SelectMicType(mic_connect_params_t combined_mic_params, Source *mic_sources,
                                 mic_type_t selected_mic_type, mic_connections_internal_t *selected_mics)
{
    uint8 new_num_of_mics = 0;

    for(unsigned i = 0; i < combined_mic_params.connections.num_of_mics; i++)
    {
        if(kymera_DoesMicTypeMatchSelected(combined_mic_params.connections.mic_ids[i], selected_mic_type))
        {
            DEBUG_LOG("kymera_SelectMicType: mic_id %d enum:mic_type_t:%d mic_sink 0x%x",
                      combined_mic_params.connections.mic_ids[i], selected_mic_type, combined_mic_params.connections.mic_sinks[i]);
            selected_mics->mic_sinks[new_num_of_mics] = combined_mic_params.connections.mic_sinks[i];
            selected_mics->mic_sources[new_num_of_mics] = mic_sources[i];
            new_num_of_mics++;
        }
    }
    selected_mics->num_of_mics = new_num_of_mics;
}

static void kymera_ConnectUserWithMic(mic_users_t user, mic_connections_internal_t *selected_mics, uint16 client_sample_rate)
{
    /* The first set of resamplers is used for normal splitter connection.
       The second set of resamplers is used for direct mic - client connections */
    uint8 stream_index = kymera_GetStreamIndex(user) + MAX_NUM_OF_CONCURRENT_MIC_USERS;
    uint8 resampler_index = 0;
    bool use_resampler = FALSE;
    unsigned mic_sample_rate = DEFAULT_MIC_RATE;

    if(selected_mics->num_of_mics == 0)
    {
        return;
    }

#ifdef INCLUDE_LIS25BA_ACCELEROMETER
    mic_sample_rate = 16000; /* LIS25BA always runs with a pcm sample rate of 16kHz */
    use_resampler = (client_sample_rate != mic_sample_rate);
#else
    DEBUG_LOG_PANIC("kymera_ConnectUserWithMic: Unknown PCM mic detected.");
#endif

    if (use_resampler)
    {
        Kymera_MicResamplerCreate(stream_index, mic_sample_rate, client_sample_rate);
        for(unsigned i = 0; i < selected_mics->num_of_mics; i++)
        {
            if(selected_mics->mic_sinks[i] != NULL)
            {
                DEBUG_LOG("kymera_ConnectUserWithMic: using resampler: source 0x%x sink 0x%x", selected_mics->mic_sources[i], selected_mics->mic_sinks[i]);
                PanicNull(StreamConnect(Kymera_MicResamplerGetMicOutput(stream_index, resampler_index), selected_mics->mic_sinks[i]));
                PanicNull(StreamConnect(selected_mics->mic_sources[i], Kymera_MicResamplerGetMicInput(stream_index, resampler_index)));
                resampler_index ++;
            }
        }
    }
    else
    {
        for(unsigned i = 0; i < selected_mics->num_of_mics; i++)
        {
            if(selected_mics->mic_sinks[i] != NULL)
            {
                DEBUG_LOG("kymera_ConnectUserWithMic: source 0x%x sink 0x%x", selected_mics->mic_sources[i], selected_mics->mic_sinks[i]);
                PanicNull(StreamConnect(selected_mics->mic_sources[i], selected_mics->mic_sinks[i]));
            }
        }
    }

    if (use_resampler)
    {
        Kymera_MicResamplerStart(stream_index);
    }
}

static void kymera_DisconnectUserFromMic(mic_users_t user, mic_connections_internal_t *selected_mics)
{
    /* The first set of resamplers is used for normal splitter connection.
       The second set of resamplers is used for direct mic - client connections */
    uint8 stream_index = kymera_GetStreamIndex(user) + MAX_NUM_OF_CONCURRENT_MIC_USERS;

    if (Kymera_MicResamplerIsCreated(stream_index))
    {
        Kymera_MicResamplerStop(stream_index);
    }

    for(unsigned i = 0; i < selected_mics->num_of_mics; i++)
    {
        if(selected_mics->mic_sinks[i] != NULL)
        {
            DEBUG_LOG("kymera_DisconnectUserFromMic: source 0x%x", selected_mics->mic_sources[i]);
            StreamDisconnect(selected_mics->mic_sources[i], NULL);
        }
    }


    if (Kymera_MicResamplerIsCreated(stream_index))
    {
        Kymera_MicResamplerDestroy(stream_index);
    }
}

static void kymera_ConnectUserViaConcurrencyChain(mic_users_t user, mic_connections_internal_t *selected_mics, uint32 sample_rate, Sink aec_ref_sink)
{
    uint8 stream_index = kymera_GetStreamIndex(user);

    if (state.splitter == NULL)
    {
        state.splitter = Kymera_SplitterCreate(MAX_NUM_OF_CONCURRENT_MIC_USERS, state.aec_ref_offset + selected_mics->num_of_mics, &splitter_config);
        kymera_ConnectSplitterChainToAec(selected_mics->num_of_mics, selected_mics->mic_sources);
    }

    kymera_ConnectUserToConcurrencyChain(stream_index, selected_mics->num_of_mics, selected_mics->mic_sinks, sample_rate, aec_ref_sink);

    Kymera_SplitterStartOutputStream(&state.splitter, stream_index);
}

static void kymera_DisconnectUserFromConcurrencyChain(mic_users_t user)
{
    uint8 stream_index = kymera_GetStreamIndex(user);

    if (Kymera_MicResamplerIsCreated(stream_index))
    {
        Kymera_MicResamplerStop(stream_index);
    }

    Kymera_SplitterDisconnectFromOutputStream(&state.splitter, stream_index);

    if (Kymera_MicResamplerIsCreated(stream_index))
    {
        Kymera_MicResamplerDestroy(stream_index);
    }

    if ((state.current_users & ~mic_user_leakthrough) == user)
    {
        // Destroy splitter and disconnect from AEC since there are no other users
        Kymera_SplitterDestroy(&state.splitter);
        state.aec_ref_offset = 0;
    }
}

static void kymera_UserGetConnectionParameters(mic_users_t user, mic_connect_params_t *mic_params, uint16 *mic_ids, Sink *mic_sinks, Sink *aec_ref_sink)
{
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    reg_entry->callbacks->MicGetConnectionParameters(mic_ids, mic_sinks, &mic_params->connections.num_of_mics, &mic_params->sample_rate, aec_ref_sink);
}

static void kymera_AddToOrderedListOfMics(uint16 *mic_ids, Sink *mic_sinks, uint8 *num_of_mics, uint16 new_mic_id, Sink new_mic_sink)
{
    PanicFalse(*num_of_mics < MAX_NUM_OF_CONCURRENT_MICS);
    DEBUG_LOG("kymera_AddToOrderedListOfMics: new_mic_id %d Sink 0x%x", new_mic_id ,new_mic_sink);

    int8 insert_idx;

    for(insert_idx = (MAX_NUM_OF_CONCURRENT_MICS-1); insert_idx >= 0; insert_idx--)
    {
        if((mic_ids[insert_idx] != MICROPHONE_NONE) && (mic_ids[insert_idx] < new_mic_id))
        {
            break;
        }
    }
    insert_idx++;
    PanicFalse(insert_idx < MAX_NUM_OF_CONCURRENT_MICS);

    for(int8 i = *num_of_mics; i > insert_idx; i--)
    {
        mic_ids[i]   = mic_ids[i-1];
        mic_sinks[i] = mic_sinks[i-1];
    }

    mic_ids[insert_idx] = new_mic_id;
    mic_sinks[insert_idx] = new_mic_sink;
    (*num_of_mics)++;
}

static void kymera_AddMicsToOrderedList(const uint16 *current_mic_ids, const Sink *current_mic_sinks, uint8 current_num_of_mics,
                                        uint16 *ordered_mic_ids, Sink *ordered_mic_sinks, uint8 *ordered_num_of_mics)
{
    for(unsigned i = 0; i < current_num_of_mics; i++)
    {
        PanicFalse(current_mic_ids[i] != MICROPHONE_NONE);

        bool already_available = FALSE;

        for(unsigned j = 0; j < *ordered_num_of_mics; j++)
        {
            // This needs to be cast to avoid an IncompatibleQualified warning by the compiler
            // As this is just comparing by value the warning seems out of place and should be safe to workaround
            if (ordered_mic_ids[j] == (uint16)current_mic_ids[i])
            {
                already_available = TRUE;
                break;
            }
        }

        if(!already_available)
        {
            kymera_AddToOrderedListOfMics(ordered_mic_ids, ordered_mic_sinks, ordered_num_of_mics,
                                          current_mic_ids[i], current_mic_sinks[i]);
        }
    }
}

static void kymera_AddMandatoryMicsFromUser(mic_users_t user, uint16 *mic_ids, Sink *mic_sinks, uint8 *num_of_mics)
{
    Sink empty_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);

    DEBUG_LOG("kymera_AddMandatoryMicsFromUser: Checking for enum:mic_users_t:%d", user);
    kymera_AddMicsToOrderedList(reg_entry->mandatory_mic_ids, empty_mic_sinks, reg_entry->num_of_mandatory_mics, mic_ids, mic_sinks, num_of_mics);
}

static void kymera_AddMandatoryMicsFromAllUsers(uint16 *mic_ids, Sink *mic_sinks, uint8 *num_of_mics)
{
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        kymera_AddMandatoryMicsFromUser(current_user, mic_ids, mic_sinks, num_of_mics);
    }
}

static uint32 kymera_ExtractTaskPeriodFromRegistry(mic_users_t user)
{
    uint32 task_period = 0;
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    if(reg_entry->mandatory_task_period_us)
    {
        task_period = *reg_entry->mandatory_task_period_us;
    }
    DEBUG_LOG("kymera_ExtractTaskPeriodFromRegistry: enum:mic_users_t:%d: %d us", user, task_period);
    return task_period;
}

static uint32 kymera_CollectMinTaskPeriodFromRegisteredUsers(mic_users_t users)
{
    uint32 min_task_period = 2000;
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if(((users & current_user) != 0) || (users == mic_user_none))
        {
            uint32 task_period = kymera_ExtractTaskPeriodFromRegistry(current_user);
            if(task_period > 0)
            {
                min_task_period = MIN(task_period, min_task_period);
            }
        }
    }
    return min_task_period;
}

static uint32 kymera_ExtractTtpDelayFromRegistry(mic_users_t user)
{
    uint32 ttp_delay = 0;
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    if(reg_entry->mandatory_ttp_delay_us)
    {
        ttp_delay = *reg_entry->mandatory_ttp_delay_us;
    }
    DEBUG_LOG("kymera_ExtractTtpDelayFromRegistry: enum:mic_users_t:%d: %d us", user, ttp_delay);
    return ttp_delay;
}

static uint32 kymera_CollectTtpDelayFromAllUsers(void)
{
    uint32 collected_ttp_delay = 0;
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        uint32 ttp_delay = kymera_ExtractTtpDelayFromRegistry(current_user);
        if(ttp_delay > 0)
        {
            if(collected_ttp_delay != 0)
            {
                DEBUG_LOG_ERROR("kymera_CollectTtpDelayFromAllUsers: enum:mic_users_t:%d collected_ttp_delay %u ttp_delay %u",
                                current_user, collected_ttp_delay, ttp_delay);
                PanicFalse(collected_ttp_delay == ttp_delay);
            }
            collected_ttp_delay = ttp_delay;
        }
    }
    return collected_ttp_delay;
}

static bool kymera_ExtractTimestampsFromRegistry(mic_users_t user)
{
    bool timestamps = FALSE;
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    timestamps = reg_entry->activate_timestamps;
    DEBUG_LOG("kymera_ExtractTimestampsFromRegistry: enum:mic_users_t:%d: %d", user, timestamps);
    return timestamps;
}

static bool kymera_HasAnyUserRequestedTimestamps(void)
{
    bool timestamps = FALSE;
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if(kymera_ExtractTimestampsFromRegistry(current_user))
        {
            timestamps = TRUE;
            break;
        }
    }
    return timestamps;
}

static void kymera_PopulateMicSources(mic_users_t user, const mic_connect_params_t *mic_params, mic_users_t reconnect_users,
                                      uint16 *combined_mic_ids, Sink *combined_mic_sinks,
                                      uint8 *combined_num_of_mics, uint32 *combined_sample_rate)
{
    if (user != mic_user_none)
    {
        /* Populate combined array with sorted mic_params if available */
        DEBUG_LOG("kymera_PopulateMicSources: Fill with enum:mic_users_t:%d",user);
        *combined_sample_rate = MAX(mic_params->sample_rate, (MIN_SAMPLE_RATE_IN_KHZ * 1000));
        kymera_AddMicsToOrderedList(mic_params->connections.mic_ids, mic_params->connections.mic_sinks, mic_params->connections.num_of_mics,
                                    combined_mic_ids, combined_mic_sinks, combined_num_of_mics);

        if(state.current_users != mic_user_none)
        {
            DEBUG_LOG("kymera_PopulateMicSources: Adding existing users enum:mic_users_t:%d",state.current_users);
            kymera_CollectMicsFromMultipleUsers(state.current_users, combined_mic_ids, combined_mic_sinks,
                                                  combined_num_of_mics, combined_sample_rate);
        }
    }

    if (reconnect_users != mic_user_none)
    {
        /* Collect mic params from all users that want to be reconnected */
        DEBUG_LOG("kymera_PopulateMicSources: Adding reconnect users enum:mic_users_t:%d",reconnect_users);
        kymera_CollectMicsFromMultipleUsers(reconnect_users, combined_mic_ids, combined_mic_sinks,
                                              combined_num_of_mics, combined_sample_rate);
    }

    /* If one of the active users is non-interruptible or the newly connecting user is non-interruptible:
     * add the mandatory mics
     */
    if ((kymera_IsAnyUserNonInterruptible(mic_user_none)) ||
        (kymera_IsConnectingUserNonInterruptible(user)))
    {
        DEBUG_LOG("kymera_PopulateMicSources: Non-interruptible user: Checking mandatory mics");
        kymera_AddMandatoryMicsFromAllUsers(combined_mic_ids, combined_mic_sinks, combined_num_of_mics);
    }
}

static bool kymera_UserDisconnectIndication(mic_users_t user, mic_change_info_t *info)
{
    bool want_to_reconnect = FALSE;
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    if (reg_entry->callbacks->MicDisconnectIndication)
    {
        DEBUG_LOG("kymera_UserDisconnectIndication: informing enum:mic_users_t:%d, enum:mic_event_t:%d", user, info->event);
        want_to_reconnect = reg_entry->callbacks->MicDisconnectIndication(info);
        DEBUG_LOG("kymera_UserDisconnectIndication: enum:mic_users_t:%d want_to_reconnect=%d", user, want_to_reconnect);
    }
    return want_to_reconnect;
}

static void kymera_UserReadyForReconnectionIndication(mic_users_t user, mic_change_info_t *info)
{
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    if (reg_entry->callbacks->MicReadyForReconnectionIndication)
    {
        DEBUG_LOG("kymera_UserReadyForReconnectionIndication: enum:mic_users_t:%d enum:mic_event_t:%d",
                  user, info->event);
        reg_entry->callbacks->MicReadyForReconnectionIndication(info);
    }
}

static void kymera_UserUpdatedStateIndication(mic_users_t user)
{
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    if (reg_entry->callbacks->MicUserUpdatedState)
    {
        DEBUG_LOG("kymera_UserUpdatedStateIndication: enum:mic_users_t:%d", user);
        reg_entry->callbacks->MicUserUpdatedState();
    }
}

static void kymera_MicUserChangePendingNotification(mic_users_t user, mic_change_info_t *info)
{
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    if (reg_entry->callbacks->MicUserChangePendingNotification)
    {
        DEBUG_LOG("kymera_MicUserChangePendingNotification: sent to enum:mic_users_t:%d", user);
        reg_entry->callbacks->MicUserChangePendingNotification(info);
    }
}

static void kymera_SendMicUserChangePendingNotificationToAllUsers(mic_users_t user, mic_change_info_t *info)
{
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if((kymera_IsCurrentUser(current_user)) && (current_user != user))
        {
            kymera_MicUserChangePendingNotification(current_user, info);
        }
    }
}

static void kymera_UserReconnectedIndication(mic_users_t user)
{
    const mic_registry_per_user_t * reg_entry;
    reg_entry = kymera_GetRegistryEntry(user);
    if (reg_entry->callbacks->MicReconnectedIndication)
    {
        DEBUG_LOG("kymera_UserReconnectedIndication: user enum:mic_users_t:%d reconnected", user);
        reg_entry->callbacks->MicReconnectedIndication();
    }
}

static void kymera_SendReconnectedIndicationToAllUsers(mic_users_t reconnected_users)
{
    if (kymera_IsMicConcurrencyEnabled())
    {
        if (reconnected_users != mic_user_none)
        {
            for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
            {
                mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
                if ((reconnected_users & current_user) != 0)
                {
                    kymera_UserReconnectedIndication(current_user);
                }
            }
        }
    }
}

static void kymera_ConnectLeakthrough(const mic_connect_params_t *mic_params, Source *mic_sources)
{
    kymera_TurnOnMics(mic_params->sample_rate,
                      mic_params->connections.num_of_mics,
                      mic_params->connections.mic_ids,
                      mic_sources);

    if(state.current_users == mic_user_none)
    {
        aec_audio_config_t config = {0};
        aec_mic_path_input_t connect_params = {0};
        connect_params.num_of_mics = mic_params->connections.num_of_mics;
        connect_params.mics = mic_sources;
        kymera_PopulateAecConfig(state.mic_sample_rate, &config);
        Kymera_ConnectToAecMicPathInput(&connect_params, &config);
    }
    kymera_AddToCurrentUsers(mic_user_leakthrough);
    kymera_SetAecRefUseCase(state.current_users);
}

static void kymera_ReconnectAllUsers(mic_users_t new_user, mic_users_t reconnect_users, Source *mic_sources)
{
    if (reconnect_users != mic_user_none)
    {
        for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
        {
            mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
            if ((reconnect_users & current_user) != 0)
            {
                mic_connect_params_t local_mic_params = { 0 };
                uint16 local_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
                Sink local_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
                local_mic_params.connections.mic_ids = local_mic_ids;
                local_mic_params.connections.mic_sinks = local_mic_sinks;

                mic_connect_params_t combined_mic_params = { 0 };
                uint16 combined_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
                Sink combined_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
                combined_mic_params.connections.mic_ids = combined_mic_ids;
                combined_mic_params.connections.mic_sinks = combined_mic_sinks;

                mic_connections_internal_t selected_mics = {0};
                Sink selected_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
                Source selected_mic_sources[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
                selected_mics.mic_sinks = selected_mic_sinks;
                selected_mics.mic_sources = selected_mic_sources;

                Sink aec_ref_sink;

                DEBUG_LOG("kymera_ReconnectAllUsers: enum:mic_users_t:%d", current_user);
                kymera_UserGetConnectionParameters(current_user, &local_mic_params, local_mic_ids, local_mic_sinks, &aec_ref_sink);
                kymera_AddMicsToOrderedList(local_mic_params.connections.mic_ids, local_mic_params.connections.mic_sinks, local_mic_params.connections.num_of_mics,
                                            combined_mic_ids, combined_mic_sinks, &combined_mic_params.connections.num_of_mics);

                kymera_CollectMicsFromMultipleUsers(reconnect_users | new_user, combined_mic_ids, combined_mic_sinks,
                                                      &combined_mic_params.connections.num_of_mics, &combined_mic_params.sample_rate);

                if (kymera_IsAnyUserNonInterruptible(mic_user_none))
                {
                    kymera_AddMandatoryMicsFromAllUsers(combined_mic_ids, combined_mic_sinks, &combined_mic_params.connections.num_of_mics);
                }

                if(current_user != mic_user_leakthrough)
                {
                    /* Register each non-exclusive user, mics are already turned on */
                    kymera_TurnOnMics(combined_mic_params.sample_rate, combined_mic_params.connections.num_of_mics,
                                      combined_mic_params.connections.mic_ids, mic_sources);

                    kymera_AddMicUser(current_user);
                    kymera_DefineUsageOfAecRef(reconnect_users | new_user);

                    kymera_SelectMicType(combined_mic_params, state.mic_sources, (mic_type_analog | mic_type_digital), &selected_mics);
                    kymera_ConnectUserViaConcurrencyChain(current_user, &selected_mics, local_mic_params.sample_rate, aec_ref_sink);

                    kymera_SelectMicType(combined_mic_params, state.mic_sources, mic_type_pcm, &selected_mics);
                    kymera_ConnectUserWithMic(current_user, &selected_mics, local_mic_params.sample_rate);
                }
                else
                {
                    kymera_ConnectLeakthrough(&combined_mic_params, mic_sources);
                }
            }
        }
        Kymera_LeakthroughSetupSTGain();
    }
}

static void kymera_ConnectUserToMics(mic_users_t user, const mic_connect_params_t *mic_params, Sink aec_ref_sink, mic_users_t reconnect_users)
{
    mic_connect_params_t local_mic_params = {0};
    uint16 local_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
    Sink local_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    local_mic_params.connections.mic_ids = local_mic_ids;
    local_mic_params.connections.mic_sinks = local_mic_sinks;

    mic_connections_internal_t selected_mics = {0};
    Sink selected_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    Source selected_mic_sources[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    selected_mics.mic_sinks = selected_mic_sinks;
    selected_mics.mic_sources = selected_mic_sources;

    if (kymera_IsMicConcurrencyEnabled())
    {
        kymera_PopulateMicSources(user, mic_params, reconnect_users, local_mic_ids, local_mic_sinks,
                                  &local_mic_params.connections.num_of_mics, &local_mic_params.sample_rate);
        kymera_TurnOnMics(local_mic_params.sample_rate, local_mic_params.connections.num_of_mics,
                          local_mic_params.connections.mic_ids, state.mic_sources);

        Kymera_AecSetTtpDelay(kymera_CollectTtpDelayFromAllUsers());

        /* Connect new user first */
        kymera_AddMicUser(user);
        kymera_DefineUsageOfAecRef(reconnect_users);

        kymera_SelectMicType(local_mic_params, state.mic_sources, (mic_type_analog | mic_type_digital), &selected_mics);
        kymera_ConnectUserViaConcurrencyChain(user, &selected_mics, mic_params->sample_rate, aec_ref_sink);

        kymera_SelectMicType(local_mic_params, state.mic_sources, mic_type_pcm, &selected_mics);
        kymera_ConnectUserWithMic(user, &selected_mics, mic_params->sample_rate);

        kymera_ReconnectAllUsers(user, reconnect_users, state.mic_sources);
        kymera_SendReconnectedIndicationToAllUsers(reconnect_users);
    }
    else
    {
        DEBUG_LOG("kymera_ConnectUserToMics: Concurrency disabled. Using legacy mode");
        kymera_TurnOnMics(mic_params->sample_rate, mic_params->connections.num_of_mics, mic_params->connections.mic_ids, state.mic_sources);
        kymera_AddMicUser(user);

        selected_mics.num_of_mics = mic_params->connections.num_of_mics;
        memcpy(&selected_mics.mic_sinks, mic_params->connections.mic_sinks, selected_mics.num_of_mics * sizeof(mic_params->connections.mic_sinks[0]));
        selected_mics.mic_sources = state.mic_sources;

        kymera_ConnectUserDirectlyToAec(&selected_mics, aec_ref_sink);
    }
}

static mic_users_t kymera_InformUsersAboutDisconnection(mic_users_t user, mic_change_info_t *info)
{
    mic_users_t reconnect_users = 0;

    /* Inform active users about disconnection */
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if((kymera_IsCurrentUser(current_user)) && (current_user != user))
        {
            /* Active user found -> send disconnect indication */
            bool reconnect_request = kymera_UserDisconnectIndication(current_user, info);
            if (reconnect_request)
            {
                /* Collect which user wants to be reconnected */
                reconnect_users |= current_user;
            }
            else
            {
                /* Inform others if a user does not want to be reconnected again */
                info->user = current_user;
                info->event = mic_event_disconnecting;
                kymera_SendMicUserChangePendingNotificationToAllUsers(current_user, info);
            }
        }
    }
    return reconnect_users;
}

static void kymera_InformUsersAboutReadyForReconnection(mic_users_t new_user, mic_users_t users_to_be_informed, mic_change_info_t *info)
{
    /* Inform active users about ReadyForReconnection */
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if(((current_user & users_to_be_informed) != mic_user_none) && (current_user != new_user))
        {
            /* user found -> send ReadyForReconnection indication */
            kymera_UserReadyForReconnectionIndication(current_user, info);
        }
    }
}

static void kymera_SendUpdatedStateIndication(mic_users_t user)
{
    /* Inform active users about a change in state */
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if((kymera_IsCurrentUser(current_user)) && (current_user != user))
        {
            kymera_UserUpdatedStateIndication(current_user);
        }
    }
}

static bool kymera_IsAnyUserNonInterruptible(mic_users_t exclude_user)
{
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if((kymera_IsCurrentUser(current_user)) && (current_user != exclude_user))
        {
            mic_user_state_t current_state = kymera_ExtractStateFromRegistry(user_index);
            if (current_state == mic_user_state_non_interruptible)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static bool kymera_IsAnyUserAlwaysInterrupt(mic_users_t exclude_user)
{
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if((kymera_IsCurrentUser(current_user)) && (current_user != exclude_user))
        {
            mic_user_state_t current_state = kymera_ExtractStateFromRegistry(user_index);
            if (current_state == mic_user_state_always_interrupt)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static void kymera_DisconnectAllUsers(void)
{
    mic_users_t existing_users = state.current_users;
    uint8 num_of_mics_to_unsync = 0;
    uint16 mic_ids_to_unsync[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};

    bool is_any_user_non_interruptible = kymera_IsAnyUserNonInterruptible(mic_user_none);
    for(uint8 user_index = 0; user_index < state.registry.nr_entries; user_index++)
    {
        mic_users_t current_user = kymera_ExtractUserFromRegistry(user_index);
        if(kymera_IsCurrentUser(current_user))
        {
            mic_connect_params_t local_mic_params = { 0 };
            uint16 local_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
            Sink local_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
            local_mic_params.connections.mic_ids = local_mic_ids;
            local_mic_params.connections.mic_sinks = local_mic_sinks;

            mic_connect_params_t combined_mic_params = { 0 };
            uint16 combined_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
            Sink combined_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
            combined_mic_params.connections.mic_ids = combined_mic_ids;
            combined_mic_params.connections.mic_sinks = combined_mic_sinks;

            mic_connections_internal_t selected_mics = {0};
            Sink selected_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
            Source selected_mic_sources[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
            selected_mics.mic_sinks = selected_mic_sinks;
            selected_mics.mic_sources = selected_mic_sources;

            Sink aec_ref_sink;

            if (kymera_IsMicConcurrencyEnabled())
            {
                kymera_UserGetConnectionParameters(current_user, &local_mic_params, local_mic_ids, local_mic_sinks, &aec_ref_sink);
                combined_mic_params.sample_rate = MAX(local_mic_params.sample_rate, (MIN_SAMPLE_RATE_IN_KHZ * 1000));
                kymera_AddMicsToOrderedList(local_mic_params.connections.mic_ids, local_mic_params.connections.mic_sinks, local_mic_params.connections.num_of_mics,
                                            combined_mic_ids, combined_mic_sinks, &combined_mic_params.connections.num_of_mics);

                kymera_CollectMicsFromMultipleUsers(existing_users, combined_mic_ids, combined_mic_sinks,
                                                      &combined_mic_params.connections.num_of_mics, &combined_mic_params.sample_rate);

                if (is_any_user_non_interruptible)
                {
                    kymera_AddMandatoryMicsFromAllUsers(combined_mic_ids, combined_mic_sinks, &combined_mic_params.connections.num_of_mics);
                }

                DEBUG_LOG("kymera_DisconnectAllUsers: enum:mic_users_t:%d", current_user);
                if(current_user != mic_user_leakthrough)
                {
                    kymera_DisconnectUserFromConcurrencyChain(current_user);

                    kymera_SelectMicType(combined_mic_params, state.mic_sources, mic_type_pcm, &selected_mics);
                    kymera_DisconnectUserFromMic(current_user, &selected_mics);

                    kymera_RemoveMicUser(current_user);
                }
                else
                {
                    kymera_RemoveFromCurrentUsers(current_user);
                    kymera_SetAecRefUseCase(state.current_users);
                }
                kymera_TurnOffMics(combined_mic_params.connections.num_of_mics, combined_mic_ids);
                /* All users have the same microphones registered and connected.
                 * Therefore it is possible to use the num_of_mics from the last user for unsynchronise mics */
                num_of_mics_to_unsync = combined_mic_params.connections.num_of_mics;
                memcpy(mic_ids_to_unsync, combined_mic_ids, num_of_mics_to_unsync * sizeof(combined_mic_ids[0]));
            }
            else
            {
                kymera_UserGetConnectionParameters(current_user, &local_mic_params, local_mic_ids, local_mic_sinks, &aec_ref_sink);
                kymera_RemoveMicUser(current_user);
                kymera_TurnOffMics(local_mic_params.connections.num_of_mics, local_mic_ids);
                num_of_mics_to_unsync = local_mic_params.connections.num_of_mics;
                memcpy(mic_ids_to_unsync, local_mic_ids, num_of_mics_to_unsync * sizeof(local_mic_ids[0]));
            }
        }
    }
    Kymera_DisconnectAudioInputFromAec();
    kymera_UnsynchroniseMics(num_of_mics_to_unsync, mic_ids_to_unsync, state.mic_sources);
}

static bool kymera_IsTaskPeriodChangeRequired(mic_users_t new_user)
{
    bool is_required = FALSE;
    if (Kymera_AecIsMicPathInputConnected())
    {
        uint32 task_period = kymera_ExtractTaskPeriodFromRegistry(new_user);
        if (task_period > 0)
        {
            is_required = (Kymera_AecGetTaskPeriod() > task_period);
        }
    }
    return is_required;
}

static bool kymera_PrepareForConnection(mic_users_t new_user, const mic_connect_params_t *mic_params, mic_users_t *reconnect_users)
{
    mic_change_info_t info = {0};
    bool non_interruptible_user_present;
    mic_users_t inform_users;

    if (state.current_users == mic_user_none)
    {
        return TRUE;
    }

    /* If concurrency is enabled:
           1) Mics in use must include all mics requested (you can only sync mics and connect to AEC REF once)
           2) Sample rate requested cannot be higher than sample rate of mics in use
    */
    if (kymera_IsMicConcurrencyEnabled())
    {
        info.user = new_user;
        info.event = mic_event_connecting;

        if ((new_user == mic_user_leakthrough) && (Kymera_GetAecUseCase() < aec_usecase_enable_leakthrough))
        {
            info.event |= mic_event_ucid_change_required;
        }

        if (kymera_IsTaskPeriodChangeRequired(new_user))
        {
            info.event |= mic_event_task_period_change_required;
        }

        if (state.mic_sample_rate < mic_params->sample_rate)
        {
            info.event |= mic_event_higher_sample_rate;
        }

        if (!kymera_AreMicsInUse(mic_params->connections.num_of_mics, mic_params->connections.mic_ids))
        {
            info.event |= mic_event_extra_mic;
        }
        if (kymera_CheckMandatoryMicsIfNewUserIsNonInterruptible(new_user))
        {
           info.event |= mic_event_extra_mic;
        }
        if (kymera_IsAecRefConfigChangeRequired(new_user))
        {
            info.event |= mic_event_aec_ref_change_required;
        }

        kymera_SendMicUserChangePendingNotificationToAllUsers(new_user, &info);

        if ((info.event == mic_event_connecting) && (!kymera_IsAnyUserAlwaysInterrupt(new_user)))
        {
            /* No change in sample rate or extra mic necessary. Connection is possible */
            /* In case always_interrupt: User wants to be disconnected / reconnected every time -> skip the return */
            return TRUE;
        }

        /* Conflicting mic params detected or state = always_interrupt */
        non_interruptible_user_present = kymera_IsAnyUserNonInterruptible(new_user);
        if (!non_interruptible_user_present)
        {
            DEBUG_LOG("kymera_PrepareForConnection: Connection is possible. non_interruptible:%d enum:mic_event_t:%d",
                      non_interruptible_user_present, info.event);
            *reconnect_users = kymera_InformUsersAboutDisconnection(new_user, &info);
            inform_users = state.current_users;
            kymera_DisconnectAllUsers();
            kymera_InformUsersAboutReadyForReconnection(new_user, inform_users, &info);
            return TRUE;
        }

        /* At least one user is non_interruptible */
        DEBUG_LOG("kymera_PrepareForConnection: Conflict detected. Connection is not possible. enum:mic_event_t:%d", info.event);
        return FALSE;
    }
    return TRUE;
}

static bool kymera_PrepareForDisconnection(mic_users_t user, mic_change_info_t *info, mic_users_t *reconnect_users)
{
    if (kymera_IsMicConcurrencyEnabled())
    {
        /* If concurrency is enabled:
               1) Disconnection of mics is done when only one user is active
               2) If more than one user is active, the remaining users are checked for interruptibility.
                  - If all remaining users are interruptible, reconnection is done
                  - If at least one remaining user is non-interruptible no reconnection is allowed
        */

        /* User to be disconnected is the only user -> disconnect mics */
        if (state.current_users == user)
            return TRUE;

        kymera_SendMicUserChangePendingNotificationToAllUsers(user, info);
        if (!kymera_IsAnyUserNonInterruptible(user))
        {
            *reconnect_users = kymera_InformUsersAboutDisconnection(user, info);
            return TRUE;
        }
        /* At least one user is non_interruptible: Do not disconnect the mics */
        return FALSE;
    }
    /* If concurrency is disabled:
          Only single client connections are expected.
          A disconnection is always allowed.
    */
    return TRUE;
}

static void kymera_PreserveSources(Source *array, unsigned length_of_array)
{
    PanicFalse(OperatorFrameworkPreserve(0, NULL, length_of_array, array, 0, NULL));
}

static void kymera_ReleaseSources(Source *array, unsigned length_of_array)
{
    PanicFalse(OperatorFrameworkRelease(0, NULL, length_of_array, array, 0, NULL));
}

static void kymera_RunOnAllMics(SourceFunction function)
{
    uint8 number_of_mics = 0;
    Source source, mics[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};

    for(uint8 mic_id = 0; mic_id < MAX_SUPPORTED_MICROPHONES; mic_id++)
    {
        if(state.use_count[mic_id] > 0)
        {
            source = PanicNull(Microphones_GetMicrophoneSource(mic_id));
            PanicFalse(number_of_mics < MAX_NUM_OF_CONCURRENT_MICS);
            mics[number_of_mics] = source;
            number_of_mics++;
        }
    }

    function(mics, number_of_mics);
}

static void kymera_Sleep(void)
{
    bool result = state.current_users != 0 &&                           /* At least one user */
                 (state.wake_states & (state.current_users)) == 0;      /* and they're all asleep */
    if (result && state.chains_are_awake)
    {
        kymera_RunOnAllMics(kymera_PreserveSources);
        Kymera_MicResamplerSleep();
        Kymera_SplitterSleep(&state.splitter);
        Kymera_AecSleep();
        state.chains_are_awake = FALSE;
    }
}

static void kymera_Wake(void)
{
    if (!state.chains_are_awake)
    {
        Kymera_AecWake();
        Kymera_SplitterWake(&state.splitter);
        Kymera_MicResamplerWake();
        kymera_RunOnAllMics(kymera_ReleaseSources);
        state.chains_are_awake = TRUE;
    }
}

void Kymera_MicUserUpdatedState(mic_users_t user)
{
    kymera_SendUpdatedStateIndication(user);
}

void Kymera_MicRegisterUser(const mic_registry_per_user_t * const info)
{
    DEBUG_LOG("Kymera_MicRegisterUser: enum:mic_users_t:%d", info->user);

    state.registry.entry = PanicNull(realloc(state.registry.entry, (state.registry.nr_entries + 1) * sizeof(*state.registry.entry)));

    PanicNull((void*)info->callbacks);
    PanicNull((void*)info->callbacks->MicGetConnectionParameters);  /* This callback is mandatory, the others will be sent if available */

    state.registry.entry[state.registry.nr_entries] = info;
    state.registry.nr_entries++;

    uint32 task_period = kymera_CollectMinTaskPeriodFromRegisteredUsers(mic_user_none);
    Kymera_AecSetTaskPeriod(task_period);

    Kymera_AecSetTtpDelay(kymera_CollectTtpDelayFromAllUsers());
    Kymera_AecActivateDeactivateTimestamping(kymera_HasAnyUserRequestedTimestamps());
}

mic_users_t Kymera_MicGetActiveUsers(void)
{
    DEBUG_LOG("Kymera_MicGetActiveUsers: 0x%x",state.current_users);
    return state.current_users;
}

bool Kymera_MicConnect(mic_users_t user)
{
    mic_users_t reconnect_users = mic_user_none;
    Sink aec_ref_sink;
    uint8 i;

    mic_connect_params_t local_mic_params = { 0 };
    uint16 local_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
    Sink local_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    local_mic_params.connections.mic_ids = local_mic_ids;
    local_mic_params.connections.mic_sinks = local_mic_sinks;

    kymera_Wake();

    DEBUG_LOG("Kymera_MicConnect received from user enum:mic_users_t:%d", user);
    if (MAX_NUM_OF_CONCURRENT_MIC_USERS <= kymera_GetNrOfCurrentUsers())
    {
        DEBUG_LOG("Kymera_MicConnect: Maximum mic users (%d) already connected. Try again later.", MAX_NUM_OF_CONCURRENT_MIC_USERS);
        return FALSE;
    }
    kymera_UserGetConnectionParameters(user, &local_mic_params, local_mic_ids, local_mic_sinks, &aec_ref_sink);
    PanicFalse((local_mic_params.connections.num_of_mics > 0) && (local_mic_params.connections.num_of_mics <= MAX_NUM_OF_CONCURRENT_MICS));

    bool connection_possible = kymera_PrepareForConnection(user, &local_mic_params, &reconnect_users);
    if (connection_possible)
    {
        for(i = 0; i < local_mic_params.connections.num_of_mics; i++)
        {
            DEBUG_LOG("Kymera_MicConnect: - mic_id %d Sink: 0x%x", local_mic_ids[i], local_mic_sinks[i]);
            PanicFalse(local_mic_ids[i] != MICROPHONE_NONE);    // Expect receiving valid microphones from client
            PanicFalse(local_mic_sinks[i] != NULL);             // Expect receiving valid sinks for each microphone
        }
        DEBUG_LOG("Kymera_MicConnect: - aec_ref_sink: 0x%x", aec_ref_sink);
        kymera_ConnectUserToMics(user, &local_mic_params, aec_ref_sink, reconnect_users);
    }
    else
    {
        DEBUG_LOG_WARN("Kymera_MicConnect: Connection for user enum:mic_users_t:%d rejected, try again later.", user);
    }
    kymera_Sleep();
    return connection_possible;
}

void Kymera_MicDisconnect(mic_users_t user)
{
    mic_connect_params_t local_mic_params = { 0 };
    uint16 local_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
    Sink local_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
    local_mic_params.connections.mic_ids = local_mic_ids;
    local_mic_params.connections.mic_sinks = local_mic_sinks;
    mic_users_t inform_users;
    mic_users_t reconnect_users = mic_user_none;
    bool disconnect_mics;
    mic_change_info_t info = {0};

    kymera_Wake();

    DEBUG_LOG("Kymera_MicDisconnect: received from enum:mic_users_t:%d", user);
    info.user = user;
    info.event = mic_event_disconnecting;
    disconnect_mics = kymera_PrepareForDisconnection(user, &info, &reconnect_users);

    if (disconnect_mics)
    {
        inform_users = state.current_users;
        kymera_DisconnectAllUsers();
        kymera_InformUsersAboutReadyForReconnection(user, inform_users, &info);
    }
    else
    {
        DEBUG_LOG("Kymera_MicDisconnect: User enum:mic_users_t:%d disconnects but mics are not removed", user);
        kymera_DisconnectUserFromConcurrencyChain(user);

        mic_connect_params_t combined_mic_params = { 0 };
        uint16 combined_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
        Sink combined_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
        combined_mic_params.connections.mic_ids = combined_mic_ids;
        combined_mic_params.connections.mic_sinks = combined_mic_sinks;

        mic_connections_internal_t selected_mics = {0};
        Sink selected_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
        Source selected_mic_sources[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
        selected_mics.mic_sinks = selected_mic_sinks;
        selected_mics.mic_sources = selected_mic_sources;

        Sink aec_ref_sink;

        kymera_UserGetConnectionParameters(user, &local_mic_params, local_mic_ids, local_mic_sinks, &aec_ref_sink);
        combined_mic_params.sample_rate = MAX(local_mic_params.sample_rate, (MIN_SAMPLE_RATE_IN_KHZ * 1000));
        kymera_AddMicsToOrderedList(local_mic_ids, local_mic_sinks, local_mic_params.connections.num_of_mics,
                                    combined_mic_ids, combined_mic_sinks, &combined_mic_params.connections.num_of_mics);

        kymera_CollectMicsFromMultipleUsers(state.current_users, combined_mic_ids, combined_mic_sinks,
                                              &combined_mic_params.connections.num_of_mics, &combined_mic_params.sample_rate);

        if (kymera_IsAnyUserNonInterruptible(mic_user_none))
        {
            kymera_AddMandatoryMicsFromAllUsers(combined_mic_ids, combined_mic_sinks, &combined_mic_params.connections.num_of_mics);
        }

        kymera_SelectMicType(combined_mic_params, state.mic_sources, mic_type_pcm, &selected_mics);
        kymera_DisconnectUserFromMic(user, &selected_mics);

        kymera_RemoveMicUser(user);

        kymera_TurnOffMics(combined_mic_params.connections.num_of_mics, combined_mic_params.connections.mic_ids);
        // No unsynchronise needed since AEC_Ref is not changed
    }

    if (reconnect_users != mic_user_none)
    {
        kymera_ReconnectAllUsers(mic_user_none, reconnect_users, state.mic_sources);
        kymera_SendReconnectedIndicationToAllUsers(reconnect_users);
    }
    kymera_Sleep();
}

void Kymera_MicSleep(mic_users_t user)
{
    state.wake_states &= ~user;
    kymera_Sleep();
}

void Kymera_MicWake(mic_users_t user)
{
    if (kymera_IsCurrentUser(user))
    {
        state.wake_states |= user;
        kymera_Wake();
    }
}

void Kymera_MicInit(const kymera_mic_config_t* mic_config)
{
    PanicFalse(mic_config != NULL);
    splitter_config.cap_id_splitter = mic_config->cap_id_splitter;
}

#ifdef ENABLE_AEC_LEAKTHROUGH
bool Kymera_MicAttachLeakthrough(mic_users_t user)
{
    if(state.leakthrough_enabled == FALSE)
    {
        mic_users_t reconnect_users = mic_user_none;
        Sink aec_ref_sink;

        mic_connect_params_t mic_params = { 0 };
        uint16 mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
        Sink mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
        mic_params.connections.mic_ids = mic_ids;
        mic_params.connections.mic_sinks = mic_sinks;

        kymera_Wake();

        DEBUG_LOG("Kymera_MicAttachLeakthrough: enum:mic_users_t:%d", user);
        kymera_UserGetConnectionParameters(user, &mic_params, mic_ids, mic_sinks, &aec_ref_sink);
        PanicFalse((mic_params.connections.num_of_mics > 0) && (mic_params.connections.num_of_mics <= MAX_NUM_OF_CONCURRENT_MICS));


        bool connection_possible = kymera_PrepareForConnection(user, &mic_params, &reconnect_users);
        if (connection_possible)
        {
            mic_connect_params_t combined_mic_params = { 0 };
            uint16 combined_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
            Sink combined_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
            combined_mic_params.connections.mic_ids = combined_mic_ids;
            combined_mic_params.connections.mic_sinks = combined_mic_sinks;

            kymera_AddMicsToOrderedList(mic_params.connections.mic_ids, mic_params.connections.mic_sinks, mic_params.connections.num_of_mics,
                                        combined_mic_ids, combined_mic_sinks, &combined_mic_params.connections.num_of_mics);
            combined_mic_params.sample_rate = MAX(mic_params.sample_rate, (MIN_SAMPLE_RATE_IN_KHZ * 1000));
            /* Get current mic configuration */
            if(state.current_users != mic_user_none)
            {
                kymera_CollectMicsFromMultipleUsers(state.current_users, combined_mic_ids, combined_mic_sinks,
                                                      &combined_mic_params.connections.num_of_mics, &combined_mic_params.sample_rate);
            }

            /* Add reconnect mic configuration */
            if(reconnect_users != mic_user_none)
            {
                kymera_CollectMicsFromMultipleUsers(reconnect_users, combined_mic_ids, combined_mic_sinks,
                                                      &combined_mic_params.connections.num_of_mics, &combined_mic_params.sample_rate);
            }

            if (kymera_IsAnyUserNonInterruptible(mic_user_none))
            {
                kymera_AddMandatoryMicsFromAllUsers(combined_mic_ids, combined_mic_sinks, &combined_mic_params.connections.num_of_mics);
            }
            state.leakthrough_enabled = TRUE;
            kymera_ConnectLeakthrough(&combined_mic_params, state.mic_sources);
            Kymera_LeakthroughSetupSTGain();
            kymera_ReconnectAllUsers(user, reconnect_users, state.mic_sources);
            kymera_SendReconnectedIndicationToAllUsers(reconnect_users);
        }
        else
        {
            DEBUG_LOG("Kymera_MicAttachLeakthrough: Connection for user enum:mic_users_t:%d rejected, try again later.", user);
        }

        kymera_Sleep();
        return connection_possible;
    }
    else
    {
        DEBUG_LOG("Kymera_MicAttachLeakthrough: Already connected");
        return TRUE;
    }
}

void Kymera_MicDetachLeakthrough(mic_users_t user)
{
    mic_users_t reconnect_users = mic_user_none;
    mic_change_info_t info = {0};

    if(state.leakthrough_enabled == TRUE)
    {
        kymera_Wake();

        DEBUG_LOG("Kymera_MicDetachLeakthrough: received from enum:mic_users_t:%d", user);
        info.user = user;
        info.event = mic_event_disconnecting;
        bool disconnect_mics = kymera_PrepareForDisconnection(user, &info, &reconnect_users);

        if (disconnect_mics)
        {
            mic_users_t inform_users = state.current_users;
            kymera_DisconnectAllUsers();
            kymera_InformUsersAboutReadyForReconnection(user, inform_users, &info);
        }
        else
        {
            DEBUG_LOG("Kymera_MicDetachLeakthrough: User enum:mic_users_t:%d disconnects but mics are not removed", user);
            uint16 combined_mic_ids[MAX_NUM_OF_CONCURRENT_MICS] = {MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE,MICROPHONE_NONE};
            Sink combined_mic_sinks[MAX_NUM_OF_CONCURRENT_MICS] = {NULL};
            uint8 combined_num_of_mics = 0;
            uint32 combined_sample_rate = 0;

            /* Get current mic configuration, incl user LeakThrough */
            kymera_CollectMicsFromMultipleUsers(state.current_users, combined_mic_ids, combined_mic_sinks,
                                                &combined_num_of_mics, &combined_sample_rate);

            if (kymera_IsAnyUserNonInterruptible(mic_user_none))
            {
                kymera_AddMandatoryMicsFromAllUsers(combined_mic_ids, combined_mic_sinks, &combined_num_of_mics);
            }

            kymera_RemoveFromCurrentUsers(user);
            kymera_TurnOffMics(combined_num_of_mics, combined_mic_ids);
            // No unsynchronise needed since AEC_Ref is not changed
        }

        state.leakthrough_enabled = FALSE;

        if (reconnect_users != mic_user_none)
        {
            kymera_ReconnectAllUsers(mic_user_none, reconnect_users, state.mic_sources);
            kymera_SendReconnectedIndicationToAllUsers(reconnect_users);
        }
        kymera_SetAecRefUseCase(state.current_users);

        kymera_Sleep();
    }
    else
    {
        DEBUG_LOG("Kymera_MicDetachLeakthrough: Already disconnected");
    }
}
#endif

#ifdef HOSTED_TEST_ENVIRONMENT
void Kymera_MicClearState(void)
{
    uint8 i;

    if (state.registry.entry)
    {
        DEBUG_LOG("Kymera_MicClearState: Registry %p with %d entries", state.registry.entry, state.registry.nr_entries);
        free(state.registry.entry);
        state.registry.entry = NULL;
        state.registry.nr_entries = 0;
    }
    state.current_users = mic_user_none;
    state.leakthrough_enabled = FALSE;
    state.mic_sample_rate = 0;
    for(i = 0; i < MAX_NUM_OF_CONCURRENT_MIC_USERS; i++)
    {
        state.stream_map[i] = mic_user_none;
    }
    state.splitter = NULL;
    state.wake_states = mic_user_all_mask;
    state.chains_are_awake = TRUE;
    for(i = 0 ; i < MAX_NUM_OF_CONCURRENT_MICS; i++)
    {
        state.mic_sources[i] = NULL;
    }
    for(i = 0 ; i < MAX_SUPPORTED_MICROPHONES; i++)
    {
        state.use_count[i] = 0;
    }
}

Sink Kymera_MicGetAecSplitterConnection(uint8 stream_index)
{
    Sink connected_sink = NULL;
    uint8 num_of_inputs;

    num_of_inputs = Kymera_SplitterGetNumOfInputs(state.splitter);
    if (num_of_inputs > 0)
    {
        connected_sink = Kymera_SplitterGetSink(state.splitter, stream_index, 0);
        DEBUG_LOG("Kymera_MicGetAecSplitterConnection stream_index %d channel[0] connected_sink 0x%x",
                  stream_index, connected_sink);
    }
    return connected_sink;
}

Sink Kymera_MicGetMicSplitterConnection(uint8 stream_index, uint8 channel)
{
    Sink connected_sink = NULL;
    uint8 num_of_inputs;
    uint8 mic_channel = channel + 1;

    num_of_inputs = Kymera_SplitterGetNumOfInputs(state.splitter);
    if (mic_channel <= num_of_inputs)
    {
        connected_sink = Kymera_SplitterGetSink(state.splitter, stream_index, mic_channel);
        DEBUG_LOG("Kymera_MicGetMicSplitterConnection stream_index %d channel[%d] connected_sink 0x%x",
                  stream_index, mic_channel, connected_sink);
    }
    return connected_sink;
}
#endif    //HOSTED_TEST_ENVIRONMENT
