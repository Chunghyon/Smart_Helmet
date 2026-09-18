# Testing the pulse-change detector

The SENS_IN / LIS3DH vitals path produces a **change flag**, not a heart
rate. Testing is therefore organised around two questions:

* does `hr_change` fire when the rate really changes (sensitivity, latency)?
* does it stay quiet when it does not (false-alarm rate)?

Three tiers, in order. Do not start a tier before the previous one passes —
a failure downstream is uninterpretable otherwise.

| tier | what | where |
|---|---|---|
| 0 | observability | options in `smart_helmet_config.h` |
| 1 | host regression, automated | [`headset/test/smart_helmet`](../../test/smart_helmet/README.md) |
| 2 | bench hardware-in-the-loop, no subject | this document |
| 3 | human subject with a reference device | this document |

> The module is not a medical device. Nothing in these procedures produces a
> clinically valid heart rate, and results must not be used as a basis for a
> medical judgement.

---

## Tier 0 — observability

Measurement is impossible without these, so turn them on before any bench
work.

| option | default | effect |
|---|---|---|
| `SMART_HELMET_ENABLE_VITALS_LOG2` | 1 | second calm-log line with `chg`, `base`, `d`, `cup`, `cdn`, `resp`, `rq`, `fs`, `jit`, `drop`, `rem`, `act`. Split from the main line because the log macro takes a limited number of arguments. |
| `SMART_HELMET_ENABLE_VITALS_CSV` | 0 | one CSV record per window through the telemetry sink. Column order is documented in the test README. |
| `SMART_HELMET_ENABLE_SENS_DUMP` | 0 | every raw SENS sample as `seq,mv,time_us`. High bandwidth (a line every 40 ms), but a capture can be replayed through the Tier 1 harness, which makes retuning possible without hardware. |
| `SMART_HELMET_TELEMETRY_LINE_MAX` | 160 | record size cap. |

The last two are inert until a sink is registered. `smart_helmet.c` registers
one that writes ASCII lines to the Wi-SUN UART
(`SMART_HELMET_WISUN_UART_BAUD`, 115200 by default); capture it with any
serial terminal that can log to a file.

Do not ship a production build with CSV or dump enabled.

---

## Tier 1 — host regression

Automated, run on every algorithm change:

```sh
headset/test/smart_helmet/run_tests.sh
```

See [the test README](../../test/smart_helmet/README.md) for the scenario
list, the option matrix and the three defects this tier has already caught.

---

## Tier 2 — bench HIL (real board, no subject)

Catches what only appears once the ADC, the timers and I2C are in the loop.

### 2.1 Signal injection

Replace the PD-V12 with a function generator on the SENS_IN path, attenuated
to the IF output's real range (check the DC operating point in the `sens=`
field of the calm log first, and keep the injected swing inside it). Repeat
the Tier 1 scenarios on hardware:

* 1.0 Hz sustained → no `chg`
* 1.0 Hz → 1.7 Hz step → `chg=2` (rising) within the Tier 1 latency budget
* 1.7 Hz → 1.0 Hz step → `chg=3` (falling)
* generator off for 30 s, then back on at 1.05 Hz → **no** change event
  (this is the scenario-7 regression on real hardware)

### 2.2 Sampling cadence — regression for the 1 Hz artefact

The gas scan runs at 1 s. Before `SMART_HELMET_ADC_SENS_PRIORITY`, a SENS
tick colliding with it was deferred and then fired immediately afterwards,
modulating the sampling interval at exactly 1 Hz — i.e. 60 BPM, in the middle
of the search band.

Record `fs`, `jit` and `drop` from the Tier 0 log line over at least 10
minutes in both settings:

| build | expectation |
|---|---|
| `SMART_HELMET_ADC_SENS_PRIORITY=0` | visible once-per-second bunching: `jit` clearly non-zero, `drop` occasionally non-zero |
| `SMART_HELMET_ADC_SENS_PRIORITY=1` (default) | `fs` ≈ 2500 (25.00 Hz), `jit` small and stable |

**If that difference is not observable, the fix is not actually taking
effect** — stop and investigate before going further.

Then inject a tone near 60 BPM and confirm that the `PRIORITY=0` build
reports a spurious line that the `PRIORITY=1` build does not.

### 2.3 Timing budget

Build with every estimator enabled (FFT + autocorrelation + NLMS + oversample
+ respiration) and confirm the per-window processing time does not encroach
on the 40 ms SENS tick. Watch `drop`: a rising drop count under the worst-case
option set means the DSP is stealing acquisition time.

### 2.4 Mock reflector

A rotating disc or a linear actuator producing a known small displacement
exercises the whole radar chain (antenna → IF → ADC), not just the DSP.
Stepping it from 1.0 Hz to 1.7 Hz must produce `rising`. This is the only
bench test that covers the analogue front end.

---

## Tier 3 — human subject

Requires a **ground truth**; without one no number from this tier can be
interpreted.

### Setup

* A commercial PPG or chest-strap HR monitor recording simultaneously.
* An agreed time-sync marker — e.g. tap the helmet sharply at the start of
  the session, which shows up in both the accelerometer trace and the
  reference log.
* Tier 0 CSV telemetry enabled and captured.

### Protocol

5 min rest → 1 min light exercise (stairs or squats) → 5 min recovery.

The exercise minute itself will be discarded by the motion gate, so do not
try to evaluate it. The meaningful comparison is **pre-exercise rest vs.
early recovery**, where the rate is genuinely different and the subject is
still again.

### Metrics

| metric | definition |
|---|---|
| availability | share of windows with `hr_valid` set. **Check this first** — if it is low, every other number is noise |
| sensitivity | share of reference-confirmed changes (≥ X BPM) that produced an `hr_change` |
| false-alarm rate | `hr_change` events per minute during reference-confirmed stable periods |
| detection latency | `hr_change` time minus the reference change time |

Pick the change threshold X from the reference log, not from the module's own
output.

### Conditions to vary

Helmet position, hair and headwear, distance and angle to the chest, and
nearby metal. Each of these changes the coupling; record which condition each
session used, otherwise the sessions are not comparable.

---

## Priority

1. Tier 0 — without it Tiers 2 and 3 are guesswork.
2. Tier 1 — the safety net for every later change.
3. Tier 2.2 — confirms the cadence fix works on real hardware. If it does
   not, Tier 3 is meaningless.
4. Tier 3 — only after the above pass.
