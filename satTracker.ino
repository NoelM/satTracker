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
#define MAXSERVO 2350
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

  attachInterrupt(0, changeMinAz, RISING);
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

    updateMountPosition();
  } 
}


int getAzPolarized() {
  int azDeg;
  /*switch(ctrlr.pol) {
    case 0: // North 270 -> 90
      azDeg = ctrlr.azDeg >= 270 ? ctrlr.azDeg - 270 : ctrlr.azDeg + 90; break;
    case 1: // East 0 -> 180
      azDeg = ctrlr.azDeg; break;
    case 2: // South 90 -> 270
      azDeg = ctrlr.azDeg - 90; break;
    case 3: // West 180 -> 360
      azDeg = ctrlr.azDeg - 180; break;
  }*/
  return min(max(azDeg,0),180);
}


void updateMountPosition() {
  int azDeg = getAzPolarized();
  if (abs(azDeg - mount.azDeg) > MINMOVE) {
    moveAzToDeg(azDeg);
  }
  int elDeg = 90 - ctrlr.elDeg;
  if (abs(elDeg - mount.elDeg) > MINMOVE) {
    moveElToDeg(elDeg);
  }
  printScreenPos();
}


int degToUs(int deg) {
  return int((MAXSERVO - MINSERVO)/180. * deg + MINSERVO);
}


void moveElToDeg(int deg) {
  mount.elDeg = deg;
  mount.elUs = degToUs(deg);
  elServo.writeMicroseconds(mount.elUs);
}


void moveAzToDeg(int deg) {
  mount.azDeg = deg;
  mount.azUs = degToUs(deg);
  azServo.writeMicroseconds(mount.azUs);
}


void printScreenPos() {
  display.fillRect(0, 0, SCREEN_WIDTH, 32, SSD1306_BLACK);
  display.display();

  char buf[80];
  sprintf(buf, ">>> MOUNT\nAZ  %3ddeg  %4dus\nEL  %3ddeg  %4dus\nMIN %3ddeg", mount.azDeg, mount.azUs, mount.elDeg, mount.elUs, ctrlr.minAz);
  display.setCursor(0, 0);
  display.println(buf);

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
