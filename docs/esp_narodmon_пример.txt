//Датчик для народного мониторинга на ESP8266
//https://youtu.be/Jti0vVLhXiI

#include <ESP8266WiFi.h>
#include <OneWire.h>

OneWire  ds(2);  // GPIO2 (a 4.7K resistor is necessary)

const char* ssid     = "******"; // название и пароль точки доступа
const char* password = "******";

const char* host = "narodmon.ru";
const int httpPort = 8283;

byte addr[8]; 
float temperature;

float getTemp(){
  byte data[12];  

  if (!ds.search(addr)) {
    Serial.println("No more addresses."); 
    while(1);
  }
   ds.reset_search(); 
 
  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      while(1);
  }

  
  ds.reset();            
  ds.select(addr);        
  ds.write(0x44);      
  delay(1000);   
  
  ds.reset();
  ds.select(addr);    
  ds.write(0xBE);          

  for (int i = 0; i < 9; i++) {           
    data[i] = ds.read();  
  }

  int raw = (data[1] << 8) | data[0]; 
  if (data[7] == 0x10) raw = (raw & 0xFFF0) + 12 - data[6];  
  return raw / 16.0;
    
} 

void setup() {
  Serial.begin(115200);
  delay(10);

}

void loop() {
     // забираем температуру.
  temperature = getTemp();

     // Подключаемся к wifi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
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
  
  // подключаемся к серверу 
  Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  // отправляем данные  
  Serial.println("Sending..."); 
      // заголовок
  client.print("#");
  client.print(WiFi.macAddress()); // отправляем МАС нашей ESP8266
  client.print("#");
  client.print("ESP8266"); // название устройства
//  client.print("#45.031660#39.004750"); // координаты местонахождения датчика
  client.println();
  
      // отправляем данные с градусника
    client.print("#"); 
    for(int i = 0; i < 8; i++) client.print(addr[i], HEX); // номер 18b20 
    client.print("#");
    client.print(temperature);
    client.print("#temp");  // название датчика
 
   client.println("##");
    
  delay(10);

  // читаем ответ с и отправляем его в сериал
  Serial.print("Requesting: ");  
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line); // хотя это можно убрать
  }
  
  client.stop();
  Serial.println();
  Serial.println();
  Serial.println("Closing connection");

  WiFi.disconnect(); // отключаемся от сети
  Serial.println("Disconnect WiFi.");
  
  delay(1000*60*10); // перекур 10 минут
}

