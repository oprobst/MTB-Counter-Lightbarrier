#include "sensors.h"
// #include "debug.h" // debug.h is no longer needed if all DEBUG_PRINT* are removed
#include "store.h" // For startANewDay(), storeTodayCount(), storeOverallCount(), readOverallCount()
#include "ui.h"    // For turnDisplay()
#include "counter.h" // For updating todayCount and overallCount

#include <avr/wdt.h>
#include <avr/sleep.h>

#define PHOTODIODE_ON 0
#define PHOTODIODE A5

#define LIGHTBARRIER_ON 4
#define LIGHTBARRIER A3
#define LIGHTBARRIER2 A2

#define BATTERY_VOLTAGE A4

#define DEBUG_BUTTON 3
#define USER_BUTTON 6 // Assuming PB6 from original code corresponds to Arduino pin 6

#define CHECK_FOR_DAYLIGHT_EVERY 120000  //ms how often to check daylight
#define DURATION_DISPLAY_ON 300000  //ms how long the display should stay on
#define CYCLES_TO_WAIT 4           // multiplied with 8sec = time to wait between brightness checks to wake up from night sleep

#define LIMIT_NIGHT 14             // Measured limit to activate night mode
#define LIMIT_DAY 30               // Measured limit to activate day mode
#define MEASUREMENTS_LIMIT 5       //consecutive measurements below LIMIT_NIGHT required before power down.
#define HOURS_DAY_MIN 10           //minimum hours it must run every day before checking daylight

#define OFF 0

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

long todayStartedAtMillis = millis();

//indicates if the Light barrier is durable blocked (longer than 2 sec).
bool permanentBlocked = false;


// Forward declarations for internal functions
void watchdogSetup(void);
bool readDebugButton();
bool readUserButton();
void debugLED(bool on);
void activateDisplay(bool on);
double readBatteryVoltageInternal();
bool objectDetected();
bool checkForDaylight();
void resetMemoryInternal();

void initSensors() {
  // DEBUG_PRINTLN(F("Initialisiere Sensormodul")); // Removed DEBUG_PRINTLN

  pinMode(PHOTODIODE_ON, OUTPUT);
  pinMode(PHOTODIODE, INPUT);
  pinMode(LIGHTBARRIER_ON, OUTPUT);
  pinMode(LIGHTBARRIER, INPUT);
  pinMode(LIGHTBARRIER2, INPUT);
  pinMode(DEBUG_BUTTON, INPUT_PULLUP);
  //Pin6 is not arduino API compatible, need to set it manually for USER_BUTTON:
  DDRB &= ~(1 << PB6);  //input
  PORTB |= (1 << PB6);  //pullup

  digitalWrite(PHOTODIODE_ON, HIGH);
  digitalWrite(LIGHTBARRIER_ON, HIGH);
  debugLED(OFF); // Ensure debug LED is off initially

  // Check for reset memory on startup
  if (readDebugButton()) {
    resetMemoryInternal();
  }

  // overallCount and todayCount are initialized and managed by the Counter module
  // No need to read them here

  watchdogSetup(); // Setup watchdog for power down mode
  activateDisplay(true); // Turn on display initially
}

bool checkBarrier() {
  long startContact = millis();
  bool detection = false;
  permanentBlocked = false;
  while (objectDetected()) {
    detection = true;
    if (millis() > startContact + 2000) {
      permanentBlocked = true;
      // DEBUG_PRINTLN(F("Permanent Blocked!")); // Removed DEBUG_PRINTLN
      return false;
    }
  }
  if (detection) {
    lastContactDuration = millis() - startContact;
    incrementCounts(); // Delegate to Counter module to update todayCount and overallCount
    
    // DEBUG_PRINTF("Contact! Today: %d, Overall: %ld, Duration: %ldms\n", getTodayCount(), getOverallCount(), lastContactDuration); // Removed DEBUG_PRINTF
    
    // Quick debug LED blinks
    debugLED(true);
    delay(50);
    debugLED(false);
    delay(50);
    debugLED(true);
    delay(50);
    debugLED(false);
    delay(50);
    debugLED(true);
    delay(50);
    debugLED(false);
    return true; // Indicate a new detection
  }
  return false; // No new detection
}

bool isPermanentBlocked() {
  return permanentBlocked;
}

void handleSleepAndDaylight() {
  if (displayOn && millisDisplayOff < millis()) {
    activateDisplay(false); // Turn off display if timeout reached
  }

  if (isDay) {
    if (nextMillisToCheckForDaylight < millis()) {
      checkForDaylight();    
      nextMillisToCheckForDaylight = millis() + CHECK_FOR_DAYLIGHT_EVERY; // Reset timer for next check
    }
  } else {  // If it's night
    wdt_reset();
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // choose power down mode
    sleep_mode();                         // sleep now!

    if (pwrdwnCycles++ > CYCLES_TO_WAIT) {  // After CYCLES_TO_WAIT wakeups by the watchdog timer, check for brightness
      pwrdwnCycles = 0;
      delay(100); // Small delay after wakeup before checking sensor
      checkForDaylight();
    }
  }
}

void handleButtonPresses() {
  if (readDebugButton()) {
    debugLED(true);
    activateDisplay(true); // Turn on display for debug output
    // DEBUG_PRINTLN(F("Debug Button Pressed. Showing debug info.")); // Removed DEBUG_PRINTLN
    delay(200);
    debugLED(false);
  }

  if (readUserButton()) {
    resetPage(); // Delegate to Counter module to reset page
    debugLED(true);
    activateDisplay(true); // Turn on display for user output
    // DEBUG_PRINTLN(F("User Button Pressed. Showing info.")); // Removed DEBUG_PRINTLN
    delay(200);
    debugLED(false);
  }
}

int getBrightness() {
  digitalWrite(PHOTODIODE_ON, HIGH);
  delay(25); // Give sensor time to settle
  int brightness = analogRead(PHOTODIODE);
  digitalWrite(PHOTODIODE_ON, LOW);
  return brightness;
}

bool getIsDay() {
  return isDay;
}

int getBatteryLevel() {
  double voltage = readBatteryVoltageInternal();
  // Assuming a 12V lead-acid battery or similar
  // Adjust these values based on the actual battery type and its charge curve
  if (voltage >= 12.7) return 100; // Fully charged
  if (voltage >= 12.5) return 90;
  if (voltage >= 12.3) return 80;
  if (voltage >= 12.0) return 70;
  if (voltage >= 11.8) return 60;
  if (voltage >= 11.6) return 50;
  if (voltage >= 11.4) return 40;
  if (voltage >= 11.2) return 30;
  if (voltage >= 11.0) return 20;
  if (voltage >= 10.8) return 10;
  return 0; // Critically low
}


// --- Internal Helper Functions from original sketch ---

/*
 * Reset memory, if Debug button is pushed on startup for > 9 sec.
 */
void resetMemoryInternal() {
  // DEBUG_PRINTLN(F("Resetting Memory. Hold button for 9 seconds.")); // Removed DEBUG_PRINTLN
  for (short i = 9; i > 0; i--) {   
    // DEBUG_PRINTF("Reset in %d sec.\n", i); // Removed DEBUG_PRINTF
    delay(1000);
    if (!readDebugButton()) {
      // DEBUG_PRINTLN(F("Memory reset cancelled.")); // Removed DEBUG_PRINTLN
      return;
    }
  }
  
  clearEEPROM(); // Delegate to store module
  initCounts(); // Re-initialize counts in Counter module

  // DEBUG_PRINTLN(F("EEPROM erased.")); // Removed DEBUG_PRINTLN
  delay (2000); // Keep delay for user feedback
}

/* * Check barrier is blocked
 */
bool objectDetected() {
  // Read both sensors
  int val1 = analogRead(LIGHTBARRIER);
  int val2 = analogRead(LIGHTBARRIER2);

  // If either sensor detects an object
  if (val1 > 100 || val2 > 100) {
    // Check if both are blocked within a short timeframe
    unsigned long entryTime = millis();
    unsigned long timeout = 100; // Check for 100ms for the second sensor to trigger

    while (millis() - entryTime < timeout) {
      if (val1 > 100 && analogRead(LIGHTBARRIER2) > 100) return true; // Both blocked
      if (val2 > 100 && analogRead(LIGHTBARRIER) > 100) return true; // Both blocked
    }
  }
  return false;
}


/* * True as long as the debug debug button is active.
 */
bool readDebugButton() {
  return (digitalRead(DEBUG_BUTTON) == LOW);
}

/*
 * True as long as the User debug button is active.
 * Workaround for missing Pin6 digitalRead of arduino API (due to not used crystal)
 */
bool readUserButton() {
  return !(PINB & (1 << PB6)); // Directly access PORTB register for PB6
}

/*
 * Turn debugLED on/off
 */
void debugLED(bool on) {
  // DEBUG_LED is defined in original code as 7, so it's probably connected to digital pin 7
  digitalWrite(7, on);
}

/*
 * Will check for brightness. 
 * If the brightness is measured MEASUREMENT_LIMIT times higher than defined LIMIT_DAY, this method will returns always true. 
 * If the brightness is measured MEASUREMENT_LIMIT times lower then defined LIMIT_NIGHT, this method will returns always false.
 */
bool checkForDaylight() {
  if (todayStartedAtMillis + (HOURS_DAY_MIN * 3600000UL) > millis()) { // Using UL for unsigned long literal
    return isDay;  // do not enter night mode before HOURS_DAY_MIN h daylight runtime
  }
  
  digitalWrite(PHOTODIODE_ON, HIGH); // Power on photodiode
  delay(50); // Give sensor time to settle
  int currentBrightness = analogRead(PHOTODIODE); // Read brightness
  digitalWrite(PHOTODIODE_ON, LOW); // Power off photodiode to save power

  if (isDay) {
    if (currentBrightness < LIMIT_NIGHT) { // If brightness is below night limit
      countOfMeasurementsOfNight++;
      // DEBUG_PRINTF("Night measurements: %d/%d (Brightness: %d)\n", countOfMeasurementsOfNight, MEASUREMENTS_LIMIT, currentBrightness); // Removed DEBUG_PRINTF
    } else {
      countOfMeasurementsOfNight = 0; // Reset counter if it's bright enough
    }

    if (countOfMeasurementsOfNight >= MEASUREMENTS_LIMIT) { // If enough consecutive night measurements
      isDay = false; // Set night mode
      countOfMeasurementsOfNight = 0; // Reset counter
      // DEBUG_PRINTLN(F("Entering Night Mode.")); // Removed DEBUG_PRINTLN
      debugLED(true); // Indicate state change
      delay(200);
      debugLED(false);
      digitalWrite(PHOTODIODE_ON, LOW); // Ensure photodiode is off
      digitalWrite(LIGHTBARRIER_ON, LOW); // Turn off light barriers to save power
     // storeTodayCount(getTodayCount()); // Store today's count before going to sleep
      turnDisplay(false); // Turn off display via UI module
    }
  } else { // If it's currently night
    if (currentBrightness > LIMIT_DAY) { // If brightness is above day limit
      isDay = true; // Set day mode
      // DEBUG_PRINTLN(F("Entering Day Mode.")); // Removed DEBUG_PRINTLN
      digitalWrite(PHOTODIODE_ON, HIGH); // Power on photodiode
      digitalWrite(LIGHTBARRIER_ON, HIGH); // Turn on light barriers
      debugLED(true); // Indicate state change
      delay(500);
      debugLED(false);
      todayStartedAtMillis = millis();  //set start of the day to now
    //  initTodayCount(); // Reset today's count for new day (handled by Counter module)
      startANewDay(); // Determine new EEPROM address for today (handled by Store module)
      turnDisplay(true); // Turn on display via UI module
    }
  }
  return isDay;
}

/*
 * Turn display ON/OFF
 */
void activateDisplay(bool on) {
  turnDisplay(on); // Delegate to UI module
  if (on) {
    displayOn = true;
    millisDisplayOff = millis() + DURATION_DISPLAY_ON; // Set timeout for display off
  } else {
    displayOn = false;
  }
}

/* * Measure Battery voltage
 */
double readBatteryVoltageInternal() {
  return 14.3 / 1023.0 * (double)analogRead(BATTERY_VOLTAGE) + 1.0d;
}

/*
 * Watchdog interrupt every 8 sec, to wake up from power down mode at night.
 */
void watchdogSetup(void) {
  cli(); // Disable interrupts
  wdt_reset(); // Reset watchdog timer
  // Setup watchdog control register to allow configuration change (WDCE) and enable watchdog (WDE)
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  // Set watchdog interrupt enable (WDIE), disable system reset (WDE=0), and set timeout to 8s (WDP3, WDP0)
  WDTCSR = (1 << WDIE) | (0 << WDE) | (1 << WDP3) | (1 << WDP0);  // 8s / interrupt, no system reset
  sei(); // Enable interrupts
}

// Watchdog Interrupt Service Routine (ISR) - simply wakes up from sleep
ISR(WDT_vect) {
  // This ISR is intentionally left empty. Its purpose is to wake the microcontroller from sleep mode.
}