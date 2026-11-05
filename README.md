# Lab 8: CAN Bus Telemetry and Remote Motor Control

Two TM4C123GH6PM LaunchPads (on EduARM4 trainer boards) talk over a CAN bus at 500 kbps.

- **Node 1 (Sensor & Telemetry):** reads a potentiometer and a DHT11 temperature sensor, tracks SW1/SW2 presses, sends everything in one CAN frame every 100 ms and shows it on the LCD.
- **Node 2 (Actuation & Display):** receives the frame, drives two DC motors through an L298N with PWM, and shows each motor's speed and direction on the LCD.

## Files

| File | Board | Purpose |
|---|---|---|
| `node1_sensor.c` | Node 1 | Pot (ADC), DHT11, switches, LCD, CAN transmit |
| `node2_actuator.c` | Node 2 | CAN receive, PWM motor control, direction, LCD |

Both are plain register-level C for the default 16 MHz clock and only need `tm4c123gh6pm.h`.

## Pin connections

### MCP2551 module (same on both boards)

| Module pin | Tiva |
|---|---|
| VCC | VBUS (5 V) |
| GND | GND |
| TX | PE5 (CAN0 TX) |
| RX | PE4 (CAN0 RX) through 1 kΩ, with 2.2 kΩ from PE4 to GND |
| CANH / CANL | to the other module's CANH / CANL |

### Node 1: Sensor & Telemetry

| Device | Pin | Tiva |
|---|---|---|
| Potentiometer | wiper | PE3 (AIN0), ends to 3.3 V and GND |
| DHT11 | DATA | PD2 (J3 pin 5) |
| | VCC / GND | 5 V / GND |
| SW1 | onboard | PF4 |
| SW2 | onboard | PF0 (unlocked in code) |
| LCD (onboard, 8-bit) | RS / E | PA6 / PA7 |
| | D0-D7 | PB0-PB7 |
| | RW | GND (on board) |

### Node 2: Actuation & Display

| Device | Pin | Tiva / connection |
|---|---|---|
| L298N | ENA (jumper removed) | PF2 (M1PWM6), Motor-1 speed |
| | IN1 / IN2 | PA2 / PA3, Motor-1 direction |
| | ENB (jumper removed) | PF3 (M1PWM7), Motor-2 speed |
| | IN3 / IN4 | PA4 / PA5, Motor-2 direction |
| | OUT1-OUT2 / OUT3-OUT4 | Motor-1 / Motor-2 |
| | +12V / GND | external supply + / supply - and Tiva GND |
| | 5V | not connected |
| LCD (onboard, 8-bit) | RS / E / D0-D7 | PA6 / PA7 / PB0-PB7 |

### Between the boards

| Node 1 | Node 2 |
|---|---|
| CANH | CANH |
| CANL | CANL |
| GND | GND |

- 120 Ω termination at each end; with power off, CANH to CANL should read about 60 Ω.
- All grounds are common: both Tivas, both MCP2551s, the L298N and the motor supply.
- EduARM4 jumper **J5 shorted on 3-4** selects the LCD on both boards.

## CAN frame

ID `0x100` (standard 11-bit), DLC 5, sent every 100 ms by Node 1.

| Byte | Content |
|---|---|
| 0 | Potentiometer, 0-100 % |
| 1 | Temperature high byte (°C) |
| 2 | Temperature low byte (°C) |
| 3 | Flags: bit0 = Motor-1 direction (SW1), bit1 = Motor-2 direction (SW2), bit2 = sensor fault |
| 4 | Rolling counter |

Direction: 0 = clockwise, 1 = anticlockwise. Bit timing at 16 MHz: prescaler 2, TSEG1 = 12, TSEG2 = 3, SJW = 2 (`CAN0_BIT_R = 0x2B41`), sample point about 81 %.

## Control logic

| Output | Rule |
|---|---|
| Motor-1 speed | PWM duty = pot % |
| Motor-2 speed | 0 % at or below 20 °C; 30 % at 20 °C rising linearly to 100 % at 50 °C |
| Motor-1 direction | toggled by each SW1 press |
| Motor-2 direction | toggled by each SW2 press |
| Link lost | no frame for 500 ms: both motors stop, LCD shows `NO CAN DATA` |

PWM: PWM1 generator 3, 1 kHz. DHT11 is read every 2 s (it allows at most one reading per second).

## LCD screens

| Node | Line 1 | Line 2 |
|---|---|---|
| Node 1 | `T:027°C P:045%` | `S1:CW  S2:ACW` |
| Node 2 | `M1:045% CW` | `M2:046% ACW` |

Node 1 shows `E1`-`E5` in place of the temperature if the DHT11 read fails (E1 no response, E2/E3 line stuck, E4 bit timing, E5 checksum).

## Build and run

1. Create a CCS project per board for TM4C123GH6PM and add the matching `.c` file.
2. Set the LaunchPad power switch to DEBUG and use the debug USB port.
3. Flash Node 1 and Node 2, wire the bus, power the motor supply.
4. Turn the pot (Motor-1 follows), warm the DHT11 (Motor-2 speeds up), press SW1/SW2 (directions reverse).
