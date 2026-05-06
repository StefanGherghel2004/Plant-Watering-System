#include <Adafruit_GFX.h> // Include core graphics library for the display
#include <Adafruit_SSD1306.h> // Include Adafruit_SSD1306 library to drive the display

Adafruit_SSD1306 display(128, 64); // Create display

#define CHANGE_MODE_PIN 2
#define WAKE_PIN        3
#define TEMP_PIN        A0
#define HUMIDITY_PIN    A2
#define BATTERY_PIN     A3

float temperature = 0;
float humidity    = 0;
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

enum FunctionMode {
  AUTOMAT,
  MANUAL,
  TEMPORIZAT
};

FunctionMode mode = AUTOMAT;

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

  delay(100); // This delay is needed to let the display to initialize
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Initialize display with the I2C address of 0x3C
  display.clearDisplay(); // Clear the buffer
  display.setTextColor(WHITE); // Set color of the text

  display.setRotation(0); // Set orientation. Goes from 0, 1, 2 or 3
  display.setTextWrap(false); // By default, long lines of text are set to automatically ΓÇ£wrapΓÇ¥ back to the leftmost column.

  // To override this behavior (so text will run off the right side of the display - useful for
  // scrolling marquee effects), use setTextWrap(false). The normal wrapping behavior is restored
  // with setTextWrap(true).

  display.dim(1); //Set brightness (0 is maximun and 1 is a little dim)

  pinMode(CHANGE_MODE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CHANGE_MODE_PIN), handleChangeModeButton, FALLING);

  pinMode(WAKE_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), handleWakeButton, FALLING);
}


void displayHandler() {
  
  // Convert Variable1 into a string, so we can change the text alignment to the right:
  // It can be also used to add or remove decimal numbers.
  char string[10]; // Create a character array of 10 characters

  float number = 10.0;

  // Convert float to a string:
  dtostrf(temperature, 4, 2, string); // (<variable>,<amount of digits we are going to use>,<amount of decimal digits>,<string name>)
  display.clearDisplay(); // Clear the display so we can refresh

  display.setCursor(0, 8);
  display.print("Mode: ");

  switch(mode) {
    case AUTOMAT:
      display.print("Automat");
      break;
    case MANUAL:
      display.print("Manual");
      break;
    case TEMPORIZAT:
      display.print("Temporizat");
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

  display.display(); // print everything set
}

void loop() {

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

  if (millis() - lastReadTimeTemp >= intervalTemp) {
    lastReadTimeTemp = millis();
    temperature = analogRead(TEMP_PIN) * 0.488;
  }

  if (millis() - lastReadTimeBattery >= intervalBattery) {
    lastReadTimeBattery = millis();
    voltage = analogRead(BATTERY_PIN) * (5.0 / 1023.0) * 2;
  }

  // humidity LOW => value HIGH
  if (millis() - lastReadTimeHumidity >= intervalHumidity) {
    lastReadTimeHumidity = millis();
    humidity = analogRead(HUMIDITY_PIN) * (5.0 / 1023.0);
  }

  displayHandler();
}