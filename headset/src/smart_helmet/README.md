# Smart Helmet interfaces (QCC3044)

Schematic reference: `artifacts/schematics/SMART_HELMET_260816_1.DSN`

| Interface | Nets | Devices |
|-----------|------|---------|
| I2C0 | SCL, SDA | CJMCU-8118 (CCS811), LIS3DH, GY-906-BAA (MLX90614), SSD1315 optional |
| I2C1 | SCL1, SDA1 | SSD1315 optional |
| ADC | SENS_IN, CO, NH3, NO2 | MICS-6814 / air path |
| UART | TXD, RXD | Wi-SUN module |

## Integration

1. Edit **PIO / ADC source** placeholders in `smart_helmet_config.h` to match the QCC3044 netlist.
2. Add the `.c` files under this folder to `headset.x2p` (or SCons source list).
3. From headset init:

```c
#include "smart_helmet.h"

SmartHelmet_Init(appGetAppTask());
/* later */
SmartHelmet_PollSensors();
```

4. CCS811 and MLX90614 both commonly use address `0x5A` — confirm PCB addressing if both are on I2C0.
