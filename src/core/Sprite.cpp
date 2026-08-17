#include "Sprite.h"
#include "../Seeed_GFX.h"
#include "../font/Font_GLCD.h"
#include <algorithm>
#include <math.h>

#if (defined(ESP32) || defined(ARDUINO_ARCH_ESP32)) && defined(__has_include)
#if __has_include(<esp32-hal-psram.h>)
#include <esp32-hal-psram.h>
#define SEEED_GFX_SPRITE_HAS_PSRAM 1
#endif
#endif
#ifndef SEEED_GFX_SPRITE_HAS_PSRAM
#define SEEED_GFX_SPRITE_HAS_PSRAM 0
#endif

namespace {
Seeed_Sprite* smoothSpriteTarget = nullptr;

void drawSmoothSpritePixel(int32_t x, int32_t y, uint16_t color) {
    if (smoothSpriteTarget) smoothSpriteTarget->drawPixel(x, y, color);
}
void drawSmoothSpriteHLine(int32_t x, int32_t y, int32_t w, uint16_t color) {
    if (smoothSpriteTarget) smoothSpriteTarget->drawFastHLine(x, y, w, color);
}
void fillSmoothSpriteRect(int32_t x, int32_t y, int32_t w, int32_t h,
                          uint16_t color) {
    if (smoothSpriteTarget) smoothSpriteTarget->fillRect(x, y, w, h, color);
}
uint16_t readSmoothSpritePixel(int32_t x, int32_t y) {
    return smoothSpriteTarget ? smoothSpriteTarget->readPixel(x, y) : TFT_BLACK;
}

uint16_t decodeUtf8Buffer(const char* text, size_t length, size_t& index) {
    if (!text || index >= length) return 0;
    const uint8_t c = static_cast<uint8_t>(text[index++]);
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && index < length) {
        const uint8_t c2 = static_cast<uint8_t>(text[index]);
        if ((c2 & 0xC0) != 0x80 || c < 0xC2) return 0xFFFD;
        ++index;
        return static_cast<uint16_t>(((c & 0x1F) << 6) | (c2 & 0x3F));
    }
    if ((c & 0xF0) == 0xE0 && index + 1 < length) {
        const uint8_t c2 = static_cast<uint8_t>(text[index]);
        const uint8_t c3 = static_cast<uint8_t>(text[index + 1]);
        if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 ||
            (c == 0xE0 && c2 < 0xA0) || (c == 0xED && c2 >= 0xA0))
            return 0xFFFD;
        index += 2;
        return static_cast<uint16_t>(((c & 0x0F) << 12) |
                                     ((c2 & 0x3F) << 6) | (c3 & 0x3F));
    }
    return 0xFFFD;
}

uint16_t decodeUtf8Stream(uint8_t c, uint32_t& codepoint, uint8_t& remaining) {
    if (remaining == 0) {
        if (c < 0x80) return c;
        if (c >= 0xC2 && c <= 0xDF) {
            codepoint = c & 0x1F; remaining = 1; return 0;
        }
        if (c >= 0xE0 && c <= 0xEF) {
            codepoint = c & 0x0F; remaining = 2; return 0;
        }
        if (c >= 0xF0 && c <= 0xF4) {
            codepoint = c & 0x07; remaining = 3; return 0;
        }
        return 0xFFFD;
    }
    if ((c & 0xC0) != 0x80) {
        remaining = 0; codepoint = 0; return 0xFFFD;
    }
    codepoint = (codepoint << 6) | (c & 0x3F);
    if (--remaining != 0) return 0;
    const uint32_t value = codepoint;
    codepoint = 0;
    return value <= 0xFFFF && !(value >= 0xD800 && value <= 0xDFFF)
        ? static_cast<uint16_t>(value) : 0xFFFD;
}

class SpritePanelAdapter : public IPanel {
public:
    SpritePanelAdapter(Seeed_Sprite& sprite, Seeed_GFX& source)
        : _sprite(sprite), _source(source) {}

    bool begin() override { return true; }
    uint16_t width() const override { return static_cast<uint16_t>(_sprite.width()); }
    uint16_t height() const override { return static_cast<uint16_t>(_sprite.height()); }
    uint8_t colorDepth() const override { return _sprite.getColorDepth(); }
    void setRotation(uint8_t) override {}
    uint8_t rotation() const override { return 0; }
    void invertDisplay(bool) override {}
    void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1,
                       uint16_t y1) override {
        _x0 = _x = x0; _y0 = _y = y0; _x1 = x1; _y1 = y1;
    }
    void writePixel(uint16_t color) override {
        _sprite.drawPixel(_x, _y, color);
        if (++_x > _x1) { _x = _x0; ++_y; }
    }
    void writePixels(const uint16_t* data, size_t len) override {
        if (!data) return;
        while (len--) writePixel(*data++);
    }
    void writeFill(uint16_t color, size_t len) override {
        while (len--) writePixel(color);
    }
    uint16_t readPixel(uint16_t x, uint16_t y) override {
        return _sprite.readPixel(x, y);
    }
    void sleep() override {}
    void wake() override {}
    void setBacklight(uint8_t) override {}
    uint8_t backlight() const override { return 0; }
    IDriver& driver() override { return *_source.driverPtr(); }
    DisplayCapabilities capabilities() const override {
        DisplayCapabilities value;
        value.readback = true;
        return value;
    }

private:
    Seeed_Sprite& _sprite;
    Seeed_GFX& _source;
    uint16_t _x0 = 0, _y0 = 0, _x1 = 0, _y1 = 0, _x = 0, _y = 0;
};
}

Seeed_Sprite::Seeed_Sprite() { createPalette(static_cast<uint16_t*>(nullptr)); }
Seeed_Sprite::~Seeed_Sprite() { deleteSprite(); }

size_t Seeed_Sprite::bytesPerFrame() const {
    const size_t pixels = static_cast<size_t>(_iwidth) * _iheight;
    if (_bpp == 16) return pixels * 2;
    if (_bpp == 8) return pixels;
    if (_bpp == 4) return (pixels + 1) / 2;
    return ((static_cast<size_t>(_iwidth) + 7) / 8) * _iheight;
}
uint8_t* Seeed_Sprite::activeData() const { return _storage ? _storage + bytesPerFrame() * _frame : nullptr; }

void* Seeed_Sprite::createSprite(Seeed_GFX* gfx, int16_t width, int16_t height, uint8_t frames) {
    deleteSprite();
    if (!gfx || width <= 0 || height <= 0 || !frames) return nullptr;
    _gfx = gfx; _iwidth = width; _iheight = height; _frames = frames; _frame = 0;
    const size_t frameBytes = bytesPerFrame();
    if (!frameBytes || frameBytes > SIZE_MAX / frames) { deleteSprite(); return nullptr; }
#if SEEED_GFX_SPRITE_HAS_PSRAM
    if (gfx->getAttribute(3) && psramFound())
        _storage = static_cast<uint8_t*>(ps_calloc(frames, frameBytes));
#endif
    if (!_storage) _storage = static_cast<uint8_t*>(calloc(frames, frameBytes));
    resetViewport(); setWindow(0, 0, width - 1, height - 1);
    return _storage;
}
void Seeed_Sprite::deleteSprite() {
    free(_storage); _storage = nullptr; _iwidth = _iheight = 0; _frames = 1; _frame = 0;
}
void* Seeed_Sprite::frameBuffer(int8_t frame) {
    if (!created() || frame < 0 || frame >= _frames) return nullptr;
    _frame = static_cast<uint8_t>(frame); return activeData();
}
void* Seeed_Sprite::setColorDepth(int8_t b) {
    if (b != 1 && b != 4 && b != 8 && b != 16) return nullptr;
    if (b == _bpp) return activeData();
    const bool recreate = created(); const int16_t w = _iwidth, h = _iheight; const uint8_t frames = _frames;
    Seeed_GFX* gfx = _gfx; deleteSprite(); _bpp = b;
    return recreate ? createSprite(gfx, w, h, frames) : nullptr;
}
void Seeed_Sprite::createPalette(uint16_t* palette, uint8_t colors) {
    const uint8_t count = colors > 16 ? 16 : colors;
    for (uint8_t i = 0; i < 16; ++i) _palette[i] = i < count && palette ? palette[i] : pgm_read_word(&default_4bit_palette[i]);
}
void Seeed_Sprite::createPalette(const uint16_t* palette, uint8_t colors) {
    const uint8_t count = colors > 16 ? 16 : colors;
    for (uint8_t i = 0; i < 16; ++i) _palette[i] = i < count && palette ? pgm_read_word(&palette[i]) : pgm_read_word(&default_4bit_palette[i]);
}
void Seeed_Sprite::setPaletteColor(uint8_t index, uint16_t color) { if (index < 16) _palette[index] = color; }
uint16_t Seeed_Sprite::getPaletteColor(uint8_t index) const { return index < 16 ? _palette[index] : 0; }

bool Seeed_Sprite::logicalToPhysical(int32_t& x, int32_t& y) const {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return false;
    const int32_t ox = x, oy = y;
    switch (_rotation & 3) { case 1: x = _iwidth - 1 - oy; y = ox; break; case 2: x = _iwidth - 1 - ox; y = _iheight - 1 - oy; break; case 3: x = oy; y = _iheight - 1 - ox; break; default: break; }
    return x >= 0 && y >= 0 && x < _iwidth && y < _iheight;
}
uint8_t Seeed_Sprite::paletteIndex(uint16_t color) const {
    uint8_t best = 0; uint32_t bestDistance = UINT32_MAX;
    for (uint8_t i = 0; i < 16; ++i) {
        int32_t dr = ((color >> 11) & 31) - ((_palette[i] >> 11) & 31);
        int32_t dg = ((color >> 5) & 63) - ((_palette[i] >> 5) & 63);
        int32_t db = (color & 31) - (_palette[i] & 31);
        uint32_t distance = dr * dr + dg * dg + db * db;
        if (distance < bestDistance) { bestDistance = distance; best = i; }
    }
    return best;
}
uint16_t Seeed_Sprite::storedColor(size_t pixel) const {
    const uint16_t value = storedValue(pixel);
    if (_bpp == 16) return value;
    if (_bpp == 8) return _gfx ? _gfx->color8to16(static_cast<uint8_t>(value))
                               : value;
    if (_bpp == 4) return _palette[value & 0x0F];
    return value ? _bitmapFg : _bitmapBg;
}
uint16_t Seeed_Sprite::storedValue(size_t pixel) const {
    const uint8_t* data = activeData();
    if (!data || _iwidth <= 0) return 0;
    if (_bpp == 16) return reinterpret_cast<const uint16_t*>(data)[pixel];
    if (_bpp == 8) return data[pixel];
    if (_bpp == 4) {
        const uint8_t packed = data[pixel >> 1];
        return (pixel & 1U) ? (packed & 0x0FU) : (packed >> 4);
    }
    const size_t stride = (static_cast<size_t>(_iwidth) + 7U) / 8U;
    const size_t x = pixel % static_cast<size_t>(_iwidth);
    const size_t y = pixel / static_cast<size_t>(_iwidth);
    return (data[y * stride + (x >> 3)] & (0x80U >> (x & 7U))) ? 1U : 0U;
}
void Seeed_Sprite::setStoredColor(size_t pixel, uint16_t color) {
    uint8_t* data = activeData();
    if (_bpp == 16) reinterpret_cast<uint16_t*>(data)[pixel] = color;
    else if (_bpp == 8) data[pixel] = _gfx ? _gfx->color16to8(color) : static_cast<uint8_t>(color);
    else if (_bpp == 4) { uint8_t& packed = data[pixel >> 1]; uint8_t value = paletteIndex(color); packed = (pixel & 1) ? static_cast<uint8_t>((packed & 0xF0) | value) : static_cast<uint8_t>((packed & 0x0F) | (value << 4)); }
    else {
        const size_t stride = (static_cast<size_t>(_iwidth) + 7U) / 8U;
        const size_t x = pixel % static_cast<size_t>(_iwidth);
        const size_t y = pixel / static_cast<size_t>(_iwidth);
        uint8_t& value = data[y * stride + (x >> 3)];
        const uint8_t bit = static_cast<uint8_t>(0x80U >> (x & 7U));
        if (color != _bitmapBg && (color == _bitmapFg || color != 0))
            value |= bit;
        else
            value &= static_cast<uint8_t>(~bit);
    }
}

uint16_t Seeed_Sprite::readLogicalColor(int32_t x, int32_t y) const {
    if (!created() || !logicalToPhysical(x, y)) return 0;
    return storedColor(static_cast<size_t>(y) * _iwidth + x);
}

uint16_t Seeed_Sprite::readLogicalValue(int32_t x, int32_t y) const {
    if (!created() || !logicalToPhysical(x, y)) return 0;
    return storedValue(static_cast<size_t>(y) * _iwidth + x);
}

bool Seeed_Sprite::prepareRenderer(Seeed_GFX& renderer) const {
    if (!_gfx || !renderer.begin()) return false;
    renderer.setTextSize(_textSize);
    renderer.setTextColor(_textColor, _textBgColor, _fillTextBackground);
    renderer.setTextDatum(_textDatum);
    renderer.setTextPadding(_textPadding);
    renderer.setTextWrap(_textWrapX, _textWrapY);
    renderer.setSwapBytes(_swapBytes);
    renderer.setBitmapColor(_bitmapFg, _bitmapBg);
    renderer.setPivot(_pivotX, _pivotY);
    if (_gfxFont) renderer.setFreeFont(_gfxFont);
    else renderer.setTextFont(_textFont);
    return true;
}

void Seeed_Sprite::drawPixel(int32_t x, int32_t y, uint32_t color) {
    if (!created()) return;
    x += _xDatum;
    y += _yDatum;
    if (_vpOoB || x < _vpX || y < _vpY || x >= _vpW || y >= _vpH || !logicalToPhysical(x, y)) return;
    setStoredColor(static_cast<size_t>(y) * _iwidth + x, static_cast<uint16_t>(color));
}
uint16_t Seeed_Sprite::readPixel(int32_t x, int32_t y) {
    x += _xDatum;
    y += _yDatum;
    if (_vpOoB || x < _vpX || y < _vpY || x >= _vpW || y >= _vpH) return 0;
    return readLogicalColor(x, y);
}
uint16_t Seeed_Sprite::readPixelValue(int32_t x, int32_t y) {
    x += _xDatum;
    y += _yDatum;
    if (_vpOoB || x < _vpX || y < _vpY || x >= _vpW || y >= _vpH) return 0;
    return readLogicalValue(x, y);
}
void Seeed_Sprite::drawLine(int32_t x0,int32_t y0,int32_t x1,int32_t y1,uint32_t c) { int32_t dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy; while(true){drawPixel(x0,y0,c);if(x0==x1&&y0==y1)break;int32_t e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}} }
void Seeed_Sprite::drawFastVLine(int32_t x,int32_t y,int32_t h,uint32_t c){while(h-->0)drawPixel(x,y++,c);}
void Seeed_Sprite::drawFastHLine(int32_t x,int32_t y,int32_t w,uint32_t c){while(w-->0)drawPixel(x++,y,c);}
void Seeed_Sprite::drawRect(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t c){if(w<=0||h<=0)return;drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);}
void Seeed_Sprite::fillRect(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t c){for(int32_t yy=0;yy<h;++yy)drawFastHLine(x,y+yy,w,c);}
void Seeed_Sprite::fillScreen(uint32_t c){fillRect(0,0,width(),height(),c);}
void Seeed_Sprite::drawCircle(int32_t x,int32_t y,int32_t r,uint32_t c){int32_t f=1-r,ddx=1,ddy=-2*r,xx=0,yy=r;drawPixel(x,y+r,c);drawPixel(x,y-r,c);drawPixel(x+r,y,c);drawPixel(x-r,y,c);while(xx<yy){if(f>=0){--yy;ddy+=2;f+=ddy;}++xx;ddx+=2;f+=ddx;drawPixel(x+xx,y+yy,c);drawPixel(x-xx,y+yy,c);drawPixel(x+xx,y-yy,c);drawPixel(x-xx,y-yy,c);drawPixel(x+yy,y+xx,c);drawPixel(x-yy,y+xx,c);drawPixel(x+yy,y-xx,c);drawPixel(x-yy,y-xx,c);}}
void Seeed_Sprite::fillCircle(int32_t x,int32_t y,int32_t r,uint32_t c){for(int32_t yy=-r;yy<=r;++yy){int32_t dx=static_cast<int32_t>(sqrt(r*r-yy*yy));drawFastHLine(x-dx,y+yy,dx*2+1,c);}}

void Seeed_Sprite::drawTriangle(int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t x2,int32_t y2,uint32_t c){drawLine(x0,y0,x1,y1,c);drawLine(x1,y1,x2,y2,c);drawLine(x2,y2,x0,y0,c);}

void Seeed_Sprite::fillTriangle(int32_t x0,int32_t y0,int32_t x1,int32_t y1,int32_t x2,int32_t y2,uint32_t c){
    int32_t a,b,y,last;
    if(y0>y1){int32_t t=y0;y0=y1;y1=t;t=x0;x0=x1;x1=t;}
    if(y1>y2){int32_t t=y1;y1=y2;y2=t;t=x1;x1=x2;x2=t;}
    if(y0>y1){int32_t t=y0;y0=y1;y1=t;t=x0;x0=x1;x1=t;}
    if(y0==y2){a=b=x0;if(x1<a)a=x1;else if(x1>b)b=x1;if(x2<a)a=x2;else if(x2>b)b=x2;drawFastHLine(a,y0,b-a+1,c);return;}
    int32_t dx01=x1-x0,dy01=y1-y0,dx02=x2-x0,dy02=y2-y0,dx12=x2-x1,dy12=y2-y1;
    int32_t sa=0,sb=0;
    if(y1==y2)last=y1;else last=y1-1;
    for(y=y0;y<=last;y++){a=x0+sa/dy01;b=x0+sb/dy02;sa+=dx01;sb+=dx02;if(a>b){int32_t t=a;a=b;b=t;}drawFastHLine(a,y,b-a+1,c);}
    sa=(int32_t)dx12*(y-y1);sb=(int32_t)dx02*(y-y0);
    for(;y<=y2;y++){a=x1+sa/dy12;b=x0+sb/dy02;sa+=dx12;sb+=dx02;if(a>b){int32_t t=a;a=b;b=t;}drawFastHLine(a,y,b-a+1,c);}
}

void Seeed_Sprite::drawRoundRect(int32_t x,int32_t y,int32_t w,int32_t h,int32_t r,uint32_t c){
    drawFastHLine(x+r,y,w-2*r,c);drawFastHLine(x+r,y+h-1,w-2*r,c);
    drawFastVLine(x,y+r,h-2*r,c);drawFastVLine(x+w-1,y+r,h-2*r,c);
    int32_t f=1-r,ddx=1,ddy=-2*r,xx=0,yy=r;
    drawPixel(x+r+xx,y+r+yy,c);drawPixel(x+w-r-1-xx,y+r+yy,c);
    drawPixel(x+r+xx,y+h-r-1-yy,c);drawPixel(x+w-r-1-xx,y+h-r-1-yy,c);
    drawPixel(x+r+yy,y+r+xx,c);drawPixel(x+w-r-1-yy,y+r+xx,c);
    drawPixel(x+r+yy,y+h-r-1-xx,c);drawPixel(x+w-r-1-yy,y+h-r-1-xx,c);
    while(xx<yy){if(f>=0){yy--;ddy+=2;f+=ddy;}xx++;ddx+=2;f+=ddx;
    drawPixel(x+r+xx,y+r+yy,c);drawPixel(x+w-r-1-xx,y+r+yy,c);
    drawPixel(x+r+xx,y+h-r-1-yy,c);drawPixel(x+w-r-1-xx,y+h-r-1-yy,c);
    drawPixel(x+r+yy,y+r+xx,c);drawPixel(x+w-r-1-yy,y+r+xx,c);
    drawPixel(x+r+yy,y+h-r-1-xx,c);drawPixel(x+w-r-1-yy,y+h-r-1-xx,c);}
}

void Seeed_Sprite::fillRoundRect(int32_t x,int32_t y,int32_t w,int32_t h,int32_t r,uint32_t c){
    fillRect(x+r,y,w-2*r,h,c);
    int32_t f=1-r,ddx=1,ddy=-2*r,xx=0,yy=r;
    drawFastHLine(x+r-yy,y+r+yy,2*yy+1+w-2*r-2*xx,c);
    while(xx<yy){if(f>=0){yy--;ddy+=2;f+=ddy;}xx++;ddx+=2;f+=ddx;
    drawFastHLine(x+r-yy,y+r+yy,2*yy+1+w-2*r,c);
    drawFastHLine(x+r-yy,y+h-r-1-yy,2*yy+1+w-2*r,c);
    drawFastHLine(x+r-xx,y+r+xx,2*xx+1+w-2*r,c);
    drawFastHLine(x+r-xx,y+h-r-1-xx,2*xx+1+w-2*r,c);}
}

void Seeed_Sprite::drawEllipse(int32_t cx,int32_t cy,int32_t rx,int32_t ry,uint32_t c){
    if (rx <= 0 || ry <= 0) return;
    int32_t x = 0, y = ry;
    int32_t rx2 = rx * rx, ry2 = ry * ry;
    int32_t p1=ry2-rx2*ry+rx2/4;
    while(ry2*x<=rx2*y){drawPixel(cx+x,cy+y,c);drawPixel(cx-x,cy+y,c);drawPixel(cx+x,cy-y,c);drawPixel(cx-x,cy-y,c);x++;if(p1<0){p1+=2*ry2*x+ry2;}else{y--;p1+=2*ry2*x-2*rx2*y+ry2;}}
    int32_t p2=ry2*(x*2+1)*(x*2+1)/4+rx2*(y-1)*(y-1)-rx2*ry2;
    while(y>=0){drawPixel(cx+x,cy+y,c);drawPixel(cx-x,cy+y,c);drawPixel(cx+x,cy-y,c);drawPixel(cx-x,cy-y,c);y--;if(p2>0){p2-=2*rx2*y+rx2;}else{x++;p2+=2*ry2*x-2*rx2*y+rx2;}}
}

void Seeed_Sprite::fillEllipse(int32_t cx,int32_t cy,int32_t rx,int32_t ry,uint32_t c){
    if(rx<=0||ry<=0)return;
    for(int32_t y=-ry;y<=ry;y++){int32_t dx=static_cast<int32_t>(rx*sqrt(1.0-(double)(y*y)/(double)(ry*ry)));drawFastHLine(cx-dx,cy+y,dx*2+1,c);}
}

void Seeed_Sprite::drawRectVGradient(int16_t x, int16_t y, int16_t w,
                                     int16_t h, uint32_t color1,
                                     uint32_t color2) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawRectVGradient(x, y, w, h, color1, color2);
}

void Seeed_Sprite::drawRectHGradient(int16_t x, int16_t y, int16_t w,
                                     int16_t h, uint32_t color1,
                                     uint32_t color2) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawRectHGradient(x, y, w, h, color1, color2);
}

uint16_t Seeed_Sprite::drawPixel(int32_t x, int32_t y, uint32_t color,
                                 uint8_t alpha, uint32_t bgColor) {
    if (!_gfx) return 0;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    return prepareRenderer(renderer)
        ? renderer.drawPixel(x, y, color, alpha, bgColor) : 0;
}

void Seeed_Sprite::drawSmoothArc(int32_t x, int32_t y, int32_t r,
                                 int32_t ir, uint32_t startAngle,
                                 uint32_t endAngle, uint32_t fgColor,
                                 uint32_t bgColor, bool roundEnds) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawSmoothArc(
        x, y, r, ir, startAngle, endAngle, fgColor, bgColor, roundEnds);
}

void Seeed_Sprite::drawArc(int32_t x, int32_t y, int32_t r, int32_t ir,
                           uint32_t startAngle, uint32_t endAngle,
                           uint32_t fgColor, uint32_t bgColor,
                           bool smoothArc) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawArc(
        x, y, r, ir, startAngle, endAngle, fgColor, bgColor, smoothArc);
}

void Seeed_Sprite::drawSmoothCircle(int32_t x, int32_t y, int32_t r,
                                    uint32_t fgColor, uint32_t bgColor) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawSmoothCircle(x, y, r, fgColor, bgColor);
}

void Seeed_Sprite::fillSmoothCircle(int32_t x, int32_t y, int32_t r,
                                    uint32_t color, uint32_t bgColor) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.fillSmoothCircle(x, y, r, color, bgColor);
}

void Seeed_Sprite::drawSmoothRoundRect(int32_t x, int32_t y, int32_t r,
                                       int32_t ir, int32_t w, int32_t h,
                                       uint32_t fgColor, uint32_t bgColor,
                                       uint8_t quadrants) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawSmoothRoundRect(
        x, y, r, ir, w, h, fgColor, bgColor, quadrants);
}

void Seeed_Sprite::fillSmoothRoundRect(int32_t x, int32_t y, int32_t w,
                                       int32_t h, int32_t radius,
                                       uint32_t color, uint32_t bgColor) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.fillSmoothRoundRect(
        x, y, w, h, radius, color, bgColor);
}

void Seeed_Sprite::drawSpot(float x, float y, float radius,
                            uint32_t fgColor, uint32_t bgColor) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawSpot(x, y, radius, fgColor, bgColor);
}

void Seeed_Sprite::drawWideLine(float x0, float y0, float x1, float y1,
                                float lineWidth, uint32_t fgColor,
                                uint32_t bgColor) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawWideLine(
        x0, y0, x1, y1, lineWidth, fgColor, bgColor);
}

void Seeed_Sprite::drawWedgeLine(float x0, float y0, float x1, float y1,
                                 float startWidth, float endWidth,
                                 uint32_t fgColor, uint32_t bgColor) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawWedgeLine(
        x0, y0, x1, y1, startWidth, endWidth, fgColor, bgColor);
}

void Seeed_Sprite::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                              int16_t w, int16_t h, uint16_t fgColor) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawBitmap(x, y, bitmap, w, h, fgColor);
}

void Seeed_Sprite::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                              int16_t w, int16_t h, uint16_t fgColor,
                              uint16_t bgColor) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawBitmap(x, y, bitmap, w, h, fgColor, bgColor);
}

void Seeed_Sprite::drawXBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                               int16_t w, int16_t h, uint16_t fgColor) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawXBitmap(x, y, bitmap, w, h, fgColor);
}

void Seeed_Sprite::drawXBitmap(int16_t x, int16_t y, const uint8_t* bitmap,
                               int16_t w, int16_t h, uint16_t fgColor,
                               uint16_t bgColor) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx); Seeed_GFX renderer(panel);
    if (prepareRenderer(renderer)) renderer.drawXBitmap(x, y, bitmap, w, h, fgColor, bgColor);
}

void Seeed_Sprite::readRect(int32_t x, int32_t y, int32_t w, int32_t h,
                            uint16_t* data) {
    if (!data || w <= 0 || h <= 0) return;
    for (int32_t yy = 0; yy < h; ++yy)
        for (int32_t xx = 0; xx < w; ++xx)
            *data++ = readPixel(x + xx, y + yy);
}

void Seeed_Sprite::pushRect(int32_t x, int32_t y, int32_t w, int32_t h,
                            const uint16_t* data) {
    pushImage(x, y, w, h, data);
}

void Seeed_Sprite::setWindow(int32_t x0,int32_t y0,int32_t x1,int32_t y1){_winX0=x0;_winY0=y0;_winX1=x1;_winY1=y1;_winX=x0;_winY=y0;}
void Seeed_Sprite::pushColor(uint16_t c){if(_winX>_winX1||_winY>_winY1)return;drawPixel(_winX,_winY,c);if(++_winX>_winX1){_winX=_winX0;++_winY;}}
void Seeed_Sprite::pushColor(uint16_t c,uint32_t len){while(len--)pushColor(c);}
void Seeed_Sprite::pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                             const uint16_t* data) {
    if (!created() || !data || w <= 0 || h <= 0) return;

    // Match the original TFT_eSprite behavior: when the destination is a
    // 4-bit sprite, the source is a packed indexed image (high nibble first),
    // even though the compatibility signature uses a uint16_t pointer.
    if (_bpp == 4) {
        (void)pushImage4BPP(x, y, w, h,
                            reinterpret_cast<const uint8_t*>(data));
        return;
    }

    for (int32_t yy = 0; yy < h; ++yy)
        for (int32_t xx = 0; xx < w; ++xx)
            {
                uint16_t color = pgm_read_word(
                    data + static_cast<size_t>(yy) * w + xx);
                if (_swapBytes) color = static_cast<uint16_t>((color >> 8) | (color << 8));
                drawPixel(x + xx, y + yy, color);
            }
}

bool Seeed_Sprite::pushImage4BPP(int32_t x, int32_t y, int32_t w,
                                 int32_t h, const uint8_t* data,
                                 const uint16_t* colorMap) {
    if (!created() || !data || w <= 0 || h <= 0) return false;
    const size_t rowStride = (static_cast<size_t>(w) + 1U) / 2U;
    for (int32_t yy = 0; yy < h; ++yy)
        for (int32_t xx = 0; xx < w; ++xx) {
            const uint8_t pair = pgm_read_byte(
                data + static_cast<size_t>(yy) * rowStride + (xx >> 1));
            const uint8_t index = static_cast<uint8_t>(
                (xx & 1) ? (pair & 0x0FU) : (pair >> 4));
            drawPixel(x + xx, y + yy,
                      colorMap ? pgm_read_word(colorMap + index)
                               : _palette[index]);
        }
    return true;
}

void Seeed_Sprite::pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                             const uint16_t* data, uint16_t transparent) {
    if (!created() || !data || w <= 0 || h <= 0) return;
    for (int32_t yy = 0; yy < h; ++yy) {
        for (int32_t xx = 0; xx < w; ++xx) {
            uint16_t color = pgm_read_word(data + static_cast<size_t>(yy) * w + xx);
            if (_swapBytes) color = static_cast<uint16_t>((color >> 8) | (color << 8));
            if (color != transparent) drawPixel(x + xx, y + yy, color);
        }
    }
}

void Seeed_Sprite::pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                             const uint8_t* data, bool bpp8,
                             const uint16_t* colorMap) {
    if (!created() || !data || w <= 0 || h <= 0) return;
    const size_t monoStride = (static_cast<size_t>(w) + 7U) / 8U;
    for (int32_t yy = 0; yy < h; ++yy) {
        for (int32_t xx = 0; xx < w; ++xx) {
            uint16_t color;
            if (bpp8) {
                const uint8_t value = pgm_read_byte(data + static_cast<size_t>(yy) * w + xx);
                color = colorMap ? pgm_read_word(colorMap + value)
                                 : (_gfx ? _gfx->color8to16(value) : value);
            } else {
                const uint8_t value = pgm_read_byte(
                    data + static_cast<size_t>(yy) * monoStride + (xx >> 3));
                color = (value & (0x80U >> (xx & 7))) ? _bitmapFg : _bitmapBg;
            }
            drawPixel(x + xx, y + yy, color);
        }
    }
}

void Seeed_Sprite::pushImage(int32_t x, int32_t y, int32_t w, int32_t h,
                             const uint8_t* data, uint8_t transparent,
                             bool bpp8, const uint16_t* colorMap) {
    if (!created() || !data || w <= 0 || h <= 0) return;
    const size_t monoStride = (static_cast<size_t>(w) + 7U) / 8U;
    for (int32_t yy = 0; yy < h; ++yy) {
        for (int32_t xx = 0; xx < w; ++xx) {
            uint8_t value;
            if (bpp8)
                value = pgm_read_byte(data + static_cast<size_t>(yy) * w + xx);
            else {
                const uint8_t packed = pgm_read_byte(
                    data + static_cast<size_t>(yy) * monoStride + (xx >> 3));
                value = (packed & (0x80U >> (xx & 7))) ? 1U : 0U;
            }
            if (value == transparent) continue;
            const uint16_t color = bpp8
                ? (colorMap ? pgm_read_word(colorMap + value)
                            : (_gfx ? _gfx->color8to16(value) : value))
                : (value ? _bitmapFg : _bitmapBg);
            drawPixel(x + xx, y + yy, color);
        }
    }
}

void Seeed_Sprite::pushMaskedImage(int32_t x, int32_t y, int32_t w,
                                   int32_t h, const uint16_t* image,
                                   const uint8_t* mask) {
    if (!created() || !image || !mask || w <= 0 || h <= 0) return;
    const size_t stride = (static_cast<size_t>(w) + 7U) / 8U;
    for (int32_t yy = 0; yy < h; ++yy)
        for (int32_t xx = 0; xx < w; ++xx) {
            const uint8_t bits = pgm_read_byte(
                mask + static_cast<size_t>(yy) * stride + (xx >> 3));
            if (bits & (0x80U >> (xx & 7))) {
                uint16_t color = pgm_read_word(
                    image + static_cast<size_t>(yy) * w + xx);
                if (_swapBytes) color = static_cast<uint16_t>((color >> 8) | (color << 8));
                drawPixel(x + xx, y + yy, color);
            }
        }
}

bool Seeed_Sprite::pushSprite(int32_t x, int32_t y) {
    return pushSprite(x, y, 0, 0, width(), height());
}

bool Seeed_Sprite::pushSprite(int32_t x, int32_t y, uint16_t transparent) {
    if (!_gfx || !created()) return false;
    const bool previousSwap = _gfx->getSwapBytes();
    _gfx->setSwapBytes(false);
    uint16_t row[64];
    for (int32_t yy = 0; yy < height(); ++yy) {
        int32_t xx = 0;
        while (xx < width()) {
            while (xx < width() && readLogicalColor(xx, yy) == transparent) ++xx;
            const int32_t runStart = xx;
            while (xx < width() && readLogicalColor(xx, yy) != transparent) ++xx;
            int32_t offset = runStart;
            while (offset < xx) {
                const int32_t count = std::min<int32_t>(64, xx - offset);
                for (int32_t i = 0; i < count; ++i)
                    row[i] = readLogicalColor(offset + i, yy);
                _gfx->pushImage(x + offset, y + yy, count, 1, row);
                offset += count;
            }
        }
    }
    _gfx->setSwapBytes(previousSwap);
    return true;
}

bool Seeed_Sprite::pushSprite(int32_t tx, int32_t ty, int32_t sx,
                              int32_t sy, int32_t sw, int32_t sh) {
    if (!_gfx || !created() || sw <= 0 || sh <= 0) return false;
    if (sx < 0) { tx -= sx; sw += sx; sx = 0; }
    if (sy < 0) { ty -= sy; sh += sy; sy = 0; }
    if (sx >= width() || sy >= height()) return false;
    sw = std::min<int32_t>(sw, width() - sx);
    sh = std::min<int32_t>(sh, height() - sy);
    if (sw <= 0 || sh <= 0) return false;

    const bool previousSwap = _gfx->getSwapBytes();
    _gfx->setSwapBytes(false);
    if (_bpp == 16 && _rotation == 0) {
        const uint16_t* pixels = reinterpret_cast<const uint16_t*>(activeData());
        for (int32_t yy = 0; yy < sh; ++yy)
            _gfx->pushImage(tx, ty + yy, sw, 1,
                            pixels + static_cast<size_t>(sy + yy) * _iwidth + sx);
    } else {
        uint16_t row[64];
        for (int32_t yy = 0; yy < sh; ++yy) {
            for (int32_t offset = 0; offset < sw; offset += 64) {
                const int32_t count = std::min<int32_t>(64, sw - offset);
                for (int32_t i = 0; i < count; ++i)
                    row[i] = readLogicalColor(sx + offset + i, sy + yy);
                _gfx->pushImage(tx + offset, ty + yy, count, 1, row);
            }
        }
    }
    _gfx->setSwapBytes(previousSwap);
    return true;
}

bool Seeed_Sprite::pushToSprite(Seeed_Sprite* destination, int32_t x,
                                int32_t y) {
    if (!destination || !created()) return false;
    if (destination->getColorDepth() != 16) {
        for (int32_t yy = 0; yy < height(); ++yy)
            for (int32_t xx = 0; xx < width(); ++xx)
                destination->drawPixel(x + xx, y + yy,
                                       readLogicalColor(xx, yy));
        return true;
    }
    uint16_t row[64];
    for (int32_t yy = 0; yy < height(); ++yy)
        for (int32_t offset = 0; offset < width(); offset += 64) {
            const int32_t count = std::min<int32_t>(64, width() - offset);
            for (int32_t i = 0; i < count; ++i)
                row[i] = readLogicalColor(offset + i, yy);
            destination->pushImage(x + offset, y + yy, count, 1, row);
        }
    return true;
}

bool Seeed_Sprite::pushToSprite(Seeed_Sprite* destination, int32_t x,
                                int32_t y, uint16_t transparent) {
    if (!destination || !created()) return false;
    for (int32_t yy = 0; yy < height(); ++yy)
        for (int32_t xx = 0; xx < width(); ++xx) {
            const uint16_t color = readLogicalColor(xx, yy);
            if (color != transparent) destination->drawPixel(x + xx, y + yy, color);
        }
    return true;
}

void Seeed_Sprite::setScrollRect(int32_t x,int32_t y,int32_t w,int32_t h,uint16_t c){_scrollX=x;_scrollY=y;_scrollW=w;_scrollH=h;_scrollColor=c;}
void Seeed_Sprite::scroll(int16_t dx,int16_t dy){
    if(_scrollW<=0||_scrollH<=0)return;
    uint16_t* row=static_cast<uint16_t*>(malloc(static_cast<size_t>(_scrollW)*sizeof(uint16_t)));
    if(!row)return;
    const int32_t first=dy>0?_scrollH-1:0;
    const int32_t end=dy>0?-1:_scrollH;
    const int32_t step=dy>0?-1:1;
    for(int32_t y=first;y!=end;y+=step){
        const int32_t sourceY=y-dy;
        if(sourceY>=0&&sourceY<_scrollH)
            for(int32_t x=0;x<_scrollW;++x)row[x]=readLogicalColor(_scrollX+x,_scrollY+sourceY);
        for(int32_t x=0;x<_scrollW;++x){const int32_t sourceX=x-dx;const uint16_t color=(sourceY>=0&&sourceY<_scrollH&&sourceX>=0&&sourceX<_scrollW)?row[sourceX]:_scrollColor;drawPixel(_scrollX+x,_scrollY+y,color);}
    }
    free(row);
}
void Seeed_Sprite::setRotation(uint8_t r){_rotation=r&3;resetViewport();}

bool Seeed_Sprite::getRotatedBounds(int16_t angle,int16_t* minX,int16_t* minY,int16_t* maxX,int16_t* maxY){return getRotatedBounds(nullptr,angle,minX,minY,maxX,maxY);}
bool Seeed_Sprite::getRotatedBounds(Seeed_Sprite*,int16_t angle,int16_t* minX,int16_t* minY,int16_t* maxX,int16_t* maxY){if(!created()||!minX||!minY||!maxX||!maxY)return false;float r=angle*0.01745329252f,cs=cos(r),sn=sin(r);float cx=_hasPivot?(float)_pivotX:width()/2.0f,cy=_hasPivot?(float)_pivotY:height()/2.0f;float xs[4],ys[4];int32_t px[4]={0,width()-1,width()-1,0},py[4]={0,0,height()-1,height()-1};for(int i=0;i<4;++i){xs[i]=(px[i]-cx)*cs-(py[i]-cy)*sn+cx;ys[i]=(px[i]-cx)*sn+(py[i]-cy)*cs+cy;}float lx=xs[0],hx=xs[0],ly=ys[0],hy=ys[0];for(int i=1;i<4;++i){if(xs[i]<lx)lx=xs[i];if(xs[i]>hx)hx=xs[i];if(ys[i]<ly)ly=ys[i];if(ys[i]>hy)hy=ys[i];}*minX=floor(lx);*maxX=ceil(hx);*minY=floor(ly);*maxY=ceil(hy);return true;}
bool Seeed_Sprite::copyRotatedTo(Seeed_Sprite* d,int16_t angle,uint32_t t){if(!d)return false;float r=-angle*0.01745329252f,cs=cos(r),sn=sin(r);int16_t lx,ly,hx,hy;if(!getRotatedBounds(angle,&lx,&ly,&hx,&hy))return false;float cx=_hasPivot?(float)_pivotX:width()/2.0f,cy=_hasPivot?(float)_pivotY:height()/2.0f;for(int y=ly;y<=hy;++y)for(int x=lx;x<=hx;++x){float sx=(x-cx)*cs-(y-cy)*sn+cx,sy=(x-cx)*sn+(y-cy)*cs+cy;int ix=static_cast<int>(sx+0.5f),iy=static_cast<int>(sy+0.5f);if(ix>=0&&iy>=0&&ix<width()&&iy<height()){uint16_t c=readLogicalColor(ix,iy);if(c!=t)d->drawPixel(x,y,c);}}return true;}
bool Seeed_Sprite::pushRotated(Seeed_Sprite* d,int16_t a,uint32_t t){return copyRotatedTo(d,a,t);}
bool Seeed_Sprite::pushRotated(int16_t angle,uint32_t transparent){
    if(!_gfx || !created()) return false;
    const float sourcePivotX=_hasPivot?(float)_pivotX:(width()-1)/2.0f;
    const float sourcePivotY=_hasPivot?(float)_pivotY:(height()-1)/2.0f;
    const float radians=angle*0.01745329252f,cs=cos(radians),sn=sin(radians);
    float minX=0,maxX=0,minY=0,maxY=0;
    const float cornerX[4]={-sourcePivotX,width()-1-sourcePivotX,width()-1-sourcePivotX,-sourcePivotX};
    const float cornerY[4]={-sourcePivotY,-sourcePivotY,height()-1-sourcePivotY,height()-1-sourcePivotY};
    for(int i=0;i<4;++i){const float rx=cornerX[i]*cs-cornerY[i]*sn;const float ry=cornerX[i]*sn+cornerY[i]*cs;if(i==0||rx<minX)minX=rx;if(i==0||rx>maxX)maxX=rx;if(i==0||ry<minY)minY=ry;if(i==0||ry>maxY)maxY=ry;}
    const int32_t firstX=static_cast<int32_t>(floor(minX));
    const int32_t lastX=static_cast<int32_t>(ceil(maxX));
    const int32_t firstY=static_cast<int32_t>(floor(minY));
    const int32_t lastY=static_cast<int32_t>(ceil(maxY));
    const int32_t targetPivotX=_gfx->getPivotX(),targetPivotY=_gfx->getPivotY();
    const bool previousSwap=_gfx->getSwapBytes(); _gfx->setSwapBytes(false);
    uint16_t run[64];
    for(int32_t dy=firstY;dy<=lastY;++dy){
        int32_t dx=firstX;
        while(dx<=lastX){
            int32_t ix,iy;
            uint16_t color;
            do{const float sx=dx*cs+dy*sn+sourcePivotX;const float sy=-dx*sn+dy*cs+sourcePivotY;ix=static_cast<int32_t>(floor(sx+0.5f));iy=static_cast<int32_t>(floor(sy+0.5f));color=(ix>=0&&iy>=0&&ix<width()&&iy<height())?readLogicalColor(ix,iy):static_cast<uint16_t>(transparent);if(color!=transparent)break;++dx;}while(dx<=lastX);
            const int32_t runStart=dx;int32_t count=0;
            while(dx<=lastX&&count<64){const float sx=dx*cs+dy*sn+sourcePivotX;const float sy=-dx*sn+dy*cs+sourcePivotY;ix=static_cast<int32_t>(floor(sx+0.5f));iy=static_cast<int32_t>(floor(sy+0.5f));if(ix<0||iy<0||ix>=width()||iy>=height())break;color=readLogicalColor(ix,iy);if(color==transparent)break;run[count++]=color;++dx;}
            if(count)_gfx->pushImage(targetPivotX+runStart,targetPivotY+dy,count,1,run);
            else ++dx;
        }
    }
    _gfx->setSwapBytes(previousSwap);
    return true;
}

void Seeed_Sprite::setViewport(int32_t x,int32_t y,int32_t w,int32_t h,bool datum){_vpX=max<int32_t>(0,x);_vpY=max<int32_t>(0,y);_vpW=min<int32_t>(width(),x+w);_vpH=min<int32_t>(height(),y+h);_vpOoB=w<=0||h<=0||_vpX>=_vpW||_vpY>=_vpH;_vpDatum=datum;_xDatum=datum?_vpX:0;_yDatum=datum?_vpY:0;}
bool Seeed_Sprite::checkViewport(int32_t x,int32_t y,int32_t w,int32_t h){x+=_xDatum;y+=_yDatum;return !_vpOoB&&x<_vpW&&y<_vpH&&x+w>_vpX&&y+h>_vpY;}
void Seeed_Sprite::resetViewport(){_vpX=_vpY=_xDatum=_yDatum=0;_vpW=width();_vpH=height();_vpOoB=false;}
void Seeed_Sprite::frameViewport(uint16_t color,int32_t lineWidth){if(_vpOoB||lineWidth<=0)return;const int32_t savedX=_xDatum,savedY=_yDatum;_xDatum=_yDatum=0;for(int32_t i=0;i<lineWidth;++i)drawRect(_vpX+i,_vpY+i,_vpW-_vpX-2*i,_vpH-_vpY-2*i,color);_xDatum=savedX;_yDatum=savedY;}

int16_t Seeed_Sprite::drawNumber(long intNumber, int32_t x, int32_t y, uint8_t font) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", intNumber);
    return drawString(buf, x, y, font);
}

int16_t Seeed_Sprite::drawFloat(float floatNumber, uint8_t decimal, int32_t x, int32_t y, uint8_t font) {
    if (decimal > 7) decimal = 7;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimal, floatNumber);
    return drawString(buf, x, y, font);
}

int16_t Seeed_Sprite::drawCentreString(const char* str, int32_t x, int32_t y, uint8_t font) {
    const uint8_t previousDatum = _textDatum;
    _textDatum = TC_DATUM;
    const int16_t drawnWidth = drawString(str, x, y, font);
    _textDatum = previousDatum;
    return drawnWidth;
}

int16_t Seeed_Sprite::drawRightString(const char* str, int32_t x, int32_t y, uint8_t font) {
    const uint8_t previousDatum = _textDatum;
    _textDatum = TR_DATUM;
    const int16_t drawnWidth = drawString(str, x, y, font);
    _textDatum = previousDatum;
    return drawnWidth;
}

int16_t Seeed_Sprite::drawString(const char* text, int32_t x, int32_t y,
                                 uint8_t font) {
    if (!text || !_gfx) return 0;
    if (!_smoothFont.isLoaded()) {
        SpritePanelAdapter panel(*this, *_gfx);
        Seeed_GFX renderer(panel);
        renderer.begin();
        renderer.setTextSize(_textSize);
        renderer.setTextColor(_textColor, _textBgColor, _fillTextBackground);
        renderer.setTextDatum(_textDatum);
        renderer.setTextPadding(_textPadding);
        if (_gfxFont) renderer.setFreeFont(_gfxFont);
        else renderer.setTextFont(font);
        return renderer.drawString(text, x, y, font);
    }

    int16_t totalWidth = textWidth(text, font);
    const int16_t height = fontHeight(font);
    if (_textDatum == TC_DATUM || _textDatum == MC_DATUM || _textDatum == BC_DATUM)
        x -= totalWidth / 2;
    else if (_textDatum == TR_DATUM || _textDatum == MR_DATUM || _textDatum == BR_DATUM)
        x -= totalWidth;
    if (_textDatum == ML_DATUM || _textDatum == MC_DATUM || _textDatum == MR_DATUM)
        y -= height / 2;
    else if (_textDatum == BL_DATUM || _textDatum == BC_DATUM || _textDatum == BR_DATUM)
        y -= height;

    smoothSpriteTarget = this;
    const bool fillGlyphBackground = _fillTextBackground || _textPadding != 0;
    size_t index = 0;
    const size_t length = strlen(text);
    int32_t cursor = x;
    while (index < length) {
        const uint16_t code = decodeUtf8Buffer(text, length, index);
        cursor += _smoothFont.drawChar(cursor, y, code, _textColor,
                                       _textBgColor, fillGlyphBackground);
    }
    if (_textPadding > totalWidth && _textColor != _textBgColor) {
        const int32_t remaining = static_cast<int32_t>(_textPadding) - totalWidth;
        if (_textDatum == TC_DATUM || _textDatum == MC_DATUM ||
            _textDatum == BC_DATUM || _textDatum == C_BASELINE) {
            const int32_t left = remaining / 2;
            const int32_t right = remaining - left;
            if (left) fillRect(x - left, y, left, height, _textBgColor);
            if (right) fillRect(x + totalWidth, y, right, height, _textBgColor);
        } else if (_textDatum == TR_DATUM || _textDatum == MR_DATUM ||
                   _textDatum == BR_DATUM || _textDatum == R_BASELINE) {
            fillRect(x - remaining, y, remaining, height, _textBgColor);
        } else {
            fillRect(x + totalWidth, y, remaining, height, _textBgColor);
        }
    }
    _cursorX = cursor;
    _cursorY = y;
    return static_cast<int16_t>(cursor - x);
}

int16_t Seeed_Sprite::drawString(const char* text, int32_t x, int32_t y) {
    return drawString(text, x, y, _textFont);
}

int16_t Seeed_Sprite::drawChar(uint16_t code, int32_t x, int32_t y,
                               uint8_t font) {
    if (!_gfx) return 0;
    if (_smoothFont.isLoaded()) {
        smoothSpriteTarget = this;
        return static_cast<int16_t>(_smoothFont.drawChar(
            x, y, code, _textColor, _textBgColor, _fillTextBackground));
    }
    SpritePanelAdapter panel(*this, *_gfx);
    Seeed_GFX renderer(panel);
    renderer.begin();
    renderer.setTextSize(_textSize);
    renderer.setTextColor(_textColor, _textBgColor, _fillTextBackground);
    if (_gfxFont) renderer.setFreeFont(_gfxFont);
    else renderer.setTextFont(font);
    return renderer.drawChar(code, x, y, font);
}

void Seeed_Sprite::drawChar(int32_t x, int32_t y, uint16_t code,
                            uint32_t color, uint32_t background,
                            uint8_t size) {
    if (!_gfx) return;
    SpritePanelAdapter panel(*this, *_gfx);
    Seeed_GFX renderer(panel);
    if (!prepareRenderer(renderer)) return;
    renderer.drawChar(x, y, code, color, background, size);
}

uint16_t Seeed_Sprite::drawGlyph(uint16_t code) {
    const int16_t advance = drawChar(code, _cursorX, _cursorY, _textFont);
    _cursorX += advance;
    return static_cast<uint16_t>(advance);
}

void Seeed_Sprite::printToSprite(const char* text, uint16_t len) {
    if (!text) return;
    for (uint16_t i = 0; i < len; ++i) write(static_cast<uint8_t>(text[i]));
}

void Seeed_Sprite::setCursor(int16_t x, int16_t y) { _cursorX = x; _cursorY = y; }
void Seeed_Sprite::setTextColor(uint16_t color) {
    _textColor = _textBgColor = color;
    _fillTextBackground = false;
}
void Seeed_Sprite::setTextColor(uint16_t fg, uint16_t bg) {
    _textColor = fg; _textBgColor = bg; _fillTextBackground = fg != bg;
}
void Seeed_Sprite::setTextSize(uint8_t size) {
    if (size > 7) size = 7;
    _textSize = size ? size : 1;
}
void Seeed_Sprite::setTextFont(uint8_t font) {
    _textFont = font; _gfxFont = nullptr;
}
void Seeed_Sprite::setFreeFont(const GFXfont* font) {
    _gfxFont = font;
    _textFont = FONT_GLCD;
}
void Seeed_Sprite::setTextDatum(uint8_t datum) { _textDatum = datum; }
void Seeed_Sprite::setTextWrap(bool x, bool y) { _textWrapX = x; _textWrapY = y; }

int16_t Seeed_Sprite::textWidth(const char* text, uint8_t font) {
    if (!text || !_gfx) return 0;
    if (_smoothFont.isLoaded()) {
        int32_t width = 0;
        size_t index = 0;
        const size_t length = strlen(text);
        while (index < length)
            width += _smoothFont.getCharWidth(decodeUtf8Buffer(text, length, index));
        return static_cast<int16_t>(width);
    }
    SpritePanelAdapter panel(*this, *_gfx);
    Seeed_GFX renderer(panel);
    renderer.setTextSize(_textSize);
    if (_gfxFont) renderer.setFreeFont(_gfxFont);
    else renderer.setTextFont(font);
    return renderer.textWidth(text, font);
}
int16_t Seeed_Sprite::textWidth(const char* text) {
    return textWidth(text, _textFont);
}
int16_t Seeed_Sprite::fontHeight(uint8_t font) {
    if (_smoothFont.isLoaded()) return _smoothFont.fontHeight();
    if (!_gfx) return 0;
    SpritePanelAdapter panel(*this, *_gfx);
    Seeed_GFX renderer(panel);
    renderer.setTextSize(_textSize);
    if (_gfxFont) renderer.setFreeFont(_gfxFont);
    else renderer.setTextFont(font);
    return renderer.fontHeight(font);
}
int16_t Seeed_Sprite::fontHeight() { return fontHeight(_textFont); }

bool Seeed_Sprite::loadFont(const uint8_t* fontData) {
    smoothSpriteTarget = this;
    _smoothFont.begin(drawSmoothSpritePixel, drawSmoothSpriteHLine,
                      fillSmoothSpriteRect, readSmoothSpritePixel);
    return _smoothFont.loadFont(fontData);
}
#if SEEED_GFX_HAS_FS
bool Seeed_Sprite::loadFont(const char* path, fs::FS& fileSystem) {
    smoothSpriteTarget = this;
    _smoothFont.begin(drawSmoothSpritePixel, drawSmoothSpriteHLine,
                      fillSmoothSpriteRect, readSmoothSpritePixel);
    return _smoothFont.loadFont(path, fileSystem);
}
#endif
void Seeed_Sprite::unloadFont() {
    _smoothFont.unloadFont();
    if (smoothSpriteTarget == this) smoothSpriteTarget = nullptr;
}

size_t Seeed_Sprite::write(uint8_t c) {
    if (c == '\n') {
        _utf8BytesRemaining = 0;
        _cursorX = 0;
        _cursorY += fontHeight();
        if (_textWrapY && _cursorY >= height()) _cursorY = 0;
        return 1;
    }
    if (c == '\r') return 1;
    const uint16_t code = decodeUtf8Stream(c, _utf8Codepoint,
                                           _utf8BytesRemaining);
    if (!code) return 1;
    const int16_t advance = _smoothFont.isLoaded()
        ? static_cast<int16_t>(_smoothFont.getCharWidth(code))
        : drawChar(code, -32768, -32768, _textFont);
    if (_textWrapX && _cursorX + std::max<int16_t>(0, advance) > width()) {
        _cursorX = 0;
        _cursorY += fontHeight();
    }
    if (_textWrapY && _cursorY >= height()) _cursorY = 0;
    if (_smoothFont.isLoaded()) drawGlyph(code);
    else {
        const int16_t drawn = drawChar(code, _cursorX, _cursorY, _textFont);
        _cursorX += drawn;
    }
    return 1;
}
