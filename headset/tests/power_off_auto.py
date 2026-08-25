import time

#This headset version of the test

from csr.front_end.pydbg_front_end import PydbgFrontEndBase
from test_helper import TestError, test_timeout, test_assert, setup_logging
from test_setup import device_options

def isSmInState(device, state):
    return device.fw.gbl.headset_sm.state.value == device.fw.env.enums["headsetState"][state]

device1,trans1 = PydbgFrontEndBase.attach(device_options[0])
hs = device1.chips[0].apps_subsystem.p1

print("Reset headset")
device1.reset()


print("Wait 1 second to allow bootup")
time.sleep(1)
device1.apply_reset()


print(hs.fw.gbl.system_state_ctx.state)
print(hs.fw.gbl.headset_sm.state)

test_timeout(20, lambda: isSmInState(hs, "HEADSET_STATE_LIMBO"))

print("Turn on headset")

hs.fw.call.appTestHeadsetPowerOn()

test_timeout(20, lambda: isSmInState(hs, "HEADSET_STATE_IDLE"))

print("Headset is on")

if hs.fw.call.appTestIsHandsetConnected():
    print("ERROR: Make sure handset is not connected, that will prevent auto power off")
    
time.sleep(1)
print(hs.fw.gbl.system_state_ctx.state)
print(hs.fw.gbl.headset_sm.state)
print(hs.fw.gbl.app_power.state)

print("Trigger auto power off")
hs.fw.gbl.app_power.panic_on_sleep.value = 1
hs.fw.call.appTestTriggerSleep()

print(hs.fw.gbl.system_state_ctx.state)
 
for retry in range(1,10):
    time.sleep(1)
    went_to_sleep = hs.fw.gbl.app_power.went_to_sleep.value
    if went_to_sleep == 1:
        print("TEST PASS: headset powered off")
        break
    else:
        print("attempt {:d}, still waiting".format(retry))
 
print(hs.fw.gbl.system_state_ctx.state)
