//Код для Esp8266 (ESP01) Narodmon
//

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

// 1 c = 2
#define WIFI_CONNECT_TIMEOUT 40

const char* ssid     = "WdLink"; // название и пароль точки доступа
const char* password = "aeroglass";
const char* deviceMac = "1A:FE:34:FC:B3:44";

const char* host = "narodmon.ru";
const int httpPort = 8283;

// replace with your channel's thingspeak API key, 
String tsApiKey = "SBS8SASVY5E921Z6"; 
const char* tsServer = "api.thingspeak.com";

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
int INTERNET_STATUS = 0;

char jsonIn[100];


void ConnectToWiFi()
{
    // Подключаемся к wifi
    WIFI_STATUS = 0;
    INTERNET_STATUS = 0;
//  Serial.println();
//  Serial.println();
//  Serial.print("Connecting to ");
//  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int wifiCounter = 0;
  while ((WiFi.status() != WL_CONNECTED) & (wifiCounter < WIFI_CONNECT_TIMEOUT)) 
  {
    wifiCounter++;
    delay(500);
   // Serial.print(".");
  }

//  Serial.println();
//  Serial.println("WiFi connected");  
//  Serial.print("IP address: ");
//  Serial.println(WiFi.localIP());
//  Serial.print("MAC address: ");
//  Serial.println(WiFi.macAddress());
//  Serial.println();
  if (WiFi.status() ==  WL_CONNECTED)
  {
    WIFI_STATUS = 1;
  }
//  SendDataToArduino();
}

void SendDataToArduino()
{
  //
  // Step 1: Reserve memory space
  //
  StaticJsonBuffer<100> jsonBuffer;

  //
  // Step 2: Build object tree in memory
  //
  JsonObject& js = jsonBuffer.createObject();
  js["wifi_status"] = WIFI_STATUS;
  js["internet_status"] = INTERNET_STATUS;

//
// Step 3: Generate the JSON string
//
js.printTo(Serial);
}


bool SendDataToTS()
{
  bool result = false;

  // проверяем подключен ли WiFi и переподключаемся, если что
  if (WiFi.status() != WL_CONNECTED) 
  {
      WiFi.disconnect();
      ConnectToWiFi();
  }

  if (WiFi.status() != WL_CONNECTED) 
  {
      return result;
  }

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(tsServer,80)) 
  {
    //Serial.println("Connect to TS Server failed");
    return result;
  }

 // Serial.println("Connect to TS Server OK"); 
  String postStr = "api_key=" + tsApiKey;
           postStr +="&field1=";
           postStr += String(TEMP_OUT);
           postStr +="&field2=";
           postStr += String(BAT);
           postStr +="&field3=";
           postStr += String(TEMP_IN);
           postStr +="&field4=";
           postStr += String(HUM);
           postStr +="&field5=";
           postStr += String(PRESS);
           postStr +="&field6=";
           postStr += String(TEMP_E);
           postStr += "\r\n\r\n";
 
     client.print("POST /update HTTP/1.1\n"); 
     client.print("Host: api.thingspeak.com\n"); 
     client.print("Connection: close\n"); 
     client.print("X-THINGSPEAKAPIKEY: "+tsApiKey+"\n"); 
     client.print("Content-Type: application/x-www-form-urlencoded\n"); 
     client.print("Content-Length: "); 
     client.print(postStr.length()); 
     client.print("\n\n"); 
     client.print(postStr);

     // читаем ответ с и отправляем его в сериал
//     Serial.print("Requesting: ");  
//     while(client.available())
//     {
//       String line = client.readStringUntil('\r');
//       Serial.print(line); // хотя это можно убрать
//     }
     
     client.stop();
     
     result = true;
     return result;
}

bool SendDataToNarodMon()

{
   // debug
//   Serial.println("SendData ...");
//   Serial.print("TEMP_OUT: ");
//   Serial.println(TEMP_OUT);
//   Serial.print("BAT: ");
//   Serial.println(BAT);
//   Serial.print("TEMP_IN: ");
//   Serial.println(TEMP_IN);
//   Serial.print("HUM: ");
//   Serial.println(HUM);
//   Serial.print("PRESS: ");
//   Serial.println(PRESS);
//   Serial.print("TEMP_E: ");
//   Serial.println(TEMP_E);

bool result = false;

  // проверяем подключен ли WiFi и переподключаемся, если что
  if (WiFi.status() != WL_CONNECTED) 
  {
      WiFi.disconnect();
      ConnectToWiFi();
  }

  if (WiFi.status() != WL_CONNECTED) 
  {
      return result;
  }
    //  // подключаемся к серверу 
//  Serial.print("connecting to ");
//  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  if (!client.connect(host, httpPort)) 
  {   
     //Serial.println("connection to NarodMon failed");
     return result;
  }
  
  // отправляем данные  
  //Serial.println("Sending..."); 
      // заголовок
  client.print("#");
  client.print(deviceMac); // отправляем МАС нашей ESP8266
  client.print("#");
  client.print("Novopolotsk_station"); // название устройства
//  client.print("#55.53582#28.65198"); // координаты местонахождения датчика
  client.println();
  
  // отправляем данные с первого датчика
  client.print("#"); 
  client.print("T1");
  client.print("#");
  client.print(TEMP_OUT);
  // название датчика
  client.print("#Окружающая температура");  
  client.println();
  
   // отправляем данные с 5 датчика
  client.print("#"); 
  client.print("PRESS");
  client.print("#");
  client.print(PRESS);
  // название датчика
  client.print("#Атмосферное давление");    
  client.println("##");
    
//  Serial.print("Requesting: ");  
  while(client.available())
  {
    String line = client.readStringUntil('\r');
    Serial.print(line); // хотя это можно убрать
    if (line.indexOf("OK") >= 0)
    {
     // NARODMON.STATUS = 1;
    }
  }
  
  client.stop();
 
 // Serial.println("Closing connection");

  result = true;
  return result;
}



void setup() 
{
  memset(jsonIn, 0, sizeof(jsonIn));
  Serial.begin(9600);
  
  ConnectToWiFi();
  SendDataToArduino();
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
     // Serial.print("json: ");
     // Serial.print(jsonIn);
      // парсим Json
      //
      // Step 1: Reserve memory space
      //
      StaticJsonBuffer<200> jsonBuffer;

      //
      // Step 2: Deserialize the JSON string
      //
      JsonObject& root = jsonBuffer.parseObject(jsonIn);

      if (!root.success())
      {
        Serial.println("parseObject() failed");
        return;
      }

      //
      // Step 3: Retrieve the values
      //

      if (root.containsKey("temp_out"))
      {
         TEMP_OUT = root["temp_out"];
      }

      if (root.containsKey("bat"))
      {
         BAT = root["bat"];
      }

      if (root.containsKey("temp_in"))
      {
         TEMP_IN = root["temp_in"];
      }

      if (root.containsKey("hum"))
      {
         HUM = root["hum"];
      }

      if (root.containsKey("press"))
      {
         PRESS = root["press"];
      }

      if (root.containsKey("temp_e"))
      {
         TEMP_E = root["temp_e"];
      }
      
      // очищаем входной буфер
      memset(jsonIn, 0, sizeof(jsonIn));
      sw = false;
      jsonCnt = 0;    

       // отправляем данные в интернет
      // SendDataToNarodMon();
      INTERNET_STATUS = 0;
      if (SendDataToTS())
      {
         INTERNET_STATUS = 1;  
      }
        else
        {
          INTERNET_STATUS = 0;
        }
      
      if (SendDataToNarodMon())
      {
         INTERNET_STATUS = 1;  
      }
        else
        {
          INTERNET_STATUS = 0;
        }
      SendDataToArduino();
  }  
  delay(10);
}

