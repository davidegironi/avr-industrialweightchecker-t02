/*
avr industrial weight checker t02 - v1.0

copyright (c) Davide Gironi, 2021

Released under GPLv3.
Please refer to LICENSE file for licensing information.
*/

#include "main.h"

//button enabled
volatile uint8_t key_enabled = 0;

//machine state
enum state {running, programming, calibration};
enum state currentstate = running; //default state

//onesec trigger
volatile uint8_t onesectrigger = 0;

//get weight trigger
volatile uint8_t getweighttrigger = 0;

//current errors state
volatile uint8_t error_state = 0;

//alert enabler
volatile uint8_t alert_enabled = 0;

//define the eeprom structure
typedef struct {
	uint8_t initeeprom;
	uint8_t getweight_interval;
	uint8_t getweight_thresholderr;
	int32_t getweight_thresholdmax;
	int32_t getweight_thresholdmin;
	uint16_t weightcal_weight;
	int32_t weightcal_offset;
	uint8_t weightcal_gain;
	double weightcal_scale;
} eepromitem_eet;
eepromitem_eet EEMEM  eepromitem_eemem;
eepromitem_eet  eepromitem_eevar;


/*
 * init indwgtcheck eeprom
 */
void eepromitem_eeprominit() {
	eepromitem_eevar.initeeprom = 1;
	eepromitem_eevar.getweight_interval = GETWEIGHT_INTERVAL_DEFAULT;
	eepromitem_eevar.getweight_thresholderr = GETWEIGHT_THRESHOLDERR_DEFAULT;
	eepromitem_eevar.getweight_thresholdmax = GETWEIGHT_THRESHOLDMAX_DEFAULT;
	eepromitem_eevar.getweight_thresholdmin = GETWEIGHT_THRESHOLDMIN_DEFAULT;
	eepromitem_eevar.weightcal_weight = WEIGHTCAL_WEIGHT_DEFAULT;
	eepromitem_eevar.weightcal_offset = WEIGHTCAL_OFFSET_DEFAULT;
	eepromitem_eevar.weightcal_gain = WEIGHTCAL_GAIN_DEFAULT;
	eepromitem_eevar.weightcal_scale = WEIGHTCAL_SCALE_DEFAULT;
	eeprom_write_block((const void*)&eepromitem_eevar, (void*)&eepromitem_eemem, sizeof(eepromitem_eet));
}


/*
 * read indwgtcheck eeprom
 */
void eepromitem_eepromread() {
	eeprom_read_block((void*)&eepromitem_eevar, (const void*)&eepromitem_eemem, sizeof(eepromitem_eet));
}


/*
 * write indwgtcheck eeprom
 */
void eepromitem_eepromwrite() {
	eeprom_write_block((const void*)&eepromitem_eevar, (void*)&eepromitem_eemem, sizeof(eepromitem_eet));
}


/**
 * main timer interrupt
 */
MAINTIMER_INTERRUPT {
	static uint16_t key_10msstepcounter = 0;
	static uint16_t counter_1000msstepcounter = 0;

	if(key_enabled) {
		key_10msstepcounter++;
		if(key_10msstepcounter == MAINTIMER_10MSSTEP) {
			//we are here ~almost~ 10ms seconds
			key_10msstepcounter = 0;
			//key interrupt
			key_timerinterrupt();
		}
	}

	counter_1000msstepcounter++;
	if(counter_1000msstepcounter == MAINTIMER_1000MSSTEP) {
		//we must be there every 1s second
		counter_1000msstepcounter = 0;

		//trigger a get weight event
		getweighttrigger = 1;

		//one seconds trigger
		onesectrigger = 1;
	}
}

/*
 * print a number
 */
void lcd_writelong(int32_t n) {
	//print number
	char tnum[11];
	ltoa(n, tnum, 10);
	lcd_puts(tnum);
}

/*
 * print a number
 */
void lcd_writedouble(double n, uint8_t width, uint8_t prec) {
	//print number
	char tnum[width + prec + 2];
	dtostrf(n, width, prec, tnum);
	lcd_puts(tnum);
}

/*
 * fast set a number
 */
int32_t set_plusminus(int32_t n, int32_t max, int32_t min) {
	int32_t ret = n;
	static uint16_t buttons_rptUP = 0;
	static uint16_t buttons_rptDOWN = 0;

	if(key_getpress(1<<BUTTON_UP) && n < max) {
		buttons_rptUP = 0;
		ret += 1;
	}
	if(key_getpress(1<<BUTTON_DOWN) && n > min) {
		buttons_rptDOWN = 0;
		ret -= 1;
	}

	if(key_getrpt(1<<BUTTON_UP)) {
		if(buttons_rptUP < 100) {
			buttons_rptUP++;
			if(n < max)
				ret += 1;
			else
				buttons_rptUP = 0;
		} else if(buttons_rptUP < 1000) {
			buttons_rptUP+=10;
			if(n+(100-1) < max)
				ret += 100;
			else
				buttons_rptUP = 0;
		} else {
			if(n+(1000-1) < max)
				ret += 1000;
			else
				buttons_rptUP = 100;
		}
	}

	if(key_getrpt(1<<BUTTON_DOWN)) {
		if(buttons_rptDOWN < 100) {
			buttons_rptDOWN++;
			if(n > min)
				ret -= 1;
			else
				buttons_rptDOWN = 0;
		} else if(buttons_rptDOWN < 1000) {
			buttons_rptDOWN+=10;
			if(n-(100) > min)
				ret -= 100;
			else
				buttons_rptDOWN = 0;
		} else {
			if(n-(1000) > min)
				ret -= 1000;
			else
				buttons_rptDOWN = 100;
		}
	}

	return ret;
}


/*
 * main loop
 */
int main(void) {    
	//watchdog disable
	wdt_disable();

	//init keypad
	key_init();
	key_enabled = 1;

	//set relay alert
	RELALERT_DDR |= (1<<RELALERT_PINNUM); //output
	RELALERT_OFF;

	//init main timer
	MAINTIMER_INIT

	//init interrupts
    sei();

    //init lcd
    lcd_init(LCD_DISP_ON);
    uint8_t refreshlcd = 1;

    //lcd go home
    lcd_home();

    //print welcome message
    lcd_gotoxy(0, 0);
    lcd_puts_p(PSTR("Ind. Wgt. Check "));
    lcd_gotoxy(0, 1);
	lcd_puts_p(PSTR("      t02 - v1.0"));
    _delay_ms(1000);
    lcd_clrscr();
    lcd_gotoxy(0, 0);
    lcd_puts_p(PSTR("                "));
    lcd_gotoxy(0, 1);
	lcd_puts_p(PSTR("        D.Gironi"));
    _delay_ms(1000);

	//init eeprom
    eepromitem_eepromread();
	if(((int)eepromitem_eevar.initeeprom & 0XFF) == 0xFF) { //init values
		eepromitem_eeprominit();
	}
	
	//init hx711
	hx711_init(eepromitem_eevar.weightcal_gain, eepromitem_eevar.weightcal_scale, eepromitem_eevar.weightcal_offset);

	//set default status
	currentstate = running;
	lcd_clrscr();

    //program status
	uint8_t programming_status = PROGSTATUS_GETWEIGHTINTERVAL;

	//calibration status
	uint8_t calibration_status = CALSTATUS_GAIN;

	//check weight calibration
	if(key_getpress(1<<BUTTON_SELECT)) {
		currentstate = calibration;
	}

	//current weight
	double weight_current = 0;

	//get weight counter
	uint8_t getweight_counter = 0;

	//weight errors
	uint8_t weight_errors = 0;

	//current weight diff error
	int8_t weight_differr = 0;

	//underline selector
	uint8_t underlineselector = 0;

	//watchdog enable
	wdt_enable(WDTO_1S);
	
	//main loop
    for(;;) {
    	//watchdog reset
    	wdt_reset();	
		
    	//running
    	if(currentstate == running) {
			//refresh lcd at leat every second
			if(onesectrigger) {
				onesectrigger  = 0;
				refreshlcd = 1;

				underlineselector++;
				underlineselector %= 2;
			}
			
			//enable or disabled alert
			if(!error_state && key_getlong(1<<BUTTON_DOWN)) {
				alert_enabled = !alert_enabled;
			}

			//check errors
			if(error_state) {
				//reset error
				if(key_getlong(1<<BUTTON_DOWN)) {
					//reset alert enabled
					alert_enabled = 0;

					//reset errors
					weight_differr = 0;
					weight_errors = 0;
					error_state = 0;

					//reset alert
					RELALERT_OFF;

					//refresh lcd
					refreshlcd = 1;
				}
			} else {
				//set the tare
				if(!alert_enabled && key_getlong(1<<BUTTON_UP)) {
					hx711_taretozero();
					eepromitem_eevar.weightcal_offset = hx711_getoffset();
					eepromitem_eepromwrite();
				}

				//get weight and check diff
				if(getweighttrigger) {
					getweighttrigger = 0;

					getweight_counter++;
					if(getweight_counter >= eepromitem_eevar.getweight_interval) {
						getweight_counter = 0;

						//get weight
						weight_current = hx711_getweight();
						
						//check weight diff
						if(alert_enabled) {
							if(eepromitem_eevar.getweight_thresholdmin != 0 && weight_current < (double)eepromitem_eevar.getweight_thresholdmin/1000.0)
								weight_differr = -1;
							else if(eepromitem_eevar.getweight_thresholdmax != 0 && weight_current > (double)eepromitem_eevar.getweight_thresholdmax/1000.0)
								weight_differr = 1;
							else
								weight_differr = 0;
							if(weight_differr != 0) {
								weight_errors++;
							} else {
								//reset errors
								weight_errors = 0;
							}
						}
					}				

					//refresh lcd
					refreshlcd = 1;
				}

				//check error threshold
				if(weight_errors >= eepromitem_eevar.getweight_thresholderr) {
					error_state = 1;

					//set alert
					RELALERT_ON; 
				}
			}

			//print out to lcd
			if(refreshlcd) {
				refreshlcd = 0;

				lcd_clrscr();

				if(error_state) {
					//write alert on
					lcd_gotoxy(0, 0);
					lcd_puts_p(PSTR("Alert!"));
				} else {
					//write current weight
					lcd_gotoxy(0, 0);
					if(underlineselector) {
						lcd_gotoxy(0, 0);
						lcd_puts_p(PSTR("_"));
					}
					lcd_gotoxy(1, 0);
					lcd_writedouble(eepromitem_eevar.getweight_interval - getweight_counter, 2, 0);
					lcd_gotoxy(6, 0);
					lcd_writedouble(weight_current, 10, 2);

					if(alert_enabled) {
						//write current weight difference and errors
						lcd_gotoxy(0, 1);
						lcd_puts_p(PSTR("e"));
						lcd_gotoxy(1, 1);
						lcd_writedouble(weight_errors, 2, 0);
						lcd_gotoxy(14, 1);
						if(weight_differr > 0)
							lcd_puts_p(PSTR(" >"));
						else if(weight_differr < 0)
							lcd_puts_p(PSTR(" <"));
						else
							lcd_puts_p(PSTR("Ok"));
					}
				}
			}
			
			//check change status
			if(key_getlong(1<<BUTTON_SELECT)) {
				//reset alert enabled
				alert_enabled = 0;

				//reset errors
				weight_differr = 0;
				weight_errors = 0;
				error_state = 0;

				//reset alert
				RELALERT_OFF;

				currentstate = programming;
				lcd_clrscr();

				//refresh lcd
				refreshlcd = 1;
			}
		}

		//calibration
    	else if(currentstate == calibration) {

			if(calibration_status == CALSTATUS_GAIN) {
				//calibration gain
				lcd_gotoxy(0, 0);
				lcd_puts_p(PSTR("Cal. Gain 1/4"));

				lcd_gotoxy(0, 1);
				if(eepromitem_eevar.weightcal_gain == HX711_GAINCHANNELA128)
					lcd_puts_p(PSTR("128"));
				else if(eepromitem_eevar.weightcal_gain == HX711_GAINCHANNELA64)
					lcd_puts_p(PSTR(" 64"));

				if(key_getshort(1<<BUTTON_UP)) {
					eepromitem_eevar.weightcal_gain = HX711_GAINCHANNELA128;
					hx711_setgain(HX711_GAINCHANNELA128);
				}
				if(key_getshort(1<<BUTTON_DOWN)) {
					eepromitem_eevar.weightcal_gain = HX711_GAINCHANNELA64;
					hx711_setgain(HX711_GAINCHANNELA64);
				}
			} else if(calibration_status == CALSTATUS_OFFSET) {
				//calibration offset
				lcd_gotoxy(0, 0);
				lcd_puts_p(PSTR("Cal. Offset 2/4"));

				lcd_gotoxy(0, 1);
				lcd_writelong(eepromitem_eevar.weightcal_offset);

				if(key_getlong(1<<BUTTON_UP)) {
					hx711_calibrate1setoffset();
					eepromitem_eevar.weightcal_offset = hx711_getoffset();
					lcd_clrscr();
				}
				
			} else if(calibration_status == CALSTATUS_WEIGHT) {
				//calibration weight
				lcd_gotoxy(0, 0);
				lcd_puts_p(PSTR("Cal. Weight 3/4"));

				lcd_gotoxy(0, 1);
				lcd_writelong(eepromitem_eevar.weightcal_weight);

				uint16_t weightcal_weight = eepromitem_eevar.weightcal_weight;				
				eepromitem_eevar.weightcal_weight = set_plusminus((uint16_t)eepromitem_eevar.weightcal_weight, WEIGHTCAL_WEIGHT_MAX, WEIGHTCAL_WEIGHT_MIN);
				if(eepromitem_eevar.weightcal_weight != weightcal_weight)
					lcd_clrscr();
			} else if(calibration_status == CALSTATUS_SCALE) {
				//calibration scale
				lcd_gotoxy(0, 0);
				lcd_puts_p(PSTR("Cal. Scale 4/4"));

				lcd_gotoxy(0, 1);
				lcd_writelong((int32_t)eepromitem_eevar.weightcal_scale);

				if(key_getlong(1<<BUTTON_UP)) {
					hx711_calibrate2setscale(((double)eepromitem_eevar.weightcal_weight));
					eepromitem_eevar.weightcal_scale = hx711_getscale();
					lcd_clrscr();
				}
			}

			//check change status
    		if(key_getshort(1<<BUTTON_SELECT)) {
    			calibration_status++;
    			calibration_status %= CALSTATUSTOT;

    			lcd_clrscr();
			}
			
			//check change status
			if(key_getlong(1<<BUTTON_SELECT)) {
				//reset alert enabled
				alert_enabled = 0;

				//reset errors
				weight_differr = 0;
				weight_errors = 0;
				error_state = 0;

				currentstate = running;
				lcd_clrscr();

				//refresh lcd
				refreshlcd = 1;

				eepromitem_eepromwrite();
			}
		}

    	//programming
    	else if(currentstate == programming) {

			if(programming_status == PROGSTATUS_GETWEIGHTINTERVAL) {
				//motor direction
				lcd_gotoxy(0, 0);
				lcd_puts_p(PSTR("Interval"));

				lcd_gotoxy(0, 1);
				lcd_writelong(eepromitem_eevar.getweight_interval);

				uint8_t getweight_interval = eepromitem_eevar.getweight_interval;
				eepromitem_eevar.getweight_interval = set_plusminus(eepromitem_eevar.getweight_interval, GETWEIGHT_INTERVAL_MAX, GETWEIGHT_INTERVAL_MIN);
				if(getweight_interval != eepromitem_eevar.getweight_interval)
					lcd_clrscr();
			} else if(programming_status == PROGSTATUS_GETWEIGHTTHRESHOLDERR) {
				//motor direction
				lcd_gotoxy(0, 0);
				lcd_puts_p(PSTR("Num Errors"));

				lcd_gotoxy(0, 1);
				lcd_writelong(eepromitem_eevar.getweight_thresholderr);

				uint8_t getweight_thresholderr = eepromitem_eevar.getweight_thresholderr;
				eepromitem_eevar.getweight_thresholderr = set_plusminus(eepromitem_eevar.getweight_thresholderr, GETWEIGHT_THRESHOLDERR_MAX, GETWEIGHT_THRESHOLDERR_MIN);
				if(getweight_thresholderr != eepromitem_eevar.getweight_thresholderr)
					lcd_clrscr();
			} else if(programming_status == PROGSTATUS_GETWEIGHTTHRESHOLDMIN) {
				//motor direction
				lcd_gotoxy(0, 0);
				lcd_puts_p(PSTR("Min W.Err x1000"));

				lcd_gotoxy(0, 1);
				lcd_writelong(eepromitem_eevar.getweight_thresholdmin);

				int32_t getweight_thresholdmin = eepromitem_eevar.getweight_thresholdmin;
				eepromitem_eevar.getweight_thresholdmin = set_plusminus(eepromitem_eevar.getweight_thresholdmin, GETWEIGHT_THRESHOLDMIN_MAX, GETWEIGHT_THRESHOLDMIN_MIN);
				if(getweight_thresholdmin != eepromitem_eevar.getweight_thresholdmin)
					lcd_clrscr();
			} else if(programming_status == PROGSTATUS_GETWEIGHTTHRESHOLDMAX) {
				//motor direction
				lcd_gotoxy(0, 0);
				lcd_puts_p(PSTR("Max W.Err x1000"));

				lcd_gotoxy(0, 1);
				lcd_writelong(eepromitem_eevar.getweight_thresholdmax);

				int32_t getweight_thresholdmax = eepromitem_eevar.getweight_thresholdmax;
				eepromitem_eevar.getweight_thresholdmax = set_plusminus(eepromitem_eevar.getweight_thresholdmax, GETWEIGHT_THRESHOLDMAX_MAX, GETWEIGHT_THRESHOLDMAX_MIN);
				if(getweight_thresholdmax != eepromitem_eevar.getweight_thresholdmax)
					lcd_clrscr();
			} else if(programming_status == PROGSTATUS_TARE) {
				//motor max speed
				lcd_gotoxy(0, 0);
				lcd_puts_p(PSTR("Tare"));

				lcd_gotoxy(0, 1);
				lcd_writelong(eepromitem_eevar.weightcal_offset);

				if(key_getlong(1<<BUTTON_UP)) {
					hx711_taretozero();
					eepromitem_eevar.weightcal_offset = hx711_getoffset();
					lcd_clrscr();
				}					
			}

			//check change status
			if(key_getlong(1<<BUTTON_SELECT)) {
				//reset alert enabled
				alert_enabled = 0;

				//reset errors
				weight_differr = 0;
				weight_errors = 0;
				error_state = 0;

				//reset alert
				RELALERT_OFF;

				currentstate = running;
				lcd_clrscr();

				//refresh lcd
				refreshlcd = 1;

				eepromitem_eepromwrite();
			}

			//check change status
    		if(key_getshort(1<<BUTTON_SELECT)) {
    			programming_status++;
    			programming_status %= PROGSTATUSTOT;

    			lcd_clrscr();
			}
		}

    }
}


