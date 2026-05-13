#include <Adafruit_GFX.h> // core graphics library
#include <Adafruit_SSD1306.h> // library to drive the display
#include <EEPROM.h> // for persistence

Adafruit_SSD1306 display(128, 64); // create display

#define EEPROM_MODE_ADDR     0
#define EEPROM_HUMIDITY_ADDR 1
#define EEPROM_TIMED_ADDR    2

#define LED_RED_PIN          11  // PB3
#define LED_GREEN_PIN        10  // PB2
#define LED_BLUE_PIN         9   // PB1

#define RELAY_PIN            8   // PB0
  
#define CHANGE_MODE_PIN      2   // PD2 / INT0
#define CHANGE_VALS_PIN      3   // PD3 / INT1
#define WAKE_PIN             4   // PD4 / PCINT20

#define TEMP_PIN             A0  // PC0
#define WATER_LEVEL_PIN      A1  // PC1
#define HUMIDITY_PIN         A2  // PC2
#define BATTERY_PIN          A3  // PC3

#define DRY_VOLTAGE          3.0
#define WET_VOLTAGE          1.4

#define NUM_LEVELS           4

const int humidityThresholds[] = {50, 60, 70, 80};
const int wateringIntervals[]  = {1, 2, 3, 4};

int selectedValuesList = 0;

int humidityIndex;
int timedIndex;

unsigned long wateringStart = 0;
bool watering = false;

unsigned long lastHumidityWatering = 0;
bool humidityCooldown = false;
unsigned long humidityCooldownTime = 120000;

float temperature = 0;
float humidity    = 0; // percent
float voltage     = 0;

volatile bool             changeMode = false;
unsigned long       lastReadTimeTemp = 0;
const unsigned long     intervalTemp = 1000; // ms

unsigned long lastReadTimeBattery = 0;
const unsigned long intervalBattery = 30000; // ms

unsigned long lastReadTimeHumidity = 0;
const unsigned long intervalHumidity = 30000; // ms

unsigned long lastActivityTime = 0;
const unsigned long standbyTime = 60000; // 1 minute
volatile bool standby = false;
volatile bool wakeFlag = false;

unsigned long lastRelayCommand = 0;

int waterLevel = 0;

unsigned long lastLedUpdate = 0;
int brightness = 0;
int fadeAmount = 5;

enum LEDState {
  OFF,
  RED_FADE,
  BLUE_FADE
};

LEDState LEDMode = OFF;

enum FunctionMode {
  HUMIDITY,
  MANUAL,
  TIMED
};

FunctionMode mode;

enum ScreenMode {
  STANDARD,
  MENU
};

ScreenMode screen = STANDARD;

// ISR for changing mode of operation in STANDARD screen
// and changing parameter to modify in MENU screen
ISR(INT0_vect) {
  if (!standby && !watering) {

    // debounce
    if (millis() - lastActivityTime < 200) {
      return;
    }

    // stop noise from the relay
    if (millis() - lastRelayCommand < 200) {
      return;
    }

    lastActivityTime = millis();

    // temporary solution for weird bug that increments saved mode on reset
    if (lastActivityTime < 500 || lastActivityTime - lastHumidityWatering < 500) {
      return;
    }

    if (screen == STANDARD) {
      changeMode = true;
    } else {
      selectedValuesList = (selectedValuesList + 1) % 2;
    }

  }
}

// ISR for changing values of parameters
ISR(INT1_vect) {
  if (standby || screen == STANDARD || watering) {
    return;
  }

  // debounce
  if (millis() - lastActivityTime < 200) {
    return;
  }

  if (selectedValuesList == 0) {
    humidityIndex = (humidityIndex + 1) % NUM_LEVELS;
  } else {
    timedIndex = (timedIndex + 1) % NUM_LEVELS;
  }

  lastActivityTime = millis();
}

void updateEEPROM() {
  Serial.println(mode);
  EEPROM.update(EEPROM_MODE_ADDR, mode);
  EEPROM.update(EEPROM_HUMIDITY_ADDR, humidityIndex);
  EEPROM.update(EEPROM_TIMED_ADDR, timedIndex);
}

// ISR for exiting standby mode and for changing screens
ISR(PCINT2_vect) {

  // debounce
  if (millis() - lastActivityTime < 200) {
    return;
  }

  if (standby) {
    wakeFlag = true;
  } else {

    if (digitalRead(WAKE_PIN) == LOW) {
      
      if (screen == MENU) {
        updateEEPROM();
      }

      screen = (screen + 1) % 2;

      lastActivityTime = millis();

    }
  }
}

void setupWakePin() {
  // wake button interrupt logic
  pinMode(WAKE_PIN, INPUT_PULLUP);
  // enable PCINT for PORTD
  PCICR |= (1 << PCIE2);
  // enable D4 (PCINT20)
  PCMSK2 |= (1 << PCINT20);
}

void setupChangeModePin() {
  // PD2 intrare
  DDRD &= ~(1 << DDD2);

  // pull-up
  PORTD |= (1 << PORTD2);

  // FALLING edge
  EICRA |= (1 << ISC01);
  EICRA &= ~(1 << ISC00);

  // activam INT0
  EIMSK |= (1 << INT0);
}

void setupChangeValsPin() {

  // PD3 intrare
  DDRD &= ~(1 << DDD3);

  // pull-up
  PORTD |= (1 << PORTD3);

  // FALLING edge
  EICRA |= (1 << ISC11);
  EICRA &= ~(1 << ISC10);

  // activam INT1
  EIMSK |= (1 << INT1);
}


void setupDisplay() {
  delay(100); // delay needed to let display initiate
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize display with the I2C address of 0x3C
  display.clearDisplay(); // clear buffer
  display.setTextColor(WHITE);

  display.setRotation(0); // set orientation
  display.setTextWrap(false);

  display.dim(1); // set brightness (0 is maximun and 1 is a little dim)

}

void setupLED() {
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);

  // led off
  analogWrite(LED_RED_PIN,  255);
  analogWrite(LED_GREEN_PIN,255);
  analogWrite(LED_BLUE_PIN, 255);
}


void setupEEPROMread() {
  // persistence of settings
  mode = EEPROM.read(EEPROM_MODE_ADDR);

  if (mode > TIMED) { 
    mode = HUMIDITY;
  }

  humidityIndex = EEPROM.read(EEPROM_HUMIDITY_ADDR);
  timedIndex    = EEPROM.read(EEPROM_TIMED_ADDR);

  if (humidityIndex >= NUM_LEVELS) {
    humidityIndex = 0;
  }

  if (timedIndex >= NUM_LEVELS) {
    timedIndex = 0;
  }

}


void setup() {

  Serial.begin(9600);
  sei();

  setupDisplay();
  
  setupChangeModePin();
  setupChangeValsPin();
  setupWakePin();

  setupLED();

  setupEEPROMread();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // pump OFF
}

void displayArray(int size, int *array, int x, int y) {
  display.setCursor(x, y);
  for (int i = 0; i < size; i++) {
    display.print(array[i]);
    display.print(" ");
  }
}

void displayStandardScreen() {
   
  char string[10];

  // convert float to a string
  // (<variable>,<amount of digits we are going to use>,<amount of decimal digits>,<string name>)
  dtostrf(temperature, 4, 2, string);
  display.clearDisplay();

  display.setCursor(0, 8);
  display.print("Mode: ");

  switch(mode) {
    case HUMIDITY:
      display.print("Humidity");
      break;
    case MANUAL:
      display.print("Manual");
      break;
    case TIMED:
      display.print("Timed");
      break;
  }

  display.setCursor(0, 50);
  display.print("Temp: ");
  display.print(string);

  dtostrf(voltage, 3, 2, string);
  display.setCursor(75, 50);
  display.print("V: ");
  display.print(string);

  dtostrf(humidity, 3, 2, string);
  display.setCursor(0, 30);
  display.print("Hum: ");
  display.print(string);
  display.print("%");

  display.display(); // print everything set
}

void displayMenuScreen() {
  display.clearDisplay();

  display.setCursor(5, 5);
  display.print("Humidity levels:");
  displayArray(NUM_LEVELS, humidityThresholds, 5, 20);

  display.setCursor(5, 35);
  display.print("Time intervals:");
  displayArray(NUM_LEVELS, wateringIntervals, 5, 50);

  // display selected values and parameter selected for modifying
  if (selectedValuesList == 0) {
    display.fillRect(3 + 18 * humidityIndex, 16, 15, 15, SSD1306_INVERSE);
    display.drawRect(3 + 12 * timedIndex, 46, 9, 15, SSD1306_WHITE);
  } else {
    display.drawRect(3 + 18 * humidityIndex, 16, 15, 15, SSD1306_WHITE);
    display.fillRect(3 + 12 * timedIndex, 46, 9, 15, SSD1306_INVERSE);
  }

  display.display();
}


void displayHandler() {

  switch(screen) {
    case STANDARD:
      displayStandardScreen();
      break;
    case MENU:
      displayMenuScreen();
      break;
  }
}

void handleLEDSignal() {

  if (millis() - lastLedUpdate >= 20) {
    lastLedUpdate = millis();

    brightness += fadeAmount;

    if (brightness <= 0 || brightness >= 255) {
      fadeAmount = -fadeAmount;
    }
  }

  switch(LEDMode) {
    case OFF:
      analogWrite(LED_RED_PIN,  255);
      analogWrite(LED_BLUE_PIN, 255);
      break;
    case RED_FADE:
      analogWrite(LED_RED_PIN,  255 - brightness);
      analogWrite(LED_BLUE_PIN, 255);
      break;
    case BLUE_FADE:
      analogWrite(LED_RED_PIN,  255);
      analogWrite(LED_BLUE_PIN, 255 - brightness);
      break;
  }
}

void readTemperature() {
  if (watering) {
    return;
  }
  if (millis() - lastReadTimeTemp >= intervalTemp) {
    lastReadTimeTemp = millis();
    temperature = analogRead(TEMP_PIN) * 0.488;
  }
}

void readBatteryVoltage() {
  if (millis() - lastReadTimeBattery >= intervalBattery) {
    lastReadTimeBattery = millis();
    voltage = analogRead(BATTERY_PIN) * (5.0 / 1023.0) * 2;
  }
}

void readHumidity() {
  if (millis() - lastReadTimeHumidity >= intervalHumidity) {
    lastReadTimeHumidity = millis();

    float sensorVoltage = analogRead(HUMIDITY_PIN) * (5.0 / 1023.0);

    // Debug 
    //Serial.print("Voltage:");
    //Serial.println(sensorVoltage);

    humidity = (DRY_VOLTAGE - sensorVoltage) * 100.0 / (DRY_VOLTAGE - WET_VOLTAGE);

    if (humidity > 100) {
      humidity = 100;
    }
    if (humidity < 0) {
      humidity = 0;
    }
  }
}

bool checkVoltage() {
  if (voltage < 3) {
    LEDMode = RED_FADE; // battery low error
    //Serial.println("SI AICI");
    return false;
  }
  return true;
}

bool checkWaterLevel() {

  if (analogRead(WATER_LEVEL_PIN) < 400) {
    LEDMode = BLUE_FADE; // water level low error
    //Serial.println("AICI");
    return false;
  }
  return true;
}

void humidityWatering() {

  if (millis() < 500) {
    return;
  }

  if (millis() - lastHumidityWatering >= humidityCooldownTime) {
    humidityCooldown = false;
  }

  if (humidityCooldown) {
    return;
  }

  if (!watering && humidity < humidityThresholds[humidityIndex]) {
    Serial.println("Start udat umiditate");
    Serial.println(millis());
    digitalWrite(RELAY_PIN, LOW);

    watering = true;
    wateringStart = millis();
  }

  // 1 second for testing
  if (watering && millis() - wateringStart >= 1000) {
    Serial.println("Sfarsit udat umiditate");
    Serial.println(millis());
    digitalWrite(RELAY_PIN, HIGH);
    
    lastHumidityWatering = millis();
    watering = false;
    humidityCooldown = true;
  }
}

void manualWatering() {
  bool buttonPressed = (digitalRead(CHANGE_VALS_PIN) == LOW);

  if (buttonPressed && digitalRead(RELAY_PIN) == HIGH) {

    if (checkWaterLevel() && checkVoltage()) {
      digitalWrite(RELAY_PIN, LOW);
      watering = true;
      lastRelayCommand = millis();
    }
  } 
  else if (!buttonPressed && digitalRead(RELAY_PIN) == LOW) {
    digitalWrite(RELAY_PIN, HIGH);
    watering = false;
    lastRelayCommand = millis();
  }

}

void timedWatering() {
  
}


// this checks every time is called i need to make it more power efficient
void checkReturnFromError() {
  if (LEDMode == BLUE_FADE) {
    if (analogRead(WATER_LEVEL_PIN) > 400) {
      LEDMode = OFF;
    }
  }

  if (LEDMode == RED_FADE) {
    if (voltage > 3) {
      LEDMode = OFF;
    }
  }

}

void handleMode() {

  // battery or water error
  if (LEDMode != OFF) {
    checkReturnFromError();
    return;
  }

   switch(mode) {
    case HUMIDITY:
      humidityWatering();
      break;
    case MANUAL:
      manualWatering();
      break;
    case TIMED:
      timedWatering();
      break;
  }
}

// TO DO un fel de reset pentru ecran ca isi mai ia freeze de la releu

void loop() {

  readHumidity();

  readBatteryVoltage();

  handleLEDSignal();

  handleMode();

  if (!standby && (millis() - lastActivityTime >= standbyTime)) {
    standby = true;
    updateEEPROM();
    // power management settings maybe
  }

  if (wakeFlag) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    wakeFlag = false;
    standby = false;
    // back to main menu after standby
    screen = STANDARD;
    lastActivityTime = millis();
  }

  if (standby) {
    display.clearDisplay();   // clear buffer
    display.display();        // empty screen
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    return;
  }

  if (changeMode) {
    digitalWrite(RELAY_PIN, HIGH);
    changeMode = false;
    mode = (FunctionMode)((mode + 1) % 3);
  }

  // temperature not read when in standby
  readTemperature();

  displayHandler();
}