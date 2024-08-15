#include <Adafruit_SH110X.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "oled.h"

Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
void display_ini(){
  display.begin(0x3c, true);
}

static const unsigned char PROGMEM image_music_10px_bits[] = {0x07,0x00,0x07,0x80,0x04,0x00,0x04,0x00,0x04,0x00,0x7c,0x00,0xfc,0x00,0xfc,0x00,0x78,0x00,0x00,0x00};//nota musical

void show_play(uint8_t fig, uint8_t bpm) //fig--> 0-3
{
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(4);
  display.setCursor(20, 22);
  display.setTextWrap(false);
  display.print("PLAY");
  /*display.drawLine(37, 59, 37, 5, SH110X_WHITE); //recta cte
  display.drawLine(37, 5, 91, 32, SH110X_WHITE); //lado 1
  display.drawLine(37, 59, 91, 32, SH110X_WHITE); //lado 2
  display.drawLine(91, 32 , 91, 32, SH110X_WHITE); // line central*/
  switch (fig){
    case 0:
    display.drawBitmap( 109, 1, image_music_10px_bits, 10, 10, SH110X_WHITE);
    display.drawPixel(112, 8, SH110X_BLACK);
    display.drawPixel(111, 8, SH110X_BLACK);
    display.drawPixel(111, 7, SH110X_BLACK);
    display.drawPixel(112, 7, SH110X_BLACK);
    display.drawPixel(117, 2, SH110X_BLACK);
    display.drawPixel(116, 2, SH110X_BLACK);
    display.drawPixel(116, 1, SH110X_BLACK);
    display.drawPixel(115, 1, SH110X_BLACK);
    display.drawPixel(115, 2, SH110X_BLACK);
    break;
    case 1:
    display.drawBitmap( 109, 1, image_music_10px_bits, 10, 10, SH110X_WHITE);
    display.drawPixel(117, 2, SH110X_BLACK);
    display.drawPixel(116, 2, SH110X_BLACK);
    display.drawPixel(116, 1, SH110X_BLACK);
    display.drawPixel(115, 1, SH110X_BLACK);
    display.drawPixel(115, 2, SH110X_BLACK);
    break;
    
    case 2:
    display.drawBitmap( 109, 1, image_music_10px_bits, 10, 10, SH110X_WHITE);
    break;
    
    case 3:
    display.drawBitmap( 109, 1, image_music_10px_bits, 10, 10, SH110X_WHITE);
    display.drawPixel(115, 4, SH110X_WHITE);
    display.drawPixel(116, 4, SH110X_WHITE);
    display.drawPixel(115, 5, SH110X_WHITE);
    display.drawPixel(116, 5, SH110X_WHITE);
    display.drawPixel(117, 5, SH110X_WHITE);
    break;

    default:
    display.drawBitmap( 109, 1, image_music_10px_bits, 10, 10, SH110X_WHITE);

  }
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.setTextWrap(false);
  display.print("BPM:");
  display.print(bpm);
  display.display();
}

void animated_stop()
{
  for (int aum = 0; aum<=27; aum=aum+3) //height =54
    {
      display.clearDisplay();
      display.drawLine(37, 59, 37, 5, SH110X_WHITE); //recta cte
      display.drawLine(37, 5, 91, 32 - aum, SH110X_WHITE); //lado 1
      display.drawLine(37, 59, 91, 32 + aum, SH110X_WHITE); //lado 2
      display.drawLine(91, 32 + aum, 91, 32 - aum, SH110X_WHITE); // line central
      display.display();
      delay(1);
    }
    display.fillRect(37, 5, 54, 54, SH110X_WHITE);
    display.fillRect(40, 8, 49, 49, SH110X_BLACK);
    display.display();  
}

void show_stop()
{
    display.clearDisplay();       //limpia display
    display.setTextSize(4);       //tamaño texto
    display.setTextColor(SH110X_WHITE);
    display.setCursor(20,22);       //posicion (x,y)
    display.println("STOP");
    display.display();
}

void show_number(uint8_t num)
{
    display.clearDisplay();       //limpia display
    display.setTextSize(7);       //tamaño texto
    display.setTextColor(SH110X_WHITE);
    if(num>=10){
    display.setCursor(32,10);       //posicion (x,y)
    }
    else{
    display.setCursor(50,10);       //posicion (x,y)
    }
    display.print(num);
    display.display();
}

void show_bpm(uint8_t n)
{
    display.clearDisplay();       //limpia display
    display.setTextSize(3);       //tamaño texto
    display.setTextColor(SH110X_WHITE);
    display.setCursor(38,9);       //posicion (x,y)
    display.println("BPM");
    if(n>=100){
    display.setCursor(38,38);       //posicion (x,y)
    }
    else{
    display.setCursor(49,38);       //posicion (x,y)
    }
    display.print(n);
    display.display();
}

void show_max_notes()
{
    display.clearDisplay();       //limpia display
    display.setTextSize(4);       //tamaño texto
    display.setTextColor(SH110X_WHITE);
    display.setCursor(20,22);       //posicion (x,y)
    display.println("No More");
    display.display();
}

void show_sync()
{
    display.clearDisplay();       //limpia display
    display.setTextSize(2);       //tamaño texto
    display.setTextColor(SH110X_WHITE);
    display.setCursor(20,10);       //posicion (x,y)
    display.println("SYNC PLAY");
    display.display();
}