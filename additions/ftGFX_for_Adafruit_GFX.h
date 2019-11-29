#ifndef _ftGFX_for_Adafruit_GFX_H_
#define _ftGFX_for_Adafruit_GFX_H_

#include <Adafruit_GFX.h>
#include <Adafruit_ftGFX.h>

class ftGFX_for_Adafruit_GFX : public Adafruit_ftGFX
{
  public:
    ftGFX_for_Adafruit_GFX(Adafruit_GFX& gfx) : Adafruit_ftGFX(gfx.width(), gfx.height()), _gfx(gfx) {};
    void drawPixel(int16_t x, int16_t y, uint16_t color)
    {
      _gfx.drawPixel(x, y, color);
    };
  private:
    Adafruit_GFX& _gfx;
};


#endif
