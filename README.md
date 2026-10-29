# STM32 DC Motor Controller

DC motor control system based on the **STM32 NUCLEO-F401RE**.

The project implements PWM motor speed control, forward/reverse direction control, temperature monitoring, an OLED user interface, physical control buttons, status LEDs, and UART diagnostics.

## Features

- DC motor speed control using hardware PWM
- Potentiometer-based speed setpoint from 0 to 100%
- RUN and STOP control
- Forward / Reverse motor direction
- SSD1306 128x64 OLED display over I2C
- Two NTC 10k B3950 temperature sensors
- Three status LEDs
- UART monitoring through ST-LINK Virtual COM Port
- L298N H-bridge motor driver
- XL4015 DC-DC power converter

## Hardware

- STM32 NUCLEO-F401RE
- L298N motor driver
- XL4015 CC/CV DC-DC converter
- 6 V DC geared motor
- SSD1306 128x64 I2C OLED
- 10 kΩ potentiometer
- 2 × NTC 10 kΩ B3950
- 3 × push buttons
- Green, yellow and red LEDs
- 12 V / 3 A power supply

## Pinout

| Function         | STM32 Pin      | NUCLEO Pin |
| ---------------- | -------------- | ---------- |
| Potentiometer    | PA0            | A0         |
| NTC1             | PA1            | A1         |
| NTC2             | PA4            | A2         |
| Motor PWM / ENB  | PB3 / TIM2_CH2 | D3         |
| Green LED        | PB5            | D4         |
| Yellow LED       | PB4            | D5         |
| Red LED          | PB10           | D6         |
| RUN button       | PA8            | D7         |
| STOP button      | PA9            | D8         |
| Direction button | PC7            | D9         |
| L298N IN3        | PA7            | D11        |
| L298N IN4        | PA6            | D12        |
| OLED SDA         | PB9            | D14        |
| OLED SCL         | PB8            | D15        |

## Motor Control

The motor is connected to the second channel of the L298N:

```text
STM32 D3 / PB3  -> ENB
STM32 D11 / PA7 -> IN3
STM32 D12 / PA6 -> IN4

L298N OUT3 -> Motor
L298N OUT4 -> Motor
```

`TIM2_CH2` generates the PWM signal used to control the motor speed.

The potentiometer provides a speed setpoint from 0 to 100%.

## Controls

- **RUN** — starts the motor
- **STOP** — stops the motor
- **DIR** — changes the motor rotation direction

Status LEDs:

- Green — motor running
- Red — motor stopped
- Yellow — reverse direction selected

## Display

The OLED displays:

- Motor speed setpoint
- Temperature sensor 1
- Temperature sensor 2
- Motor state
- Motor direction

Example:

```text
SET: 65 %
T1: 24 C
T2: 23 C
STATE: RUN
DIR: FORWARD
```

## Power

```text
12 V / 3 A PSU
      |
      v
XL4015 DC-DC
      |
      | ~7 V
      v
L298N
      |
      v
DC Motor
```

The STM32 and L298N share a common ground.

## Software

- STM32CubeMX
- STM32CubeIDE
- STM32 HAL
- C
- Altium Designer

## Project Status

Current version: **v0.1**

Implemented:

- [x] ADC acquisition
- [x] Potentiometer input
- [x] NTC temperature measurement
- [x] SSD1306 OLED
- [x] RUN / STOP buttons
- [x] Forward / Reverse control
- [x] PWM motor speed control
- [x] L298N motor driver
- [x] UART diagnostics
- [x] Status LEDs
- [x] Altium schematic

Planned:

- [ ] RS485 communication
- [ ] Modbus RTU
- [ ] RPM measurement
- [ ] Closed-loop speed control
- [ ] PID controller

## Repository Structure

```text
STM32-DC-Motor-Controller/
├── Core/
├── Drivers/
├── Hardware/
│   └── STM32_DC_Motor_Controller.SchDoc
├── Images/
├── Motor_Controller.ioc
├── README.md
└── .gitignore
```
