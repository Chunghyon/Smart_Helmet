'''
Copyright (c) 2020 Qualcomm Technologies International, Ltd.
    %%version

A script to test limbo timer after powering on or in limbo state before powering on.

input parameters: 
"-l limbo" if you want to power off from limbo state.
"-c charger" if you want to test limbo timer with charger connected"

NOTE:: Do not run the platform_creator.py from any other location than suggested. 
'''
import sys
import time

#This headset version of the test
import argparse
from csr.front_end.pydbg_front_end import PydbgFrontEndBase
from test_helper import TestError, test_timeout, test_assert, setup_logging
from test_setup import device_options


def isSmInState(device, state):
    return device.fw.gbl.headset_sm.state.value == device.fw.env.enums["headsetState"][state]

def CheckPowerOff(hs):
    time.sleep(1)
    print(hs.fw.gbl.system_state_ctx.state)
    print(hs.fw.gbl.headset_sm.state)
    print(hs.fw.gbl.app_power.state)
    
    for retry in range(1,10):
        time.sleep(1)
        went_to_sleep = hs.fw.gbl.app_power.went_to_sleep.value
        if went_to_sleep == 1:
            print("TEST PASS: headset powered off")
            break
        else:
            print("attempt {:d}, still waiting".format(retry))

def removeChargerAndCheckLimboTimerPowerOff(hs):
    time.sleep(1)
    print(hs.fw.gbl.system_state_ctx.state)
    print(hs.fw.gbl.headset_sm.state)
    print(hs.fw.gbl.app_power.state)
    went_to_sleep = hs.fw.gbl.app_power.went_to_sleep.value
    if went_to_sleep == 1:
        print("TEST FAIL: headset powered off even with charger connected")
        return
    raw_input("please remove the charger & then press Enter to continue")
    hs.fw.call.appTestTriggerLimboTimerTimeout()
    CheckPowerOff(hs)

def checkLimboTimerPowerOff(hs):
    hs.fw.call.appTestTriggerLimboTimerTimeout()
    CheckPowerOff(hs)
	

def main(input_args):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter,
                                    epilog='IMPORTANT: python 2.6+ is required')    		
    parser.add_argument('-c', '--charger',  type=str.lower,
                    help='[Optional] "-c charger" if charger is connected')
    parser.add_argument('-l', '--limbo', type=str.lower,
                    help='[Optional] "-l limbo	" if want to power off from limbo state')
    args = parser.parse_args(input_args)
    limbo = False
    charger = False
    if args.charger:
        charger=True
    if args.limbo:
        limbo=True	
    device1,trans1 = PydbgFrontEndBase.attach(device_options[0])
    hs = device1.chips[0].apps_subsystem.p1

    print("Reset headset")
    device1.reset()

    print("Wait 1 second to allow boot-up")
    time.sleep(1)
    device1.apply_reset()
    print(hs.fw.gbl.system_state_ctx.state)
    print(hs.fw.gbl.headset_sm.state)
    test_timeout(20, lambda: isSmInState(hs, "HEADSET_STATE_LIMBO"))
    hs.fw.gbl.app_power.panic_on_sleep.value = 1
    if limbo == True:
        if charger==True:
            removeChargerAndCheckLimboTimerPowerOff(hs)
        else:
            checkLimboTimerPowerOff(hs)    
        return			
		

    print("Turn on headset")
    hs.fw.call.appTestHeadsetPowerOn()
    print("Headset is on")
    time.sleep(1)
    print(hs.fw.gbl.system_state_ctx.state)
    print(hs.fw.gbl.headset_sm.state)
    print(hs.fw.gbl.app_power.state)

    print("User is requesting to power off")
    hs.fw.call.appTestHeadsetPowerOff()
    time.sleep(1)	 
    if charger==True:
        removeChargerAndCheckLimboTimerPowerOff(hs);
    else:
        checkLimboTimerPowerOff(hs)    
			
		

if __name__ == "__main__":
    main(sys.argv[1:])	

