// Код для передатчика ProMini Narodmon, ThingSpeak v.2.0
//
// Контроллер ProMini подключен к ESP8266 с прошивкой esp-link v. 2.1 b2
// Передача всех параметров и удалённое управление реализовано через MQTT посредством esp-link
//
// Для получения отладочных сообщений на SoftSerial - объявить define DEBUG
//#define DEBUG

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h> // https://github.com/maniacbug/RF24
 
#include <DHT.h>
#include <BMP085.h>
#include <Wire.h>
#include <espduino.h>
#include <mqtt.h>
#include <rest.h>

// светодиод статуса
#define STS_LED_PIN 7


// таймауты *10 (мс.)
#define DHT_COUNTER_TIMEOUT 6000
#define SEND_COUNTER_TIMEOUT 6000 // 60000
// период проверки статуса WiFi у ESP-LINK (* 10 мс)
#define WIFI_CHECK_TIMEOUT 1000
   
#define DHT11_PIN 6

// Initialize a connection to esp-link using the normal hardware serial port both for
// SLIP and for debug messages.
ESP esp(&Serial, 5);

// Initialize the MQTT client
MQTT mqtt(&esp);

REST rest(&esp);

String tsApiKey = "S2CRYTCZOHDR9FQG";
const char* tsServer = "api.thingspeak.com";

dht DHT;
BMP085 bmp = BMP085();  

const uint64_t pipe = 0xF1F9F8F3AALL; // индитификатор передачи, "труба"

RF24 radio(9, 10); // CE, CSN

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

// байт состояния датчика DHT
byte HUMSts = 6;

//DEBUG!!!
int cnt = 0;

// переменная для светодиода 0-выкл, 1-вкл, 2-мигание, 3 - одна вспышка, 4 - две вспышки
int  stsLed = 0;
int stsLedCounter = 0;

// счётчик для чтения данных DHT и BMP (1 раз в минуту)
long dhtCounter = 0;
// счётчик отправки на thingspeak и по  (1 раз в 10 мин)
long sendCounter = 0;

// статус Wi-Fi подключения
bool wifiIsConnected = false;
// MQTT status
bool mqtt_connected = false;
// ThingSpeak timer
int tsSendCounter = 0;
// счётчик проверки статуса WiFi у ESP-LINK
int wifiCheckCounter = 0;

// WiFi AP Data
const char* WIFI_SSID = "WdLink";
const char* WIFI_PASSWORD = "aeroglass";

// MQTT Broker Data
const char* MQTT_BROKER_HOST = "192.168.100.11";

// константы DHT STATUS

const char DHT_OK[] PROGMEM = "OK";
const char DHT_ERROR_CONNECT[] PROGMEM = "ERROR_CONNECT";
const char DHT_ERROR_TIMEOUT[] PROGMEM = "ERROR_TIMEOUT";
const char DHT_ERROR_CHECKSUM[] PROGMEM = "ERROR_CHECKSUM";
const char DHT_ERROR_ACK_L[] PROGMEM = "ERROR_ACK_L";
const char DHT_ERROR_ACK_H[] PROGMEM = "ERROR_ACK_H";
const char DHT_EMPTY[] PROGMEM = "";

const char* const dht_sts[7] PROGMEM = { DHT_OK, DHT_ERROR_CONNECT, DHT_ERROR_TIMEOUT,
DHT_ERROR_CHECKSUM, DHT_ERROR_ACK_L, DHT_ERROR_ACK_H, DHT_EMPTY };

// константы топиков для подписки/публикации
const char* TOPIC_HUMIDITY = "/home/weater_station/hum";
const char* TOPIC_TEMPERATURE_OUT = "/home/weater_station/temp_out";
const char* TOPIC_TEMPERATURE_IN = "/home/weater_station/temp_in";
const char* TOPIC_TEMPERATURE_E = "/home/weater_station/temp_e";
const char* TOPIC_PRESSURE = "/home/weater_station/pressure";
const char* TOPIC_RADIO_BAT = "/home/weater_station/radio_bat";
const char* TOPIC_DHT_STS = "/home/weater_station/dht_sts";



// MQTT FUNCTIONS

// Callback made from esp-link to notify of wifi status changes
// Here we just print something out for grins
void wifiCb(void* response)
{
	uint32_t status;
	RESPONSE res(response);

	if (res.getArgc() == 1) 
	{
		res.popArgs((uint8_t*)&status, 4);
		if (status == STATION_GOT_IP) {
			//debugPort.println("WIFI CONNECTED");
			//  mqtt.connect("192.168.100.3", 1883, false);
			wifiIsConnected = true;
			mqtt.connect(MQTT_BROKER_HOST, 1883); /*without security ssl*/
			stsLed = 1;
		}
		else 
		{
			wifiIsConnected = false;
			mqtt.disconnect();
			stsLed = 0;
		}

	}
}

// Callback when MQTT is connected
void mqttConnected(void* response)
{
	mqtt_connected = true;
	//debugPort.println("MQTT CONNECTED");
	
	// mqtt.subscribe(TOPIC_HUMIDITY_HH);
	// mqtt.subscribe(TOPIC_HUMIDITY_LL);
	// mqtt.subscribe(TOPIC_CONTROL_MODE);
	// mqtt.subscribe(TOPIC_FAN);
	// mqtt.subscribe(TOPIC_LIGHT);

	//PublishAllData();
}
void mqttDisconnected(void* response)
{
	mqtt_connected = false;
	//debugPort.println("MQTT DISCONNECTED");
}

// Callback when an MQTT message arrives for one of our subscriptions
void mqttData(void* response) 
{
	RESPONSE res(response);

	//Serial.print(F("Received: topic="));
	String topic = res.popString();
	//Serial.println(topic);

	//Serial.print(F("data="));
	String data = res.popString();
	//Serial.println(data);

	//byte val = 0;
	
	// Анализ входных данных и сохранение в EEPROM
	// if (topic.compareTo(TOPIC_HUMIDITY_LL) == 0)
	// {
		// val = (byte) data.toInt();
		// if ((val <= 100) & (val >= 0) & (LL != val))
		// {
			// LL = val;
			// EEPROM_write_byte(1, LL);
		// }
	// }
	// else if (topic.compareTo(TOPIC_HUMIDITY_HH) == 0)
	// {
		// val = (byte)data.toInt();
		// if ((val <= 100) & (val >= 0) & (HH != val))
		// {
			// HH = val;
			// EEPROM_write_byte(2, HH);
		// }
	// }
	// else if (topic.compareTo(TOPIC_LIGHT) == 0)
	// {
		// val = (byte)data.toInt();
		// if ((val < 2) & (val >= 0) & (LI != val))
		// {
			// LI = val;
			// digitalWrite(LIGHT, LI);
		// }
	// }
	// else if (topic.compareTo(TOPIC_CONTROL_MODE) == 0)
	// {
		// val = (byte)data.toInt();
		// if ((val < 2) & (val >= 0) & (CM != val))
		// {
			// CM = val;
			//EEPROM_write_byte(3, CM);
		// }
	// }
	// else if (topic.compareTo(TOPIC_FAN) == 0)
	// {
		// val = (byte)data.toInt();
		// if ((val < 2) & (val >= 0) & (FO != val))
		// {
			// FO = val;
			//если пришла команда извне на включение вентилятора, то переводим режим работы в "РУЧ." и публикуем изменение режима в MQTT
			// if ((FO == 1) & (CM == 1))
			// {
				// CM = 0;
				// PublishOneData(TOPIC_CONTROL_MODE);
			// }
			//если пришла команда извне на выключение вентилятора, то переводим режим работы в "АВТ." и публикуем изменение режима в MQTT
			// else if ((FO == 0) & (CM == 0))
			// {
				// CM = 1;
				// PublishOneData(TOPIC_CONTROL_MODE);
			// }
			//применяем полученную команду к вентилятору
			// digitalWrite(FAN, FO);
			// PublishOneData(TOPIC_FAN);
		// }
	// }
}

void mqttPublished(void* response)
{
	//debugPort.print("MQTT PUBLISHED");
}

// Передача всех данных по MQTT
//void PublishAllData()
//{
//	if ((!mqttConnected) | (!wifiIsConnected)) return;
//
//	 char buf[10];

	//itoa(HU, buf, 10);
	//mqtt.publish(TOPIC_HUMIDITY, buf, 0, 1);
	// itoa(TE, buf, 10);
	// mqtt.publish(TOPIC_TEMPERATURE, buf, 0, 1);
	// itoa(LL, buf, 10);
	// mqtt.publish(TOPIC_HUMIDITY_LL, buf, 0, 1);
	// itoa(HH, buf, 10);
	// mqtt.publish(TOPIC_HUMIDITY_HH, buf, 0, 1);
	// itoa(LI, buf, 10);
	// mqtt.publish(TOPIC_LIGHT, buf, 0, 1);
	// itoa(CM, buf, 10);
	// mqtt.publish(TOPIC_CONTROL_MODE, buf, 0, 1);
	// itoa(FO, buf, 10);
	// mqtt.publish(TOPIC_FAN, buf, 0, 1);
	
	// char buf1[20];
	// strcpy_P(buf1, (char*)pgm_read_word(&(dht_sts[DH])));
	// mqtt.publish(TOPIC_HUMIDITY_SENSOR_STATUS, buf1, 0, 1);
//}

// Передача одного параметра по MQTT PublishOneData(const char * mqtt_topic_name)
void PublishOneData(const char* mqtt_topic_name)
{
	//Serial.println("PublishOneData");
	if ((!mqttConnected) | (!wifiIsConnected)) return;

	char val[20] = "";

	 String topicName = String(mqtt_topic_name);
	// Serial.print("topic: ");
	// Serial.println(topicName);
	 
	 if (topicName.compareTo(TOPIC_HUMIDITY) == 0)
	 {
		// Serial.println("TOPIC_HUMIDITY");
		 dtostrf(HUM, 1, 1, val);
		// Serial.print("Hum: ");
		// Serial.println(val);
		 mqtt.publish(mqtt_topic_name, val, 0, 1);
		 
	 }
	 else if (topicName.compareTo(TOPIC_TEMPERATURE_OUT) == 0)
	 {
		 dtostrf(TEMP_OUT, 1, 1, val);
		 mqtt.publish(mqtt_topic_name, val, 0, 1);
	 }
	 else if (topicName.compareTo(TOPIC_TEMPERATURE_IN) == 0)
	 {
		 dtostrf(TEMP_IN, 1, 1, val);
		 mqtt.publish(mqtt_topic_name, val, 0, 1);
	 }
	 else if (topicName.compareTo(TOPIC_TEMPERATURE_E) == 0)
	 {
		 dtostrf(TEMP_E, 1, 1, val);
		 mqtt.publish(mqtt_topic_name, val, 0, 1);
	 }
	 else if (topicName.compareTo(TOPIC_PRESSURE) == 0)
	 {
		 dtostrf(PRESS, 1, 1, val);
		 mqtt.publish(mqtt_topic_name, val, 0, 1);
	 }
	 else if (topicName.compareTo(TOPIC_RADIO_BAT) == 0)
	 {
		 dtostrf(BAT, 1, 1, val);
		 mqtt.publish(mqtt_topic_name, val, 0, 1);
	 }
	 else if (topicName.compareTo(TOPIC_DHT_STS) == 0)
	 {
		 strcpy_P(val, (char*)pgm_read_word(&(dht_sts[HUMSts])));
		 mqtt.publish(mqtt_topic_name, val, 0, 1);
	 }
}

void ReadDHT()
{
	//Serial.println("ReadDHT()");
	// DEBUG !!!
	cnt++;
	HUM = (double) cnt;
	PublishOneData(TOPIC_HUMIDITY);

		// READ DATA
	byte newHUMSts = 0;
  	int chk = DHT.read11(DHT11_PIN);
  	switch (chk)
  	{
	  	case DHTLIB_OK:
	  	 newHUMSts = 0;
	  	break;
	  	case DHTLIB_ERROR_CONNECT:
	  	 newHUMSts = 1;
	  	break;
	  	case DHTLIB_ERROR_TIMEOUT:
	  	 newHUMSts = 2;
	  	break;	
		  case DHTLIB_ERROR_CHECKSUM:
	  	 newHUMSts = 3;
	  	break;
	  	case DHTLIB_ERROR_ACK_L:
	  	 newHUMSts = 4;
	  	break;
	  	case DHTLIB_ERROR_ACK_H:
	  	 newHUMSts = 5;
	  	break;
	  	default:
	  	 newHUMSts = 6;
	  	break;
  	}

	if (newHUMSts != HUMSts)
	{
		HUMSts = newHUMSts;
		PublishOneData(TOPIC_DHT_STS);
	}

  	if ((chk == DHTLIB_OK) & (DHT.humidity > 0) & (DHT.humidity <= 100) & (DHT.temperature > 0) & (DHT.temperature < 90))
	{
		  HUM = DHT.humidity;
		  TEMP_IN = DHT.temperature;

		  // Publish MQTT
		  PublishOneData(TOPIC_HUMIDITY);		
		  PublishOneData(TOPIC_TEMPERATURE_IN);	 
	}
}

void ReadBMP()
{
	//Serial.println("ReadBMP()");
  long pres = 0; //bmp.readPressure();
  bmp.getPressure(&pres);
  long temp = 0;
  bmp.getTemperature(&temp);

  float tmp = temp;

  PRESS = (double) pres / 1000.0;
  TEMP_E = tmp * 0.1;

  PublishOneData(TOPIC_PRESSURE);
  PublishOneData(TOPIC_TEMPERATURE_E);
}

void SendDataToESP()
{
	if (!rest.begin(tsServer))
	{
		stsLed = 2;
		return;
	}

	char response[266];
	if (wifiIsConnected) 
	{
		
			char buff[64];
			char str_hum[20];
			dtostrf(HUM, 1, 1, str_hum);
			sprintf(buff, "/update?api_key=6EE0PANPMO4FELI6&field1=%s", str_hum);
			
			rest.get((const char*)buff);
			

			if (!rest.getResponse(response, 266) == HTTP_STATUS_OK) 
			{
				stsLed = 3;
			} 
			else
			{
				stsLed = 1;
			}
	}

}

void setup() 
{
  Serial.begin(57600);
  
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

  // ESP INIT
  esp.enable();
  delay(500);
  esp.reset();
  delay(500);
  while (!esp.ready());

  //debugPort.println("ARDUINO: setup mqtt client");
  if (!mqtt.begin("Weater_Station", "", "", 120, 1))
  {
	  return;
  }


  /*setup mqtt events */
  mqtt.connectedCb.attach(&mqttConnected);
  mqtt.disconnectedCb.attach(&mqttDisconnected);
  mqtt.publishedCb.attach(&mqttPublished);
  mqtt.dataCb.attach(&mqttData);

  /*setup wifi*/
  //debugPort.println("ARDUINO: setup wifi");
  esp.wifiCb.attach(&wifiCb);

  esp.wifiConnect(WIFI_SSID, WIFI_PASSWORD);
}

void loop() 
{
   esp.process();

	// Проверка статуса WiFi
	wifiCheckCounter++;
	if (wifiCheckCounter >= WIFI_CHECK_TIMEOUT)
	{
		wifiCheckCounter = 0;
		
		if (!wifiIsConnected)
		{
			esp.wifiConnect(WIFI_SSID, WIFI_PASSWORD);
		}
	}
   
      // отображаем статус на LED
      // if ((WIFI_STATUS) & (THINGSPEAK_STATUS) & (NARODMON_STATUS))
      // {
        // stsLed = 1;

        // #ifdef DEBUG
        // softSerial.println(F("[loop. sw == true] stsLed set to 1"));
        // #endif
      // }
       // else if ((WIFI_STATUS) & (!THINGSPEAK_STATUS) & (!NARODMON_STATUS))
       // {
          // stsLed = 2;

          // #ifdef DEBUG
          // softSerial.println(F("[loop. sw == true] stsLed set to 2"));
          // #endif
       // }
       // else if (!WIFI_STATUS)
       // {
          // stsLed = 0;

          // #ifdef DEBUG
          // softSerial.println(F("[loop. sw == true] stsLed set to 0"));
          // #endif
       // }
        // else if ((WIFI_STATUS) & (!THINGSPEAK_STATUS) & (NARODMON_STATUS))
        // {
           // stsLed = 3;

           // #ifdef DEBUG
           // softSerial.println(F("[loop. sw == true] stsLed set to 3"));
           // #endif
        // }
        // else if ((WIFI_STATUS) & (THINGSPEAK_STATUS) & (!NARODMON_STATUS))
        // {
           // stsLed = 4;

           // #ifdef DEBUG
           // softSerial.println(F("[loop. sw == true] stsLed set to 4"));
           // #endif
        // }

 
  
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
    }

	PublishOneData(TOPIC_TEMPERATURE_OUT);
	PublishOneData(TOPIC_RADIO_BAT);

}

 // считываем DHT
 // чтение датчиков
 dhtCounter++;
 if (dhtCounter > DHT_COUNTER_TIMEOUT)
 {
    dhtCounter = 0;
    ReadDHT();
  //  ReadBMP();


 }

 
  sendCounter++;
  if (sendCounter > SEND_COUNTER_TIMEOUT)
  {
    sendCounter = 0;
    SendDataToESP();
  }
  
  delay(10);   
}

