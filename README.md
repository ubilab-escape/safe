# Team Safe and Puzzles

## Motivation:
The Players are told to steal the Prototype from the Lab. The Prototype will be locked in the safe.



## General Idea:
After Entering the Lab Room the safe will be deactivated (red LED light). The players first have to supply the safe with power by solving a puzzle (see section puzzle).
This action will change the light color of the safes LED to orange and activate the second puzzle. This puzzle will ultimately open the safe to give access to the prototype.


## Puzzles:
### Puzzle to activate the safe: 
* Switching levers in correct position
* Find the correct resistor to generate the correct voltage
		

### Puzzle to open the safe:
* Riddle with numpad input
		

## Partlist

**part**|**count**|**price/part in €**|**price total**|**link**
:-----:|:-----:|:-----:|:-----:|:-----:
Keypad|1|5.8|5.8|[Octopart](https://octopart.com/3845-adafruit+industries-94636475?r=sp&s=QRIaZTRmSZu4F2mVarnONA)
Solenoid Lock|1|13.35|13.35|[Octopart](https://octopart.com/1512-adafruit+industries-51880349?r=sp&s=Jx9Gw6qzSH2-PR3HbMgn8w)
Banana Plug|10|2.06|20.6|[Octopart](https://octopart.com/1325-2-pomona+electronics-55397240?r=sp&s=kmyIgsMeQOCZjpt9e\_rAXw)
Banana Socket|10|1.84|18.4|[Octopart](https://octopart.com/bu-00233-0-mueller-824050?r=sp&s=FBVoi6XFSGGoTvQtIDogQw)
Piezo Buzzer|5|1.03|5.15|[Octopart](https://octopart.com/mckpt-g1711a3-05-3691-multicomp-12157125?r=sp&s=TdKAcML0QkSYqrwuZLE5lQ)
Switch|8|2.17|17.36|[Octopart](https://octopart.com/100sp1t1b4m2qe-e-switch-1058620?r=sp&s=26ppMzJ4SaadG3iCrhg9HA)
N-MOS|10|0.7|7|[Octopart](https://octopart.com/irf510pbf-vishay-46532502?r=sp&s=Ft8oBWb7Tw--a\_FmfUap9w#)
Limit switches|2|1.81|3.62|[Octopart](https://octopart.com/db1c-b1lb-zf-71118816?r=sp&s=sDTxIuqxSJOc-Uf9S2HFEg)
Wooden chest|1|18|18|[Obi](https://www.obi.de/aufbewahrungsboxen/laublust-holzkiste-deckel-und-griffe-natur-kiefer-40-cm-x-30-cm-x-24-cm-fsc-/p/4480497?wt_mc=gs.pla_css.Wohnen.Haushalt.AufbewahrungOrdnung&wt_cc1=2032213149&wt_cc4=c&wt_cc9=72234010499&gclid=Cj0KCQiAtOjyBRC0ARIsAIpJyGOc110gyAUM7jzfQY2w4vCwtppWHI9Yl8gK_ffBL1Zt-crWl_SSy10aAtSsEALw_wcB)
Total: 109.28 €

## Group

Markus Schwörer, Florian Bregler, Frank Mutter

## Protocol to control leds and piezo via mqtt

standart JSON message on topic 5/safe/control with data = "x:y". x is color and y is the mode.


Colors (x):
* LED_COLOR_WHITE 0
* LED_COLOR_RED 1
* LED_COLOR_GREEN 2
* LED_COLOR_BLUE  3
* LED_COLOR_ORANGE 4

Mode (y):
* LED_MODE_ON 0
* LED_MODE_OFF 1
* LED_MODE_PULSE 2
* LED_MODE_BLINK 3
* LED_MODE_PIEZO 9

For LED_MODE_PIEZO the color parameter is the time of the buzzer sound in seconds.

examples:
* "{\"method\": \"trigger\", \"state\": \"on\", \"data\": \"3:0\" }"
=> Turns blue LEDs on.
* "{\"method\": \"trigger\", \"state\": \"on\", \"data\": \"4:3\" }"
=> LEDs orange blinking.
* "{\"method\": \"trigger\", \"state\": \"on\", \"data\": \"5:9\" }"
=> Buzzer sound for 5 seconds (max value: 9)

## Schematics

### Safe Activate
![Safe Activate Schematic](Pics/SchematicActivate.png)
**Image 1**: Schematic of the activation.

### Safe Control
![Safe Control Schematic](Pics/SchematicControl.png)
**Image 2**: Schematic of the safe.

## Flow Charts

### Safe Activate

![Safe Activate Flow Chart](Pics/SafeActivateFlowChart.png)
**Image 3**: Flow Chart of the safe activation

### Safe Control

![Safe Control State Machine](Pics/safe_control_sm.png)
**Image 4**: State machine of the safe control with the main states

## Instructions for the Escape Room Operator

### Room Reset:
* Close the safe 
* Pull out the resistors and place them in front of the puzzle
* Put the safe datasheet (back) on the table

# Pictures

![Safe and activation picture](Pics/safe_and_activation.jpeg)
**Image 5**: Picture of the safe (left) and the activatioin puzzle (right).

![Safe with floppy discs](Pics/safe_with_floppy_discs.jpeg)
**Image 6**: Picture of the Safe with green LED color and floppy discs inside.

![lock with switch](Pics/lock.jpeg)
**Image 7**: Picture of the lock with switch to detect whether the safe is opened or closed.
