#include <TimeLib.h>
#include "ui.h"
#include "sim.h"
#include "debug.h"

#define ON 1
#define OFF 0



void setup() {
  initScreen();
  showSplash();

  initFona();
  //showSimSignal(getRSSI());

  syncTimeNTP ();
}

void loop() {

  turnDisplay(ON);

  showBatteryStatus(0);
  showSimSignal(getSimRSSI());
  //  getRSSI();

  

  //char timebuf[23];
  //getTime(timebuf, sizeof(timebuf));
  //parseAndSetTime(timebuf);

 
  showTime();

  /*

showToday (11);
 show4Weeks (134);
 showAllCount (17821);


  DateCount data[36] = {
        { "01.05.25", 10 },
        { "02.05.25", 20 },
        { "03.05.25", 30 },
        { "04.05.25", 40 },
        { "05.05.25", 50 },
        { "06.05.25", 60 },
        { "07.05.25", 70 },
        { "08.05.25", 80 },
        { "09.05.25", 90 },
        { "10.05.25", 100 },
        { "11.05.25", 110 },
        { "12.05.25", 120 },
        { "01.05.25", 10 },
        { "13.05.25", 20 },
        { "14.05.25", 30 },
        { "15.05.25", 40 },
        { "16.05.25", 50 },
        { "17.05.25", 60 },
        { "18.05.25", 70 },
        { "19.05.25", 80 },
        { "20.05.25", 90 },
        { "21.05.25", 100 },
        { "22.05.25", 110 },
        { "23.05.25", 120 },
        { "24.05.25", 10 },
        { "25.05.25", 20 },
        { "26.05.25", 30 },
        { "27.05.25", 40 },
        { "28.05.25", 50 },
        { "29.05.25", 60 },
        { "30.05.25", 70 },
        { "31.05.25", 80 },
        { "01.06.25", 90 },
        { "02.06.25", 100 },
        { "03.06.25", 110 },
        { "04.06.25", 120 }
    };

  showCountPerDate(data);
  showLastTransmission("Letzte Uebertragung: 25.05.2025 14:39:22,123");
  showStatus(" Status okay\n Lichtschranke okay", 0);

  

  showBatteryStatus(35);
  // delay(3010);
 showStatus(" Status Fehler\n Kein Netz", 2);
  showBatteryStatus(65);
  
 // delay(3010);

 showToday (1);
 show4Weeks (133);
 showAllCount (17811);
  showLastTransmission("Letzte Uebertragung: 18.08.2022 16:39:42,173"); 
  showStatus(" Status Fehler\n Lichtschranke blockiert", 1);
 showBatteryStatus(100);
  */
  delay(1010);
}
