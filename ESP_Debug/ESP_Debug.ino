//Код для Esp8266 (ESP01) Narodmon
//

#include <ESP8266WiFi.h>

#define LED 2
#define LED_RED 15
#define LED_GREEN 12
#define LED_BLUE 13

#define ADC A0

const char* ssid     = "WdLink"; // название и пароль точки доступа
const char* password = "aeroglass";

String tsApiKey = "6EE0PANPMO4FELI6"; 
const char* tsServer = "thingspeak.com";

// период отправки данных, *100 мс
#define SEND_TIMEOUT 6000

int send_counter_timeout = SEND_TIMEOUT;
long send_counter = 0;

void SetLedError(bool val)
{
  digitalWrite(LED_RED, val);
}

void SendDataGet()
{
    SetLedError(0);
    digitalWrite(LED_GREEN, 1);
    Serial.println("");
    Serial.println("->[SEND_COUNTER]");
 
    send_counter_timeout = 0;
    send_counter ++;
    
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("[SEND_COUNTER] WiFi not connected!");
      SetLedError(1);
      digitalWrite(LED_GREEN, 0);
      return;
    }
    Serial.println("[SEND_COUNTER] WiFi connected");

    int lightSensorValue = analogRead(ADC);
    
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(tsServer, httpPort)) 
    {
      SetLedError(1);
      digitalWrite(LED_GREEN, 0);
      Serial.println("[SEND_COUNTER] Connection to Host Failed!");
      return;
    }
    Serial.println("[SEND_COUNTER] Connected to Host");
    // Создаем URI для запроса
    String url = "/update?key=";
    url += tsApiKey;
    url += "&field1=";
    url += lightSensorValue;
    url += "&field2=";
    url += send_counter;

    Serial.println("[SEND_COUNTER] Requesting URL:");
    Serial.println(url);

    // отправляем запрос на сервер
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
    "Host: " + tsServer + "\r\n" +
    "Connection: close\r\n\r\n");
    client.flush(); // ждем отправки всех данных
    Serial.println("[SEND_COUNTER] Request sended");

    Serial.println("[SEND_COUNTER] Host Answer:");
    // Read all the lines of the reply from server and print them to Serial
    while(client.available())
    {
      String line = client.readStringUntil('\r');
      //char line = client.read();
      Serial.print(line);
    }
    
    client.stop();
    
    Serial.println();
    Serial.println("<-[SEND_COUNTER] closing connection");
    Serial.println();
    digitalWrite(LED_GREEN, 0);
}

void SendDataPost()
{
    SetLedError(0);
    digitalWrite(LED_GREEN, 1);
    Serial.println("");
    Serial.println("->[SEND_COUNTER]");
 
    send_counter_timeout = 0;
    send_counter ++;
    
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("[SEND_COUNTER] WiFi not connected!");
      SetLedError(1);
      digitalWrite(LED_GREEN, 0);
      return;
    }
    Serial.println("[SEND_COUNTER] WiFi connected");

    int lightSensorValue = analogRead(ADC);
    
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(tsServer, httpPort)) 
    {
      SetLedError(1);
      digitalWrite(LED_GREEN, 0);
      Serial.println("[SEND_COUNTER] Connection to Host Failed!");
      return;
    }
    Serial.println("[SEND_COUNTER] Connected to Host");
    // Создаем POST для запроса
    String postStr = "api_key=" + tsApiKey;
    postStr +="&field1=";
    postStr += String(lightSensorValue);
    postStr +="&field2=";
    postStr += String(send_counter);
    postStr += "\r\n\r\n";

    Serial.println("[SEND_COUNTER] Requesting URL:");
    Serial.println(postStr);

    // отправляем запрос на сервер
    client.print("POST /update HTTP/1.1\n"); 
    client.print("Host: thingspeak.com\n"); 
    client.print("Connection: close\n"); 
    client.print("X-THINGSPEAKAPIKEY: "+tsApiKey+"\n"); 
    client.print("Content-Type: application/x-www-form-urlencoded\n"); 
    client.print("Content-Length: "); 
    client.print(postStr.length()); 
    client.print("\n\n"); 
    client.print(postStr);
     
    client.flush(); // ждем отправки всех данных
    Serial.println("[SEND_COUNTER] Request sended");

    Serial.println("[SEND_COUNTER] Host Answer:");
    // Read all the lines of the reply from server and print them to Serial
    while(client.available())
    {
      String line = client.readStringUntil('\r');
      //char line = client.read();
      Serial.print(line);
    }
    
    client.stop();
    
    Serial.println();
    Serial.println("<-[SEND_COUNTER] closing connection");
    Serial.println();
    digitalWrite(LED_GREEN, 0);
}


void setup() 
{
  pinMode(ADC, INPUT);
  
  pinMode(LED, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED, 1);
  digitalWrite(LED_RED, 1);
  digitalWrite(LED_GREEN, 1);
  digitalWrite(LED_BLUE, 0);

  Serial.begin(115200);
  delay(10000);
  digitalWrite(LED_RED, 0);
  digitalWrite(LED_GREEN, 0);
  delay(1000);

  // подключаемся к WiFi сети

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
  
  digitalWrite(LED, 0);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() 
{  
  send_counter_timeout ++;
  digitalWrite(LED_GREEN, 0);

  if (send_counter_timeout >= SEND_TIMEOUT)
  {
    SendDataGet();
  }
  
  delay(100);
}


