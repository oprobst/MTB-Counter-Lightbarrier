
#define ON 1
#define OFF 0

#define DEBUG_LED 7
#define LCD_ON 6
#define LCD_LED A2

#define PHOTODIODE_ON 0
#define PHOTODIODE A5

#define LIGHTBARRIER_ON 4
#define LIGHTBARRIER A3

#define BATTERY_VOLTAGE A4

#define DEBUG_BUTTON 3

#define CHECK_FOR_DAYLIGHT_EVERY 120000  //ms how often to check daylight

#define DURATION_DISPLAY_ON 300000  //ms how long the display should stay on
#define CYCLES_TO_WAIT 4           // multiplied with 8sec = time to wait between brightness checks to wake up from night sleep


#define LIMIT_NIGHT 14             // Measured limit to activate night mode
#define LIMIT_DAY 30               // Measured limit to activate day mode
#define MEASUREMENTS_LIMIT 5       //consecutive measurements below LIMIT_NIGHT required before power down.
#define HOURS_DAY_MIN 10           //minimum hours it must run every day before checking daylight

#include <LiquidCrystal.h>  
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <EEPROM.h>

/* Fuses:
 -U lfuse:w:0x62:m -U hfuse:w:0xD1:m -U efuse:w:0xFF:m -U lock:w:0x3F:m

  //EEPROM address > 12: each byte represents a single day. Memory entries have the following meaning:
  // 255 = invalid memory range
  // 254 = null / empty
  // 253 = maximum
  // 0-252 = amount of bikers crossed.

*/

LiquidCrystal lcd(8, 9, 10, 2, 5, A0, A1);

// indicates display on or off
bool displayOn = true;
// millis, when to stop display
long millisDisplayOff = 0;

//when to check for the next daylight
long nextMillisToCheckForDaylight = millis() + CHECK_FOR_DAYLIGHT_EVERY;

//number of brightness measurements below cutoff value
short countOfMeasurementsOfNight = 0;
//indicating night
bool isDay = true;
//counter for pwr down check on watchdog interrupt
short pwrdwnCycles = 0;
//duration of last barrier contact
long lastContactDuration = 0;

//The day of recording
long currentDayCycle = 1;

long todayStartedAtMillis = millis();

// EEPROM Location to store todays count.
int todaysEepromByteAddress = 0;

// millis when last display content was updated.
long lastDisplayUpdate = millis();

// when the last daylight recording page was switched
long lastDisplayPageSwitch = millis();

//count of contacts today
int todayCount = 0;
long overallCount;

//indicates if the Light barrier is durable blocked (longer than 2 sec).
bool permanentBlocked = false;

//pagination of stored daylight recordings
int page = 0;
// last measurements during past daycycles
short pageContent[4] = { 0, 0, 0, 0 };

//daymode of the last check: react only on changes
bool lastDayMode = false;

/*
 * Here we go
 */
void setup() {

  pinMode(DEBUG_LED, OUTPUT);
  pinMode(LCD_ON, OUTPUT);
  pinMode(LCD_LED, OUTPUT);
  pinMode(PHOTODIODE_ON, OUTPUT);
  pinMode(PHOTODIODE, INPUT);
  pinMode(LIGHTBARRIER_ON, OUTPUT);
  pinMode(LIGHTBARRIER, INPUT);
  pinMode(DEBUG_BUTTON, INPUT_PULLUP);
  //Pin6 is not arduino API compatible, need to set it manually:
  DDRB &= ~(1 << PB6);  //input
  PORTB |= (1 << PB6);  //pullup

  digitalWrite(LCD_ON, HIGH);
  digitalWrite(LCD_LED, HIGH);
  digitalWrite(PHOTODIODE_ON, HIGH);
  digitalWrite(LIGHTBARRIER_ON, HIGH);
  digitalWrite(DEBUG_LED, LOW);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);  //col, row
  lcd.print("MTB-Murgtal e.V.");
  lcd.setCursor(0, 1);  //col, row
  lcd.print("Fahrtenzaehler");

  for (short i = 0; i < 4; i++) {
    debugLED(ON);
    delay(250);
    debugLED(OFF);
    delay(250);
  }

  if (readDebugButton()) {
    resetMemory();
  }
  overallCount = readOverallCount();
  determineTodayStorageByte();
  storeTodayCount();
  watchdogSetup();
  activateDisplay(ON);
}


/*
 * Reset memory, if Debug button is pushed on startup for > 9 sec.
 */
void resetMemory() {
  for (short i = 9; i > 0; i--) {    
    lcd.clear();
    lcd.setCursor(0, 0);  //col, row
    lcd.print("Loesche Speicher");
    lcd.setCursor(0, 1);  //col, row
    lcd.print(i);
    lcd.print(" sec. halten");
    delay(1000);
    if (!readDebugButton()) return;
  }
  lcd.clear();
  for (int i = 13; i < EEPROM.length(); i++) {
    EEPROM.write(i, 254);
  }
  for (int i = 0; i < 13; i++) {
    EEPROM.write(i, 0);
  }

  lcd.setCursor(0, 0);  //col, row
  lcd.print("Speicher");  
  lcd.setCursor(0, 1);  //col, row
  lcd.print("geloescht");
  delay (2000);
}

void loop() {

  checkBarrier();

  //update display every 0,5 sec if on:
  if (displayOn && lastDisplayUpdate + 500 < millis()) {
    showOutput();
    lastDisplayUpdate = millis();
  }

  if (displayOn && lastDisplayPageSwitch + 10000 < millis()) {
    page++;
    lastDisplayPageSwitch = millis();
  }

  // check if debug button is pushed
  if (readDebugButton()) {
    debugLED(ON);
    activateDisplay(ON);
    showDebugOutput();
    delay(200);
    debugLED(OFF);
  }

  // check if user button is pushed
  if (readUserButton()) {
    page = 0;
    lastDisplayPageSwitch = millis();
    debugLED(ON);
    activateDisplay(ON);
    showOutput();
    delay(200);
    debugLED(OFF);
  }

  if (isDay) {
    if (displayOn && millisDisplayOff < millis()) activateDisplay(OFF);

    if (nextMillisToCheckForDaylight < millis()) {
      checkForDaylight();     
    }

  } else if (!isDay) {
    if (displayOn) activateDisplay(OFF);

    wdt_reset();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // choose power down mode
    sleep_mode();                         // sleep now!

    if (pwrdwnCycles++ > CYCLES_TO_WAIT) {  // After CYCLES_TO_WAIT wakeups by the watchdog timer, check for brightness
      pwrdwnCycles = 0;
      delay(100);
      checkForDaylight();
    }
  }
}


/* 
 * Barrier should contacts longer 25ms (30cm object at 5km/h) and shorter 250ms (30cm object at 40km/h). 
 * After first contact, we block the sensor for 300ms (two biker at 30km/h with a gap of 2,5m between).
 * Return true, if at least 3 consecutive measurements are detecting a contact.
 */
bool checkBarrier() {
  long startContact = millis();
  bool detection = false;
  permanentBlocked = false;
  while (objectDetected()) {
    // delay(1);
    detection = true;
    if (millis() > startContact + 2000) {
      for (short i = 0; i < 10; i++) {
        debugLED(ON);
        delay(50);
        debugLED(OFF);
        delay(50);
      }
      permanentBlocked = true;
      return false;
    }
  }
  if (detection) {
    lastContactDuration = millis() - startContact;
    storeOverallCount(++overallCount);
    todayCount++;
    storeTodayCount();
    delay(300);
  }
  return detection;
}


/*
 * Visualize current count, battery voltage and last daylight period counts
 */
void showOutput() {

  lcd.clear();
  lcd.noCursor();
  lcd.setCursor(0, 0);  //col, row
  lcd.print(overallCount);


  lcd.setCursor(12, 0);  //col, row
  lcd.print(readBatteryVoltage(), 1);
  lcd.print("V");


  if (permanentBlocked) {
    lcd.setCursor(0, 1);  //col, row
    lcd.print("LS BLOCKIERT    ");
  } else {
    if (overallCount < 100000000) {
      lcd.setCursor(8, 0);
      lcd.print("P");
    }
    lcd.print(page);
    lcd.setCursor(0, 1);  //col, row
    getDaylightRecordings();
    for (short i = 0; i < 4; i++) {
      if (pageContent[i] >= 254) {
        lcd.print("---");
      } else if (pageContent[i] == 253) {
        lcd.print("MAX");
      } else {
        if (pageContent[i] < 100) lcd.print("0");
        if (pageContent[i] < 10) lcd.print("0");
        lcd.print(pageContent[i]);
      }
      lcd.print(" ");
    }
  }
}

/*
 * Show some debug output on lcd display
 */
void showDebugOutput() {
  digitalWrite(PHOTODIODE_ON, HIGH);
  delay(25);
  int brightness = analogRead(PHOTODIODE);
  digitalWrite(PHOTODIODE_ON, LOW);
  int barrier = analogRead(LIGHTBARRIER);

  lcd.clear();
  lcd.noCursor();
  lcd.setCursor(0, 0);  //col, row
  lcd.print("B:");
  lcd.print(brightness);
  if (isDay) {
    lcd.print("D");
  } else {
    lcd.print("N");
  }

  lcd.setCursor(12, 0);  //col, row
  lcd.print(readBatteryVoltage(), 1);
  lcd.print("V");

  lcd.setCursor(0, 1);
  lcd.print("C:");
  lcd.print(todayCount);

  lcd.setCursor(9, 1);
  lcd.print(" D:");
  lcd.print(lastContactDuration);

  //lcd.setCursor(8, 1);
  //lcd.print("M:");
  //lcd.print(todaysEepromByteAddress);
}

/* 
 * True as long as the debug debug button is active.
 */
bool readDebugButton() {
  return (digitalRead(DEBUG_BUTTON) == LOW);
}

/*
 * True as long as the User debug button is active.
 * Workaround for missing Pin6 digitalRead of arduino API (due to not used crystal)
 */
bool readUserButton() {
  return !(PINB & (1 << PB6));
}

/*
 * Turn debugLED on/off
 */
void debugLED(bool on) {
  digitalWrite(DEBUG_LED, on);
}

/*
 * Will check for brightness. 
 * If the brightness is measured MEASUREMENT_LIMIT times higher than defined LIMIT_DAY, this method will returns always true. 
 * If the brightness is measured MEASUREMENT_LIMIT times lower then defined LIMIT_NIGHT, this method will returns always false.
 */
bool checkForDaylight() {
  if (todayStartedAtMillis + (HOURS_DAY_MIN * 3600000) > millis()) {
    return isDay;  // do not enter night mode before HOURS_DAY_MIN h daylight runtime
  }
  digitalWrite(PHOTODIODE_ON, HIGH);
  delay(50);
  if (isDay) {
    if (analogRead(PHOTODIODE) < LIMIT_NIGHT) {
      countOfMeasurementsOfNight++;
    }
    if (countOfMeasurementsOfNight >= MEASUREMENTS_LIMIT) {
      isDay = false;
      countOfMeasurementsOfNight = 0;
      debugLED(ON);
      delay(200);
      debugLED(OFF);
      digitalWrite(PHOTODIODE_ON, LOW);
      digitalWrite(LIGHTBARRIER_ON, LOW);
    }
  } else {
    if (analogRead(PHOTODIODE) > LIMIT_DAY) {
      isDay = true;      
      digitalWrite(PHOTODIODE_ON, HIGH);
      digitalWrite(LIGHTBARRIER_ON, HIGH);
      debugLED(ON);
      delay(500);
      debugLED(OFF);
      todayStartedAtMillis = millis();  //set start of the day to now
      todayCount = 0;
      determineTodayStorageByte();     
    }
  }
  digitalWrite(PHOTODIODE_ON, LOW);
  nextMillisToCheckForDaylight = millis() + CHECK_FOR_DAYLIGHT_EVERY;
  return isDay;
}



/*
 * Turn display ON/OFF
 */
void activateDisplay(bool on) {
  if (!on) {
    digitalWrite(LCD_LED, LOW);
    lcd.noDisplay();
    displayOn = false;
  } else {
    digitalWrite(LCD_LED, HIGH);
    lcd.display();
    displayOn = true;
    millisDisplayOff = millis() + DURATION_DISPLAY_ON;
  }
}

/* 
 *  Measure Battery voltage
 */
double readBatteryVoltage() {
  return 14.3 / 1023.0 * (double)analogRead(BATTERY_VOLTAGE) + 0.7d;
}


/* 
 *  returns true, when something blocks the light barrier
 */
bool objectDetected() {
  return analogRead(LIGHTBARRIER) > 100;
}


/*
 * Search for the first free EEPROM Address to store today's count later there.
 */
int determineTodayStorageByte() {
  todaysEepromByteAddress = 12;
  short value = 1;
  while (value != 254 && todaysEepromByteAddress++ < EEPROM.length() - 1) {
    value = EEPROM.read(todaysEepromByteAddress);
  }  //found next empty entry
  if (todaysEepromByteAddress >= EEPROM.length() - 1) {
    todaysEepromByteAddress = 12;
  }
  return todaysEepromByteAddress;
}


void getDaylightRecordings() {
  long address = todaysEepromByteAddress - 4 * page;
  for (short i = 3; i >= 0; i--) {
    if (address - i > 12) {
      pageContent[i] = EEPROM.read(address - i);
    } else {
      pageContent[i] = 255;
    }
  }
  if (pageContent[0] == 255) {
    page = 0;
  }
}
/*
 * Store the count for today only
 */
void storeTodayCount() {
  if (todayCount > 253) {
    return;
  }
  EEPROM.write(todaysEepromByteAddress, todayCount);
  EEPROM.update(todaysEepromByteAddress + 1, 254);
}

/*
 * Read the fist 3 long value from EEPROM. Check if at least 2 of 3 have the same value and return it. If they have completely different values, return the value of address 0.
 * This is implemented to mitigate an EEPROM failure.
 */
long readOverallCount() {
  long l1 = EEPROMReadlong(0);
  long l2 = EEPROMReadlong(4);
  long l3 = EEPROMReadlong(8);

  if (l1 != l2) {
    if (l1 == l3) {
      return l1;
    } else if (l2 == l3) {
      return l2;
    } else {
      return l1;
    }
  } else {
    return l1;
  }
}

/*
 * Store the current overall count into the first 12 (3x4) byte of the EEPROM.
 */
void storeOverallCount(long value) {
  EEPROMWritelong(0, value);
  EEPROMWritelong(4, value);
  EEPROMWritelong(8, value);
}

/*
 * Read a long (4byte) value from EEPROM
 */
long EEPROMReadlong(long address) {
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

/*
 * Write a long (4byte) value to EEPROM
 */
void EEPROMWritelong(int address, long value) {
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}


/*
 * Watchdog interrupt every 8 sec, to wake up from power down mode at night.
 */
void watchdogSetup(void) {
  cli();
  wdt_reset();
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  WDTCSR = (1 << WDIE) | (0 << WDE) | (1 << WDP3) | (1 << WDP0);  // 8s / interrupt, no system reset
  sei();
}

ISR(WDT_vect) {
}
