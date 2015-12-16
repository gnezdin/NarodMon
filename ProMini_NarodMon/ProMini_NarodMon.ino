//Код для ProMini Narodmon
//

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h> // https://github.com/maniacbug/RF24
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

#define STS_LED_PIN 7 

const uint64_t pipe = 0xF1F9F8F3AALL; // индитификатор передачи, "труба"

RF24 radio(9, 10); // CE, CSN

// организуем софтовый UART для общения с BT модулем HC-05
SoftwareSerial softSerial(5, 6); // RX, TX

// очередной принятый по UART байт
byte incommingByte = 0;

// счётчик байтов входного буфера Json
byte jsonCnt = 0;

// флаг приёма окончания запроса Json
bool sw = false;

// Температура окр. воздуха 
double TEMP_OUT = 0;
// Напряжение батареи темп. передатчика
long BAT = 0;
// Температура в помещении (DHT21) 
double TEMP_IN = 0;
// Влажность в помещении (DHT21) 
double HUM = 0;
// Атм. давление, кПа (BMP180) 
double PRESS = 0;
// Температура электроники (BMP180) 
double TEMP_E = 0;

// переменные от ESP8266
int WIFI_STATUS = 0;
int NARODMON_STATUS = 0;

// переменная для светодиода 0-выкл, 1-вкл, 2-мигание
int  stsLed = 0;
int stsLedCounter = 0;

char jsonIn[100];

static byte data[8];

union
{
  float f;
  unsigned char buf[4];
}tmp;

union
{
  long l;
  unsigned char lBuf[4];
} lng;

void SendDataToESP()
{
  
}

void setup() 
{
  memset(jsonIn, 0, sizeof(jsonIn));
  Serial.begin(9600);
  softSerial.begin(9600);
  stsLed = 0;
  pinMode(STS_LED_PIN, OUTPUT);
  digitalWrite(STS_LED_PIN, 0);
  delay(10);

  // NRF init
  radio.begin();
  delay(2);
  radio.setChannel(109); // канал (0-127)
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_HIGH);

  radio.openReadingPipe(1, pipe); // открываем первую трубу с индитификатором "pipe"
  radio.startListening(); // включаем приемник, начинаем слушать трубу
}

void loop() 
{
   // считываем очередной байт, если он есть в буфере
  if (Serial.available())
  {
     incommingByte = Serial.read();
   
   switch(incommingByte)
   {
     case '{':  //Проверяем признак начала строки JSON
       // очищаем буфер и готовимся к приёму комманды
       memset(jsonIn, 0, sizeof(jsonIn));
       sw = false;
       jsonCnt = 0;
       sw = false;
       jsonIn[jsonCnt] = (char) incommingByte;
       break;  
     case '}':  //Проверяем признак конца команды
        sw = true;
        jsonCnt++;
        jsonIn[jsonCnt] = (char) incommingByte;
        break;
     default:
       sw = false;
       jsonCnt++;
       jsonIn[jsonCnt] = (char) incommingByte;
       
       // если данных пришло больше, чем размер буфера, то всё очищаем
       if (jsonCnt > sizeof(jsonIn))
       {
          memset(jsonIn, 0, sizeof(jsonIn));
          sw = false;
          jsonCnt = 0;
       }
         break;
   }
  }
  // анализируем входящие данные JSON
  if (sw)
  {
      Serial.print("json: ");
      Serial.print(jsonIn);

      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(jsonIn);

      if (!root.success())
      {
        softSerial.println("parseObject() failed");
        return;
      }

      if (root.containsKey("wifi_status"))
      {
         WIFI_STATUS = root["wifi_status"];
      }

       if (root.containsKey("narodmon_status"))
      {
         NARODMON_STATUS = root["narodmon_status"];
      }
      
      // очищаем входной буфер
      memset(jsonIn, 0, sizeof(jsonIn));
      sw = false;
      jsonCnt = 0;    

      // отображаем статус на LED
      if ((WIFI_STATUS) & (NARODMON_STATUS))
      {
        stsLed = 1;
      }
       else if ((WIFI_STATUS) & (!NARODMON_STATUS))
       {
          stsLed = 2;
       }
       else if (!WIFI_STATUS))
       {
          stsLed = 0;
       }
        else
        {
           stsLed = 2;
        }

       // отправляем данные в ESP8266
       SendDataToESP();
  }  
  // Update StatusLed
  switch(stsLed)
  {
    case 0:
      digitalWrite(STS_LED_PIN, 0);
      stsLedCounter = 0;
    break;
    case 1:
      digitalWrite(STS_LED_PIN, 1);
      stsLedCounter = 0;
    break;
    case 2:
      stsLedCounter++;
      if (stsLedCounter > 100)
      {
        digitalWrite(STS_LED_PIN, 0); 
        stsLedCounter = 0;
      }
       else if (stsLedCounter > 50)
      {
        digitalWrite(STS_LED_PIN, 1); 
      }
    break;      
  }
  
  delay(10);   
}

