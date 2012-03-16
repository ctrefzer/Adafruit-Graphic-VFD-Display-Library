#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdlib.h>

#include "Adafruit_GFX.h"
#include "Adafruit_GP9002.h"
#include "glcdfont.c"

Adafruit_GP9002::Adafruit_GP9002(int8_t SCLK, int8_t MISO, int8_t MOSI, 
				 int8_t CS, int8_t DC) {
  _sclk = SCLK;
  _miso = MISO;
  _mosi = MOSI;
  _cs = CS;
  _dc = DC;

  constructor(128, 64);
}

void Adafruit_GP9002::begin(void) {
  // set pin directions
  pinMode(_mosi, OUTPUT);
  pinMode(_miso, INPUT);
  pinMode(_sclk, OUTPUT);
  pinMode(_dc, OUTPUT);
  pinMode(_cs, OUTPUT);

  clkport     = portOutputRegister(digitalPinToPort(_sclk));
  clkpinmask  = digitalPinToBitMask(_sclk);
  mosiport    = portOutputRegister(digitalPinToPort(_mosi));
  mosipinmask = digitalPinToBitMask(_mosi);
  csport    = portOutputRegister(digitalPinToPort(_cs));
  cspinmask = digitalPinToBitMask(_cs);
  dcport    = portOutputRegister(digitalPinToPort(_dc));
  dcpinmask = digitalPinToBitMask(_dc);
  misopin = portInputRegister(digitalPinToPort(_miso));
  misopinmask = digitalPinToBitMask(_miso);

  command(GP9002_DISPLAY);
  dataWrite(GP9002_DISPLAY_MONOCHROME);
  command(GP9002_LOWERADDR1);
  dataWrite(0x0);
  command(GP9002_HIGHERADDR1);
  dataWrite(0x0);
  command(GP9002_LOWERADDR2);
  dataWrite(0x0);
  command(GP9002_HIGHERADDR2);
  dataWrite(0x4);
  command(GP9002_OR);
  command(GP9002_CLEARSCREEN);
  command(GP9002_DISPLAY1ON);

  // hold the address so we can read and then write
  command(GP9002_ADDRHELD);
}



// updated high speed drawing!
void Adafruit_GP9002::drawFastVLine(uint16_t x, uint16_t orig_y, uint16_t h, uint16_t color) {
  if ((x >= width()) || (orig_y >= height())) return;
  //if ((orig_y+h) >= height()) 
  //  h = height() - orig_y -1;

  while (h && (orig_y % 8)) {
    drawPixel(x, orig_y, color);
    orig_y++;
    h--;
  }    

  if (h >= 8) {
    // calculate addr
    uint16_t addr = 0;
    addr = x*8;
    addr += orig_y/8;
    
    command(GP9002_ADDRINCR);
    command(GP9002_ADDRL);
    dataWrite(addr & 0xFF);
    command(GP9002_ADDRH);
    dataWrite(addr >> 8);
    command(GP9002_DATAWRITE);

    *dcport &= ~dcpinmask;
    *csport &= ~cspinmask;
    while (h >= 8) {
      // draw 8 pixels at once!
      if (color) 
	dataWrite(0xFF);
      else 
	dataWrite(0x00);
      h -= 8;
      orig_y += 8;
    }
    *csport |= cspinmask;
    command(GP9002_ADDRHELD);
  }
  
  while (h) {
    drawPixel(x, orig_y, color);
    orig_y++;
    h--;
  }
}

// the most basic function, set a single pixel
void Adafruit_GP9002::drawPixel(uint16_t x, uint16_t y, uint16_t color) {
  if ((x >= width()) || (y >= height()))
    return;

  uint8_t p;
  
  // calculate addr
  uint16_t addr = 0;
  addr = x*8;
  y = 63 - y;
  addr += y/8;

  command(GP9002_ADDRL);
  dataWrite(addr & 0xFF);
  command(GP9002_ADDRH);
  dataWrite(addr >> 8);
  command(GP9002_DATAREAD);
  dataRead();
  p = dataRead();

  if (color)
    p |= (1 << (7-(y % 8)));
  else
    p &= ~(1 << (7-(y % 8)));
  command(GP9002_DATAWRITE);
  dataWrite(p);
}

  

void Adafruit_GP9002::invert(boolean i) {
  // This is kinda clumsy but it does work
  // fill the opposite screen with all on pixels so we can invert!
  uint16_t addr = 0x400;

  command(GP9002_ADDRINCR);
  command(GP9002_ADDRL);
  dataWrite(addr & 0xFF);
  command(GP9002_ADDRH);
  dataWrite(addr >> 8);
  command(GP9002_DATAWRITE);

  if (i) {
    while (addr < 0x0800) {
      dataWrite(0xFF);
      addr++;
    }
    command(GP9002_XOR);
  } else {
    while (addr < 0x0800) {
      dataWrite(0x00);
      addr++;
    }
    command(GP9002_OR);
  }
  command(GP9002_ADDRHELD);

}

void Adafruit_GP9002::slowSPIwrite(uint8_t d) {
 for (uint8_t i=0; i<8; i++) {
   digitalWrite(_sclk, LOW);
   if (d & _BV(i)) {
     digitalWrite(_mosi, HIGH);
   } else {
     digitalWrite(_mosi, LOW);
   }
   digitalWrite(_sclk, HIGH);
 }
}

inline void Adafruit_GP9002::fastSPIwrite(uint8_t d) {
  for(uint8_t bit = 0x1; bit != 0x00; bit <<= 1) {
    *clkport &= ~clkpinmask;
    if(d & bit) *mosiport |=  mosipinmask;
    else        *mosiport &= ~mosipinmask;
    *clkport |=  clkpinmask;
  }
}

uint8_t Adafruit_GP9002::slowSPIread(void) {
 uint8_t reply = 0;
 for (uint8_t i=0; i<8; i++) {
   digitalWrite(_sclk, LOW);

   digitalWrite(_sclk, HIGH);
   if (digitalRead(_miso)) 
     reply |= _BV(i);
 }
 return reply;
}

inline uint8_t Adafruit_GP9002::fastSPIread(void) {
 uint8_t reply = 0;
 for (uint8_t i=0; i<8; i++) {
   *clkport &= ~clkpinmask;
   
   *clkport |=  clkpinmask;
   if ((*misopin) & misopinmask)
     reply |= _BV(i);
 }
 return reply;
}

void Adafruit_GP9002::command(uint8_t d) { 
  *dcport |= dcpinmask;
  *csport &= ~cspinmask;
  fastSPIwrite(d);
  *csport |= cspinmask;
  delayMicroseconds(1); // should be 400ns actually
}

inline void Adafruit_GP9002::dataWrite(uint8_t d) {
  *dcport &= ~dcpinmask;
  *csport &= ~cspinmask;

  fastSPIwrite(d);

  *csport |= cspinmask;
  delayMicroseconds(1); // should be 600ns actually
}
inline uint8_t Adafruit_GP9002::dataRead() {
  uint8_t r;

  *dcport &= ~dcpinmask;
  *csport &= ~cspinmask;

  r = fastSPIread();

  *csport |= cspinmask;
  delayMicroseconds(1); 
  
 return r;
}

void Adafruit_GP9002::setBrightness(uint8_t val) {
  
}

// clear everything
void Adafruit_GP9002::clearDisplay(void) {
  command(GP9002_CLEARSCREEN);

  delay(1);
}

void Adafruit_GP9002::displayOff(void) {
  command(GP9002_DISPLAYSOFF); 
}
void Adafruit_GP9002::displayOn(void) {
   command(GP9002_DISPLAY1ON);
}


