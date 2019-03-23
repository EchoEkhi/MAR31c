#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <Wire.h>
#include "RTClib.h"
#include <LCD5110_Graph.h>
#include <Encoder.h>
#include <EEPROM.h>
#define passcode "0118999881999119725"
#include <TimerOne.h>

/*
   EEPROM datasheet
   0 - volume
   1 - showMP3UI
   2 - lcdbacklit
   3 - alarm.hour()
   4 - alarm.minute()
   5 - alarm.second()
   6 - alarmon
   7 - lockdown
   8 - alarmvolume //set to 0 to do automatic volume climb
   9 - alarmfolder
   10 - alarmsongnumber
   11 - mp3EQ
*/

SoftwareSerial mp3(4, 5); // RX, TX
Encoder Enc(2, 3);
LCD5110 myGLCD(8, 9, 10, 11, 12);
RTC_DS3231 rtc;
DFRobotDFPlayerMini DFPlayer;
DateTime now, alarm;

extern unsigned char SmallFont[];
extern uint8_t MediumNumbers[];
String rtcStatus, mp3Status, temp;
bool rtcerror = false;
bool mp3error = false;
bool flag = false;
bool whitenoisemode = false;
bool alarmon = EEPROM.read(6);
bool mp3paused = false;
bool showmp3UI = EEPROM.read(1);
bool lcdbacklit = EEPROM.read(2);
bool mp3loop = false;
bool blinkstate = false;
unsigned long int select, encoffset, secint;
String options[10];

void setup() {
  pinMode(13, OUTPUT);
  pinMode(6, OUTPUT);
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
  myGLCD.print(F("MAR31c"), CENTER, 15);
  myGLCD.print(F("BOOTING"), CENTER, 23);
  myGLCD.print(F("mar31c 1.0"), CENTER, 31);
  myGLCD.print(F("OMEGA RELEASE"), CENTER, 39);
  myGLCD.update();
  Timer1.initialize(2000000);
  Timer1.attachInterrupt(blinkpwr);
  interrupts();
  mp3.begin(9600);
  if (! rtc.begin()) {
    rtcStatus = F("rtc init failed");
    rtcerror = true;
  } else if (rtc.lostPower()) {
    rtcStatus = F("rtc lost power");
    rtcerror = true;
  }
  if (!DFPlayer.begin(mp3)) {  //Use softwareSerial to communicate with mp3.
    mp3Status = F("mp3 init failed");
    mp3error = true;
  } else {
    if (EEPROM.read(0) > 30) {
      EEPROM.write(0, 10);
    }
    DFPlayer.volume(EEPROM.read(0));
    DFPlayer.EQ(DFPLAYER_EQ_NORMAL);
    DFPlayer.setTimeOut(500);
    DFPlayer.outputDevice(EEPROM.read(11));
  }
  myGLCD.clrScr();
  myGLCD.update();
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
  alarm = DateTime(2000, 1, 1, EEPROM.read(3), EEPROM.read(4), EEPROM.read(5));
  if (EEPROM.read(7)) {
    myGLCD.clrScr();
    myGLCD.print(F("The MAR31c is"), CENTER, 8);
    myGLCD.print(F("under lockdown"), CENTER, 16);
    myGLCD.print(F("enter passcode"), CENTER, 24);
    myGLCD.print(F("to activate"), CENTER, 32);
    myGLCD.update();
    while (clicked(false));
    while (debounce());
    while (true) {
      temp = F("");
      encoffset = Enc.read();
      while (true) {
        myGLCD.clrScr();
        myGLCD.print(F("ENTER PASSCODE"), CENTER, 0);
        myGLCD.drawLine(0, 8, 84, 8);
        ouflowpreventer(11, 0, 0, 9);
        for (int i = 0; i < 10; i++) {
          myGLCD.invertText(enc() == i);
          myGLCD.print(String(char(i + 48)), (i + 1) * 6, 10);
          myGLCD.invertText(enc() == 10);
          myGLCD.print(F("Undo"), 6, 18);
          myGLCD.invertText(false);
          myGLCD.invertText(enc() == 11);
          myGLCD.print(F("Confirm"), 36, 18);
          myGLCD.invertText(false);
        }
        if (clicked(true)) {
          while (debounce());
          if (enc() < 10) {
            temp = temp + String(char(enc() + 48));
          } else if (enc() == 10) {
            temp.remove(temp.length() - 1);
          } else if (enc() == 11) {
            break;
          }
        }
        for (int i = 0; i < 14; i++) {
          myGLCD.print(String(temp.charAt(i)), i * 6, 26);
        }
        for (int i = 0; i < 14; i++) {
          myGLCD.print(String(temp.charAt(i + 14)), i * 6, 34);
        }
        myGLCD.update();
      }
      if (temp == passcode) {
        EEPROM.write(7, false);
        EEPROM.write(1, false);
        delay(500);
        break;
      }
    }
  }
  encoffset = Enc.read();
}

void loop() {
  myGLCD.clrScr();
  if (millis() - secint > 250) {
    secint = millis();
    now = rtc.now();
  }

  if (!mp3paused && analogRead(A2) > 500 && mp3loop) {
    DFPlayer.start();
  }

  mp3paused = analogRead(A2) > 500;
  if (round(now.second() / 10) == 1 || round(now.second() / 10) == 3 || round(now.second() / 10) == 5 || !alarmon) {
    myGLCD.print(String(now.year()) + '/' + String(now.month()) + '/' + String(now.day()), CENTER, 0);
  } else {
    temp = zeroadder(alarm.hour());
    temp += ':' + zeroadder(alarm.minute());
    temp += ':' + zeroadder(alarm.second());
    myGLCD.print(temp, CENTER, 0);
  }
  myGLCD.setFont(MediumNumbers);
  myGLCD.print(zeroadder(now.hour()), 0, 8);
  myGLCD.print(zeroadder(now.minute()), CENTER, 8);
  myGLCD.print(zeroadder(now.second()), 60, 8);
  myGLCD.setFont(SmallFont);

  if (showmp3UI) {
    mp3UI();
  } else if (clicked(true)) {
    menutree();
  }

  if (alarm.hour() == now.hour()) {
    if (alarm.minute() == now.minute()) {
      if (alarm.second() == now.second()) {
        if (alarmon) {
          int vol = 0;
          unsigned long int timer = millis();
          digitalWrite(13, HIGH);
          DFPlayer.volume(EEPROM.read(8));
          DFPlayer.loopFolder(EEPROM.read(9));
          for (int i = 0; i < EEPROM.read(10); i++) {
            DFPlayer.next();
            delay(100);
          }
          timer = millis(); //do NOT merge this with the declaration
          if (EEPROM.read(8) == 0) {
            while (clicked(false)) {
              if (vol != int((millis() - timer) / 1000)) {
                DFPlayer.volume(int((millis() - timer) / 1000));
                vol = int((millis() - timer) / 1000);
              }
            }
          } else {
            while (clicked(false));
          }
          while (debounce());
          DFPlayer.pause();
          DFPlayer.volume(EEPROM.read(0));
          digitalWrite(13, LOW);
        }
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
  int renderoffset = 0;
  encoffset = Enc.read() - 1;
  while (clicked(false)) {
    myGLCD.clrScr();
    ouflowpreventer(optnum, optnum, 0, 0);

    if (enc() - renderoffset < 1) {
      renderoffset--;
    } else if (enc() - renderoffset > 4) {
      renderoffset++;
    }

    if (renderoffset < 0) {
      renderoffset = 0;
    } else if (renderoffset > optnum) {
      renderoffset = optnum;
    }

    for (int i = 0; i + renderoffset < optnum; i++) { //print options
      myGLCD.print(opts[i + renderoffset], 6, i * 8 + 11);
    }
    if (enc() == 0) { //return
      myGLCD.drawLine(0, 0, 84, 0);
      myGLCD.invertText(true);
      myGLCD.print(F("     HOME     "), CENTER, 1);
      myGLCD.print(F("*"), 0, 1);
      myGLCD.invertText(false);
    } else { //norm
      myGLCD.print(title, CENTER, 1);
      myGLCD.drawLine(0, 9, 84, 9);
      myGLCD.print(">", 0, int(enc() - renderoffset) * 8 + 3);
    }
    myGLCD.update();
  }
  while (debounce());
  select = enc();
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
    return defaultint;//default because it underflowed to RETURN
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
    options[0] = F("Alarm");
    options[1] = F("Settings");
    menu(2, F("Clock"), options);
    if (select == 1) { //alarm
      options[0] = F("Set alarm");
      if (alarmon) {
        options[1] = F("Turn off");
      } else {
        options[1] = F("Turn on");
      }
      options[2] = F("Set volume");
      options[3] = F("Set music");
      menu(4, F("Alarm"), options);
      if (select == 1) { //set alarm
        alarm = DateTime(2000, 1, 1, selnum(F("Hour"), 0, 23, alarm.hour()), selnum(F("Minute"), 0, 59, alarm.minute()), selnum(F("Second"), 0, 59, alarm.second()));
        EEPROM.write(3, alarm.hour());
        EEPROM.write(4, alarm.minute());
        EEPROM.write(5, alarm.second());
        alarmon = true;
        EEPROM.write(6, true);
      } else if (select == 2) { //toggle
        alarmon = !alarmon;
        EEPROM.write(6, !EEPROM.read(6));
      } else if (select == 3) {
        EEPROM.write(8, selnum(F("Alarm volume"), 0, 30, EEPROM.read(8)));
      } else if (select == 4) {
        EEPROM.write(9, selnum(F("Alarm folder"), 1, 9, EEPROM.read(9)));
        EEPROM.write(10, selnum(F("Alarm music"), 0, 100, EEPROM.read(10)));
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
    options[2] = F("Lockdown");
    options[3] = F("Reset");
    menu(4, F("Preferences"), options);
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
      lcdbacklit = !lcdbacklit;
      digitalWrite(13, lcdbacklit);
      EEPROM.write(2, !EEPROM.read(2));
    } else if (select == 3) {
      EEPROM.write(7, true);
      setup();
    } else if (select == 4) {
      setup();
    }
  } else if (select == 3) { //mp3 player
    options[0] = F("MP3 UI");
    options[1] = F("Playlist");
    options[2] = F("Settings");
    options[3] = F("Reset module");
    menu(4, F("MP3 Player"), options);
    if (select == 1) {
      if (showmp3UI) {
        options[0] = F("Turn off");
      } else {
        options[0] = F("Turn on");
      }
      options[1] = F("White noise");
      menu(2, F("MP3 UI"), options);
      if (select == 1) {
        showmp3UI = !showmp3UI;
        EEPROM.write(1, !EEPROM.read(1));
      } else if (select == 2) {
        whitenoisemode = !whitenoisemode;
      }
    } else if (select == 2) {
      options[0] = F("English");
      options[1] = F("Mandarin");
      options[2] = F("Series");
      options[3] = F("White noise");
      options[4] = F("Musicals");
      options[5] = F("Casual");
      options[6] = F("Alarm");
      options[7] = F("Nostalgia");
      options[8] = F("OpenTTD");
      options[9] = F("Sleep");
      menu(10, F("Playlist"), options);
      if (select != 0) {
        DFPlayer.loopFolder(select);
        DFPlayer.disableLoopAll();
        DFPlayer.start();
      }
    } else if (select == 3) {
      options[0] = F("Normal");
      options[1] = F("Pop");
      options[2] = F("Rock");
      options[3] = F("Jazz");
      options[4] = F("Classic");
      options[5] = F("Bass");
      menu(6, F("EQ Settings"), options);
      if (select != 0) {
        DFPlayer.EQ(select);
      }
    } else if (select == 4) {
      DFPlayer.reset();
      DFPlayer.volume(EEPROM.read(0));
      DFPlayer.EQ(EEPROM.read(11));
      DFPlayer.setTimeOut(500);
      DFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
    }
  }
}


void mp3UI() {
  for (int i = 27; i < 34; i += 2) {//menu icon
    myGLCD.invPixel(9, i);
    myGLCD.drawLine(11, i, 16, i);
  }

  myGLCD.drawLine(21, 27, 24, 27); //backlight button
  myGLCD.drawLine(20, 28, 20, 32);
  myGLCD.drawLine(24, 28, 24, 32);
  myGLCD.drawLine(22, 30, 22, 32);
  myGLCD.drawRect(21, 32, 23, 33);

  if (whitenoisemode) {
    myGLCD.print(F("ON"), CENTER, 27);
  } else {
    for (int i = 29; i < 36; i += 3) {//prev button
      myGLCD.drawLine(i, 28, i, 33);
    }
    for (int i = 31; i < 35; i += 3) {
      myGLCD.invPixel(i - 1, 30);
      myGLCD.drawLine(i, 29, i, 32);
    }

    if (analogRead(A2) > 500) {
      for (int i = 1; i < 8; i++) {//play button
        myGLCD.drawLine(i + 38, round(i / 2) + 27, i + 38, 34 - round(i / 2));
      }
    } else {
      myGLCD.drawLine(40, 27, 40, 34);//pause button
      myGLCD.drawLine(44, 27, 44, 34);
    }

    for (int i = 49; i < 56; i += 3) {//next button
      myGLCD.drawLine(i, 28, i, 33);
    }
    for (int i = 50; i < 54; i += 3) {
      myGLCD.invPixel(i + 1, 30);
      myGLCD.drawLine(i, 29, i, 32);
    }
  }

  myGLCD.invPixel(61, 27);//loop icon
  myGLCD.invPixel(62, 27);
  myGLCD.invPixel(63, 27);
  myGLCD.invPixel(60, 28);
  myGLCD.invPixel(64, 28);
  myGLCD.invPixel(59, 29);
  myGLCD.invPixel(62, 29);
  myGLCD.invPixel(65, 29);
  myGLCD.invPixel(59, 30);
  myGLCD.invPixel(62, 30);
  myGLCD.invPixel(65, 30);
  myGLCD.invPixel(59, 31);
  myGLCD.invPixel(62, 31);
  myGLCD.invPixel(65, 31);
  myGLCD.invPixel(60, 32);
  myGLCD.invPixel(61, 33);
  myGLCD.invPixel(62, 33);
  myGLCD.invPixel(63, 33);
  myGLCD.invPixel(64, 33);
  myGLCD.invPixel(65, 33);

  myGLCD.invPixel(72, 27);//volume icon
  myGLCD.invPixel(71, 28);
  myGLCD.invPixel(74, 28);
  myGLCD.invPixel(69, 29);
  myGLCD.invPixel(70, 29);
  myGLCD.invPixel(72, 29);
  myGLCD.invPixel(75, 29);
  myGLCD.invPixel(69, 30);
  myGLCD.invPixel(70, 30);
  myGLCD.invPixel(72, 30);
  myGLCD.invPixel(75, 30);
  myGLCD.invPixel(69, 31);
  myGLCD.invPixel(70, 31);
  myGLCD.invPixel(72, 31);
  myGLCD.invPixel(75, 31);
  myGLCD.invPixel(71, 32);
  myGLCD.invPixel(74, 32);
  myGLCD.invPixel(72, 33);

  ouflowpreventer(6, 6, 0, 0);
  if (whitenoisemode) {
    if (enc() > 1 && enc() < 5) {
      myGLCD.drawRoundRect(27, 25, 57, 35);
    } else {
      myGLCD.drawRoundRect(int(enc()) * 10 + 7, 25, int(enc()) * 10 + 17, 35);//selection box
    }
  } else {
    myGLCD.drawRoundRect(int(enc()) * 10 + 7, 25, int(enc()) * 10 + 17, 35);//selection box
  }

  if (clicked(true)) {
    bool hold = true;
    select = millis();
    if (enc() == 0) {
      menutree();
      hold = false;
    } else if (enc() == 1) {
      lcdbacklit = !lcdbacklit;
      digitalWrite(13, lcdbacklit);
      EEPROM.write(2, !EEPROM.read(2));
    } else if (enc() == 2) {
      mp3loop = false;
      DFPlayer.previous();
    } else if (enc() == 3) {
      mp3loop = false;
      if (analogRead(A2) > 500) {
        DFPlayer.start();
      } else {
        DFPlayer.pause();
      }
    } else if (enc() == 4) {
      mp3loop = false;
      DFPlayer.next();
    } else if (enc() == 5) {
      mp3loop = !mp3loop;
    } else if (enc() == 6) {
      hold = false;
      myGLCD.setFont(MediumNumbers);
      encoffset = Enc.read() - EEPROM.read(0);
      while (debounce());
      while (true) {
        myGLCD.clrScr();
        ouflowpreventer(30, 30, 0, 0);
        myGLCD.print(String(enc()), CENTER, 10);
        myGLCD.drawRect(12, 35, 74, 44);
        for (int i = 0; i < enc() * 2; i += 2) {
          myGLCD.drawRect(i + 13, 36, i + 14, 44);
        }
        DFPlayer.volume(enc());
        myGLCD.update();
        if (clicked(true)) {
          EEPROM.write(0, enc());
          break;
        }
      }
    }
    while (debounce());
    if (millis() - select > 2000 && hold) {
      whitenoisemode = !whitenoisemode;
      DFPlayer.loopFolder(4);
    }
  }
}

long int enc() {
  return int(Enc.read() - encoffset);
}

void ouflowpreventer(int maxi, int maxidef, int mini, int minidef) {
  if (int(enc()) > maxi) {
    encoffset = Enc.read() - maxidef;
  }
  if (int(enc()) < mini) {
    encoffset = Enc.read() - minidef;
  }
}

String zeroadder(int str) {
  if (str < 10) {
    return '0' + String(str);
  } else {
    return String(str);
  }
}

void blinkpwr() {
  blinkstate = !blinkstate;
  digitalWrite(6, blinkstate);
  Serial.println("interr");
}
