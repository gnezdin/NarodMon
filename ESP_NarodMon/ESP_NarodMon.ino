//Код для Esp8266 (ESP01) Narodmon
//

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>

const char* ssid     = "WdLink"; // название и пароль точки доступа
const char* password = "aeroglass";
const char* deviceMac = "1A:FE:34:FC:B3:44";

const char* host = "narodmon.ru";
const int httpPort = 8283;

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

char jsonIn[100];

void SendDataToNarodMon()
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

// проверяем подключен ли WiFi и переподключаемся, если что
if (WiFi.status() != WL_CONNECTED) 
{
    WiFi.disconnect();
    ConnectToWiFi();

    //  // подключаемся к серверу 
  Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  
  if (!client.connect(host, httpPort)) 
  {
    Serial.println("connection failed");
    return;
  }
  
  // отправляем данные  
  Serial.println("Sending..."); 
      // заголовок
  client.print("#");
  client.print(deviceMac); // отправляем МАС нашей ESP8266
  client.print("#");
  client.print("Novopolotsk_station"); // название устройства
//  client.print("#55.53582#28.65198"); // координаты местонахождения датчика
  client.println();
  
  // отправляем данные с первого датчика
  client.print("#"); 
  client.print("TEMP_OUT");
  client.print("#");
  client.print(TEMP_OUT);
  // название датчика
  client.print("#Окружающая температура");  
  client.println();

  // отправляем данные с 2 датчика
  client.print("#"); 
  client.print("BAT");
  client.print("#");
  client.print(BAT);
  // название датчика
  client.print("#Напряжение батарей");  
  client.println();

  // отправляем данные с 3 датчика
  client.print("#"); 
  client.print("TEMP_IN");
  client.print("#");
  client.print(TEMP_IN);
  // название датчика
  client.print("#Температура внутри помещения");  
  client.println();

   // отправляем данные с 4 датчика
  client.print("#"); 
  client.print("HUM");
  client.print("#");
  client.print(HUM);
  // название датчика
  client.print("#Отн. влажность внутри помещения");  
  client.println();

   // отправляем данные с 5 датчика
  client.print("#"); 
  client.print("PRESS");
  client.print("#");
  client.print(PRESS);
  // название датчика
  client.print("#Атмосферное давление");  
  client.println();

   // отправляем данные с 6 датчика
  client.print("#"); 
  client.print("TEMP_E");
  client.print("#");
  client.print(TEMP_E);
  // название датчика
  client.print("#Температура электроники");  
  client.println();
 
   client.print("##");
    
  delay(10);

  // читаем ответ с и отправляем его в сериал
  Serial.print("Requesting: ");  
  while(client.available())
  {
    String line = client.readStringUntil('\r');
    Serial.print(line); // хотя это можно убрать
  }
  
  client.stop();
  Serial.println();
  Serial.println();
  Serial.println("Closing connection");
}

}

void ConnectToWiFi()
{
    // Подключаемся к wifi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");  
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println();
}


void setup() 
{
  memset(jsonIn, 0, sizeof(jsonIn));
  Serial.begin(9600);
  delay(10);
  ConnectToWiFi();
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
       SendDataToNarodMon();
  }  
  delay(10);
}

