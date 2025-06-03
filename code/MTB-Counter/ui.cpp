#include "ui.h"
#include "colors.h"
#include <TimeLib.h>

#define TFT_CS 8
#define TFT_DC 6
#define TFT_RST 7
#define POWER_DISPLAY_PIN 9

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

String getDateTimeString() {
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          year(), month(), day(),
          hour(), minute(), second());
  return String(buffer);  
}

void turnDisplay(bool status) {
  digitalWrite(POWER_DISPLAY_PIN, status);
}

void initScreen() {
  pinMode(POWER_DISPLAY_PIN, OUTPUT);
  turnDisplay(HIGH);
  tft.init(SCREEN_HEIGHT, SCREEN_WIDTH);
  tft.setRotation(3);
  tft.invertDisplay(0);
}

void showSplash() {

  tft.fillScreen(TFT_DARKGREEN2);
  tft.setTextColor(TFT_BLUEVIOLET);
  tft.setCursor(20, 30); 
  tft.setTextSize(7);
  tft.println("MTB");
  tft.setTextSize(3);
  tft.setCursor(20, 100);
  tft.println("Fahrtenzaehler");
  tft.setTextSize(2);
  tft.setCursor(220, 140);
  tft.println("V 3.0");
  delay(500);
  tft.fillScreen(TFT_BLACK);
}

/*
 * Show current battery percentage on screen
 */
int lastBatteryLevel = 0;
void showBatteryStatus (int level){

  if (level == lastBatteryLevel) return;  // no Update needed.
  lastBatteryLevel = level;

  tft.fillRect(268, 0, 60, 16, TFT_BLACK);

  int color = 0;
  if (level > 80) {
    color = TFT_GREEN;
  } else if (level <= 80 && level >= 25) {
    color = TFT_YELLOW;
  } else {
    color = TFT_RED;
  }

  tft.drawRect(296, 2, 20, 10, color);
  tft.fillRect(296, 2, level / 5, 10, color);
  tft.drawRect(316, 5, 2, 4, color);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  if (level == 100) {
    tft.setCursor(270, 3);
  } else {
    tft.setCursor(276, 3);
  }

  tft.print(level);
  tft.print("%");
}

int lastSimLevel = -1;
void showSimSignal(int level){

 if (level == lastSimLevel) return;  // no Update needed.
  lastSimLevel = level;

  tft.fillRect(189, 0, 46, 14, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1); 
  tft.setCursor(189, 6);
  tft.print(level);
  tft.print("dBm"); 

  tft.fillRect(234,0 ,30,16, TFT_BLACK);
  tft.drawRect(234, 8, 4, 6, TFT_WHITE);
  tft.drawRect(240, 6, 4, 8, TFT_WHITE);
  tft.drawRect(246, 4, 4, 10, TFT_WHITE);
  tft.drawRect(252, 2, 4, 12, TFT_WHITE);
  tft.drawRect(258, 0, 4, 14, TFT_WHITE);

  int color = TFT_RED;
  if (level > -100) color = TFT_YELLOW;
  if (level > -79) color = TFT_GREEN;
    
  if (level > -109) tft.fillRect(235, 9, 2, 4, color);
  if (level > -100) tft.fillRect(241, 7, 2, 6, color);
  if (level > -90) tft.fillRect(247, 5, 2, 8, color);
  if (level > -80) tft.fillRect(253, 3, 2, 10, color);
  if (level > -71) tft.fillRect(259, 1, 2, 12, color);
}


void showTime (){
  tft.fillRect(0,0,140,16, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1); 
  tft.setCursor(1, 3);
  tft.print(getDateTimeString());
}


void showCountPerDate(const DateCount (&data)[36]) {
  tft.fillRect(8,54,302,148 , TFT_BLACK);
  tft.drawRect(8,54,302,148 , TFT_ORANGE);
  tft.setTextSize(1);
  for (int column = 0; column < 3; column++) {      
     for (int i = 0; i < 12; i++) {
        tft.setCursor( column * 100 + 20, 58 + i * 12);
        char buf[9];
        memcpy(buf, data[i+column].date, 8);
        buf[8] = '\0';
        tft.setTextColor(TFT_WHITE);
        tft.print(buf);
        tft.print(" ");
        tft.setTextColor(TFT_CYAN);        
        tft.print(data[i+column].count);
      }
    }
}

long countAll = 0;
void showAllCount (long count){
  if (countAll == count) return;
  countAll = count;
   tft.fillRect(2,18,135,18, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(2, 18); 
  tft.setTextSize(2);
  tft.print("Alle: ");
  tft.setTextColor(TFT_CYAN); 
  tft.print(count);
 
}


int countToday = 0;
void showToday (int count){
  if (countToday == count) return;
  countToday = count;
  tft.fillRect(2,36,135,18, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(2, 36); 
  tft.setTextSize(2);
  tft.print("Heute:");
  tft.setTextColor(TFT_CYAN); 
  tft.print(count);
}

int countWeeks = 0;
void show4Weeks (int count){
  if (countWeeks == count) return;
  countWeeks = count;

  tft.fillRect(140,36,140,18, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(140, 36); 
  tft.setTextSize(2);  
  tft.print("7 Tage:");
  tft.setTextColor(TFT_CYAN); 
  tft.print(count);  

}



void showStatus (const char * status, short severity){
  int backcolor = TFT_BLACK;
  if (severity == 1) backcolor = TFT_ORANGE;   
  if (severity == 2) backcolor = TFT_RED;   
  
  tft.fillRect(0,206,340,18, backcolor);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 206); 
  tft.setTextSize(1);
  tft.print(status);
}

void showLastTransmission (const char * lastTransmission){
  tft.fillRect(0,228,340,18, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(2,228); 
  tft.setTextSize(1);
  tft.print(lastTransmission);
}
