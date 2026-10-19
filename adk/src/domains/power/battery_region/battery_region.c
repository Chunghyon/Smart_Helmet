/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file        battery_region.c
\brief      Switching to different charging current regions based on voltage and temperature
*/

#ifndef HAVE_NO_BATTERY

#define DEBUG_LOG_MODULE_NAME battery_region
#ifdef DEBUG
#define PP_DEBUG_LOG_ON
#endif

#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <stdlib.h>

#include "app_task.h"
#include "battery_region_private.h"
#include "battery_monitor.h"
#include "charger_monitor.h"
#include "temperature.h"
#include "system_state.h"
#include "hydra_macros.h"
#include "panic.h"
#include "unexpected_message.h"

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(battery_region_messages)

typedef struct
{
    const charge_region_t *charge_table;
    unsigned charge_table_len;
    const charge_region_t *discharge_table;    
    unsigned discharge_table_len;
    const battery_region_handlers_t *handler_funcs;
}battery_region_ctx_t;

static battery_region_ctx_t region_ctx;

/*! Flag to indicate charger disabled due to charging timer timeout.
 * Can only be re-enabled by disconnect and connect again so this
 * flag will be reset on disconnect.
 */
bool charging_timer_timeout;
battery_region_data_t app_battery_region;

/*! Enumerated type for messages sent within the headset battery
    region only. */
enum battery_region_internal_messages
{
    /*! Message sent to start charging timer when entering an battery operating region */
    MESSAGE_BATTERY_REGION_INTERNAL_CHARGING_TIMER = INTERNAL_MESSAGE_BASE,
    MESSAGE_BATTERY_REGION_INTERNAL_UPDATE_TRIGGER,

    /*! This must be the final message */
    MESSAGE_BATTERY_REGION_INTERNAL_MESSAGE_END
};
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(MESSAGE_BATTERY_REGION_INTERNAL_MESSAGE_END)
LOGGING_PRESERVE_MESSAGE_ENUM(battery_region_internal_messages)

/*! 
    \brief Schedule next Battery Operating Region Update.
    \param battery_region pointer to battery_region_data_t structure
    \param delay time after which region update checked
*/
static void batteryRegion_ScheduleNextRegionUpdate(battery_region_data_t *battery_region,
                                                   uint32 delay)
{
    MessageSendLater(&battery_region->task, MESSAGE_BATTERY_REGION_INTERNAL_UPDATE_TRIGGER,
                        NULL, delay);
}

/*! 
    \brief Send battery region update message to all registered clients.
    \param battery_region pointer to battery_region_data_t structure
*/
static void batteryRegion_ServiceClients(battery_region_data_t *battery_region)
{
    if(TaskList_Size(TaskList_GetFlexibleBaseTaskList(BatteryRegion_GetClientTasks())))
    {
        MESSAGE_MAKE(msg, MESSAGE_BATTERY_REGION_UPDATE_STATE_T);
        msg->state = battery_region->state;                
        TaskList_MessageSendWithSize(TaskList_GetFlexibleBaseTaskList(BatteryRegion_GetClientTasks()),
                                     MESSAGE_BATTERY_REGION_UPDATE, 
                                     msg,
                                     sizeof(msg));
    }
}

/*! 
    \brief Determine if voltage and temperature readings are in specified region index.
    \param battery_region pointer to battery_region_data_t structure.
    \param region operating region array index.
    \param voltage battery voltage.
    \param temperature battery temperature.
    \param apply_hysteresis region hysteresis to be used or not.
    \return Returns TRUE if voltage and temperature match in specified operating region.
*/
static bool batteryRegion_VolTempInRegion(battery_region_data_t *battery_region,
                                                 uint8 region,                                                 
                                                 uint16 voltage,
                                                 int8 temperature,
                                                 bool apply_hysteresis)
{
    const charge_region_t *region_limits = NULL;
    bool volatage_in_region, temp_in_region;
    int16 voltage_min;
    uint16 voltage_max;
    int8 temp_min, temp_max;

    if (region == BATTERY_REGION_UNDEFINED)
    {
        return FALSE;
    }
    
    region_limits = &battery_region->region_table[region];

    if (apply_hysteresis)
    {
        voltage_min = region_limits->voltage_min - region_limits->voltage_hysteresis;
        voltage_max = region_limits->voltage_max + region_limits->voltage_hysteresis;
        temp_min = region_limits->temp_min - region_limits->temp_hysteresis;
        temp_max = region_limits->temp_max + region_limits->temp_hysteresis;
    }
    else
    {
        voltage_min = region_limits->voltage_min;
        voltage_max = region_limits->voltage_max;
        temp_min = region_limits->temp_min;
        temp_max = region_limits->temp_max;
    }
    /* Check if measurements are within region limits */                
    volatage_in_region = (voltage >= voltage_min) && (voltage <= voltage_max);
    temp_in_region = (temperature >= temp_min) && (temperature <= temp_max);
    
    return (volatage_in_region && temp_in_region);
}

#ifdef HJC_11Be
#include "headset_region_config.h"
void forced_stop_charging_timer(void);
void forced_restart_charging_timer(void);
void forced_stop_charging_timer(void)
{
	CC_LOGN("%s", __func__);
	(void) MessageCancelAll(&GetBatteryRegionData()->task, MESSAGE_BATTERY_REGION_INTERNAL_CHARGING_TIMER);
}
void forced_restart_charging_timer(void)
{
	CC_LOGN("%s", __func__);
	(void) MessageCancelAll(&GetBatteryRegionData()->task, MESSAGE_BATTERY_REGION_INTERNAL_CHARGING_TIMER);
	Charger_DisableReasonClear(CHARGER_DISABLE_REASON_TIMEOUT);
	MessageSendLater( &GetBatteryRegionData()->task, MESSAGE_BATTERY_REGION_INTERNAL_CHARGING_TIMER, 0, D_MIN(CHARGING_TIMER_TIMEOUT));
}
#endif
/*! 
    \brief Determine if there has been battery region change and take appropriate actions based on that.
    \param battery_region pointer to battery_region_data_t structure.
*/
static void batteryRegion_UpdateRegion(battery_region_data_t *battery_region)
{
    uint8 i;
    uint16 voltage = appBatteryGetVoltageAverage();
    int8 temperature = appTemperatureGetAverage();
    uint8 prev_region;
    chargerConnectionState is_connected = Charger_IsConnected();

	static chargerConnectionState prev_connected = CHARGER_DISCONNECTED;
	if(prev_connected != is_connected) {
		CC_LOGN("batteryRegion_UpdateRegion, is_connected ? %s", is_connected ? "YES" : "NO");
		prev_connected = is_connected;
	}

	static int8 prev_temperature = 100;
	if(prev_temperature != temperature) {
		CC_LOGN("batteryRegion_UpdateRegion, temperature %d -> %d", prev_temperature, temperature);
		prev_temperature = temperature;
	}

	if (is_connected)
    {
        if(battery_region->region_table != region_ctx.charge_table)
        {
            battery_region->region = BATTERY_REGION_UNDEFINED;
            battery_region->region_table = region_ctx.charge_table;
            battery_region->region_table_len = region_ctx.charge_table_len;
        }
    }
    else
    {
        if(battery_region->region_table != region_ctx.discharge_table)
        {
            battery_region->region = BATTERY_REGION_UNDEFINED;
            battery_region->region_table = region_ctx.discharge_table;
            battery_region->region_table_len = region_ctx.discharge_table_len;
        }
    }

	if (batteryRegion_VolTempInRegion(battery_region, battery_region->region, voltage, temperature, TRUE))
    {
        return;
	}

    /* iterate over region table to match which region voltage and temperature lies */
    for (i=0; i<battery_region->region_table_len; i++) 
    {
        if (batteryRegion_VolTempInRegion(battery_region, i,
                voltage, temperature, FALSE))
        {
            const charge_region_t *new_region = &battery_region->region_table[i];

            /* stop running charging timer */
			(void) MessageCancelAll(&battery_region->task, MESSAGE_BATTERY_REGION_INTERNAL_CHARGING_TIMER);

            prev_region = battery_region->region;
            battery_region->region = i;

			CC_LOGN("batteryRegion_UpdateRegion\n"
					"temperature %d C, battery new Region #%d, "
			        "V=[%d:%d] t=[%d:%d], current: %dmA", temperature, i,
			        new_region->voltage_min,
                    new_region->voltage_max,
                    new_region->temp_min,
                    new_region->temp_max,
                    new_region->current);

            /* check if it is a critical region */
            if (new_region->region_type == CRITICAL_REGION)
            {
				CC_LOGN("batteryRegion: CRITICAL region #%d", i);
                battery_region->state = battery_region_critical;
            }
            /* check if it is a safety region */
            else if (new_region->region_type == SAFETY_REGION)
            {
				CC_LOGN("batteryRegion: SAFETY region #%d", i);
                battery_region->state = battery_region_unsafe;
            }
            else
            {
                battery_region->state = battery_region_ok;
            }
            Charger_UpdateCurrent();

			if (new_region->current != 0 && new_region->charging_timer != 0)
            {
				CC_LOGN("start charging timer timeout for the region");
                MessageSendLater( &battery_region->task, 
                      MESSAGE_BATTERY_REGION_INTERNAL_CHARGING_TIMER,
                      0, D_MIN(new_region->charging_timer));
            }
                
            /* since transition detected so call into transition handler */
            if (region_ctx.handler_funcs->transition_handler != NULL)
            {
				CC_LOGN("calling region_ctx.handler_funcs->transition_handler");
                region_ctx.handler_funcs->transition_handler(prev_region, battery_region->region);
            }

            if (battery_region->state == battery_region_unsafe && 
                region_ctx.handler_funcs->safety_handler != NULL)
            {
				CC_LOGN("calling region_ctx.handler_funcs->safety_handler");
				region_ctx.handler_funcs->safety_handler(prev_region, battery_region->region);
            }
            
            batteryRegion_ServiceClients(battery_region);
            return;
        }
    }
	CC_LOGN("batteryRegion: no operating region located for voltage: %d"
            " and temperature: %d", voltage, temperature);
}

/*! 
    \brief Handle various internal messages.
    \param task message handler task.
    \param id message identifier.
    \param message content.
*/
static void batteryRegion_HandleMessage(Task task, MessageId id, Message message)
{
    battery_region_data_t *battery_region = (battery_region_data_t *)task;
    UNUSED(message);
    switch (id)
    {
        case MESSAGE_BATTERY_REGION_INTERNAL_CHARGING_TIMER:
			CC_LOGN("MESSAGE_BATTERY_REGION_INTERNAL_CHARGING_TIMER");
            /* Timer expired so call into the timeout handler. Disable the charger and set flag  
             * indicating charger timer expiry the reason. 
             */
            if (region_ctx.handler_funcs->charging_timeout_handler != NULL)
            {
                region_ctx.handler_funcs->charging_timeout_handler();
            }
            Charger_DisableReasonAdd(CHARGER_DISABLE_REASON_TIMEOUT);
            charging_timer_timeout = TRUE;
            break;

        case MESSAGE_BATTERY_REGION_INTERNAL_UPDATE_TRIGGER:
            /* Start immediate battery region update check and schedule next one. */
            batteryRegion_UpdateRegion(battery_region);
            batteryRegion_ScheduleNextRegionUpdate(battery_region, battery_region->period);
            break;

        case CHARGER_MESSAGE_DETACHED:
            /* stop running charging timer */
            (void) MessageCancelAll(&battery_region->task, 
                            MESSAGE_BATTERY_REGION_INTERNAL_CHARGING_TIMER);
            Charger_DisableReasonClear(CHARGER_DISABLE_REASON_TIMEOUT);
            charging_timer_timeout = FALSE;
            break;

        default:
            break;
    }
}

/*! \brief returns battery region task pointer to requesting component

    \return battery region task pointer.
*/
#ifdef INCLUDE_CHARGER  //byCC
static Task batteryRegion_GetTask(void)
{
    return &app_battery_region.task;
}
#endif

void BatteryRegion_Init(void)
{
	CC_LOGN("BatteryRegion_Init");
    battery_region_data_t *battery_region = GetBatteryRegionData();
    memset(battery_region, 0, sizeof(*battery_region));

    /* Set up task handler */
    battery_region->task.handler = batteryRegion_HandleMessage;
    TaskList_InitialiseWithCapacity(BatteryRegion_GetClientTasks(), 
                           BATTERY_REGION_CLIENT_TASKS_LIST_INIT_CAPACITY);

    /* these need to be set from headset or earbud init. temporarily in config file. */
    battery_region->period = batteryRegion_GetReadingPeriodMs();
    battery_region->region = BATTERY_REGION_UNDEFINED;

#ifdef INCLUDE_CHARGER  //byCC
    Charger_ClientRegister(batteryRegion_GetTask());
#endif

    batteryRegion_ScheduleNextRegionUpdate(battery_region, 0);    
}

void BatteryRegion_SetChargeRegionConfigTable(charge_mode_t mode,
                                        const charge_region_t* config_table,                              
                                        unsigned config_size)
{
	CC_LOGN("BatteryRegion_SetChargeRegionConfigTable, set region config table for mode enum:charge_mode_t:%u", mode);
    if (mode == CHARGE_MODE)    
    {        
        region_ctx.charge_table = config_table;        
        region_ctx.charge_table_len = config_size;    
    }
    else
    {
        region_ctx.discharge_table = config_table;        
        region_ctx.discharge_table_len = config_size;    
    }
}

void BatteryRegion_SetHandlerStructure(                              
                                const battery_region_handlers_t* config_table)
{
	CC_LOGN("BatteryRegion_SetHandlerStructure");
    region_ctx.handler_funcs = config_table;
}

bool BatteryRegion_Register(Task task)
{
    if (TaskList_AddTask(TaskList_GetFlexibleBaseTaskList(BatteryRegion_GetClientTasks()), task))
    {
        return TRUE;
    }
    return FALSE;
}

void BatteryRegion_Unregister(Task task)
{
    TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(BatteryRegion_GetClientTasks()), task);
}

battery_region_state_t BatteryRegion_GetState(void)
{
    battery_region_data_t *battery_region = GetBatteryRegionData();
    return battery_region->state;
}

#ifdef ACTIVE_BATTERY_CHARGING
bool forced_charging_needed(void);
bool forced_charging_needed(void)
{
	return (battery_region_ok != BatteryRegion_GetState());
}
#endif

uint16 BatteryRegion_GetCurrent(void)
{
    battery_region_data_t *battery_region = GetBatteryRegionData();
    if (battery_region->region == BATTERY_REGION_UNDEFINED ||
        battery_region->state == battery_region_unknown)
    {
        return 0;
    }
    else
	{
        return battery_region->region_table[battery_region->region].current;
    }
}

#endif /* !HAVE_NO_BATTERY */
