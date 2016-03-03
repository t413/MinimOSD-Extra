/*

Copyright (c) 2011.  All rights reserved.
An Open Source Arduino based OSD and Camera Control project.

Program  : ArduCAM-OSD (Supports the variant: minimOSD)
Version  : V2.1, 24 September 2012
Author(s): Sandro Benigno
Coauthor(s):
Jani Hirvinen   (All the EEPROM routines)
Michael Oborne  (OSD Configutator)
Mike Smith      (BetterStream and Fast Serial libraries)
Gábor Zoltán
Pedro Santos
Special Contribuitor:
Andrew Tridgell by all the support on MAVLink
Doug Weibel by his great orientation since the start of this project
Contributors: James Goppert, Max Levine, Burt Green, Eddie Furey
and all other members of DIY Drones Dev team
Thanks to: Chris Anderson, Jordi Munoz


This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>

*/

/* ************************************************************ */
/* **************** MAIN PROGRAM - MODULES ******************** */
/* ************************************************************ */

#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))

#undef PSTR
#define PSTR(s) (__extension__({static prog_char __c[] PROGMEM = (s); &__c[0];}))

#define isPAL 1

/* **********************************************/
/* ***************** INCLUDES *******************/

//#define membug
//#define FORCEINIT  // You should never use this unless you know what you are doing


// AVR Includes
#include <AP_Common.h>
#include <AP_Math.h>
#include <math.h>
#include <inttypes.h>
#include <avr/pgmspace.h>
// Get the common arduino functions
#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "wiring.h"
#endif
#include <EEPROM.h>
//#include <SimpleTimer.h>
#include <GCS_MAVLink.h>

#ifdef membug
#include <MemoryFree.h>
#endif

// Configurations
#include "OSD_Config.h"
#include "ArduCam_Max7456.h"
#include "OSD_Vars.h"
#include "OSD_Func.h"

#include "FrSky.h"
#include "SimpleFIFO.h"

SoftwareSerial frSkySerial(6, 5, true); //true for inverted logic!
FrSky frSky;

/* *************************************************/
/* ***************** DEFINITIONS *******************/

//OSD Hardware
#define MinimOSD
#define TELEMETRY_SPEED  57600  // How fast our MAVLink telemetry is coming to Serial port
#define BOOTTIME         2000   // Time in milliseconds that we show boot loading bar and wait user input

OSD osd; //OSD object


void setup() {
    pinMode(MAX7456_SELECT,  OUTPUT); // OSD CS

    Serial.begin(TELEMETRY_SPEED);
    // setup mavlink port
    mavlink_comm_0_port = &Serial;
    frSkySerial.begin(9600);

#ifdef membug
    Serial.println(freeMem());
#endif

    // Prepare OSD for displaying
    unplugSlaves();
    osd.init();

    // Start
    startPanels();
    delay(500);

    // OSD debug for development (Shown at start)
#ifdef membug
    osd.setPanel(1,1);
    osd.openPanel();
    osd.write(freeMem());
    osd.closePanel();
#endif

    // Just to easy up development things
#ifdef FORCEINIT
    InitializeOSD();
#endif

    // Get correct panel settings from EEPROM
    readSettings();
    for(panel = 0; panel < npanels; panel++) readPanelSettings();
    panel = 0; //set panel to 0 to start in the first navigation screen

    delay(2000);
    Serial.flush();
}


static unsigned long frskyTimer = 0;
int frskyCounter = 0;

void loop() {
    //frsky timer
    if(millis() > frskyTimer + 200){
      mavLinkTimer = millis();
      sendFrSkyData();
    }

    //mavlink timer
    if(millis() > mavLinkTimer + 120){
      mavLinkTimer = millis();
      OnMavlinkTimer();
    }

    read_mavlink();
}

/* *********************************************** */
/* ******** functions used in main loop() ******** */
void OnMavlinkTimer() {
    setHeadingPatern();  // generate the heading patern
    //  osd_battery_pic_A = setBatteryPic(osd_battery_remaining_A);     // battery A remmaning picture
    //osd_battery_pic_B = setBatteryPic(osd_battery_remaining_B);     // battery B remmaning picture
    setHomeVars(osd);   // calculate and set Distance from home and Direction to home
    writePanels();       // writing enabled panels (check OSD_Panels Tab)
    setFdataVars();
    checkModellType();
}

void unplugSlaves(){
    //Unplug list of SPI
    digitalWrite(MAX7456_SELECT,  HIGH); // unplug OSD
}

void sendFrSkyData() {
    frskyCounter++;
    if (frskyCounter >= 25) { // Send 5000 ms frame
        frSky.sendFrSky05Hz(frSkySerial, mavDataReader);
        frskyCounter = 0;
    }
    else if ((frskyCounter % 5) == 0) { // Send 1000 ms frame
        frSky.sendFrSky1Hz(frSkySerial, mavDataReader);
    } else { // Send 200 ms frame
        frSky.sendFrSky5Hz(frSkySerial, mavDataReader);
    }
}
