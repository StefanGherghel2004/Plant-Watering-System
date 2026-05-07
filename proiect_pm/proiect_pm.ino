#include <Adafruit_GFX.h> // core graphics library
#include <Adafruit_SSD1306.h> // library to drive the display

Adafruit_SSD1306 display(128, 64); // create display

#define LED_RED_PIN     11
#define LED_GREEN_PIN   10
#define LED_BLUE_PIN    9

#define CHANGE_MODE_PIN 2
#define WAKE_PIN        3
#define TEMP_PIN        A0
#define HUMIDITY_PIN    A2
#define BATTERY_PIN     A3

#define DRY_VOLTAGE     3.0
#define WET_VOLTAGE     1.4

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

FunctionMode mode = HUMIDITY;

void handleChangeModeButton() {
  if (!standby) {
    changeMode = true;
    lastActivityTime = millis();
  }
}

void handleWakeButton() {
  if (standby) {
    wakeFlag = true;
  }
  standby = false;
  lastActivityTime = millis();
}

void setup() {

  Serial.begin(9600);

  delay(100); // delay needed to let display initiate
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // initialize display with the I2C address of 0x3C
  display.clearDisplay(); // clear buffer
  display.setTextColor(WHITE);

  display.setRotation(0); // set orientation
  display.setTextWrap(false);

  display.dim(1); // set brightness (0 is maximun and 1 is a little dim)

  pinMode(CHANGE_MODE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CHANGE_MODE_PIN), handleChangeModeButton, FALLING);

  pinMode(WAKE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), handleWakeButton, FALLING);

  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);

  // led off
  analogWrite(LED_RED_PIN,  255);
  analogWrite(LED_GREEN_PIN,255);
  analogWrite(LED_BLUE_PIN, 255);

}


void displayHandler() {
  
  // Convert Variable1 into a string, so we can change the text alignment to the right:
  // It can be also used to add or remove decimal numbers.
  char string[10];

  // convert float to a string
  dtostrf(temperature, 4, 2, string); // (<variable>,<amount of digits we are going to use>,<amount of decimal digits>,<string name>)
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

    Serial.print("Voltage:");
    Serial.println(sensorVoltage);

    humidity = (DRY_VOLTAGE - sensorVoltage) * 100.0 / (DRY_VOLTAGE - WET_VOLTAGE);

    if (humidity > 100) {
      humidity = 100;
    }
    if (humidity < 0) {
      humidity = 0;
    }
  }
}

void humidityWatering() {

}

void manualWatering() {

}

void timedWatering() {
  
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

  readHumidity();

  readBatteryVoltage();

  handleLEDSignal();

  handleMode();

  if (!standby && (millis() - lastActivityTime >= standbyTime)) {
    standby = true;
  }

  if (wakeFlag) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    wakeFlag = false;
  }

  if (standby) {
    display.clearDisplay();   // clear buffer
    display.display();        // empty screen
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    return;
  }

  if (changeMode) {
    changeMode = false;
    mode = (FunctionMode)((mode + 1) % 3);
  }

  // temperature not read when in standby
  readTemperature();

  displayHandler();
}