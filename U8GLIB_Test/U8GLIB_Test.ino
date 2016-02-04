#include <U8glib.h>
#include "cyrilic_6x10.h"
#include "terminal.h"

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE|U8G_I2C_OPT_DEV_0);	// I2C / TWI 
//U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_NO_ACK|U8G_I2C_OPT_FAST);	// Fast I2C / TWI 
//U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NO_ACK);	// Display which does not send AC

bool led = false;
int pageCounter = 0;
char str[2] = { 0,0 };
const char* strCap[] = { "ТЕМП. НА УЛИЦЕ", "ТЕМП. В КОМНАТЕ", "ВЛАЖН. В КОМНАТЕ", "АТМ. ДАВЛЕНИЕ"};
const char* strVal[] = { "-18.5 oC", "+22.3 oC", "56.3 %", "100.3 kPa" };

void draw(void) 
{
	u8g.setFont(cyrilic_6x10);
	u8g.drawStr(15, 10, strCap[pageCounter]);
	u8g.setFont(u8g_font_fub20);
	u8g.drawStr(0, 45, strVal[pageCounter]);

	u8g.drawHLine(0, 15, 128);
	//u8g.drawHLine(0, 52, 128);
}

void setup(void) 
{ 
	pinMode(13, OUTPUT);
	digitalWrite(13, 0);

    u8g.setColorIndex(1);        
	u8g.setContrast(255);
	u8g.setFontPosBaseline();
}

void loop(void) 
{
  // picture loop
  u8g.firstPage();  
  do {
    draw();
  } while( u8g.nextPage() );
  
  if (led)
  {
	  digitalWrite(13, 1);
  }
  else
  {
	  digitalWrite(13, 0);
  }
  
  led = !led;
  
  pageCounter++;
  if (pageCounter > 3) pageCounter = 0;

  delay(5000);
}

