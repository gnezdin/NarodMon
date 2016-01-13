//Код для передатчика ProMini Narodmon, ThingSpeak
//
//
//
//
// Для получения отладочных сообщений на SoftSerial - объявить define DEBUG
// #define DEBUG

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h> // https://github.com/maniacbug/RF24
  #ifdef DEBUG
  #include <SoftwareSerial.h>
  #endif
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <BMP085.h>

// светодиод статуса
#define STS_LED_PIN 7

// пин сброса ESP
#define ESP_RESET_PIN 4 

// таймауты *10 (мс.)
#define DHT_COUNTER_TIMEOUT 6000
  // если отладка включена, то шлём данные на сервера с интервалом 5 мин.
  #ifdef DEBUG
  #define SEND_COUNTER_TIMEOUT 30000
  #else
  #define SEND_COUNTER_TIMEOUT 60000 // 60000
  #endif
#define ESP_REQUEST_TIMEOUT 3000

#define DHTPIN 8 
#define DHTTYPE DHT21

DHT dht(DHTPIN, DHTTYPE, 4);


BMP085 bmp = BMP085();  

const uint64_t pipe = 0xF1F9F8F3AALL; // индитификатор передачи, "труба"

RF24 radio(9, 10); // CE, CSN

// организуем софтовый UART
  #ifdef DEBUG
  SoftwareSerial softSerial(5, 6); // RX, TX
  #endif

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
double BAT = 0;
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

// счётчик для чтения данных DHT и BMP (1 раз в минуту)
long dhtCounter = 0;
// счётчик отправки на народмон и thingspeak (1 раз в 10 мин)
long sendCounter = 0;

char jsonIn[100];

// Переменные, создаваемые процессом сборки,
// когда компилируется скетч
extern int __bss_end;
extern void *__brkval;

// Функция, возвращающая количество свободного ОЗУ (RAM)
int memoryFree()
{
   int freeValue;
   if((int)__brkval == 0)
      freeValue = ((int)&freeValue) - ((int)&__bss_end);
   else
      freeValue = ((int)&freeValue) - ((int)__brkval);
   return freeValue;
}

void ReadDHT()
{
  #ifdef DEBUG
  softSerial.print(F("[-> ReadDHT] "));
  softSerial.print(F("Memory Free: "));
  softSerial.println(memoryFree());
  #endif
      
  // READ DATA
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();   
    
  #ifdef DEBUG
  softSerial.print(F("[ReadDHT] h: "));
  softSerial.print(h);
  softSerial.print(F("; t: "));
  softSerial.println(t);
  #endif

  if (isnan(h) || isnan(t)) 
  {
    #ifdef DEBUG
    softSerial.println(F("[ReadDHT] isnan(h) | isnan(t)"));
    #endif
    return;
  }

    HUM = h;
    TEMP_IN = t;
}

void ReadBMP()
{
  #ifdef DEBUG
  softSerial.print(F("[-> ReadBMP] "));
  softSerial.print(F("Memory Free: "));
  softSerial.println(memoryFree());
  #endif
  long pres = 0; //bmp.readPressure();
  bmp.getPressure(&pres);
  long temp = 0;
  bmp.getTemperature(&temp);

  #ifdef DEBUG
  softSerial.print(F("[ReadBMP] pres: "));
  softSerial.print(pres);
  softSerial.print(F("; temp: "));
  softSerial.println(temp);
  #endif
  
  float tmp = temp;

  PRESS = (double) pres / 1000.0;
  TEMP_E = tmp * 0.1;

  #ifdef DEBUG
  softSerial.print(F("[ReadBMP] PRESS: "));
  softSerial.print(PRESS);
  softSerial.print(F("; TEMP_E: "));
  softSerial.println(TEMP_E);
  #endif
}

void SendDataToESP()
{
  #ifdef DEBUG
  softSerial.print(F("[-> SendDataToESP] "));
  softSerial.print(F("Memory Free: "));
  softSerial.println(memoryFree());
  #endif
  StaticJsonBuffer<200> jsonBuffer;
 
  JsonObject& root = jsonBuffer.createObject();
  root["temp_out"] = TEMP_OUT;
  root["bat"] = BAT;
  root["temp_in"] = TEMP_IN;
  root["hum"] = HUM;
  root["press"] = PRESS;
  root["temp_e"] = TEMP_E;
 
  root.printTo(Serial);
  
  #ifdef DEBUG
  softSerial.print(F("[SendDataToESP] JSON: "));
  root.printTo(softSerial);
  softSerial.println();
  softSerial.println(F("[<- SendDataToESP] "));
  #endif
}

void setup() 
{
  memset(jsonIn, 0, sizeof(jsonIn));

  Serial.begin(9600);
  
  #ifdef DEBUG
  softSerial.begin(9600);
  #endif
  
  delay(1000);

  #ifdef DEBUG
  softSerial.print(F("[-> Setup] "));
  softSerial.print(F("Memory Free: "));
  softSerial.println(memoryFree());
  #endif
  
  pinMode(ESP_RESET_PIN, OUTPUT);
  digitalWrite(ESP_RESET_PIN, 1);

  #ifdef DEBUG
  softSerial.println(F("[Setup] ESP_RESET_PIN set to 1"));
  #endif
  
  stsLed = 0;
  pinMode(STS_LED_PIN, OUTPUT);
  digitalWrite(STS_LED_PIN, 0);

  // NRF init
  radio.begin();
  delay(2);
  radio.setChannel(109); // канал (0-127)
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_HIGH);

  radio.openReadingPipe(1, pipe); // открываем первую трубу с индитификатором "pipe"
  radio.startListening(); // включаем приемник, начинаем слушать трубу

  #ifdef DEBUG
  softSerial.println(F("[Setup] NRF Init"));
  #endif

  dht.begin();
  bmp.init();   

  #ifdef DEBUG
  softSerial.println(F("[Setup] dht & bmp init"));
  softSerial.println(F("[Setup] Print variables & constants:"));
  softSerial.print(F("[Setup] SEND_COUNTER_TIMEOUT: "));
  softSerial.println(SEND_COUNTER_TIMEOUT);
  softSerial.print(F("[Setup] DHT_COUNTER_TIMEOUT: "));
  softSerial.println(DHT_COUNTER_TIMEOUT);
  softSerial.print(F("[Setup] stsLed: "));
  softSerial.println(stsLed);
  softSerial.println(F("[<- Setup] "));
  #endif
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
      #ifdef DEBUG
      softSerial.print(F("[-> loop. sw == true] "));
      softSerial.print(F("Memory Free: "));
      softSerial.println(memoryFree());
      softSerial.print(F("[loop. sw == true] jsonIn: "));
      softSerial.println(jsonIn);
      #endif
      // сброс счётчика ожидания ответа ESP
      if (espCounter > 0)
      {
         // если счётчик меньше 100, то на ESP уже пошёл RESET, поэтому снимаем RESET -> 1
         if (espCounter <= 100)
         {
            digitalWrite(ESP_RESET_PIN, 1);

            #ifdef DEBUG
            softSerial.print(F("[loop. sw == true] (espCounter <= 100); espCounter: "));
            softSerial.println(espCounter);
            softSerial.println(F("[loop. sw == true] ESP_RESET_PIN set to 1"));
            #endif
         }
         
         espCounter = 0;

         #ifdef DEBUG
         softSerial.print(F("[loop. sw == true] (espCounter > 0); espCounter: "));
         softSerial.println(espCounter);
         softSerial.println(F("[loop. sw == true] espCounter set to 0"));
         #endif
      }
      
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& root = jsonBuffer.parseObject(jsonIn);

      if (!root.success())
      {
         #ifdef DEBUG
         softSerial.println(F("[loop. sw == true] jsonBuffer.parseObject -> !root.success"));
         #endif
        
         return;
      }

      #ifdef DEBUG
      softSerial.println(F("[loop. sw == true] jsonBuffer.parseObject -> root.success"));
      #endif

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

      #ifdef DEBUG
      softSerial.print(F("[loop. sw == true] variables after json parse: "));
      softSerial.print(F("Memory Free: "));
      softSerial.println(memoryFree());
      softSerial.print(F("[loop. sw == true] WIFI_STATUS: "));
      softSerial.println(WIFI_STATUS);
      softSerial.print(F("[loop. sw == true] THINGSPEAK_STATUS: "));
      softSerial.println(THINGSPEAK_STATUS);
      softSerial.print(F("[loop. sw == true] NARODMON_STATUS: "));
      softSerial.println(NARODMON_STATUS);
      #endif
      
      // очищаем входной буфер
      memset(jsonIn, 0, sizeof(jsonIn));
      sw = false;
      jsonCnt = 0;    

      #ifdef DEBUG
      softSerial.println(F("[loop. sw == true] Clear jsonIn Buffer; sw set to false; jsonCnt set to 0"));
      softSerial.println(F("[loop. sw == true] stsLed calculating"));
      #endif
       
      // отображаем статус на LED
      if ((WIFI_STATUS) & (THINGSPEAK_STATUS) & (NARODMON_STATUS))
      {
        stsLed = 1;

        #ifdef DEBUG
        softSerial.println(F("[loop. sw == true] stsLed set to 1"));
        #endif
      }
       else if ((WIFI_STATUS) & (!THINGSPEAK_STATUS) & (!NARODMON_STATUS))
       {
          stsLed = 2;

          #ifdef DEBUG
          softSerial.println(F("[loop. sw == true] stsLed set to 2"));
          #endif
       }
       else if (!WIFI_STATUS)
       {
          stsLed = 0;

          #ifdef DEBUG
          softSerial.println(F("[loop. sw == true] stsLed set to 0"));
          #endif
       }
        else if ((WIFI_STATUS) & (!THINGSPEAK_STATUS) & (NARODMON_STATUS))
        {
           stsLed = 3;

           #ifdef DEBUG
           softSerial.println(F("[loop. sw == true] stsLed set to 3"));
           #endif
        }
        else if ((WIFI_STATUS) & (THINGSPEAK_STATUS) & (!NARODMON_STATUS))
        {
           stsLed = 4;

           #ifdef DEBUG
           softSerial.println(F("[loop. sw == true] stsLed set to 4"));
           #endif
        }

   #ifdef DEBUG
   softSerial.println(F("[<- loop. sw == true] "));
   #endif
           
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
    #ifdef DEBUG
    softSerial.print(F("[-> loop. NRF data available] "));
    softSerial.print(F("Memory Free: "));
    softSerial.println(memoryFree());
    #endif
           
    byte data[8];
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
    
    // читаем данные и указываем сколько байт читать
    bool done = radio.read(&data, sizeof(data));

    #ifdef DEBUG
    softSerial.print(F("[loop. NRF data available] done: "));
    softSerial.println(done);
    softSerial.print(F("[loop. NRF data available] data: "));
    for(int it = 0; it < 8; it++)
    {
       softSerial.print(data[it], HEX);
       softSerial.print(", ");
    }
    softSerial.println();
    #endif


    byte  pos = 0;
    //флаг на случай, если пришли одни нули от ESP
    bool noolFlag = true;

    for (byte i = 0; i < 4; i++)
    {
      tmp.buf[i] = data[pos]; 
      if (tmp.buf[i] != 0) noolFlag = false;
      pos++;
    }

    if (!noolFlag)
    {
      TEMP_OUT = tmp.f;
      #ifdef DEBUG
      softSerial.print(F("[loop. NRF data available] !noolFlag; TEMP_OUT: "));
      softSerial.println(TEMP_OUT);
      #endif
    }

    // получаем значение напряжения
    pos = 4;
    noolFlag = true;
    for (byte i = 0; i < 4; i++)
    {
      lng.lBuf[i] = data[pos];
      if (lng.lBuf[i] != 0) noolFlag = false; 
      pos++;
    }

    if(!noolFlag)
    {  
      BAT = (double) lng.l / 1000.0 ; //mV -> V
      
      #ifdef DEBUG
      softSerial.print(F("[loop. NRF data available] !noolFlag; BAT: "));
      softSerial.println(BAT);
      #endif
    }

    #ifdef DEBUG
    softSerial.println(F("[<- loop. NRF data available] "));
    #endif
}

 // считываем DHT
 // чтение датчиков
 dhtCounter++;
 if (dhtCounter > DHT_COUNTER_TIMEOUT)
 {
    #ifdef DEBUG
    softSerial.print(F("[loop. dhtCounter > DHT_COUNTER_TIMEOUT] "));
    softSerial.print(F("Memory Free: "));
    softSerial.println(memoryFree());
    #endif
      
    dhtCounter = 0;
    ReadDHT();
    ReadBMP();
 }

  if (espCounter > 0)
  {
    espCounter--;
    // reset ESP
    if (espCounter == 100)
    {
      digitalWrite(ESP_RESET_PIN, 0);

      #ifdef DEBUG
      softSerial.println(F("[loop. espCounter == 100] ESP_RESET_PIN set to 0"));
      #endif
    }

    if (espCounter <= 0)
    {
      digitalWrite(ESP_RESET_PIN, 1);

      #ifdef DEBUG
      softSerial.println(F("[loop. espCounter == 0] ESP_RESET_PIN set to 1"));
      #endif
    }
  }

  sendCounter++;
  if (sendCounter > SEND_COUNTER_TIMEOUT)
  {
    #ifdef DEBUG
    softSerial.println(F("[loop. sendCounter > SEND_COUNTER_TIMEOUT] sendCounter set to 0"));
    #endif
      
    sendCounter = 0;
    SendDataToESP();
    espCounter = ESP_REQUEST_TIMEOUT;

    #ifdef DEBUG
    softSerial.print(F("[loop. sendCounter > SEND_COUNTER_TIMEOUT] espCounter set to "));
    softSerial.println(espCounter);
    #endif
  }
  
  delay(10);   
}

