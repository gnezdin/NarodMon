//Код для ProMini Narodmon
//

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h> // https://github.com/maniacbug/RF24
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <BMP085.h>

// светодиод статуса
#define STS_LED_PIN 7

// пин сброса ESP
#define ESP_RESET_PIN 4 

// светодиод встроенный Arduino
//#define ALED 13 

// таймауты *10 (мс.)
#define DHT_COUNTER_TIMEOUT 6000
#define SEND_COUNTER_TIMEOUT 60000 // 60000
#define ESP_REQUEST_TIMEOUT 3000

#define DHTPIN 8 
#define DHTTYPE DHT21

DHT dht(DHTPIN, DHTTYPE);


BMP085 bmp = BMP085();  

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

// таймер ожидания ответа от ESP и флаг
long espCounter = 0;

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
int THINGSPEAK_STATUS = 0;
int NARODMON_STATUS = 0;

// переменная для светодиода 0-выкл, 1-вкл, 2-мигание, 3 - одна вспышка (ошибка TS), 4 - две вспышки (ошибка Narodmon)
int  stsLed = 0;
int stsLedCounter = 0;

// переменные для светодиода Arduino
//bool aLedSw = false;
//int aLedCounter = 0;

// счётчик для DHT и BMP
long dhtCounter = 100000;
// счётчик отправки на народмон
long sendCounter = 100000;

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


void ReadDHT()
{
    // READ DATA
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();   
    
    // debug
//    softSerial.print("HUM: ");
//    softSerial.println(HUM);
//    softSerial.print("TEMP_IN: ");
//    softSerial.println(TEMP_IN);

    if (isnan(h) || isnan(t)) 
    {
      //Serial.println("Failed to read from DHT sensor!");
      return;
    }

    HUM = h;
    TEMP_IN = t;
}

void ReadBMP()
{

  long pres = 0; //bmp.readPressure();
  bmp.getPressure(&pres);
  long temp = 0;
  bmp.getTemperature(&temp);
  float tmp = temp;

  PRESS = (double) pres / 1000.0;
  TEMP_E = tmp * 0.1;


//  softSerial.print("PRESS: ");
//  softSerial.println(PRESS);
//
//  softSerial.print("TEMP_E: ");
//  softSerial.println(TEMP_E);
}

void SendDataToESP()
{
   //
// Step 1: Reserve memory space
//
StaticJsonBuffer<200> jsonBuffer;

//
// Step 2: Build object tree in memory
//
JsonObject& root = jsonBuffer.createObject();
root["temp_out"] = TEMP_OUT;
root["bat"] = BAT;
root["temp_in"] = TEMP_IN;
root["hum"] = HUM;
root["press"] = PRESS;
root["temp_e"] = TEMP_E;
//
// Step 3: Generate the JSON string
//
root.printTo(Serial);

// debug
//root.printTo(softSerial);
}

void setup() 
{
  memset(jsonIn, 0, sizeof(jsonIn));

 
  
  Serial.begin(9600);
  softSerial.begin(9600);
  delay(1000);
   pinMode(ESP_RESET_PIN, OUTPUT);
  digitalWrite(ESP_RESET_PIN, 1);
  stsLed = 0;
  pinMode(STS_LED_PIN, OUTPUT);
  digitalWrite(STS_LED_PIN, 0);

//  pinMode(ALED, OUTPUT);
 // digitalWrite(ALED, 0);

  // NRF init
  radio.begin();
  delay(2);
  radio.setChannel(109); // канал (0-127)
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_HIGH);

  radio.openReadingPipe(1, pipe); // открываем первую трубу с индитификатором "pipe"
  radio.startListening(); // включаем приемник, начинаем слушать трубу

  dht.begin();
  bmp.init();   
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
       jsonCnt++;
       break;  
     case '}':  //Проверяем признак конца команды
        sw = true;
        jsonIn[jsonCnt] = (char) incommingByte;
        jsonCnt++;
        break;
     default:
       sw = false;
       if (jsonCnt > 0)
       {
          jsonIn[jsonCnt] = (char) incommingByte;
          jsonCnt++;
       }
       
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
      // сброс счётчика ожидания ответа ESP
      if (espCounter > 0)
      {
         digitalWrite(ESP_RESET_PIN, 1);
         espCounter = 0;
      }
      
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(jsonIn);

      if (!root.success())
      {
       // softSerial.println("parseObject() failed");
        return;
      }

      if (root.containsKey("wifi_status"))
      {
         WIFI_STATUS = root["wifi_status"];
      }

       if (root.containsKey("thingspeak_status"))
      {
         THINGSPEAK_STATUS = root["thingspeak_status"];
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
      if ((WIFI_STATUS) & (THINGSPEAK_STATUS) & (NARODMON_STATUS))
      {
        stsLed = 1;
      }
       else if ((WIFI_STATUS) & (!THINGSPEAK_STATUS) & (!NARODMON_STATUS))
       {
          stsLed = 2;
       }
       else if (!WIFI_STATUS)
       {
          stsLed = 0;
       }
        else if ((WIFI_STATUS) & (!THINGSPEAK_STATUS) & (NARODMON_STATUS))
        {
           stsLed = 3;
        }
        else if ((WIFI_STATUS) & (THINGSPEAK_STATUS) & (!NARODMON_STATUS))
        {
           stsLed = 4;
        }

       // отправляем данные в ESP8266
      // SendDataToESP();
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
     case 3:
      stsLedCounter++;
      if (stsLedCounter > 160)
      {
        stsLedCounter = 0;
      }
       else if (stsLedCounter > 60)
      {
        digitalWrite(STS_LED_PIN, 0);         
      }
       else if (stsLedCounter > 50)
      {
        digitalWrite(STS_LED_PIN, 1); 
      }
    break;
     case 4:
      stsLedCounter++;
      if (stsLedCounter > 210)
      {
        stsLedCounter = 0;
      }
       else if (stsLedCounter > 110)
      {
        digitalWrite(STS_LED_PIN, 0);         
      }
       else if (stsLedCounter > 100)
      {
        digitalWrite(STS_LED_PIN, 1); 
      }
       else if (stsLedCounter > 60)
      {
        digitalWrite(STS_LED_PIN, 0);         
      }
       else if (stsLedCounter > 50)
      {
        digitalWrite(STS_LED_PIN, 1); 
      }
    break;      
  }


  // Считываем NRF
  if (radio.available())
{
  // читаем данные и указываем сколько байт читать
  radio.read(&data, sizeof(data));


  byte  pos = 0;

  for (byte i = 0; i < 4; i++)
  {
     tmp.buf[i] = data[pos]; 
     pos++;
  }
  
 // softSerial.print("Data Recieve: ");
 // softSerial.print(tmp.f);
 // softSerial.print(" oC");

  TEMP_OUT = tmp.f;

  // получаем значение напряжения
  pos = 4;
  for (byte i = 0; i < 4; i++)
  {
     lng.lBuf[i] = data[pos]; 
     pos++;
  }
  
 // softSerial.print(" [VCC: ");
 // softSerial.print(lng.l);
 // softSerial.println("mV]");

  BAT = lng.l;
}

 // считываем DHT
 // чтение датчиков
 dhtCounter++;
  if (dhtCounter > DHT_COUNTER_TIMEOUT)
  {
    dhtCounter = 0;
    ReadDHT();
    ReadBMP();
    
    //debug!!!!!
   // SendDataToESP();
  }

  if (espCounter > 0)
  {
    espCounter--;
    // reset ESP
    if (espCounter == 100)
    {
      digitalWrite(ESP_RESET_PIN, 0);
    }

    if (espCounter <= 0)
    {
      digitalWrite(ESP_RESET_PIN, 1);
    }
  }

  sendCounter++;
  if (sendCounter > SEND_COUNTER_TIMEOUT)
  {
    sendCounter = 0;
    SendDataToESP();
    espCounter = ESP_REQUEST_TIMEOUT;
  }


  
  delay(10);   
}

