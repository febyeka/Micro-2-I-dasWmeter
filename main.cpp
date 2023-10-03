#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <TinyGPSPlus.h>
#include <StreamUtils.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <SPI.h>
#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;

int GPSswitch = 26;
TinyGPSPlus gps;
String location = "";
bool timeout = 0;

const unsigned long periodReq = 20000;
unsigned long upReqTime = 0;
int countReq = 0;
int persenBatt;

// baterey
float voltNO;
float voltNC;
const float multiplier = 0.1875F;
const int switchBatt = 2;

// Config Pin Lora
const int configLora = 5;
const int pinConfig = 25;

// Keypad Config -------------------
bool keyPressed = 0;
String inputString;
String inputInt;
int countLoad = 13;
int statusReq;
int jumIsi;

bool keyReq = 0;
bool getKeyReq = 0;
bool changeVolume = 0;

bool keyA = 0;
bool keyB = 0;
bool keyC = 0;
bool clearLCD = 0;
char stringAngka[17];
int indexKeypad = 0;
int centerInput = 6;
const byte ROWS = 5;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
    {'A', 'B', 'C', 'F'},
    {'1', '2', '3', 'U'},
    {'4', '5', '6', 'D'},
    {'7', '8', '9', 'E'},
    {'K', '0', 'T', 'N'}};

byte rowPins[ROWS] = {23, 33, 32, 4, 15};
byte colPins[COLS] = {13, 12, 14, 27};

Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
////----------------------------------

const unsigned long periodNotif = 10000;
unsigned long timeNotif_now = 0;

double latitude;
double longitude;
double staticLat;
double staticLong;
float distanceGPS;
bool idle = 1;
bool keep = 0;
bool emergency = 0;
bool logicEmergency = 0;

int jumlahPesanan;
int cyble;
int countL;
LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup()
{
  // put your setup code here, to run once:
  ads.begin();
  EEPROM.begin(512);
  pinMode(GPSswitch, OUTPUT);
  digitalWrite(GPSswitch, LOW);
  Serial.begin(19200);
  Serial2.begin(19200);
  Serial1.begin(19200, SERIAL_8N1, 19, 18);

  pinMode(configLora, OUTPUT);
  pinMode(pinConfig, OUTPUT);
  pinMode(switchBatt, OUTPUT);

  digitalWrite(configLora, LOW);
  digitalWrite(pinConfig, LOW);
  digitalWrite(switchBatt, HIGH);

  inputString.reserve(10);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(5, 0);
  lcd.print("i-DasWmeter");
  lcd.setCursor(0, 2);
  lcd.print("Stan : ");
  lcd.setCursor(18, 2);
  lcd.print("m3");

  lcd.setCursor(0, 3);
  lcd.print("Batt : ");
  lcd.setCursor(16, 3);
  lcd.print("100%");

  StaticJsonDocument<500> dataLog;
  EepromStream eepromStream(0, 500);

  deserializeJson(dataLog, eepromStream);
  if (dataLog["lf"])
  {
    Serial.println("Loaded doc's LF value.");
    cyble = dataLog["lf"];
    latitude = dataLog["lat"];
    longitude = dataLog["long"];
  }
  else
  {
    Serial.println("No log Cyble variable in eeprom.");
  }
}

void saveLog()
{
  StaticJsonDocument<500> dataLog;
  EepromStream eepromStream(0, 500);
  dataLog["lf"] = cyble;
  dataLog["lat"] = latitude;
  dataLog["long"] = longitude;
  serializeJson(dataLog, eepromStream);
  eepromStream.flush();
}

void readGPS()
{
  if (Serial2.available())
  {
    gps.encode(Serial2.read());
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    // Serial.print(latitude, 6);
    // Serial.print(",");
    // Serial.println(longitude, 6);
  }
  if (gps.location.isUpdated())
  {
    latitude = gps.location.lat();
    longitude = gps.location.lng();
    Serial.print(latitude, 6);
    Serial.print(",");
    Serial.println(longitude, 6);
  }
}

void upmicro()
{
  StaticJsonDocument<150> datagps;
  datagps["lat"] = String(latitude, 6);
  datagps["long"] = String(longitude, 6);
  serializeJson(datagps, Serial1);
}

void readMicroSerial()
{
  if (Serial1.available() > 0)
  {
    StaticJsonDocument<150> doc;
    DeserializationError error = deserializeJson(doc, Serial1);

    if (error)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return;
    }

    int req = doc["req"];     // "0000"
    int stan = doc["stan"];   // "0000"
    int count = doc["count"]; // "0000"
    int microStat = doc["microStat"];
    int batt = doc["batt"];

    cyble = stan;
    countL = count;
    jumlahPesanan = req;
    persenBatt = batt;
    
    Serial.print(microStat);
    if (microStat == 1)
    {
      keep = 1;
    }
    if (microStat == 2)
    {
      emergency = 1;
    }
    if (microStat == 0 && keyPressed == 0 && keyReq == 0 && getKeyReq == 0)
    {
      keep = 0;
      idle = 1;

      if (clearLCD == 0)
      {
        lcd.clear();
        clearLCD = 1;
      }

      lcd.setCursor(5, 0);
      lcd.print("i-DasWmeter");
      lcd.setCursor(0, 2);
      lcd.print("Stan : ");
      lcd.setCursor(0, 3);
      lcd.print("Batt : ");
      lcd.setCursor(18, 2);
      lcd.print("m3");
      // lcd.setCursor(16, 3);
      // lcd.print("100%");
    }
  }
}

void uplinkNotif()
{
  StaticJsonDocument<256> dataUp;
  location = String(latitude, 6) + "," + String(longitude, 6);

  dataUp["nodeID"] = "DASWMETER24VL0001";
  dataUp["loc"] = location + "," + String(distanceGPS);
  dataUp["status"] = "5";
  serializeJson(dataUp, Serial2);
}

void uplinkReq()
{
  StaticJsonDocument<256> dataUp;
  location = String(latitude, 6) + "," + String(longitude, 6);

  dataUp["nodeID"] = "DASWMETER24VL0001";
  dataUp["loc"] = location + "," + String(distanceGPS);
  dataUp["Batt"] = String(persenBatt) + "%";
  dataUp["status"] = statusReq;
  dataUp["flow"] = inputInt + "," + String(jumIsi);
  dataUp["logFlow"] = "0,0";
  serializeJson(dataUp, Serial2);
}

void bacaBattrey()
{
  int16_t adc0, adc1, adc2, adc3;

  adc0 = ads.readADC_SingleEnded(0);
  adc1 = ads.readADC_SingleEnded(1);
  adc2 = ads.readADC_SingleEnded(2);
  adc3 = ads.readADC_SingleEnded(3);

  voltNO = (5.744 * (adc0 * multiplier / 1000)) - 0.0964;
  voltNC = adc1 * multiplier / 1000;
  Serial.print("A0 : ");
  Serial.print(voltNO);
  Serial.print(" A1 : ");
  Serial.println(voltNC);
  // if (voltNC == -0.25 && voltNO < 3)
  // {
  //     digitalWrite(switchBatt, LOW);
  // }
  // if (voltNC == 0.00 && voltNO >= 24 )
  // {
  //   digitalWrite(switchBatt, HIGH);
  // }
  // if (voltNC == 0.00 && voltNO == 0)
  // {
  //   digitalWrite(switchBatt, HIGH);
  // }
}

void (*resetFunc)(void) = 0;

void loop()
{

  // StaticJsonDocument<500> dataLog;
  // EepromStream eepromStream(0, 500);
  // dataLog["lf"] = 100;
  // dataLog["lat"] = "-7.345902";
  // dataLog["long"] = "112.746122";
  // serializeJson(dataLog, eepromStream);
  // eepromStream.flush();

  char key = customKeypad.getKey();
  readGPS();
  if (latitude != 0 && longitude != 0)
  {
    upmicro();
  }
  // delay(1000);
  bacaBattrey();
  readMicroSerial();
  Serial.print(emergency);
  if (emergency && idle)
  {
    lcd.setCursor(1, 0);
    lcd.print("EMERGENCY BUTTON ON");
  }
  // if (!emergency && idle)
  // {
  //   lcd.setCursor(5, 0);
  //   lcd.print("i-DasWmeter");
  // }
  if (keep == 1 && idle == 1)
  {
    staticLat = latitude;
    staticLong = longitude;
    lcd.clear();
    keep = 0;
    idle = 0;
    clearLCD = 0;
  }

  if (idle == 0)
  {

    digitalWrite(switchBatt, HIGH);
    // Serial.print("relay on");
    lcd.setCursor(2, 0);
    lcd.print("PROSES PENGISIAN");
    lcd.setCursor(0, 1);
    lcd.print("Req  : ");
    lcd.setCursor(14, 1);
    lcd.print((float(jumlahPesanan) / 1000), 1);
    lcd.setCursor(17, 1);
    lcd.print(" m3");
    lcd.setCursor(0, 2);
    lcd.print("Stan : ");
    lcd.setCursor(17, 2);
    lcd.print(" m3");
    lcd.setCursor(0, 3);
    lcd.print("Count: ");
    lcd.setCursor(17, 3);
    lcd.print(" m3");

    if (((float(cyble) / 10), 1) < 1000 && ((float(cyble) / 10), 1) > 99)
    {
      lcd.setCursor(12, 2);
      lcd.print((float(cyble) / 10), 1);
    }

    if (((float(cyble) / 10), 1) < 100)
    {
      lcd.setCursor(13, 2);
      lcd.print((float(cyble) / 10), 1);
    }

    if ((float(countL) / 1000) < 10)
    {
      lcd.setCursor(14, 3);
      lcd.print((float(countL) / 1000), 1);
    }
    if ((float(countL) / 1000) < 100 && (float(countL) / 1000) > 9)
    {
      lcd.setCursor(13, 3);
      lcd.print((float(countL) / 1000), 1);
    }

    saveLog();

    float delLat = abs(staticLat - latitude) * 111194.9;
    float delLong = 111194.9 * abs(staticLong - longitude) * cos(radians((staticLat + latitude) / 2));
    float distance = sqrt(pow(delLat, 2) + pow(delLong, 2));
    Serial.print("Jarak : ");
    Serial.print(distance, 3);
    Serial.print(" | ");
    Serial.print(latitude, 6);
    Serial.print(",");
    Serial.println(longitude, 6);

    distanceGPS = distance;
    if (distanceGPS > 20)
    {
      if (millis() >= timeNotif_now + periodNotif)
      {
        timeNotif_now += periodNotif;
        digitalWrite(GPSswitch, HIGH);
        uplinkNotif();
        digitalWrite(GPSswitch, LOW);
      }
    }
    else
    {
      digitalWrite(GPSswitch, LOW);
    }
  }

  // if (idle == 1 && keep == 0 && persenBatt >= 20)
  if (idle == 1 && keep == 0)
  {
    digitalWrite(switchBatt, LOW);
    Serial.print("lat:");
    Serial.print(latitude, 6);
    Serial.print("long:");
    Serial.println(longitude, 6);
    if (keyPressed == 0 && keyReq == 0 && getKeyReq == 0)
    {
      if (persenBatt == 100)
      {
        lcd.setCursor(14, 3);
        lcd.print(persenBatt);
      }
      if (persenBatt < 100)
      {
        // lcd.clear();
        lcd.setCursor(14, 3);
        lcd.print(" ");
        lcd.setCursor(15, 3);
        lcd.print(persenBatt);
        if (persenBatt < 10)
        {
          lcd.setCursor(15, 3);
          lcd.print(" ");
          lcd.setCursor(16, 3);
          lcd.print(persenBatt);
        }
      }
      lcd.setCursor(18, 3);
      lcd.print("%");
      if (((float(cyble) / 10), 1) < 1000 && ((float(cyble) / 10), 1) > 99)
      {

        lcd.setCursor(12, 2);
        lcd.print((float(cyble) / 10), 1);
      }

      if (((float(cyble) / 10), 1) < 100)
      {

        lcd.setCursor(13, 2);
        lcd.print((float(cyble) / 10), 1);
      }
    }

    if (key == 'B' && keyB == 0 && keyPressed == 0)
    {
      keyPressed = 1;
      lcd.setCursor(13, 2);
      lcd.print("       ");
      lcd.setCursor(0, 2);
      lcd.print("      ");
      lcd.setCursor(0, 0);
      lcd.print("Input Transc Code:");
      lcd.setCursor(0, 3);
      lcd.print("---[PRESS NUMBER]---");
      keyB = 1;
    }

    if (keyB == 1 && keyPressed == 1)
    {
      Serial.println(key);
      if (key >= '0' && key <= '9')
      {
        if (clearLCD == 0)
        {
          clearLCD = 1;
          lcd.clear();
        }
        lcd.setCursor(0, 1);
        lcd.print("Code :");
        lcd.setCursor(0, 3);
        lcd.print("------[ENTER]-------");
        inputString += key;
        centerInput++;
        lcd.setCursor(centerInput, 1);
        lcd.print(key);
        
      }
      else if (key == 'N')
      {
        if (inputString.length() > 0)
        {
          digitalWrite(GPSswitch, HIGH);
          delay(100);
          inputInt = inputString;
          statusReq = 7;
          jumIsi = 0;
          uplinkReq();
          keyB = 0;
          keyPressed = 0;
          inputString = ""; // clear input
          centerInput = 6;
          clearLCD = 0;
          keyReq = 1;
        }
      }
      else if (key == 'K')
      {
        centerInput = 6;
        inputString = ""; // clear input
        lcd.setCursor(6, 1);
        lcd.print("              "); // clear 6 s/d 20
      }

      else if (key == 'E')
      {
        keyPressed = 0;
        keyB = 0;
        centerInput = 6;
        inputString = ""; // clear input
      }
    }
  }

  if (keyReq)
  {
    digitalWrite(GPSswitch, HIGH);
    readMicroSerial();
    if (clearLCD == 0)
    {
      lcd.clear();
      clearLCD = 1;
    }

    if (jumlahPesanan > 0)
    {
      if (clearLCD == 1)
      {
        lcd.clear();
        clearLCD = 0;
      }
      lcd.setCursor(0, 0);
      lcd.print("Mengisi ");
      lcd.print(jumlahPesanan / 1000);
      lcd.print(" m3 ?");
      lcd.setCursor(0, 2);
      lcd.print("[ENTER] | Confirm");
      lcd.setCursor(0, 3);
      lcd.print("[F1]    | Change");

      clearLCD = 0;
      getKeyReq = 1;
      keyReq = 0;
      countReq = 0;
      keyB = 0;
    }
    else
    {
      if (millis() >= upReqTime + periodReq)
      {
        upReqTime += periodReq;
        countReq++;
        Serial.println(countReq);

        if (countReq > 1 && countReq <= 15)
        {
          statusReq = 7;
          jumIsi = 0;
          uplinkReq();
          lcd.setCursor(0, 0);
          lcd.print("Search Request");
          lcd.setCursor(countLoad++, 0);
          lcd.print(".");
        }
        else if (countLoad == 14)
        {
          lcd.setCursor(15, 0);
          lcd.print("      ");
          countLoad = 13;
        }
        else if (countReq == 15 && jumlahPesanan == 0)
        {
          Serial.println("Timeout Request, No Data");
          lcd.setCursor(0, 0);
          lcd.print("Request Time Out    ");
          timeout = 1;
        }
        else if (countReq == 18 && timeout)
        {
          timeout = 0;
          resetFunc();
        }
      }
    }
  }

  if (getKeyReq)
  {
    if (key == 'A')
    {
      changeVolume = 1;
      if (clearLCD == 0)
      {
        lcd.clear();
        clearLCD = 1;
      }
      lcd.setCursor(0, 0);
      lcd.print("Input Volume (m3) :");
      lcd.setCursor(0, 3);
      lcd.print("---[PRESS NUMBER]---");
    }
    else if (key == 'N' && !changeVolume)
    {
      Serial.println("Enter");
      lcd.clear();
      statusReq = 8;
      jumIsi = jumlahPesanan;
      uplinkReq();
      delay(100);
      digitalWrite(GPSswitch, LOW);
      inputString = ""; // clear input
      centerInput = 6;
      getKeyReq = 0;
    }
    else if ((key >= '0' && key <= '9') && changeVolume)
    {

      lcd.setCursor(0, 1);
      lcd.print("Isi : ");
      lcd.setCursor(0, 3);
      lcd.print("------[ENTER]-------");
      inputString += key;
      centerInput++;
      lcd.setCursor(centerInput, 1);
      lcd.print(key);
    }
    else if (key == 'N' && changeVolume)
    {
      changeVolume = 0;
      Serial.println("Enter");
      lcd.clear();
      jumIsi = (inputString.toInt()) * 1000;
      statusReq = 8;
      uplinkReq();
      delay(100);
      digitalWrite(GPSswitch, LOW);
      inputString = ""; // clear input
      centerInput = 6;
      getKeyReq = 0;
    }
  }
}
