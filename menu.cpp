/*
 * Menu for VFD Modular Clock
 * (C) 2012 William B Phelps
 *
 * This program is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 */

#include "global.h"
#include "global_vars.h"

#include <util/delay.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <string.h>
#include <stdlib.h>

#include <Wire.h>
#include <WireRtcLib.h>

#include "menu.h"
#include "display.h"
#include "pitches.h"

#include "adst.h"
#include "flw.h"

menu_state_t g_menu_state = STATE_CLOCK;

// workaround: Arduino avr-gcc toolchain is missing eeprom_update_byte
#define eeprom_update_byte eeprom_write_byte

void set_date(uint8_t yy, uint8_t mm, uint8_t dd);

extern WireRtcLib rtc;
extern WireRtcLib::tm* tt; // current local date and time as TimeElements (pointer)

#if defined HAVE_AUTO_DST
void setDSToffset(uint8_t mode) {
	int8_t adjOffset;
	uint8_t newOffset;
#ifdef HAVE_AUTO_DST
	if (mode == 2) {  // Auto DST
		if (g_DST_updated) return;  // already done it once today
		if (tt == NULL) return;  // safety check
		newOffset = getDSToffset(tt, g_DST_Rules);  // get current DST offset based on DST Rules
	}
	else
#endif // HAVE_AUTO_DST
	  newOffset = mode;  // 0 or 1

	adjOffset = newOffset - g_DST_offset;  // offset delta
	if (adjOffset == 0)  return;  // nothing to do

        if (adjOffset > 0)
            tone(PinMap::piezo, NOTE_A5, 100);  // spring ahead
        else
            tone(PinMap::piezo, NOTE_A4, 100);  // fall back

        tt = rtc.getTime();
        tt->year = y2kYearToTm(tt->year);
        unsigned long tNow = makeTime(tt);  // fetch current time from RTC as time_t
        tNow += adjOffset * SECS_PER_HOUR;  // add or subtract new DST offset
        breakTime(tNow, tt);
        tt->year = tmYearToY2k(tt->year);  // remove 1970 offset
        rtc.setTime(tt);  // adjust RTC
        g_DST_offset = newOffset;
        eeprom_update_byte(&b_DST_offset, g_DST_offset);
        g_DST_updated = true;
}
#endif

#ifdef HAVE_SET_DATE
void set_date(uint8_t yy, uint8_t mm, uint8_t dd) {
	tt = rtc.getTime();  // refresh current time 
        tt->year = yy;
        tt->mon = mm;
        tt->mday = dd;
	//rtc_set_time(tm_);
#ifdef HAVE_AUTO_DST
	DSTinit(tm_, g_DST_Rules);  // re-compute DST start, end for new date
	g_DST_updated = false;  // allow automatic DST adjustment again
	setDSToffset(g_DST_mode);  // set DSToffset based on new date
#endif
}
#endif

void menu(bool update, bool show)
{
    Serial.print("menu(");
    Serial.print(update);
    Serial.print(", ");
    Serial.print(show);
    Serial.println(")");
    
    switch (g_menu_state) {
      
    case STATE_MENU_BRIGHTNESS:
        if (update) {
            g_brightness++;

            if (g_brightness > 10) g_brightness = 1;
            set_brightness(g_brightness);

            eeprom_update_byte(&b_brightness, g_brightness);
        }

        show_setting_int("BRIT", "BRITE", g_brightness, show);
        break;
        
    case STATE_MENU_24H:
        if (update) {
            g_24h_clock = !g_24h_clock;
            eeprom_update_byte(&b_24h_clock, g_24h_clock);
        }

        show_setting_string("24H", "24H", g_24h_clock ? " on" : " off", show);
        break;

    case STATE_MENU_YEAR:
        if (update) {
            g_dateyear++;
            if (g_dateyear > 29) g_dateyear = 10;
            eeprom_update_byte(&b_dateyear, g_dateyear);
            set_date(g_dateyear, g_datemonth, g_dateday);
         }
         
         show_setting_int("YEAR", "YEAR", g_dateyear, show);
         break;

    case STATE_MENU_MONTH:
        if (update) {
            g_datemonth++;
            if (g_datemonth > 12) g_datemonth = 1;
            eeprom_update_byte(&b_datemonth, g_datemonth);
            set_date(g_dateyear, g_datemonth, g_dateday);
        }
        
        show_setting_int("MNTH", "MONTH", g_datemonth, show);
        break;
        
    case STATE_MENU_DAY:
      if (update) {
        g_dateday++;
        if (g_dateday > 31) g_dateday = 1;
        eeprom_update_byte(&b_dateday, g_dateday);
        set_date(g_dateyear, g_datemonth, g_dateday);
      }
      
      show_setting_int("DAY", "DAY", g_dateday, show);
      break;
      
    case STATE_MENU_AUTODATE:
      if (update) {
        g_AutoDate = !g_AutoDate;
        
        eeprom_update_byte(&b_AutoDate, g_AutoDate);
      }
    
      show_setting_string("ADTE", "ADATE", g_AutoDate ? " on" : " off", show);
      break;
      
    case STATE_MENU_REGION:
    Serial.println("Region setting");
    Serial.println(g_date_format);
      if (update) {
        if (g_date_format == FORMAT_YMD)
          g_date_format = FORMAT_MDY;
        else if (g_date_format == FORMAT_MDY)
          g_date_format = FORMAT_DMY;
        else if (g_date_format == FORMAT_DMY)
          g_date_format = FORMAT_YMD;
        else
          g_date_format = FORMAT_YMD;
        
        eeprom_update_byte(&b_date_format, g_date_format);
      }
      
          Serial.println(g_date_format);
    
      if (g_date_format == FORMAT_YMD)
        show_setting_string("REGN", "REGION", "YMD", show);
      else if (g_date_format == FORMAT_MDY)
        show_setting_string("REGN", "REGION", "MDY", show);
      else if (g_date_format == FORMAT_DMY)
        show_setting_string("REGN", "REGION", "DMY", show);
      else
        show_setting_string("REGN", "REGION", "YMD", show);

      break;

#ifdef HAVE_AUTO_DST
    case STATE_MENU_DST:
      if (update) {	
        g_DST_mode = (g_DST_mode+1)%3;  //  0: off, 1: on, 2: auto
        
        eeprom_update_byte(&b_DST_mode, g_DST_mode);
        
        g_DST_updated = false;  // allow automatic DST adjustment again
        //setDSToffset(g_DST_mode);
      }
      
      if (g_DST_mode == 0)
        show_setting_string("DST", "DST", "off", show);
      else
        show_setting_string("DST", "DST", g_DST_mode == 1 ? "on" : "auto", show);
      break;
#endif // HAVE_AUTO_DST 



	case STATE_MENU_TEMP:
            if (update) {
                g_show_temp = !g_show_temp;
                eeprom_update_byte(&b_show_temp, g_show_temp);
            }
            
            show_setting_string("TEMP", "TEMP", g_show_temp ? " on" : " off", show);
            break;
	case STATE_MENU_DOTS:
            if (update) {
                g_show_dots = !g_show_dots;
                //eeprom_update_byte(&b_show_dots, g_show_dots);
            }

            show_setting_string("DOTS", "DOTS", g_show_dots ? " on" : " off", show);
            break;
            
#ifdef HAVE_FLW
        case STATE_MENU_FLW:
            if (update) {
                g_flw_enabled++;
                if (g_flw_enabled > FLW_FULL) g_flw_enabled = FLW_OFF;
                eeprom_update_byte(&b_flw_enabled, g_flw_enabled);
            }
            
            if (g_flw_enabled == FLW_OFF)
                show_setting_string("FLW", "FLW", " off", show);
            else if (g_flw_enabled == FLW_ON)
                show_setting_string("FLW", "FLW", " on", show);
            else
                show_setting_string("FLW", "FLW", "full", show);
        
            break;
#endif // HAVE_FLW
        default:
            break; // do nothing
    }
}

void menu_enable(menu_state_t item, bool enabled)
{
    
}

void first_menu_item()
{
    g_menu_state = STATE_MENU_BRIGHTNESS;
}

void next_menu_item()
{
    g_menu_state = (menu_state_t)(g_menu_state + 1);
    
    // fixme: check enabled and disabled menu items here
    
    if (g_menu_state == STATE_MENU_LAST)
        g_menu_state = STATE_CLOCK;   
}

void menu_init(void)
{
  /*
  	g_menu_state = STATE_CLOCK;
	menu_enable(MENU_TEMP, rtc.isDS3231());  // show temperature setting only when running on a DS3231
	menu_enable(MENU_DOTS, g_has_dots);  // don't show dots settings for shields that have no dots
#ifdef HAVE_FLW
	menu_enable(MENU_FLW, g_has_flw);  // don't show FLW settings when there is no EEPROM with database
#endif
  */
}
