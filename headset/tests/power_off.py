import sys
import time

from csr.front_end.pydbg_front_end import PydbgFrontEndBase
from test_helper import TestError, test_timeout, test_assert, setup_logging
from test_setup import device_options

def isSmInState(device, state):
    return device.fw.gbl.headset_sm.state.value == device.fw.env.enums["headsetState"][state]

from_idle = False
phone = False
if len(sys.argv) >= 2:
    if sys.argv[1] == "from_idle":
        from_idle = True
if len(sys.argv) == 3:
    if sys.argv[2] == "phone":
        phone = True

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

if from_idle:
    print("Turn on headset")
    
    hs.fw.call.appTestHeadsetPowerOn()
    
    test_timeout(20, lambda: isSmInState(hs, "HEADSET_STATE_IDLE"))
    
    print("Headset is on")

time.sleep(1)
print(hs.fw.gbl.system_state_ctx.state)
print(hs.fw.gbl.headset_sm.state)
print(hs.fw.gbl.app_power.state)

if phone:
    print("Waiting for phone to connect")
    test_timeout(20, lambda: hs.fw.call.appTestIsHandsetConnected())
    print("Phone is connected")

hs.fw.call.appTestForceAllowSleep()
hs.fw.gbl.app_power.panic_on_sleep.value = 1

print("Set battery level below critical to trigger shut down")
hs.fw.call.appTestSetBatteryVoltage(2800)

for retry in range(1,10):
    time.sleep(1)
    powered_off = hs.fw.gbl.app_power.powered_off.value
    if powered_off == 1:
        print("TEST PASS: headset powered off")
        break
    else:
        print("attempt {:d}, still waiting".format(retry))

