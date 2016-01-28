#include <U8glib.h>
#include "my5x7rus.h"
#include "terminal.h"

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE|U8G_I2C_OPT_DEV_0);	// I2C / TWI 
//U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_DEV_0|U8G_I2C_OPT_NO_ACK|U8G_I2C_OPT_FAST);	// Fast I2C / TWI 
//U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NO_ACK);	// Display which does not send AC

bool led = false;
char str[2] = { 0,0 };
byte code = 100;

void draw(void) 
{
	/*str[0] = code;

	u8g.setFont(my5x7rus);
	u8g.setPrintPos(0, 8);
	u8g.print("Code: ");
	u8g.print(code, DEC);
	u8g.drawStr(50, 8, str);
	
	u8g.setFont(terminal);
	u8g.setPrintPos(0, 30);
	u8g.drawStr(0, 22, str);*/
	u8g.setFont(my5x7rus);
	u8g.setPrintPos(0, 20);
	unsigned char cha[] =  "я";
	u8g.println(cha[0], HEX);
	u8g.println(cha[1], HEX);
	u8g.println(cha[2], HEX);
	byte b = 'л';
	u8g.setPrintPos(0, 30);
	u8g.println(b, HEX);
    //u8g.drawStr(0, 30, "у");
	//u8g.drawStr(0, 15, "\x90\xe3\xe1");
	//u8g.setFont(u8g_font_helvB24n);
	//u8g.drawStr(0, 50, "+25,5");

}

void setup(void) 
{ 
	pinMode(13, OUTPUT);
	digitalWrite(13, 0);

    u8g.setColorIndex(1);        
	u8g.setContrast(255);
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
  code++;
  if (code > 200) code = 100;
  delay(1000);
}

