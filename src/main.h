/*
avr industrial weight checker t02 - v1.0

copyright (c) Davide Gironi, 2021

Released under GPLv3.
Please refer to LICENSE file for licensing information.
*/

#ifndef MAIN_H
#define MAIN_H

#include <avr/io.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>

//include key lib
#include "key/key.h"

//include lcd lib
#include "lcd/lcd.h"

//include hx711 lib
#include "hx711/hx711.h"

//define buttons
#define BUTTON_UP KEY_BUTTON1
#define BUTTON_DOWN KEY_BUTTON2
#define BUTTON_SELECT KEY_BUTTON3

//programming status
#define PROGSTATUS_GETWEIGHTINTERVAL 0
#define PROGSTATUS_GETWEIGHTTHRESHOLDERR 1
#define PROGSTATUS_GETWEIGHTTHRESHOLDMIN 2
#define PROGSTATUS_GETWEIGHTTHRESHOLDMAX 3
#define PROGSTATUS_TARE 4
#define PROGSTATUSTOT 5

//calibration status
#define CALSTATUS_GAIN 0
#define CALSTATUS_OFFSET 1
#define CALSTATUS_WEIGHT 2
#define CALSTATUS_SCALE 3
#define CALSTATUSTOT 4

//alarm relay
#define RELALERT_DDR DDRB
#define RELALERT_PORT PORTB
#define RELALERT_PINNUM PB2
#define RELALERT_OFF RELALERT_PORT |= (1<<RELALERT_PINNUM)
#define RELALERT_ON RELALERT_PORT &= ~(1<<RELALERT_PINNUM)

//max and min weight interval
#define GETWEIGHT_INTERVAL_MIN 1
#define GETWEIGHT_INTERVAL_MAX 60

//max and min weight threshold errors
#define GETWEIGHT_THRESHOLDERR_MIN 1
#define GETWEIGHT_THRESHOLDERR_MAX 50

//max and min weight threshold min error
#define GETWEIGHT_THRESHOLDMIN_MIN -50000
#define GETWEIGHT_THRESHOLDMIN_MAX 0

//max and min weight threshold max error
#define GETWEIGHT_THRESHOLDMAX_MIN 0
#define GETWEIGHT_THRESHOLDMAX_MAX 50000

//max and min calibration weight
#define WEIGHTCAL_WEIGHT_MIN 1
#define WEIGHTCAL_WEIGHT_MAX 100

//default weight interval in sec
#define GETWEIGHT_INTERVAL_DEFAULT 1

//default weight threshold for errors in sec
#define GETWEIGHT_THRESHOLDERR_DEFAULT 5

//default weight threshold for min weight
#define GETWEIGHT_THRESHOLDMIN_DEFAULT -1000

//default weight threshold for max weight
#define GETWEIGHT_THRESHOLDMAX_DEFAULT 5000

//default weight for calibration
#define WEIGHTCAL_WEIGHT_DEFAULT 10

//default calibration offset
#define WEIGHTCAL_OFFSET_DEFAULT 8000000

//default calibration scale
#define WEIGHTCAL_SCALE_DEFAULT 1000

//default calibration gain
#define WEIGHTCAL_GAIN_DEFAULT HX711_GAINDEFAULT


//main timer setting
//freq = FCPU / (prescale * (256 - preload))
//preload = 256 - FCPU/(prescaler * freqdesired)
//freq = FCPU / (prescale * 256)
//  es. 488.28 = 8000000 / ( 64 * 256)
#define MAINTIMER_PRESCALER (1<<CS01) | (1<<CS00)
//step to count 10 ms
#define MAINTIMER_10MSSTEP 5
//step to count 1000 ms
#define MAINTIMER_1000MSSTEP 488
//timer interrupt
#define MAINTIMER_INTERRUPT ISR(TIMER0_OVF_vect) 
//timer init
#define MAINTIMER_INIT \
    TCCR0 |= MAINTIMER_PRESCALER; \
	TIMSK |= 1<<TOIE0;

#endif
