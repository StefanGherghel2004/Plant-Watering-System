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
#define MANUAL_MODE_PIN      7   // PD7

#define CHANGE_MODE_PIN      2   // PD2 / INT0
#define CHANGE_VALS_PIN      3   // PD3 / INT1
#define WAKE_PIN             4   // PD4 / PCINT20

#define TEMP_PIN             A0  // PC0
#define WATER_LEVEL_PIN      A1  // PC1
#define HUMIDITY_PIN         A2  // PC2
#define BATTERY_PIN          A3  // PC3

#define MS_PER_DAY           86400000UL
#define WATERING_DURATION    2000UL

#define MIN_BATTERY_VOLTAGE   3.0 // dummy value
#define MIN_WATER_LEVEL       400 // can change in final version

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

unsigned long lastTimedWateringTime = 0;

float temperature = 0;
float humidity    = 0; // percent
float voltage     = 0;

volatile bool             changeMode = false;
unsigned long       lastReadTimeTemp = 0;
const unsigned long     intervalTemp = 10000; // ms

unsigned long lastReadTimeBattery = 0;
const unsigned long intervalBattery = 30000; // ms

unsigned long lastReadTimeHumidity = 0;
const unsigned long intervalHumidity = 30000; // ms

unsigned long lastActivityTime = 0;
const unsigned long standbyTime = 60000; // 1 minute
volatile bool standby = false;
volatile bool wakeFlag = false;
volatile bool changeSelectedValuesList = false;
volatile bool changeSelectedValue      = false;
volatile bool changeScreen             = false;

unsigned long lastRelayCommand = 0;

int waterLevel = 0;
unsigned long lastReadTimeWaterLevel = 0;
const unsigned long intervalWaterLevel = 30000; // ms

unsigned long lastLedUpdate = 0;
int brightness = 0;
int fadeAmount = 5;

const unsigned char epd_bitmap_water_drop [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x6f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xc7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xf7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xf3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf9, 0x69, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xd9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xd9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x98, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9c, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x67, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xe3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0xd3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xd9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xa8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xf3, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xe7, 0xee, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x6f, 0xe7, 0xfe, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x67, 0xcd, 0xfb, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x57, 0xcd, 0xff, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xb3, 0x8f, 0xfd, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xb3, 0x9b, 0xb7, 0x9f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xf3, 0x19, 0xff, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x03, 0x1b, 0xff, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x07, 0x1f, 0xff, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0e, 0x0c, 0xdf, 0x87, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x0f, 0x5f, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x07, 0xee, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x01, 0xf0, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x0d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x5d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x01, 0xba, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x7a, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x01, 0xf3, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

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
  MENU,
  WATERING
};

ScreenMode screen = STANDARD;
ScreenMode lastScreen = STANDARD;

unsigned long currentMillis;


int myAnalogRead(uint8_t pin) {

  if (pin >= 14) {
    pin = pin - 14;
  }

  pin &= 0x07;

  // reference and channel
  ADMUX = (1 << REFS0) | pin;

  // ADC start conversion
  ADCSRA |= (1 << ADSC);

  // wait for conversion to finish
  while (ADCSRA & (1 << ADSC));

  return ADC;
}

// ISR for changing mode of operation in STANDARD screen
// and changing parameter to modify in MENU screen
ISR(INT0_vect) {
  if (!standby && !watering) {

    unsigned long ms = currentMillis;

    // debounce
    if (ms - lastActivityTime < 200) {
      return;
    }

    // stop noise from the relay
    if (ms- lastRelayCommand < 200) {
      return;
    }

    if (screen == STANDARD) {
      changeMode = true;
    } else if (screen == MENU) {
      changeSelectedValuesList = true;
    }

    lastActivityTime = ms;

  }
}

// ISR for changing values of parameters
ISR(INT1_vect) {
  if (standby || screen == STANDARD || watering) {
    return;
  }

  unsigned long ms = currentMillis;

  // debounce
  if (ms - lastActivityTime < 200) {
    return;
  }

  if (ms - lastRelayCommand < 200) {
    return;
  }

  changeSelectedValue = true;
  lastActivityTime = ms;
}

void updateEEPROM() {
  Serial.println(mode);
  EEPROM.update(EEPROM_MODE_ADDR, mode);
  EEPROM.update(EEPROM_HUMIDITY_ADDR, humidityIndex);
  EEPROM.update(EEPROM_TIMED_ADDR, timedIndex);
}

// ISR for exiting standby mode and for changing screens
ISR(PCINT2_vect) {

  unsigned long ms = currentMillis;

  if (watering) {
    return;
  }

  // debounce
  if (ms - lastActivityTime < 200) {
    return;
  }

  if (ms - lastRelayCommand < 200) {
    return;
  }

  if (standby) {
    wakeFlag = true;
  } else {

    if (digitalRead(WAKE_PIN) == LOW) {
      
      changeScreen = true;
      lastActivityTime = ms;

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
  // PD2 input
  DDRD &= ~(1 << DDD2);

  // pull-up
  PORTD |= (1 << PORTD2);

  // FALLING edge
  EICRA |= (1 << ISC01);
  EICRA &= ~(1 << ISC00);

  // INT0 enable
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

  // INT1 enable
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

  // ENABLE ADC and prescaler 128
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
  setupDisplay();
  
  setupChangeModePin();
  setupChangeValsPin();
  setupWakePin();

  setupLED();

  setupEEPROMread();

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(MANUAL_MODE_PIN, INPUT_PULLUP);
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
  if (temperature == 0.0) {
    display.print("NaN");
  } else {
    display.print(string);
    display.write(247); // ASCII for °
    display.print("C");
  }

  dtostrf(humidity, 3, 2, string);
  display.setCursor(0, 30);
  display.print("Hum: ");
  if (humidity == 0.0) {
    display.print("NaN");
  } else {
    display.print(string);
    display.print("%");
  }

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

void displayWateringImage() {
  display.clearDisplay();


  if (mode == MANUAL) {
    display.drawBitmap(0, 0, epd_bitmap_water_drop, 128, 64, 0, 1);
  } else {
    display.drawBitmap(0, -5, epd_bitmap_water_drop, 128, 64, 0, 1);

    unsigned long time = currentMillis - wateringStart;

    if (time > WATERING_DURATION) {
      time = WATERING_DURATION;
    }

    int percent = (time * 100UL) / WATERING_DURATION;

    display.drawRect(10, 58, 108, 6, SSD1306_WHITE);

    int length = (percent * 104) / 100;

    display.fillRect(12, 60, length, 2, SSD1306_WHITE);
  }

  display.display();
}


void displayHandler() {

  if (standby) {
    return;
  }

  switch(screen) {
    case STANDARD:
      displayStandardScreen();
      break;
    case MENU:
      displayMenuScreen();
      break;
    case WATERING:
      displayWateringImage();
      break;
  }
}

void handleLEDSignal() {

  // battery or water error
  if (LEDMode != OFF) {
    checkReturnFromError();
  }

  if (currentMillis - lastLedUpdate >= 20) {
    lastLedUpdate = currentMillis;

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

int getFilteredADC(uint8_t pin, uint8_t totalReadings, uint8_t trimAmount) {
  int readings[10]; 
  if (totalReadings > 10) totalReadings = 10;
  
  for (uint8_t i = 0; i < totalReadings; i++) {
    readings[i] = myAnalogRead(pin);
    delayMicroseconds(50);
  }

  for (uint8_t i = 0; i < totalReadings - 1; i++) {
    for (uint8_t j = 0; j < totalReadings - i - 1; j++) {
      if (readings[j] > readings[j + 1]) {
        int temp = readings[j];
        readings[j] = readings[j + 1];
        readings[j + 1] = temp;
      }
    }
  }

  long sum = 0;
  uint8_t validCount = 0;

  for (uint8_t i = trimAmount; i < (totalReadings - trimAmount); i++) {
    sum += readings[i];
    validCount++;
  }

  if (validCount == 0) {
    return readings[totalReadings / 2];
  }

  return sum / validCount;
}

void readTemperature() {
  if (watering) {
    return;
  }
  if (currentMillis - lastReadTimeTemp >= intervalTemp) {
    lastReadTimeTemp = currentMillis;
    int cleanADC = getFilteredADC(TEMP_PIN, 5, 1);
    temperature = cleanADC * 0.488;
  }
}

void readBatteryVoltage(bool forced = false) {
  if (forced || (currentMillis - lastReadTimeBattery >= intervalBattery)) {
    lastReadTimeBattery = currentMillis;
    int cleanADC = getFilteredADC(BATTERY_PIN, 3, 0);
    voltage = cleanADC * (5.0 / 1023.0) * 2;
  }
}

void readHumidity() {
  if (currentMillis - lastReadTimeHumidity >= intervalHumidity) {
    lastReadTimeHumidity = currentMillis;

    int cleanADC = getFilteredADC(HUMIDITY_PIN, 5, 1);
    float sensorVoltage = cleanADC * (5.0 / 1023.0);
    humidity = (DRY_VOLTAGE - sensorVoltage) * 100.0 / (DRY_VOLTAGE - WET_VOLTAGE);

    if (humidity > 100) {
      humidity = 100;
    }
    if (humidity < 0) {
      humidity = 0;
    }
  }
}

void readWaterLevel(bool forced = false) {
  if (forced || (currentMillis - lastReadTimeWaterLevel >= intervalWaterLevel)) {
    lastReadTimeWaterLevel = currentMillis;

    waterLevel = getFilteredADC(WATER_LEVEL_PIN, 3, 0);
    Serial.println(waterLevel);
  }
}

bool checkVoltage() {
  readBatteryVoltage(true);
  Serial.println("V CHECK");
  if (voltage < MIN_BATTERY_VOLTAGE) {
    LEDMode = RED_FADE; // battery low error
    return false;
  }
  return true;
}

bool checkWaterLevel() {
  readWaterLevel(true);
  if (waterLevel < MIN_WATER_LEVEL) {
    LEDMode = BLUE_FADE; // water level low error
    return false;
  }
  return true;
}

void humidityWatering() {

  if (currentMillis < 500) {
    return;
  }

  if (currentMillis - lastHumidityWatering >= humidityCooldownTime) {
    humidityCooldown = false;
  }

  if (humidityCooldown) {
    return;
  }

  if (!watering && humidity < humidityThresholds[humidityIndex] && humidity != 0.0) {

    if (checkWaterLevel() && checkVoltage()) {

      digitalWrite(RELAY_PIN, LOW);
      lastRelayCommand = currentMillis;

      watering = true;
      wateringStart = currentMillis;
      lastScreen = screen;
      screen = WATERING;
    }
  }

  if (watering && currentMillis - wateringStart >= WATERING_DURATION) {
    Serial.println("Sfarsit udat umiditate");
    Serial.println(currentMillis);
    Serial.println(millis());
    digitalWrite(RELAY_PIN, HIGH);
    
    lastHumidityWatering = currentMillis;
    watering = false;
    humidityCooldown = true;
    lastRelayCommand = currentMillis;
    screen   = lastScreen;
  }
}

void manualWatering() {
  bool buttonPressed = (digitalRead(MANUAL_MODE_PIN) == LOW);

  if (buttonPressed && digitalRead(RELAY_PIN) == HIGH) {

    if (checkWaterLevel() && checkVoltage()) {
      digitalWrite(RELAY_PIN, LOW);
      watering = true;
      lastScreen = screen;
      screen = WATERING;
      lastRelayCommand = currentMillis;
    }
  } 
  else if (!buttonPressed && digitalRead(RELAY_PIN) == LOW) {
    digitalWrite(RELAY_PIN, HIGH);
    watering = false;
    screen   = lastScreen;
    lastRelayCommand = currentMillis;
  }

}

void timedWatering() {
  unsigned long currentInterval = (unsigned long)wateringIntervals[timedIndex] * MS_PER_DAY;

  if (!watering && (currentMillis - lastTimedWateringTime >= currentInterval)) {
    
    if (checkWaterLevel() && checkVoltage()) {
      digitalWrite(RELAY_PIN, LOW);

      lastScreen = screen;
      screen = WATERING;
      watering = true;
      wateringStart = currentMillis;
      lastRelayCommand = currentMillis;
    }
  }

  if (watering && (currentMillis - wateringStart >= WATERING_DURATION)) {
    digitalWrite(RELAY_PIN, HIGH);
    
    lastTimedWateringTime = currentMillis;
    watering = false;
    screen   = lastScreen;
    lastRelayCommand = currentMillis;
  }
}

// this function  uses the functions with timers (millis()) with forced flag = false 
// so if the user fills the tank with water the LED error to disappear in a few seconds instead
// of the next watering (that checks with forced flag)
void checkReturnFromError() {
  if (LEDMode == BLUE_FADE) {
    readWaterLevel();
    if (waterLevel > MIN_WATER_LEVEL) {
      LEDMode = OFF;
    }
  }

  if (LEDMode == RED_FADE) {
    readBatteryVoltage();
    if (voltage > MIN_BATTERY_VOLTAGE) {
      LEDMode = OFF;
    }
  }

}

void handleMode() {

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

void loop() {

  currentMillis = millis();
  
  displayHandler();

  readHumidity();

  handleMode();

  handleLEDSignal();

  if (!standby && (currentMillis - lastActivityTime >= standbyTime)) {
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
    lastActivityTime = currentMillis;
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

  if (changeSelectedValuesList) {
    changeSelectedValuesList = false;
    selectedValuesList = (selectedValuesList + 1) % 2;
  }

  if (changeSelectedValue) {
    changeSelectedValue = false;
    if (selectedValuesList == 0) {
      humidityIndex = (humidityIndex + 1) % NUM_LEVELS;
    } else {
      timedIndex = (timedIndex + 1) % NUM_LEVELS;
    }
  }

  if (changeScreen) {
    changeScreen = false;
    if (screen == MENU) {
        updateEEPROM();
      }

    if (screen == MENU) {
      screen = STANDARD;
    } else if (screen == STANDARD) {
      screen = MENU;
    }
  }

  // temperature not read when in standby
  readTemperature();
}