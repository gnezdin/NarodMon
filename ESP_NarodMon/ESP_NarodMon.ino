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

// replace with your channel's thingspeak API key, 
String tsApiKey = "SBS8SASVY5E921Z6"; 
const char* tsServer = "api.thingspeak.com";

// очередной принятый по UART байт
byte incommingByte = 0;

// счётчик байтов входного буфера Json
byte jsonCnt = 0;

// флаг приёма окончания запроса Json
bool sw = false;

// Температура окр. воздуха, оС
double TEMP_OUT = 0;
// Напряжение батареи темп. передатчика, В
double BAT = 0;
// Температура в помещении (DHT21), оС
double TEMP_IN = 0;
// Влажность в помещении (DHT21), % 
double HUM = 0;
// Атм. давление, кПа (BMP180), кПа 
double PRESS = 0;
// Температура электроники (BMP180), оС 
double TEMP_E = 0;

// переменные от ESP8266
int WIFI_STATUS = 0;
int THINGSPEAK_STATUS = 0;
int NARODMON_STATUS = 0;

char jsonIn[100];

// Use WiFiClient class to create TCP connections
//  WiFiClient client;


void ConnectToWiFi()
{
    // Подключаемся к wifi
    WIFI_STATUS = 0;
    THINGSPEAK_STATUS = 0;
    NARODMON_STATUS = 0;
      
  WiFi.begin(ssid, password);
  
  int wifiCounter = 0;
  while ((WiFi.status() != WL_CONNECTED) & (wifiCounter < WIFI_CONNECT_TIMEOUT)) 
  {
    wifiCounter++;
    delay(500);
  }

  if (WiFi.status() ==  WL_CONNECTED)
  {
    WIFI_STATUS = 1;
  }
}

void SendDataToArduino()
{
  StaticJsonBuffer<100> jsonBuffer;

  JsonObject& js = jsonBuffer.createObject();
  js["wifi_status"] = WIFI_STATUS;
  js["thingspeak_status"] = THINGSPEAK_STATUS;
  js["narodmon_status"] = NARODMON_STATUS;

  js.printTo(Serial);
}


bool SendDataToTS()
{
  bool result = false;

  if (WiFi.status() != WL_CONNECTED) 
  {
      return result;
  }

  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(tsServer, httpPort)) 
  {    
      return result;
  }
    
  // Создаем URI для запроса
  String url = "/update?api_key=";
  url += tsApiKey;
  url += "&field1=";
  url += TEMP_OUT;
  url += "&field2=";
  url += BAT;
  url += "&field3=";
  url += TEMP_IN;
  url += "&field4=";
  url += HUM;
  url += "&field5=";
  url += PRESS;
  url += "&field6=";
  url += TEMP_E;

  // отправляем запрос на сервер
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
  "Host: " + tsServer + "\r\n" +
  "Connection: close\r\n\r\n");
  client.flush(); // ждем отправки всех данных

  result = true;
  
  unsigned long timeout = millis();
  while (client.available() == 0) 
  {
    if (millis() - timeout > 5000) 
    {
      //Serial.println("[SEND_COUNTER] Client Answer Timeout !");
      client.stop();
      return result;
    }
  }
 
  
  // Read all the lines of the reply from server and print them to Serial
  while(client.available())
  {
    String line = client.readStringUntil('\r');
    //Serial.print(line);
  }

  client.stop();  
  return result;
}

bool SendDataToNarodMon()

{
bool result = false;

  if (WiFi.status() != WL_CONNECTED) 
  {
      return result;
  }
  
  // подключаемся к серверу   
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 8283;
  
  if (!client.connect(host, httpPort)) 
  {   
     return result;
  }

  // отправляем данные   
  // заголовок
  client.print("#");
  client.print(deviceMac); // отправляем МАС нашей ESP8266
  client.print("#");
  client.print("Novopolotsk_station"); // название устройства
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
  client.print(PRESS * 7.5);// мм рт. ст.
  // название датчика
  client.print("#Атмосферное давление");    
  client.println("##");
    
  client.flush(); // ждем отправки всех данных
  result = true;
  
  unsigned long timeout = millis();
  while (client.available() == 0) 
  {
    if (millis() - timeout > 5000) 
    {
      //Serial.println("[SEND_COUNTER] Client Answer Timeout !");
      client.stop();
      return result;
    }
  }
 
  
  // Read all the lines of the reply from server and print them to Serial
  while(client.available())
  {
    String line = client.readStringUntil('\r');
    //Serial.print(line);
  }

  client.stop();  
  return result;
}



void setup() 
{
  memset(jsonIn, 0, sizeof(jsonIn));
  Serial.begin(9600);
  delay(1000);
  ConnectToWiFi();
  SendDataToArduino();//21 сек max
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
      StaticJsonBuffer<200> jsonBuffer;

      JsonObject& root = jsonBuffer.parseObject(jsonIn);

      if (!root.success())
      {
	    // очищаем входной буфер
        memset(jsonIn, 0, sizeof(jsonIn));
        sw = false;
        jsonCnt = 0;  
        return;
      }

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
      THINGSPEAK_STATUS = 0;
      NARODMON_STATUS = 0;

      // проверяем подключен ли WiFi и переподключаемся, если что
      if (WiFi.status() != WL_CONNECTED) 
      {
          WiFi.disconnect();
          ConnectToWiFi();
      }
       else
       {
         WIFI_STATUS = 1;     
       }
       
      if (SendDataToTS())
      {
         THINGSPEAK_STATUS = 1;        
      }
        else
        {
          THINGSPEAK_STATUS = 0;
        }
      
      if (SendDataToNarodMon())
      {
         NARODMON_STATUS = 1;  
      }
        else
        {
          NARODMON_STATUS = 0;
        }
      SendDataToArduino();
  }  
  delay(10);
}


