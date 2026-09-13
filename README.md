## DC Motor Speed Controller

I made this project alongside my computer science studies at ELTE to practice C
and learn the basics of feedback control.

It simulates a DC motor controlled by a PI controller. The target and measured
speeds use simulated 12 bit ADC values, and the controller produces a PWM duty
cycle every 10 ms. The ADC, PWM and GPIO interfaces are simulated, so the
program runs without hardware.

## Build

```sh
make
./build/motor_control
```

## Tests

```sh
make test
```

The motor model is intentionally simple. AI was used to assist with tests and documentation.
