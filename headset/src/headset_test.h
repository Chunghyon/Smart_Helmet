/*!
\copyright  Copyright (c) 2019 - 2022 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    %%version
\file       headset_test.h
\brief      Header file for test module
*/
#include "aec_leakthrough.h"

#ifndef HEADSET_TEST_H
#define HEADSET_TEST_H

#include <anc.h>

#define MESSAGE_BATTERY_PROCESS_READING     0x40

#include <kymera.h>
#include "headset_sm.h"
#include <charger_detect.h>


extern uint16 appTestBatteryVoltage;
/*! \brief Injects logical input for connect handset

    \return bool TRUE Logical input injected
*/
void appTestConnectHandset(void);
/*! \brief Return if headset has an Handset HFP connection

    \return bool TRUE Headset has HFP Handset connection
                 FALSE Headset does not have HFP Handset connection
*/
bool appTestIsHandsetHfpConnected(void);

/*! \brief Return if Headset has an Handset HFP SCO connection

    \return bool TRUE Headset has HFP SCO Handset connection
                 FALSE Headset does not have HFP SCO Handset connection
*/
bool appTestIsHandsetHfpScoActive(void);

/*! \brief Accept incoming call
*/
void appTestHandsetHfpCallAccept(void);

/*! \brief Reject incoming call
*/
void appTestHandsetHfpCallReject(void);

/*! \brief End the current call
*/
void appTestHandsetHfpCallHangup(void);

/*! \brief Cycle to the next call
*/
void appTestHandsetVoiceCallCycle(void);

/*! \brief Join two calls
*/
void appTestHandsetVoiceCallJoinCalls(void);

/*! \brief Join two calls and hang up
*/
void appTestHandsetVoiceCallJoinCallsAndHangUp(void);

/*! \brief Get microphone mute status

    \return bool TRUE if microphone muted,
                 FALSE if not muted
*/
bool appTestIsHandsetHfpMuted(void);

/*! \brief Toggle Microphone mute state on HFP SCO conenction to handset
*/
void appTestHandsetHfpMuteToggle(void);

/*! \brief Transfer HFP voice to the AG
*/
void appTestHandsetHfpVoiceTransferToAg(void);

/*! \brief Transfer HFP voice to the Headset
*/
void appTestHandsetHfpVoiceTransferToHeadset(void);

/*! \brief Initiated last number redial

    \return bool TRUE if last number redial was initiated
                 FALSE if HFP is not connected
*/
bool appTestHandsetHfpCallLastDialed(void);

/*! \brief Start decreasing the HFP volume
*/
void appTestHandsetHfpVolumeDownStart(void);

/*! \brief Start increasing the HFP volume
*/
void appTestHandsetHfpVolumeUpStart(void);

/*! \brief Stop increasing or decreasing HFP volume
*/
void appTestHandsetHfpVolumeStop(void);

/*! \brief Set the Hfp Sco volume

    \return bool TRUE if the Hfp Sco volume is updated
				 FALSE if not in a call or HFP Sco volume is not updated
*/
bool appTestHandsetHfpSetScoVolume(uint8 volume);

/*! \brief Check if call is in progress

    \return bool TRUE if call in progress,
                 FALSE if no call, or not connected
*/
bool appTestIsHandsetHfpCall(void);

/*! \brief Check if the incoming call is in progress

    \return bool TRUE if call incoming,
                 FALSE if no call, or not connected
*/
bool appTestIsHandsetHfpCallIncoming(void);

/*! \brief Check if the outgoing call is in progress

    \return bool TRUE if call outgoing,
                 FALSE if no call, or not connected
*/
bool appTestIsHandsetHfpCallOutgoing(void);

/*! \brief Initiate HFP Voice Dial request to the Handset
*/
void appTestHfpVoiceDial(void);

/*! \brief Activate/De-activate Voice Recognition Feature at Handset
*/
bool appTestHandsetHfpActivateVoiceRecognition(bool activate);

/*! \brief Set HF Indicator value at Handset
*/
bool appTestHandsetHfpSetIndicatorValue(uint8 idx, uint8 value);

/*! \brief Get current call list from Handset
*/
bool appTestHandsetHfpGetCurrentCallList(void);

/*! \brief Return initiate AVRCP connection to a Handset

    \return bool TRUE TRUE if the connection has been requested
                 FALSE Otherwise
*/
bool appTestHandsetAvrcpConnect(void);

/*! \brief Return if headset has an Handset AVRCP connection

    \return bool TRUE headset has AVRCP Handset connection
                 FALSE headset does not have AVRCP Handset connection
*/
bool appTestIsHandsetAvrcpConnected(void);

/*! \brief Return the AVRCP play status

    \return bool TRUE if AVRCP play status is playing
                 FALSE if AVRCP play status is not playing
*/
bool appTestIsA2dpPlaying(void);

/*! \brief Return Headset has an A2DP media connection or not

    \return bool TRUE Headset has A2DP media connection
                 FALSE Headset does not have A2DP media connection
*/
bool appTestIsHandsetA2dpMediaConnected(void);

/*! \brief Initiate A2DP connection to the Handset

    \return bool TRUE if A2DP connection is initiated
                 FALSE if no handset is paired
*/
bool appTestHandsetA2dpConnect(void);

/*! \brief Return if headset has an Handset A2DP connection

    \return bool TRUE headset has A2DP Handset connection
                 FALSE headset does not have A2DP Handset connection
*/
bool appTestIsHandsetA2dpConnected(void);

/*! \brief Return if application  has an ACL connection to the Handset

    It does not indicate if the handset is usable, with profiles
    connected. Use appTestIsHandsetConnected.

    \return bool TRUE headset has an ACL connection
                 FALSE application does not have an ACL connection to the Handset
*/
bool appTestIsHandsetAclConnected(void);

/*! \brief Return if headset has a connection to the Handset
*/
bool appTestIsHandsetConnected(void);

/*! Has the application infrastructure initialised

    When starting, the application initialises the modules it uses.
    This function checks if the sequence of module initialisation has completed

    \note When this function returns TRUE it does not indicate that the application
    is fully initialised. That would depend on the application state, and the
    status of the device.

    \returns TRUE if initialisation completed
 */
bool appTestIsInitialisationCompleted(void);

/*! \brief Return TRUE is the headet is on head.
*/
bool appTestHeadsetPhyStateIsOnHead(void);

/*! \brief Generate the event that says the headset is now on the head.
*/
void appTestHeadsetPhyStateOnHeadEvent(void);

/*! \brief Generate the event that says the headset is now off the head.
*/
void appTestHeadsetPhyStateOffHeadEvent(void);

/*! \brief Send the AV toggle play/pause command
*/
void appTestAvTogglePlayPause(void);

/*! \brief Send the Avrcp pause command to the Handset
*/
void appTestAvPause(void);

/*! \brief Send the Avrcp play command to the Handset
*/
void appTestAvPlay(void);

/*! \brief Send the Avrcp stop command to the Handset
*/
void appTestAvStop(void);

/*! \brief Send the Avrcp forward command to the Handset
*/
void appTestAvForward(void);

/*! \brief Send the Avrcp backward command to the Handset
*/
void appTestAvBackward(void);

/*! \brief Send the Avrcp fast forward state command to the Handset
*/
void appTestAvFastForwardStart(void);

/*! \brief Send the Avrcp fast forward stop command to the Handset
*/
void appTestAvFastForwardStop(void);

/*! \brief Send the Avrcp rewind start command to the Handset
*/
void appTestAvRewindStart(void);

/*! \brief Send the Avrcp rewind stop command to the Handset
*/
void appTestAvRewindStop(void);

/*! \brief Send relative AVRCP absolute volume change to the Handset

    \param step : relative Step change to apply. 

    \return bool TRUE absolute AVRCP volume step change sent
                 FALSE absolute AVRCP volume step change was not sent
*/
bool appTestAvVolumeChange(int8 step);

/*! \brief Send the Avrcp absolute set volume command to the Handset

    \param volume   New volume level to set (0-127).
*/
void appTestAvVolumeSet(uint8 volume);

/*! \brief Issue a logical Power On command to headset.
*/

/*! \brief To get current A2DP codec
    The return value follows this enum
*/
typedef enum
{
    a2dp_codec_sbc,
    a2dp_codec_aac,
    a2dp_codec_aptx,
    a2dp_codec_aptx_adaptive,
    a2dp_codec_aptx_hd,
    a2dp_codec_unknown,
} app_test_a2dp_codec_t;

/*! \brief Get the current A2dp Codec
    \return a2dp codec type
*/
app_test_a2dp_codec_t HeadsetTest_GetA2dpCodec(void);

/*! \brief Suspends A2dp Media
    \return TRUE Media suspended successfully
            FALSE Media suspend failed
*/
bool appTestHandsetA2dpMediaSuspend(void);

/*! \brief Send A2dp Media Close request to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpMediaClose(void);

/*! \brief Send A2dp Abort stream request to remote
    Added to support PTS qualification TCs.
*/
bool appTestHandsetA2dpAbort(void);

/*! \brief Request to reconfigure the A2DP audio codec on a Media channel
    \return TRUE Media reconfiguration command initiated successfully
            FALSE Media reconfiguration command initiated failed
*/
bool appTestHandsetA2dpMediaReconfigureRequest(void);

/*! \brief Starts A2dp Media
    \return TRUE Media Started successfully
            FALSE Media Start failed
*/
bool appTestHandsetA2dpMediaStart(void);

void appTestHeadsetPowerOn(void);

/*! \brief Powers off headset using ui event */
void appTestHeadsetPowerOff(void);

/*! \brief Power off.
    \return TRUE if the device can power off - the device will drop connections then power off.
            FALSE if the device cannot currently power off.
*/
bool appTestPowerOff(void);
/*! \brief Return if Headset is in a Pairing mode
    \return bool TRUE Headset is in pairing mode
            FALSE Headset is not in pairing mode
*/
bool appTestIsPairingInProgress(void);
/*! \brief Put Headset into Pairing mode
*/
void appTestPairHandset(void);

/*! \brief Return if Headset has a handset paired
 */
bool appTestIsHandsetPaired(void);

/*! \brief Delete all Handset pairing
*/
void appTestDeleteHandset(void);

/*! \brief Reset headset to factory defaults.
    Will drop any connections, delete all pairing and reboot.
*/
void appTestFactoryReset(void);

/*! \brief Determine if a reset has happened

    \return TRUE if a reset has happened since the last call to the function
*/
bool appTestResetHappened(void);

/*! \brief Determine if the all licenses are correct.
    \return bool TRUE the licenses are correct, FALSE if not.
*/
bool appTestLicenseCheck(void);

/*! \brief Are we running in PTS test mode */
bool appTestIsPtsMode(void);

/*! \brief Set or clear PTS test mode */
void appTestSetPtsMode(bool enabled);

/*! Check if the application state machine is in the specified role

    The states are defined in headset_sm.h, and can be accessed from
    python by name - example
    \code
      apps1.fw.env.enums["headsetState"]["HEADSET_STATE_POWERING_ON"])
    \endcode

    \return TRUE The headset state machine is in the specified state
*/
bool appTestIsApplicationState(headsetState checked_state);

/*! Report the contents of the Device Database. */
void appTest_DeviceDatabaseReport(void);

/*! \brief To get current HFP codec
    The return value follows this enum
    typedef enum
    {
        NO_SCO,
        SCO_NB,
        SCO_WB,
        SCO_SWB,
        SCO_UWB
    } appKymeraScoMode;
*/
appKymeraScoMode appTestGetHfpCodec(void);

#ifdef INCLUDE_ACCESSORY

/*! \brief Send an app launch request
    \param app_name The name of the application to launch, in reverse DNS notation.
*/
void appTestRequestAppLaunch(const char * app_name);

/*! \brief Check the accessory connected state
    \return bool TRUE if accessory is connected, else FALSE
*/
bool appTestIsAccessoryConnected(void);

/*! \brief Check accessory connected state of a specific handset
 *  \param addr Pointer to Bluetooth device address of handset
    \return bool TRUE if accessory is connected with the handset, else FALSE
*/
bool appTestIsAccessoryHandsetConnected(bdaddr *addr);

/*! \brief Get the number of connect events recorded by the iAP2 library
    \return The number of events since the library was initialised
*/
unsigned appTestGetIap2ConnectCount(void);

/*! \brief Get the number of disconnect events recorded by the iAP2 library
    \return The number of events since the library was initialised
*/
unsigned appTestGetIap2DisconnectCount(void);

#endif /* INCLUDE_ACCESSORY */


/*! \brief Get the current audio source volume
    \return the current audio source volume represented in audio source volume steps
*/
int appTestGetCurrentAudioVolume(void);

/*! \brief Get the current voice source volume
    \return the current audio source volume represented in voice source volume steps
*/
int appTestGetCurrentVoiceVolume(void);

/*! Checks whether there is an active BREDR connection.
    \return TRUE if there is a \b connected BREDR link. Any other cases,
        including links in the process of disconnecting and connecting
        are reported as FALSE
 */
bool appTestAnyBredrConnection(void);

/*! \brief Registers the test task to receive kymera notifications. */
void appTestRegisterForKymeraNotifications(void);

/* \brief Disconnect all currently connected handsets. */
void appTestDisconnectHandsets(void);

/*! \brief Registers the test task to receive kymera notifications. */
void appTestCvcPassthrough(void);

/*! Checks if the BREDR Scan Manager is enabled for page or inquiry scan
    \return TRUE if either is enabled
 */
bool appTestIsBredrScanEnabled(void);

/*! Test function to report if the application Topology is running.
    \return TRUE if topology is active, FALSE otherwise.
 */
bool appTestIsTopologyRunning(void);

/*! \brief Returns the current battery voltage
 */
uint16 appTestGetBatteryVoltage(void);

/*! \brief Sets the battery voltage to be returned by
    appBatteryGetVoltage().
    This test function also causes the battery module to read
    this new voltage level - which may be subjected to hysteresis.
 */
void appTestSetBatteryVoltage(uint16 new_level);

/*! \brief Unsets the test battery voltage.
 */
void appTestUnsetBatteryVoltage(void);

/*! \brief Sets the battery voltage to be used instead of ADC measurements.
      \param new_level battery voltage to be used.
 */
void appTestInjectBatteryTestVoltage(uint16 new_level);

/*! \brief Restart normal battery voltage measurements.
 */
void appTestRestartBatteryVoltageMeasurements(void);

/*! \brief Return TRUE if the current battery operating region is battery_region_ok. */
bool appTestBatteryStateIsOk(void);

/*! \brief Return TRUE if the current battery operating region is battery_region_critical. */
bool appTestBatteryStateIsCritical(void);

/*! \brief Return TRUE if the current battery operating region is battery_region_unsafe. */
bool appTestBatteryStateIsUnsafe(void);

/*! \brief Converts battery voltage to percent of battery charge.

    \param voltage_mv Battery voltage in mV.
    \return Percentage of battery charge.
*/
uint8 appTestConvertBatteryVoltageToPercentage(uint16 voltage_mv);

/*! \brief Return TRUE if the current battery state is battery_level_ok. */
bool appTestBatteryStateIsOk(void);

/*! \brief Return TRUE if the current battery state is battery_level_low. */
bool appTestBatteryStateIsLow(void);

/*! \brief Return TRUE if the current battery state is battery_level_critical. */
bool appTestBatteryStateIsCritical(void);

/*! \brief Return TRUE if the current battery state is battery_level_too_low. */
bool appTestBatteryStateIsTooLow(void);

/*! \brief Return the number of milliseconds taken for the battery measurement
    filter to fully respond to a change in input battery voltage */
uint32 appTestBatteryFilterResponseTimeMs(void);

/*! \brief Allow tests to control whether the headset will enter dormant.
    If an headset enters dormant, cannot be woken by a test.
    \note Even if dormant mode is enabled, the application may not
        enter dormant. Dormant won't be used if the application is
        busy, or connected to a charger - both of which are quite
        likely for an application under test.
    \param  enable  Use FALSE to disable dormant, or TRUE to enable.
*/
void appTestPowerAllowDormant(bool enable);

/*! \brief Simple test function to get the DSP clock speed in MegaHertz during the Active mode.
    \note Make sure DSP is up and running before using this test function on pydbg window to avoid panic.
    \returns DSP clock speed in MegaHertz during the Active mode
*/
uint32 HeadsetTest_DspClockSpeedInActiveMode(void);

/*! \brief Disable charger checks which would prevent going to sleep. */
void appTestForceAllowSleep(void);

/*! \brief Puts headset to sleep */
void appTestTriggerSleep(void);

/*! \brief Triggers headset limbo timer timeout */
void appTestTriggerLimboTimerTimeout(void);

/*! \brief  Enable leak-through in the Headset. */
void HeadsetTest_SetLeakthroughEnable(void);

/*! \brief  Disable leak-through in the Headset. */
void HeadsetTest_SetLeakthroughDisable(void);

/*! \brief  Toggle leak-through state (Enable or Disable) in the Headset. */
void HeadsetTest_SetLeakthroughToggleOnOff(void);

/*! \brief  Set the leak-through mode in the Headset.
    \param  leakthrough_mode need to be given by user to set the dedicated mode. valid values are LEAKTHROUGH_MODE_1,
    LEAKTHROUGH_MODE_2 and LEAKTHROUGH_MODE_3
*/
void HeadsetTest_SetLeakthroughMode(leakthrough_mode_t leakthrough_mode);

/*! \brief  Set the next leak-through mode in the Headset. */
void HeadsetTest_SetLeakthroughNextMode(void);

/*! \brief Get the leak-through mode in the Headset.
 *  \return LEAKTHROUGH_MODE_1, LEAKTHROUGH_MODE_2 or LEAKTHROUGH_MODE_1 if leakthrough is enabled else return Leakthrough disabled.
*/
leakthrough_mode_t HeadsetTest_GetLeakthroughMode(void);

/*! \brief Get the leak-through status (enabled or disabled) in the Headset.
    \return bool TRUE if leak-through is enabled
                    else FALSE
*/

bool HeadsetTest_IsLeakthroughEnabled(void);

#ifdef HAVE_THERMISTOR
/*! \brief Returns the expected thermistor voltage at a specified temperature.
    \param temperature The specified temperature in degrees Celsius.
    \return The equivalent milli-voltage.
*/
uint16 appTestThermistorDegreesCelsiusToMillivolts(int8 temperature);

/*! \brief Sets test temperature value to be returned back by 
    appTestGetBatteryTemperature().
    \param new_temp The temperature in degrees Celsius.
*/
void appTestSetBatteryTemperature(int8 new_temp);

/*! \brief Unsets test temperature value and restarts normal monitoring.
*/
void appTestUnsetBatteryTemperature(void);

/*! \brief Sets the battery temperature to be used instead of ADC measurements.
    \param new_temp The temperature in degrees Celsius.
*/
void appTestInjectBatteryTestTemperature(int8 new_temp);

/*! \brief Restart normal battery temperature measurements.
*/
void appTestRestartTemperatureMeasurements(void);

/*! \brief Returns the current battery temperature
 */
int8 appTestGetBatteryTemperature(void);

#endif


/* ANC specific Test APIs in Headset for ANC support */

/*! \brief  Set ANC Enable state in Headset. */
void HeadsetTest_SetAncEnable(void);

/*! \brief  Set ANC Disable state in Headset. */
void HeadsetTest_SetAncDisable(void);

/*! \brief  Set ANC Enable/Disable state in Headset. */
void HeadsetTest_SetAncToggleOnOff(void);

/*! \brief  Set ANC mode in Headset.
    \param  mode Mode need to be given by user to set the dedicated mode.
*/
void HeadsetTest_SetAncMode(anc_mode_t mode);

/*! \brief  Set ANC next mode in Headset. */
void HeadsetTest_SetAncNextMode(void);

/*! \brief  To get the current ANC State in Headset.
    \return bool TRUE if ANC enabled
                 else FALSE
*/
bool HeadsetTest_GetAncstate(void);

/*! \brief  To get the current ANC mode in Headset.
    \return Return the current ANC mode.
*/
anc_mode_t HeadsetTest_GetAncMode(void);

/*! \brief Sets Leakthrough gain of ANC H/W for current mode
    \param gain Leakthrough gain to be set
*/
void HeadsetTest_SetAncLeakthroughGain(uint8 gain);

/*! \brief Obtains Leakthrough gain of ANC H/W for current gain
    \return uint8 gain Leakthrough gain
*/
uint8 HeadsetTest_GetAncLeakthroughGain(void);

/*! \brief Write delta gains for all instances and paths
    \return bool TRUE if write successful
                 else FALSE
*/
bool HeadsetTest_WriteAncDeltaGains(int16 inst_0_ffa_delta, int16 inst_0_ffb_delta,
    int16 inst_0_fb_delta, int16 inst_1_ffa_delta, int16 inst_1_ffb_delta,
    int16 inst_1_fb_delta);

/*! \brief Write delta gains for given instances and paths
    \return bool TRUE if write successful
                 else FALSE
*/
bool HeadsetTest_WriteAncDeltaGain(audio_anc_path_id path, audio_anc_instance inst, int16 delta);

/*! \brief Read delta gain value from ANC_DELTA_USR_PSKEY(USR13 PSKEY) for given ANC path and instance.
    \return Returns delta gain for given path (in dB scale S6.9 representation; i.e., (int16) dB * 512)
*/
int16 HeadsetTest_ReadAncDeltaGain(audio_anc_path_id path, audio_anc_instance inst);

/*! \brief  Start ANC tuning mode in Headset. */
void HeadsetTest_StartAncTuning(void);


/*! \brief  Stop ANC tuning mode in Headset. */
void HeadsetTest_StopAncTuning(void);

#ifdef INCLUDE_WIRED_ANALOG_AUDIO

/*! \brief To simulate plugin in of 3.5mm jack to the board.
NOTE: NOT to be used if boards support PIO detection. The toggling of PIO from high to low indicates 3.5mm jack is plugged in */
void HeadsetTest_WiredAnalogAudioPlugged(void);

/*! \brief To simulate plugging out of 3.5mm jack from the board.
NOTE: NOT to be used if boards support PIO detection. The toggling of PIO from low to high indicates 3.5mm jack is plugged out */
void HeadsetTest_WiredAnalogAudioUnplugged(void);

/*! \brief To simulate incrementing the volume by a step.*/
void HeadsetTest_WiredAudioIncrementVolume(audio_source_t source);

/*! \brief To simulate decrementing the volume by a step.*/
void HeadsetTest_WiredAudioDecrementVolume(audio_source_t source);

/*! \brief To get the connection status for wired audio LINE IN Source .*/
bool HeadsetTest_IsLineInAudioAvailable(void);

#endif /* INCLUDE_WIRED_ANALOG_AUDIO */

/*! \brief Select USB audio application, that support both headphone
    (for music playback) and headset (for voice calls) devices within
    same audio function, as USB application in active state of headset_sm.
    Headset and Headphone devices will share common speaker
    To be called only when headset is in LIMBO state.

    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbAudioVoice1Af_Connect(void);

/*! \brief Select USB audio application, that support both headphone
    (for music playback) and headset (for voice calls) devices with
    separate audio function, as USB application in active state of headset_sm.
    To be called only when headset is in LIMBO state.

    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbAudioVoice2Af_Connect(void);

/*! \brief Select USB audio headphone device (for music playback) as
    USB application in active state of headset_sm.
    To be called only when headset is in LIMBO state.

    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbAudio_Connect(void);

/*! \brief Select USB audio headset device (for voice calls) as
    USB application in active state of headset_sm.
    Should be called when headset is in LIMBO state.

    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbVoice_Connect(void);

/*! \brief Select USB device without USB audio function as
    USB application in active state of headset_sm.
    Should be called when headset is in LIMBO state.

    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbDefault_Connect(void);

#ifdef INCLUDE_USB_NB_WB_TEST

/*! \brief Select USB audio headset device (for voice calls in narrowband) as
    USB application in active state of headset_sm.
    Should be called when headset is in LIMBO state.

    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbVoiceNb_Connect(void);

/*! \brief Select USB audio headset device (for voice calls in wideband) as
    USB application in active state of headset_sm.
    Should be called when headset is in LIMBO state.

    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbVoiceWb_Connect(void);

#endif/* INCLUDE_USB_NB_WB_TEST */

/*! \brief To check whether USB Audio device is enumerated
    \return TRUE if enumerated otherwise False
*/
bool HeadsetTest_IsUsbAudioEnumerated(void);

/*! \brief To check whether USB Voice device is enumerated
    \return TRUE if enumerated otherwise False
*/
bool HeadsetTest_IsUsbVoiceEnumerated(void);

/*! \brief To check whether USB Dual Audio and Voice devices are enumerated
    \return TRUE if enumerated otherwise False
*/
bool HeadsetTest_IsUsbDualAudioEnumerated(void);

/*! \brief To Simulate USB Host disconnection even when device connected to host via USB
     This is done by disconnecting USB application device with internal hub

    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbDetach(void);

/*! \brief To Simulate USB Host re-connection by re-connecting USB application device with internal hub

    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbAttach(void);

/*! \brief Send a HID consumer Play/Pause event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportPlayPause(void);

/*! \brief Send a HID consumer Play event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportPlay(void);

/*! \brief Send a HID consumer Pause event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportPause(void);

/*! \brief Send a HID consumer Volume Up event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportVolumeUp(void);

/*! \brief Send a HID consumer Volume Down event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportVolumeDown(void);

/*! \brief Send a HID consumer Mute event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportMute(void);

/*! \brief Send a HID consumer Fast Forward ON event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportFfwdOn(void);

/*! \brief Send a HID consumer Fast Forward OFF event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportFfwdOff(void);

/*! \brief Send a HID consumer Rewind ON event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportRewOn(void);

/*! \brief Send a HID consumer Rewind OFF event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportRewOff(void);

/*! \brief Send a HID consumer Next Track event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportNextTrack(void);

/*! \brief Send a HID consumer Previous Track event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportPreviousTrack(void);

/*! \brief Send a HID consumer Stop event over USB
    \return TRUE if operation is successful otherwise False
*/
bool HeadsetTest_UsbHidTransportStop(void);

/*!
 * \brief This returns true if handset is connected over qhs and false if 
          handset is not connected over qhs
 * \return true or false as per description.
 */
bool appTestIsHandsetQhsConnected(void);

/*! \brief Get the major, minor and config versions from the upgrade library.
    \note   Prints the result to DEBUG_LOG as "major.minor.config"
    \note   Cannot return uint64 to pydbg so having to return low byte of major and config 
    \return (major << 24) | (minor << 8) | (config & 0xFF)
 */
uint32 appTestUpgradeGetVersion(void);


#ifdef INCLUDE_GAA
#include "gaa_ota.h"
/*! \brief Get the current state of GAA OTA.
    \return uint8 from GAA_OTA_STATE_T enum, or 255 if GAA OTA is not initialised
*/
uint8 HeadsetTest_GetGaaOtaState(void);

#include "gaa_info.h"
/*! \brief Get the current GAA model ID
    \return Current GAA model ID (zero if failed)
*/
uint32 HeadsetTest_GetGaaModelId(void);
/*! \brief Set the GAA model ID to the paramter value
    \param uint32 model_id The new GAA model ID
    \return TRUE is successful
*/
bool HeadsetTest_SetGaaModelId(uint32 model_id);

/*! \brief Get the GAA OTA control value
    \return uint16 GAA OTA control value if successfu, else MAX_UINT32 if not
*/
uint32 appTestGetGaaOtaControl(void);

/*! \brief Set the GAA OTA control value
*/
void appTestSetGaaOtaControl(uint16 GaaOtaControl);

#endif  /* INCLUDE_GAA */

#ifdef INCLUDE_GAMING_MODE
/*! Check if Gaming Mode is enabled. 
    \return TRUE if gaming mode is ON, FALSE otherwise.
*/
bool appTestIsGamingModeOn(void);


/*! Enable/Disable Gaming Mode. This injects a ui_event (i.e. mimics the UI 
    button press event).
    This can be called on both primary and secondary.

    \param[in] enable TRUE to enable gaming mode,
                      FALSE to disable.
    \return TRUE if command is successful.
 */
bool appTestSetGamingMode(bool enable);
#endif /* INCLUDE_GAMING_MODE */

#ifdef INCLUDE_LATENCY_MANAGER
/*! Check if Dynamic Latency adjustment is enabled. 
    \return TRUE if enabled, FALSE otherwise.
*/
bool appTestIsDynamicLatencyAdjustmentEnabled(void);

/*! Set Dynamic Latency Adjustment configuration. This API enables or 
disables the latency
    management feature. By default Latency adjustment is disabled during 
initialization.
    This API can be invoked any time post initialization.    
    \param[in] enable TRUE to enable dynamic adjustment feature.
                      FALSE to disable
*/
void appTestSetDynamicLatencyAdjustment(bool enable);

/*! Get current A2DP latency. 
    \returns latency in milliseconds.
*/
uint16 appTestGetCurrentA2dpLatency(void);
#endif /* INCLUDE_LATENCY_MANAGER */

/*! \brief Request that the radio transmitter be enabled or disabled
 *  \return TRUE if the request was satisfied, FALSE if it was not possible.
*/
bool appTestTransmitEnable(bool);


#ifdef INCLUDE_MUSIC_PROCESSING

/*! \brief Prints out the available presets and their UCIDs
*/
void appTest_MusicProcessingGetAvailablePresets(void);

/*! \brief Sets a new EQ preset

    \param preset   The preset UCID
*/
void appTest_MusicProcessingSetPreset(uint8 preset);

/*! \brief Prints out the active EQ type
*/
void appTest_MusicProcessingGetActiveEqType(void);

/*! \brief Prints out the total number of user EQ bands that can be modified
*/
void appTest_MusicProcessingGetNumberOfActiveBands(void);

/*! \brief Prints out the current user EQ band information
*/
void appTest_MusicProcessingGetEqBandInformation(void);

/*! \brief Sets the gain of a specific band

    \param band     Band to modify

    \param gain     New gain value to set
*/
void appTest_MusicProcessingSetUserEqBand(uint8 band, int16 gain);

#endif /* INCLUDE_MEDIA_PROCESSING */

#ifdef INCLUDE_FAST_PAIR
/*! \brief This test API is used to configure fast pair model ID
    \param fast_pair_model_id
    \return void.
    \note The input paramater is of type uint32
    \note Eg: apps1.fw.call.appTestSetFPModelID(0x00D0082F)
    \note The fast pair model ID is written to USR5 PS Key using PsStore API
*/
void appTestSetFPModelID(uint32 fast_pair_model_id);

/*! \brief This test API is used to configure scrambled ASPK
    \param dword1
    \param dword2
    \param dword3
    \param dword4
    \param dword5
    \param dword6
    \param dword7
    \param dword8
    \return void.
    \note All input paramater is of type uint32
    \note SCRAMBLED_ASPK {0x427C, 0x1EE8, 0x939A, 0xFF17, 0x555E, 0x730B, 0x3B4C, 0x67E4,
                          0xD0E3, 0x68E0, 0x1D6B, 0x9A25, 0x799F, 0x4ED8, 0xAA6E, 0x33C5}
    \note For the above aspk use the below pydebug command to program aspk
    \note Eg: apps1.fw.call.appTestSetFPASPK(0x1EE8427C, 0xFF17939A, 0x730B555E, 0x67E43B4C,
                                             0x68E0D0E3, 0x9A251D6B, 0x4ED8799F, 0x33C5AA6E)
    \note The scrambled aspk is written to USR6 PS Key using PsStore API
*/
void appTestSetFPASPK(uint32 dword1, uint32 dword2, uint32 dword3, uint32 dword4,
                      uint32 dword5, uint32 dword6, uint32 dword7, uint32 dword8);
#endif /* INCLUDE_FAST_PAIR */

#ifdef INCLUDE_CHARGER

/*! Simulate charger connected/disconnected events
 *
 * \param is_connected TRUE to simulate attach or FALSE for detach */
void appTestChargerConnected(bool is_connected);

/*! Simulate detection of particular charger
 *
 * \param attached_status type of charger detected
 * \param charger_dp_millivolts voltage measured on USB_DP line
 * \param charger_dm_millivolts voltage measured on USB_DM line
 * \param cc_status USB_CC line state
 * */
void appTestChargerDetected(usb_attached_status attached_status,
                                       uint16 charger_dp_millivolts,
                                       uint16 charger_dm_millivolts,
                                       usb_type_c_advertisement );

/*! Simulate charger status events
 *
 * \param chg_status charger status to simulate */
void appTestChargerStatus(charger_status chg_status);

/*! Return back to normal operation after appsTestCharger* calls. */
void appTestChargerRestore(void);

/*! Return charger type detected by the application. */
charger_detect_type appTestChargerGetType(void);

#endif /* INCLUDE_CHARGER */

#ifdef INCLUDE_USB_DEVICE

/*! Init USB HID datalink test case */
void appTestUsbHidInit(void);

/*! Close USB HID datalink test case */
void appTestUsbHidDeInit(void);

/*! Init USB CDC test case */
void appTestUsbCdcInit(bool enable_spi_ram_test);

/*! Close USB CDC test case */
void appTestUsbCdcDeInit(void);

#endif /* INCLUDE_USB_DEVICE */

#endif // HEADSET_TEST_H
