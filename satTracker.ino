#include <SPI.h>
#include <Wire.h>
#include <Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_DC     9
#define OLED_CS     10
#define OLED_RESET  8
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         &SPI, OLED_DC, OLED_RESET, OLED_CS);

#define MINSERVO 544
#define MAXSERVO 2300
#define MINMOVE 5

struct Mount {
  int azDeg, elDeg;
  int azUs, elUs;
};

struct Mount mount = {0, 0, 0, 0};

struct Controller {
  int azDeg, elDeg, minAz;
  bool polDirect;
};

struct Controller ctrlr = {90, 0, 0, true};

static const unsigned char PROGMEM arrow_direct[] = {
  B00111000,
  B01101100,
  B11000100,
  B10011111,
  B11001110,
  B01100100,
  B00110000,
};

static const unsigned char PROGMEM arrow_undirect[] = {
  B00110000,
  B01100100,
  B11001110,
  B10011111,
  B11000100,
  B01101100,
  B00111000,
};

static const unsigned char* arrows[] = {arrow_undirect, arrow_direct};

Servo azServo;
Servo elServo;

void setup() { 
  Serial.begin(19200);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); // Don't proceed, loop forever
  }

  // Clear the buffer
  display.clearDisplay();

  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);

  azServo.attach(6);
  elServo.attach(5);

  attachInterrupt(0, changeMinAz, FALLING);
  attachInterrupt(1, changePolDirect, FALLING);
}


void loop() {
  listenAndAct();
}

void listenAndAct() {
  if (Serial.available() > 0) {
    // read the incoming byte:
    String msg = Serial.readString();

    char buf[40];
    msg.toCharArray(buf, 40);

    if (msg.startsWith("POL")) {
      int minAz;
      bool polDirect;
      sscanf(buf, "POL%d MIN%d", &polDirect, &minAz);
      ctrlr.polDirect = polDirect;
      ctrlr.minAz = minAz;
    }

    if (msg.startsWith("AZ")) {
      /* 
        https://github.com/Hamlib/Hamlib/blob/master/rotators/easycomm/easycomm.txt
  
        EASYCOMM I Standard
        -------------------
        AZaaa.a ELeee.e UPuuuuuuuuu UUU DNddddddddd DDD
      */
      int upFreq, dnFreq;
      int azDeg, elDeg, dec;
      char upMode[3], dnMode[3];
      sscanf(buf, "AZ%d.%d EL%d.%d UP%d %s DN%d %s", &azDeg, &dec, &elDeg, &dec, &upFreq, upMode, &dnFreq, dnMode);
      printSerial(msg, azDeg, elDeg);
          
      ctrlr.azDeg = azDeg;
      ctrlr.elDeg = elDeg;
    }

    updateMountPosition();
  } 
}


int getAzPolarized() {
  int azDeg;
  if (ctrlr.polDirect) { // pass in direct (0 -> 90)
    if (ctrlr.minAz > 180 && ctrlr.azDeg < 180) {
      azDeg = 360 - ctrlr.minAz + ctrlr.azDeg;
    } else {
      azDeg = ctrlr.azDeg - ctrlr.minAz;
    }
  } else { // pass in indirect (90 -> 0)
    if (ctrlr.minAz < 180 && ctrlr.azDeg > 180) {
      azDeg = ctrlr.azDeg - ctrlr.minAz - 180;
    } else {
      azDeg = 180 - (ctrlr.minAz - ctrlr.azDeg);
    }
  }
  return min(max(azDeg,0),180);
}


void updateMountPosition() {
  int azDeg = 180 - getAzPolarized();
  moveAzToDeg(azDeg);
  int elDeg = 90 - ctrlr.elDeg;
  moveElToDeg(elDeg);
  printScreenPos();
}


int degToUs(int deg) {
  return int((MAXSERVO - MINSERVO)/180. * deg + MINSERVO);
}


void moveElToDeg(int deg) {
  int startUs = degToUs(mount.elDeg);
  int stopUs = degToUs(deg);
  if (stopUs - startUs > 0) {
    for (int t = startUs; t <= stopUs; t +=10) {
      elServo.writeMicroseconds(t);
      delay(20);
    }
  } else {
    for (int t = startUs; t >= stopUs; t -=10) {
      elServo.writeMicroseconds(t);
      delay(20);
    }
  }
  mount.elDeg = deg;
  mount.elUs = stopUs;
}


void moveAzToDeg(int deg) {
  int startUs = degToUs(mount.azDeg);
  int stopUs = degToUs(deg);
  if (stopUs - startUs > 0) {
    for (int t = startUs; t <= stopUs; t +=10) {
      azServo.writeMicroseconds(t);
      delay(20);
    }
  } else {
    for (int t = startUs; t >= stopUs; t -=10) {
      azServo.writeMicroseconds(t);
      delay(20);
    }
  }
  mount.azDeg = deg;
  mount.azUs = stopUs;
}


void printScreenPos() {
  display.fillRect(0, 0, SCREEN_WIDTH, 32, SSD1306_BLACK);
  display.display();

  char buf[80];
  sprintf(buf, ">>> MOUNT\nAZ  %3ddeg  %4dus\nEL  %3ddeg  %4dus\nMIN %3ddeg", mount.azDeg, mount.azUs, mount.elDeg, mount.elUs, ctrlr.minAz);
  display.setCursor(0, 0);
  display.println(buf);

  display.drawBitmap(
    71, // 12 chars * 6 pixels - 1
    25, // 3 lines * 8 pixels - 1
    arrows[ctrlr.polDirect], 8, 7, 1);

  display.display();
}



void printSerial(String msg, int azDeg, int elDeg) {
  display.fillRect(0, 36, SCREEN_WIDTH, 28, SSD1306_BLACK);
  display.display();

  char buf[20];
  sprintf(buf, ">>> EASYCOMM %d %d", azDeg, elDeg);
  display.setCursor(0, 36);
  display.println(buf);

  display.setCursor(0, 45);
  display.println(msg);

  display.display();
}


void changeMinAz() {
  if (ctrlr.minAz < 350) {
    ctrlr.minAz += 10;
  } else {
    ctrlr.minAz = 0;
  }
  printScreenPos();
  updateMountPosition();
}

void changePolDirect() {
  ctrlr.polDirect = !ctrlr.polDirect;
  printScreenPos();
  updateMountPosition();
}
