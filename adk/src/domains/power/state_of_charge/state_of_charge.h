/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file        state_of_charge.h
\brief      Header file for State of Charge
*/

#ifndef STATE_OF_CHARGE_H_
#define STATE_OF_CHARGE_H_

#include "domain_message.h"
#include <marshal.h>
#include <task_list.h>
#include <ui.h>
#include <ui_inputs.h>

/*! State of Charge(soc) change messages.  */
enum soc_messages
{
    /*! Message signalling the battery state of charge has changed. */
    SOC_UPDATE_IND = STATE_OF_CHARGE_MESSAGE_BASE,
    /*! Event that the battery level has become low */
    SOC_BATTERY_BECAME_LOW,
    /*! Event that the battery level has become ok */
    SOC_BATTERY_BECAME_OK,
    /*! Event that the battery level has become full */
    SOC_BATTERY_BECAME_FULL,
    STATE_OF_CHARGE_MESSAGE_END
};

/*! \brief Battery ui provider contexts */
typedef enum
{
    context_battery_low,
    context_battery_ok,
    context_battery_full,
    context_charging_battery_low,  /* Below "low" threshold */
    context_charging_battery_ok,   /* Between low and full thresholds */
    context_charging_battery_full, /* Above full threshold */
} battery_provider_context_t;

/*! SoC client registration form */
typedef struct
{
    /*! The task that will receive battery state of charge messages */
    Task task;

    /*! The reporting hysteresis:          
          battery_state_of_charge_percent: in percent

        Note: This is currently not implemented.
    */
    uint16 hysteresis;

} soc_registration_form_t;

typedef struct
{
    uint16 voltage;
    uint8 percentage;
}soc_lookup_t;

/*! Message #MESSAGE_SOC_UPDATE_T content. */
typedef struct
{
    uint8 percent;
} MESSAGE_SOC_UPDATE_T;


#ifdef HAVE_NO_BATTERY

#define Soc_Init()
/* make sure it returns TRUE and dereferences the parameters */
#define Soc_Register(client) (client == client)
#define Soc_Unregister(task)
#define Soc_GetBatterySoc() (0)
#define Soc_SetConfigurationTable(config_table, config_size)
#define Soc_ConvertLevelToPercentage(battery_level) (0)

#else
/*! 
    \brief Initialisation function for SoC module
*/
void Soc_Init(void);

/*! 
    \brief Register for receiving updates from SoC module.
    \param client The client's registration form.
    \return Returns TRUE if register successful.
*/
bool Soc_Register(soc_registration_form_t *client);

/*! 
    \brief Unregister task from receiving updates.
    \param task Handler task.
*/
void Soc_Unregister(Task task);

/*! 
    \brief Get battery state of charge.
    \return Returns state of charge in percentage.
*/
uint8 Soc_GetBatterySoc(void);

/*! 
    \brief Initialize Battery SoC Config table.
    \param config_table pointer to soc_lookup_t structure array 
    \param config_size size of soc_lookup_t structure array
*/
void Soc_SetConfigurationTable(const soc_lookup_t* config_table,
                              unsigned config_size);

/*! Convert a battery voltage in mv to a percentage    
    \param  level_mv The battery level in milliVolts    
    \return The battery percentage equivalent to supplied level.
*/
uint8 Soc_ConvertLevelToPercentage(uint16 battery_level);

/*! Set the limits for reporting low, ok and full battery. Must not be called before Soc_Init();
    \param  low_percent the level below which the battery is "low"
    \param  full_percent the level above which the battery is "full"
*/
void Soc_SetReportingLimits(uint8 battery_low_percent, uint8 battery_high_percent);

/*! Force the SoC module to inform clients of its current status. This is useful in situations
    where a time-limited LED pattern has expired, for example, and the user would like to be
    reminded.
*/
void Soc_ForceNotifyBatteryChanged(void);

#endif /* !HAVE_NO_BATTERY */

#endif /* STATE_OF_CHARGE_H_ */
