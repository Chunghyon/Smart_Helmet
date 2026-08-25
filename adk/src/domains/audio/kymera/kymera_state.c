/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module for its internal state
*/

#include "kymera_state.h"
#include "kymera_data.h"
#include "kymera_anc.h"
#include "kymera_anc_common.h"
#include "kymera_a2dp.h"
#include <logging.h>

void appKymeraSetState(appKymeraState state)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    DEBUG_LOG_STATE("appKymeraSetState enum:appKymeraState:%u -> enum:appKymeraState:%u", theKymera->state, state);
    theKymera->state = state;
    KymeraAnc_PreStateTransition(state);
    theKymera->busy_lock = Kymera_A2dpIsBeingRouted() ||
                           (
                               (state != KYMERA_STATE_IDLE) &&
                               (state != KYMERA_STATE_STANDALONE_LEAKTHROUGH) &&
                               (state != KYMERA_STATE_ADAPTIVE_ANC_STARTED)
                           );
}

appKymeraState appKymeraGetState(void)
{
    return KymeraGetTaskData()->state;
}

bool appKymeraIsBusy(void)
{
    return Kymera_A2dpIsBeingRouted() || (appKymeraGetState() != KYMERA_STATE_IDLE);
}

bool appKymeraInConcurrency(void)
{
    return (KymeraAncCommon_AdaptiveAncIsConcurrencyActive());
}
