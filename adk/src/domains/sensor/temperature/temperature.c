/*!
\copyright  Copyright (c) 2018 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       temperature.c
\brief      Top level temperature sensing implementation. Uses a temperature sensor
            e.g. thermistor to actually perform the measurements.
*/

#ifdef INCLUDE_TEMPERATURE
#ifdef DEBUG
#define PP_DEBUG_LOG_ON
#endif

#define DEBUG_LOG_MODULE_NAME temperature
#include <logging.h>
DEBUG_LOG_DEFINE_LEVEL_VAR

#include <panic.h>
#include <pio.h>
#include <vm.h>
#include <hydra_macros.h>
#include <limits.h>
#include <task_list.h>
#include <logging.h>

#include "adk_log.h"
#include "temperature.h"
#include "temperature_config.h"
#include "temperature_sensor.h"
#include "unexpected_message.h"

void appThermistorPIOEnable(bool enable);

#include <stdlib.h>

/* Make the type used for message IDs available in debug tools */
LOGGING_PRESERVE_MESSAGE_ENUM(temperatureMessages)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(TEMPERATURE, TEMPERATURE_MESSAGE_END)

#define INVALID_TEMPERATURE 100

#ifndef FAKE_BATTERY_TEMP
#define FAKE_BATTERY_TEMP INVALID_TEMPERATURE
#endif
int8 fake_battery_temperature = FAKE_BATTERY_TEMP;

bool battery_temp_test_on = FALSE;

/*! Messages sent within the temperature module. */
enum headset_temperature_internal_messages
{
    /*! Message sent to trigger a temperature measurement */
    MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_TRIGGER,

	//byCC
	MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_RETRIGGER
};


/*! The indexes in a #task_list_data_t arr_s8 to store the client data. */
enum client_indexes
{
    CLIENT_LOWER_LIMIT_INDEX,
    CLIENT_UPPER_LIMIT_INDEX,
    CLIENT_CURRENT_STATE_INDEX,
};

typedef struct
{
    const temperature_lookup_t* temperature_config_table;
    unsigned temperature_config_size;
}temp_ctx_t;

static temp_ctx_t temperature_ctx;

/*!< Task information for temperature */
temperatureTaskData app_temperature;

static int8 temperature_GetTemperature(uint16 voltage, int8 prev_temperature)
{
    uint32 prev_index, index;
    uint16 prev_mv, this_mv;

    prev_index = prev_temperature - TEMPERATURE_MIN;
    prev_mv = temperature_ctx.temperature_config_table[prev_index].voltage;

    if (voltage < prev_mv)
    {
        /* Temperature increase, search forward through lower voltages / higher temperatures */
        for (index = prev_index + 1; index < temperature_ctx.temperature_config_size; index++)
        {
            this_mv = temperature_ctx.temperature_config_table[index].voltage;

            if (voltage >= this_mv)
            {
                return temperature_ctx.temperature_config_table[index].temperature;
            }
        }
        return TEMPERATURE_MAX;
    }
    else /* voltage >= prev_mv */
    {
        /* Temperature decrease, search backwards through higher voltages / lower temperatures */
        for (index = prev_index; index > 0; index--)
        {
            this_mv = prev_mv;
            prev_mv = temperature_ctx.temperature_config_table[index-1].voltage;

            if (voltage >= this_mv && voltage < prev_mv)
            {
                return temperature_ctx.temperature_config_table[index].temperature;
            }
        }
        return TEMPERATURE_MIN;
    }
}


/*! \brief Inform a single client of temperature events */
static bool appTemperatureServiceClient(Task task, task_list_data_t *data, void *arg)
{
    int8 lower_limit = data->arr_s8[CLIENT_LOWER_LIMIT_INDEX];
    int8 upper_limit = data->arr_s8[CLIENT_UPPER_LIMIT_INDEX];
    int8 t = appTemperatureGetAverage();
    temperatureState next_state;

    next_state = (t >= upper_limit) ? TEMPERATURE_STATE_ABOVE_UPPER_LIMIT :
                    (t <= lower_limit) ? TEMPERATURE_STATE_BELOW_LOWER_LIMIT :
                        TEMPERATURE_STATE_WITHIN_LIMITS;

	if (next_state != (temperatureState)data->arr_s8[CLIENT_CURRENT_STATE_INDEX])
    {
        MESSAGE_MAKE(ind, TEMPERATURE_STATE_CHANGED_IND_T);
        ind->state = next_state;
        data->arr_s8[CLIENT_CURRENT_STATE_INDEX] = next_state;
        MessageSend(task, TEMPERATURE_STATE_CHANGED_IND, ind);
    }

    UNUSED(arg);

    /* Iterate through every client */
    return TRUE;
}

/*! \brief Inform all clients of temperature events */
static void appTemperatureServiceClients(temperatureTaskData *temperature)
{
    TaskList_IterateWithDataRawFunction(temperature->clients, appTemperatureServiceClient, temperature);
}

static int32 temperature_ExponentialAverage(temperatureTaskData *temp, int8 reading)
{
    int32 ema;
    static uint8 t = 1;

    /* for first time reading is the average */
    if (t == 1)
    {
        ema = reading * 100;
        t++;
    }
    else
    {
        ema = (int32)((temp)->average.weight * reading) + 
            (int32)(((100 - (temp)->average.weight) * temp->average.last_ema) / 100);
    }

    return ema;
}

static int8 temperature_MedianFiltering(temperatureTaskData *temp, int8 reading)
{
    uint8 i,j, k;
    static int8 *input_arr = NULL;
    int8 *sort_arr = NULL;
    static uint8 index = 0;
    int8 temp_value;
    int8 median_val;
    
    if (input_arr == NULL)
    {
        input_arr = (int8*)PanicUnlessMalloc(sizeof(int8)*temp->filter.median_filter_window);
    }

    /* copy reading into input array */
    input_arr[index] = reading;

    /* copy array into sorting array */
    sort_arr = (int8*)PanicUnlessMalloc(sizeof(int8)*(index + 1));

    memcpy(sort_arr, input_arr, sizeof(int8)*(index + 1));
    
    /* sort input array in increasing order */
    for(j=0; j<(index + 1); j++)
    {       
        for(k=j+1; k<(index + 1); k++)
        {
            /* swap */
            if(sort_arr[j] > sort_arr[k])
            {
                temp_value = sort_arr[j];
                sort_arr[j] = sort_arr[k];
                sort_arr[k] = temp_value;
            }
        }
    }       

    median_val = sort_arr[((index + 1) - 1)/2];

    /* free sort array */
    free(sort_arr);

    if (index == (temp->filter.median_filter_window - 1))
    {
        /* shift input_arr values */
        for(i=0; i<(temp->filter.median_filter_window - 1); i++)
        {
            input_arr[i] = input_arr[i+1];
        }
        input_arr[temp->filter.median_filter_window - 1] = 0;  
    }
    else
    {
        index++;
    }

    /* return median value */
    return median_val;
}

static void handleTemperatureReading(temperatureTaskData *temperature_task, int8 reading)
{
    int8 median_reading;
    int32 ema_reading;
    
	median_reading = temperature_MedianFiltering(temperature_task, reading);

	temperature_task->filter.instantaneous = median_reading;

    /* calculate average */
	temperature_task->average.last_ema = temperature_task->average.current_ema;
	ema_reading = temperature_ExponentialAverage(temperature_task, median_reading);

	temperature_task->average.current_ema = ema_reading;

	appTemperatureServiceClients(temperature_task);
}

static uint16 Temperature_AdcResultHandler(const MessageAdcResult* result)
{
    /*! The latest vref measurement, which is required to calculate the voltage.
        vref must be measured before the ADC voltage. */
    static uint16 vref_raw;
    uint16 volt_mv = 0;

    uint16 reading = result->reading;

    if (adcsel_vref_hq_buff == result->adc_source)
    {
        vref_raw = reading;
    }
    else if (appTemperatureSensorAdcSource() == result->adc_source)
    {
        if (vref_raw != 0)
        {
            uint32 vref_const = VmReadVrefConstant();
            volt_mv = (uint16)(vref_const * reading / vref_raw);
        }
        else
        {
			CC_LOGN("Temperature_AdcResultHandler, vref_raw reading is 0.");
        }
        
        return volt_mv;
    }
    else
    {
		CC_LOGN("Temperature_AdcResultHandler unexpected source - %d",result->adc_source);
    }

    return 0;
}
#ifdef HJC_11Be
void retrigger_temperature_measurement(void);
void retrigger_temperature_measurement(void)
{
	temperatureTaskData *temperature = TemperatureGetTaskData();
	MessageSendLater(&temperature->task,
	                 MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_RETRIGGER, NULL,
	                 2*temperature->period);
}
#endif
/*! \brief Handle temperature messages */
static void appTemperatureHandleMessage(Task task, MessageId id, Message message)
{
	temperatureTaskData *temperature_task = (temperatureTaskData *)task;
    uint16 voltage_mv;
    
    switch (id)
    {
#ifdef HJC_11Be
	    case MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_RETRIGGER:
#endif
        case MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_TRIGGER:
        {
            appTemperatureSensorRequestMeasurement(task);
        }
        break;
                
        case MESSAGE_ADC_RESULT:
        {
#ifdef HJC_11Be
#ifdef PP_DEBUG_LOG_ON
            rtime_t now = SystemClockGetTimerTime();
			if( rtime_gt(rtime_sub(now, timestamp_log), 60*1000*1000) ) {
				CC_LOGN(".\n");
			}
#endif
#endif
			if (!battery_temp_test_on && fake_battery_temperature == INVALID_TEMPERATURE)
            {
                voltage_mv = Temperature_AdcResultHandler((const MessageAdcResult*)message);
				if (voltage_mv != 0)
                {
#ifdef HJC_11Be
					MessageCancelAll(task, MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_RETRIGGER);
#endif
					static uint16 prev_voltage_mv = 0;
					if(prev_voltage_mv != voltage_mv) {
						CC_LOGN("thermistor adc voltage change %u -> %u", prev_voltage_mv, voltage_mv);
						prev_voltage_mv = voltage_mv;
					}
					static int8 prev_temperature = INVALID_TEMPERATURE;
					int8 t_new = temperature_GetTemperature(voltage_mv, temperature_task->filter.instantaneous);
					if(prev_temperature != t_new) {
						CC_LOGN("temperature change %d -> %d", prev_temperature, t_new);
						prev_temperature = t_new;
					}

					handleTemperatureReading(temperature_task, t_new);
					appThermistorPIOEnable(FALSE);	//byCC
					MessageSendLater(&temperature_task->task,
                             MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_TRIGGER, NULL,
					         temperature_task->period);
                }
            }
        }
        break;

        default:
        {
            /* An unexpected message has arrived - must handle it */
            UnexpectedMessage_HandleMessage(id);
        }
        break;
    }
}


bool appTemperatureInit(Task init_task)
{
    temperatureTaskData *temperature = TemperatureGetTaskData();

	CC_LOGN("appTemperatureInit, fake_battery_temperature %d", fake_battery_temperature);

    temperature->clients = TaskList_WithDataCreate();
    temperature->task.handler = appTemperatureHandleMessage;

    /* these need to be set from headset or earbud init */
    temperature->period = appConfigTemperatureReadingPeriodMs();
    temperature->filter.median_filter_window = appConfigTemperatureMedianFilterWindow();
    temperature->average.weight = appConfigTemperatureSmoothingWeight();

    appTemperatureSensorInit();

    if (fake_battery_temperature == INVALID_TEMPERATURE)
    {
        appTemperatureSensorRequestMeasurement(&temperature->task);
    }
    
    UNUSED(init_task);
    
    return TRUE;
}
 
bool appTemperatureClientRegister(Task task, int8 lower_limit, int8 upper_limit)
{
    temperatureTaskData *temperature = TemperatureGetTaskData();
    task_list_data_t data = {0};

	CC_LOGN("appTemperatureClientRegister Task=%p (%d, %d)", task, lower_limit, upper_limit);

    data.arr_s8[CLIENT_LOWER_LIMIT_INDEX] = lower_limit;
    data.arr_s8[CLIENT_UPPER_LIMIT_INDEX] = upper_limit;
    data.arr_s8[CLIENT_CURRENT_STATE_INDEX] = TEMPERATURE_STATE_UNKNOWN;
    appTemperatureServiceClient(task, &data, temperature);
    PanicFalse(TaskList_AddTaskWithData(temperature->clients, task, &data));
    return TRUE;
}

void appTemperatureClientUnregister(Task task)
{
    temperatureTaskData *temperature = TemperatureGetTaskData();

	CC_LOGN("appTemperatureClientUnregister Task=%p", task);

    PanicFalse(TaskList_RemoveTask(temperature->clients, task));
}

temperatureState appTemperatureClientGetState(Task task)
{
    temperatureTaskData *temperature = TemperatureGetTaskData();
    task_list_data_t *data;
    PanicFalse(TaskList_GetDataForTaskRaw(temperature->clients, task, &data));
    return (temperatureState)data->arr_s8[CLIENT_CURRENT_STATE_INDEX];
}

int8 appTemperatureGetAverage(void)
{
   if (fake_battery_temperature != INVALID_TEMPERATURE)
   {
       return fake_battery_temperature;
   }
   else
   {
       temperatureTaskData *temperature = TemperatureGetTaskData();
       return (temperature->average.current_ema / 100);
   }
}

int8 appTemperatureGetInstantaneous(void)
{
    if (fake_battery_temperature != INVALID_TEMPERATURE)
    {
        return fake_battery_temperature;
    }
    else
    {
        temperatureTaskData *temperature = TemperatureGetTaskData();
        return temperature->filter.instantaneous;
    }
}

void Temperature_SetConfigurationTable(const temperature_lookup_t* config_table,
                              unsigned config_size)
{
	CC_LOGN("Temperature_SetConfigurationTable, set voltage->temperature lookup configuration table");

    temperature_ctx.temperature_config_table = config_table;
    temperature_ctx.temperature_config_size = config_size;
}

void appTemperatureSetFakeValue(int8 temperature)
{
    temperatureTaskData *temperature_data = TemperatureGetTaskData();
	CC_LOGN("appTemperatureSetFakeValue, set test temperature: %d ", temperature);
    
    fake_battery_temperature = temperature;
    appTemperatureServiceClients(temperature_data);    
}

void appTemperatureUnsetFakeValue(void)
{
	CC_LOGN("appTemperatureUnsetFakeValue, reset test temperature");
    temperatureTaskData *temperature = TemperatureGetTaskData();
    
    fake_battery_temperature = INVALID_TEMPERATURE;
    MessageSendLater(&temperature->task,
                     MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_TRIGGER, NULL,
                     0);
}

void appTemperatureTestInjectFakeLevel(int8 temperature)
{
    temperatureTaskData *temperature_data = TemperatureGetTaskData();
	CC_LOGN("appTemperatureTestInjectFakeLevel, inject test temperature: %d ", temperature);
    battery_temp_test_on = TRUE;

    handleTemperatureReading(temperature_data, temperature);
}

void appTemperatureResumeAdcMeasurements(void)
{
    temperatureTaskData *temperature = TemperatureGetTaskData();
    
	CC_LOGN("appTemperatureResumeAdcMeasurements");
    battery_temp_test_on = FALSE;    
    
    MessageSendLater(&temperature->task,
                     MESSAGE_TEMPERATURE_INTERNAL_MEASUREMENT_TRIGGER, NULL,
                     0);
}

#endif
