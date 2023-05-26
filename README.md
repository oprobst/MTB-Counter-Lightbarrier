# Lightbarrier with Atmega Microcontroller to count interrupts.

This implementation was initially intended for counting mountain biker on a trail on daily basis. But since this functionality is very generic it can be seen as a multi-purpose tool.

* Goal is to count the light barrier (LB) interrupts over the whole lifetime as well as on daily basis
* Additinally, it is required to use as less power as possible

This first version was build up on a perfboard, but future versions could be adopted to more advanced boards.

# Setup

The Atmega 328P Microcontroller runs without an external Quartz crystal to save power. It is restricted to 1 MHz and operated on 3,3V.
To simplify development, the arduino libraries are used. There is no onboard USB to TTL converter, so an external programmer is required anyway.

This version powers a Hitatchi HD44780 128khz display. The display is off by default and only activated if user activates it via a button.

The schematics consists of two input contacts, eg. for buttons:
* User Button for the end user to turn the display on.
* Debug Button for development purposes to show current and to reset memory.

A phototransistor is used to measure brightness and power down the device at night. This is also the trigger to count for a new day. There is no additional time module onboard.



