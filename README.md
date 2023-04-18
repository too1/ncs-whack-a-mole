# NCS-Whack-A-Mole

## Overview

This is a demo showcasing the multi link and multi role capabilities of the Nordic Bluetooth stacks, by implementing a simple game inspired by the old Whac-A-Mole arcade game where the user have to react quickly to the random appearance of "moles". 

The demo consists of a central board which implements the main game logic, and a number of peripheral devices connected to it (normally based on the Thingy:52 hardware). Rather than having to hit a mole that pops up at random times the user has to press the button on the Thingy when the LED turns on, and the response time will be measured and sent back to the central controller. 
Additionally an Android app is provided to show the status of the game, and the controller board will connect to the Android device in the peripheral role, while being connected to the Thingy's as a central. 


## Requirements

- nRF Connect SDK v2.2.0

### Tested boards:
#### Central:
- nrf52840dk_nrf52840
- (most likely other nrf52 based DK's will work, but only the 52840dk is tested)

#### Peripheral:
- thingy52_nrf52832
- nrf52840dk_nrf52840
- nrf5340dk_nrf5340_cpuapp

## Running the demo

The central and peripheral applications need to be built as any other nRF Connect SDK application. 

The central application must be flashed to the central board, and the peripheral application must be flashed to the various peripherals. In order to flash the Thingy:52 a 10-pin JLink cable will be needed, and a DK (such as the nRF52840DK) can be used as a programmer.

The demo has been tested with between 2-8 peripheral devices, and a minimum of 6 is recommended for a more challenging game. 

The central board provides a simplified GUI through the UART terminal, in case an Android tablet is not available. To use this simply connect a terminal to the JLink comport set up by the controller board, and verify that there is output on the terminal when the controller boots. 

In order to use the Android app this needs to be built manually and flashed into an Android device, based on [this repository](https://github.com/too1/android-whackamole).
After launching the app, connect to the central board which will advertise as "Whack-Central". The app uses UUID filtering to avoid showing other BLE advertisers, so normally there should only be one device to connect to. 

The Thingy:52 peripherals will blink blue when not connected (advertising), and should turn off the LED once they are connected. 
The terminal interface will show the number of peripherals currently connected to the central. 
Once all the peripherals have connected a new game can be started by pressing a button on any of the peripherals. 

From there on, simply follow the instructions in the terminal. The game will go through multiple rounds of increasing difficulty, and at the end of the game a final score will be displayed based on the performance of the player. 

## TODO
- Implement a proper high score feature, to allow players to register their name and have the results stored permanently in the flash of the controller.
