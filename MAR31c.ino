#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <Wire.h>
#include "RTClib.h"
#include <LCD5110_Graph.h>
#include <Encoder.h>
#include <EEPROM.h>

/*
   EEPROM datasheet
   0 - volume
   1 - showMP3UI
   2 - lcdbacklit
*/

SoftwareSerial mp3(4, 5); // RX, TX
Encoder Enc(2, 3);
LCD5110 myGLCD(8, 9, 10, 11, 12);
RTC_DS3231 rtc;
DFRobotDFPlayerMini DFPlayer;
DateTime now, alarm;

extern unsigned char SmallFont[];
extern uint8_t MediumNumbers[];
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String rtcStatus, mp3Status, temp;
bool rtcerror = false;
bool mp3error = false;
bool flag = false;
bool alarmon = false;
bool mp3paused = false;
bool showmp3UI = EEPROM.read(1);
bool lcdbacklit = EEPROM.read(2);
unsigned long int select, encoffset, secondinterrupt;
String options[6];
int pos;

void setup() {
  pinMode(13, OUTPUT);
  Serial.begin(9600);
  myGLCD.InitLCD();
  myGLCD.clrScr();
  myGLCD.setFont(SmallFont);
  myGLCD.drawRect(0, 0, 83, 47);
  for (int i = 0; i < 48; i += 4) {
    myGLCD.drawLine(0, i, i * 1.75, 47);
  }
  for (int i = 0; i < 48; i += 4) {
    myGLCD.drawLine(83, 47 - i, 83 - (i * 1.75), 0);
    myGLCD.update();
  }
  myGLCD.print(F("BOX OS"), CENTER, 15);
  myGLCD.print(F("BOOTING"), CENTER, 23);
  myGLCD.print(F("mar31c 0822"), CENTER, 31);
  myGLCD.print(F("BETA BUILD"), CENTER, 39);
  myGLCD.update();
  mp3.begin(9600);
  Serial.println(F("init"));
  if (! rtc.begin()) {
    rtcStatus = F("rtc init failed");
    rtcerror = true;
  } else if (rtc.lostPower()) {
    rtcStatus = F("rtc lost power");
    rtcerror = true;
  }
  Serial.println(F("complete"));
  if (!DFPlayer.begin(mp3)) {  //Use softwareSerial to communicate with mp3.
    mp3Status = F("mp3 init failed");
    mp3error = true;
  } else {
    DFPlayer.reset();
    delay(1000);
    DFPlayer.volume(EEPROM.read(0));
    DFPlayer.EQ(DFPLAYER_EQ_NORMAL);
    DFPlayer.setTimeOut(500);
    DFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  }
  myGLCD.clrScr();
  myGLCD.update();
  Serial.println(rtcerror);
  if (rtcerror) {
    while (clicked(false)) {
      for (int i = 84; i > -84; i--) {
        myGLCD.clrScr();
        myGLCD.print(rtcStatus, i, 0);
        myGLCD.update();
        delay(25);
        if (clicked(true)) {
          break;
        }
      }
    }
    while (debounce());
  }

  if (mp3error) {
    while (clicked(false)) {
      for (int i = 84; i > -84; i--) {
        myGLCD.clrScr();
        myGLCD.print(mp3Status, i, 0);
        myGLCD.update();
        delay(25);
        if (clicked(true)) {
          break;
        }
      }
    }
    while (debounce());
  }
  digitalWrite(13, EEPROM.read(2));
}

void loop() {
  pos = Enc.read() - encoffset;
  now = rtc.now();
  myGLCD.clrScr();
  if (round(now.second() / 10) == 1 || round(now.second() / 10) == 3 || round(now.second() / 10) == 5 || !alarmon) {
    myGLCD.print(String(now.year()) + '/' + String(now.month()) + '/' + String(now.day()), CENTER, 0);
  } else {
    if (alarm.hour() < 10) {
      temp = '0' + String(alarm.hour());
    } else {
      temp = String(alarm.hour());
    }

    if (alarm.minute() < 10) {
      temp = temp + ':' + '0' + String(alarm.minute());
    } else {
      temp = temp + ':' + String(alarm.minute());
    }

    if (alarm.second() < 10) {
      temp = temp + ':' + '0' + String(alarm.second() < 10);
    } else {
      temp = temp + ':' + String(alarm.second() < 10);
    }
    myGLCD.print(temp, CENTER, 0);
  }
  myGLCD.setFont(MediumNumbers);
  if (now.hour() < 10) {
    myGLCD.print('0' + String(now.hour()), 0, 8);
  } else {
    myGLCD.print(String(now.hour()), 0, 8);
  }
  if (now.minute() < 10) {
    myGLCD.print('0' + String(now.minute()), CENTER, 8);
  } else {
    myGLCD.print(String(now.minute()), CENTER, 8);
  }

  if (now.second() < 10) {
    myGLCD.print('0' + String(now.second()), 60, 8);
  } else {
    myGLCD.print(String(now.second()), 60, 8);
  }
  myGLCD.setFont(SmallFont);

  if (showmp3UI) {
    mp3UI();
  } else if (clicked(true)) {
    menutree();
  }

  if (alarm.year() == now.year()) {
    if (alarm.month() == now.month()) {
      if (alarm.day() == now.day()) {
        if (alarm.hour() == now.hour()) {
          if (alarm.minute() == now.minute()) {
            if (alarm.second() == now.second()) {
              if (alarmon) {
                digitalWrite(13, HIGH);
                while (clicked(false)) {
                  myGLCD.clrScr();
                  myGLCD.print(F("ALARM"), CENTER, 15);
                  myGLCD.update();
                  myGLCD.invert(true);
                  delay(1000);
                  myGLCD.invert(false);
                  delay(1000);
                }
                myGLCD.invert(false);
                digitalWrite(13, LOW);
                while (debounce());
              }
            }
          }
        }
      }
    }
  }

  if (0 == now.hour()) {
    if (0 == now.minute()) {
      if (0 == now.second()) {
        alarm = DateTime(now.year(), now.month(), now.day(), alarm.hour(), alarm.minute(), alarm.second());
      }
    }
  }

  myGLCD.update();
}



bool clicked(bool inverter) {
  bool state;
  if (analogRead(A1) > 100) {
    state = true;
  } else {
    state = false;
  }
  if (inverter) {
    state = !state;
  }
  return state;
}

void menu(int optnum, String title, String opts[]) {

  encoffset = Enc.read() - 1;
  while (clicked(false)) {
    myGLCD.clrScr();
    if (int(Enc.read() - encoffset) < 0) { //underflow preventer
      encoffset = Enc.read();
    }
    if (int(Enc.read() - encoffset) > optnum) {
      encoffset = Enc.read() - optnum;
    }

    for (int i = 0; i < optnum; i++) { //print options
      myGLCD.print(opts[i], 6, i * 8 + 11);
    }
    if (Enc.read() - encoffset == 0) { //return
      myGLCD.drawLine(0, 0, 84, 0);
      myGLCD.invertText(true);
      myGLCD.print(F("     HOME     "), CENTER, 1);
      myGLCD.print(F("*"), 0, 1);
      myGLCD.invertText(false);
    } else { //norm
      myGLCD.print(title, CENTER, 1);
      myGLCD.drawLine(0, 9, 84, 9);//UI
      myGLCD.print(">", 0, (Enc.read() - encoffset) * 8 + 3);
    }
    myGLCD.update();
  }
  while (debounce());
  select = Enc.read() - encoffset;
  myGLCD.clrScr();

}

int selnum(String title, int mini, int maxi, int defaultint) {
  encoffset = Enc.read() - 1 - defaultint + mini;//reset
  while (clicked(false)) {
    myGLCD.clrScr();
    if (int(Enc.read() - 1 - encoffset + mini) < mini) { //breach minimum
      myGLCD.invertText(true);
      myGLCD.print("    RETURN    ", CENTER, 1);
      myGLCD.invertText(false);
      myGLCD.drawLine(0, 0, 84, 0);
      myGLCD.setFont(MediumNumbers);
      myGLCD.print(String(mini), CENTER, 15);
      myGLCD.setFont(SmallFont);
      if (int(Enc.read() - 1 - encoffset + mini) < mini - 1) {
        encoffset = Enc.read();
      }
    } else if (int(Enc.read() - 1 - encoffset + mini) > maxi) {//breach maximum
      myGLCD.print(title, CENTER, 1);
      encoffset = Enc.read() - 1 + mini - maxi;
      myGLCD.setFont(MediumNumbers);
      myGLCD.print(maxi, CENTER, 15);
      myGLCD.setFont(SmallFont);
    } else {//norm
      myGLCD.setFont(MediumNumbers);
      myGLCD.print(String(Enc.read() - 1 - encoffset + mini), CENTER, 15);
      myGLCD.setFont(SmallFont);
      myGLCD.print(title, CENTER, 1);
    }
    myGLCD.drawLine(0, 9, 84, 9);//UI
    myGLCD.update();
  }

  while (debounce());

  if (int(Enc.read() - 1 - encoffset + mini) < mini) {
    return defaultint;//default
  }
  if (int(Enc.read() - 1 - encoffset + mini) > maxi) {
    return maxi;//max overflow
  } else {
    return Enc.read() - 1 - encoffset + mini;//norm
  }
}

bool debounce() {
  unsigned long int i = millis();
  while (millis() - i < 50) {
    if (clicked(true)) {
      i = millis();
    }
  }
  return false;
}

void menutree() {
  while (debounce());
  options[0] = F("Clock");
  options[1] = F("Preferences");
  options[2] = F("");
  menu(3, "Main Menu", options);
  if (select == 1) { //clock
    options[0] = "Alarm";
    options[1] = "Settings";
    menu(2, "Clock", options);
    if (select == 1) { //alarm

      options[0] = F("Set alarm");
      if (alarmon) {
        options[1] = F("Turn off");
      } else {
        options[1] = F("Turn on");
      }
      menu(2, F("Alarm"), options);
      if (select == 1) { //set alarm
        alarm = DateTime(selnum(F("Year"), 2000, 2100, alarm.year()), selnum(F("Month"), 1, 12, alarm.month()), selnum(F("Day"), 1, 31, alarm.day()), selnum(F("Hour"), 0, 23, alarm.hour()), selnum(F("Minute"), 0, 59, alarm.minute()), selnum(F("Second"), 0, 59, alarm.second()));
        alarmon = true;
      } else if (select == 2) { //toggle
        alarmon = !alarmon;
      }
    } else if (select == 2) { //settings
      options[0] = F("Set RTC time");
      menu(1, F("RTC Settings"), options);
      if (select == 1) {
        rtc.adjust(DateTime(selnum(F("Year"), 2000, 2100, now.year()), selnum(F("Month"), 1, 12, now.month()), selnum(F("Day"), 1, 31, now.day()), selnum(F("Hour"), 0, 23, now.hour()), selnum(F("Minute"), 0, 59, now.minute()), selnum(F("Second"), 0, 59, now.second())));
      }
    }
  } else if (select == 2) { //prefs
    options[0] = F("analogReadA1");
    options[1] = F("Backlight");
    menu(2, F("Preferences"), options);
    if (select == 1) {
      while (true) {
        myGLCD.clrScr();
        myGLCD.print(String(analogRead(A1)), CENTER, 15);
        myGLCD.update();
        delay(100);
        if (clicked(true)) {
          if (millis() - select > 2000) {
            select = 0;
            break;
          }
        } else {
          select = millis();
        }
      }
    } else if (select == 2) {
      if (lcdbacklit) {
        digitalWrite(13, LOW);
      } else {
        digitalWrite(13, HIGH);
      }
      lcdbacklit = !lcdbacklit;
      EEPROM.write(2, !EEPROM.read(2));
    }
  } else if (select == 3) { //mp3 player
    options[0] = F("MP3 UI");
    options[1] = F("Playlist");
    options[2] = F("Remove SD");
    menu(3, "MP3 Player", options);
    if (select == 1) {
      if (showmp3UI) {
        options[0] = F("Turn off");
      } else {
        options[0] = F("Turn on");
      }
      menu(1, F("MP3 UI"), options);
      if (select == 1) {
        showmp3UI = !showmp3UI;
        EEPROM.write(1, !EEPROM.read(1));
      }
    } else if (select == 2) {
      options[0] = F("Music");
      options[1] = F("Literature");
      options[2] = F("Others");
      menu(3, F("Playlist"), options);
      if (select == 1) {
        options[0] = F("English");
        options[1] = F("Mandarin");
        menu(2, F("Music"), options);
        if (select == 1) {
          DFPlayer.loopFolder(1);
        } else if (select == 2) {
          DFPlayer.loopFolder(2);
        }
      } else if (select == 2) {
        options[0] = F("Series");
        options[1] = F("Casual");
        options[2] = F("Musicals");
        options[3] = F("Explicit");
        menu(4, F("Literature"), options);
        if (select == 1) {
          DFPlayer.loopFolder(3);
        } else if (select == 2) {
          DFPlayer.loopFolder(4);
        } else if (select == 3) {
          DFPlayer.loopFolder(5);
        } else if (select == 4) {
          DFPlayer.loopFolder(6);
        }
      }

    } else if (select == 3) {
      myGLCD.clrScr();
      myGLCD.print(F("Message"), CENTER, 0);
      myGLCD.drawLine(0, 8, 84, 8);
      myGLCD.print(F("You can now"), CENTER, 9);
      myGLCD.print(F("safely remove"), CENTER, 17);
      myGLCD.print(F("the SD Card"), CENTER, 25);
      myGLCD.print(F("Click when done"), CENTER, 41);
      myGLCD.update();
      while (clicked(false));
      while (debounce());
      mp3error = true;
    } else {
      //home
    }
  }
}

void mp3UI() {
  myGLCD.invPixel(9, 27);//menu icon
  myGLCD.drawLine(11, 27, 16, 27);
  myGLCD.invPixel(9, 29);
  myGLCD.drawLine(11, 29, 16, 29);
  myGLCD.invPixel(9, 31);
  myGLCD.drawLine(11, 31, 16, 31);
  myGLCD.invPixel(9, 33);
  myGLCD.drawLine(11, 33, 16, 33);

  myGLCD.invPixel(21, 27);
  myGLCD.invPixel(22, 27);
  myGLCD.invPixel(23, 27);
  myGLCD.invPixel(20, 28);
  myGLCD.invPixel(24, 28);
  myGLCD.invPixel(20, 29);
  myGLCD.invPixel(24, 29);
  myGLCD.invPixel(20, 30);
  myGLCD.invPixel(22, 30);
  myGLCD.invPixel(24, 30);
  myGLCD.invPixel(20, 31);
  myGLCD.invPixel(22, 31);
  myGLCD.invPixel(24, 31);
  myGLCD.invPixel(21, 32);
  myGLCD.invPixel(22, 32);
  myGLCD.invPixel(23, 32);
  myGLCD.invPixel(21, 33);
  myGLCD.invPixel(22, 33);
  myGLCD.invPixel(23, 33);

  myGLCD.invPixel(29, 28);//prev button
  myGLCD.invPixel(32, 28);
  myGLCD.invPixel(35, 28);
  myGLCD.invPixel(29, 29);
  myGLCD.invPixel(31, 29);
  myGLCD.invPixel(32, 29);
  myGLCD.invPixel(34, 29);
  myGLCD.invPixel(35, 29);
  myGLCD.invPixel(29, 30);
  myGLCD.invPixel(30, 30);
  myGLCD.invPixel(31, 30);
  myGLCD.invPixel(32, 30);
  myGLCD.invPixel(33, 30);
  myGLCD.invPixel(34, 30);
  myGLCD.invPixel(35, 30);
  myGLCD.invPixel(29, 31);
  myGLCD.invPixel(31, 31);
  myGLCD.invPixel(32, 31);
  myGLCD.invPixel(34, 31);
  myGLCD.invPixel(35, 31);
  myGLCD.invPixel(29, 32);
  myGLCD.invPixel(32, 32);
  myGLCD.invPixel(35, 32);

  if (mp3paused) {
    myGLCD.invPixel(39, 27);//play button
    myGLCD.invPixel(39, 28);
    myGLCD.invPixel(40, 28);
    myGLCD.invPixel(41, 28);
    myGLCD.invPixel(39, 29);
    myGLCD.invPixel(40, 29);
    myGLCD.invPixel(41, 29);
    myGLCD.invPixel(42, 29);
    myGLCD.invPixel(43, 29);
    myGLCD.invPixel(39, 30);
    myGLCD.invPixel(40, 30);
    myGLCD.invPixel(41, 30);
    myGLCD.invPixel(42, 30);
    myGLCD.invPixel(43, 30);
    myGLCD.invPixel(44, 30);
    myGLCD.invPixel(45, 30);
    myGLCD.invPixel(39, 31);
    myGLCD.invPixel(40, 31);
    myGLCD.invPixel(41, 31);
    myGLCD.invPixel(42, 31);
    myGLCD.invPixel(43, 31);
    myGLCD.invPixel(39, 32);
    myGLCD.invPixel(40, 32);
    myGLCD.invPixel(41, 32);
    myGLCD.invPixel(39, 33);

  } else {
    myGLCD.drawLine(40, 27, 40, 34);//pause icon
    myGLCD.drawLine(44, 27, 44, 34);
  }

  myGLCD.invPixel(49, 28);//next icon
  myGLCD.invPixel(52, 28);
  myGLCD.invPixel(55, 28);
  myGLCD.invPixel(49, 29);
  myGLCD.invPixel(50, 29);
  myGLCD.invPixel(52, 29);
  myGLCD.invPixel(53, 29);
  myGLCD.invPixel(55, 29);
  myGLCD.invPixel(49, 30);
  myGLCD.invPixel(50, 30);
  myGLCD.invPixel(51, 30);
  myGLCD.invPixel(52, 30);
  myGLCD.invPixel(53, 30);
  myGLCD.invPixel(54, 30);
  myGLCD.invPixel(55, 30);
  myGLCD.invPixel(49, 31);
  myGLCD.invPixel(50, 31);
  myGLCD.invPixel(52, 31);
  myGLCD.invPixel(53, 31);
  myGLCD.invPixel(55, 31);
  myGLCD.invPixel(49, 32);
  myGLCD.invPixel(52, 32);
  myGLCD.invPixel(55, 32);

  myGLCD.invPixel(59, 30);//volume down icon
  myGLCD.invPixel(60, 30);
  myGLCD.invPixel(61, 30);
  myGLCD.invPixel(62, 30);
  myGLCD.invPixel(63, 30);
  myGLCD.invPixel(64, 30);
  myGLCD.invPixel(65, 30);

  myGLCD.invPixel(72, 27);//volume up icon
  myGLCD.invPixel(72, 28);
  myGLCD.invPixel(72, 29);
  myGLCD.invPixel(69, 30);
  myGLCD.invPixel(70, 30);
  myGLCD.invPixel(71, 30);
  myGLCD.invPixel(72, 30);
  myGLCD.invPixel(73, 30);
  myGLCD.invPixel(74, 30);
  myGLCD.invPixel(75, 30);
  myGLCD.invPixel(72, 31);
  myGLCD.invPixel(72, 32);
  myGLCD.invPixel(72, 33);


  //myGLCD.drawRect(0, 25, 83, 47);

  if (pos * 10 + 7 < 0) { //underflow preventer
    encoffset = Enc.read();
  } else if (pos * 10 + 7 > 67) { //overflow preventer
    encoffset = Enc.read() - 6;
  }

  myGLCD.drawRoundRect(pos * 10 + 7, 25, pos * 10 + 17, 35);//selection box

  if (analogRead(A2) < 500) {
    mp3paused = false;
  } else {
    mp3paused = true;
  }



  if (clicked(true)) {
    while (debounce());
    if (pos == 0) {
      menutree();
    } else if (pos == 1) {
      if (lcdbacklit) {
        digitalWrite(13, LOW);
      } else {
        digitalWrite(13, HIGH);
      }
      lcdbacklit = !lcdbacklit;
      EEPROM.write(2, !EEPROM.read(2));
    } else if (pos == 2) {
      DFPlayer.previous();
    } else if (pos == 3) {
      if (mp3paused) {
        mp3paused = false;
        DFPlayer.start();
      } else {
        mp3paused = true;
        DFPlayer.pause();
      }
    } else if (pos == 4) {
      DFPlayer.next();
    } else if (pos == 5) {
      DFPlayer.volumeDown();
      EEPROM.write(0, EEPROM.read(0) - 1);
    } else if (pos == 6) {
      DFPlayer.volumeUp();
      EEPROM.write(0, EEPROM.read(0) + 1);
    }
    while (debounce());
  }
}



