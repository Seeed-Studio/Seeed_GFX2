/**
 * @file   Bus_Parallel8.cpp
 * @brief  Portable digitalWrite-based 8-bit parallel bus implementation
 */

#include "Bus_Parallel8.h"

Bus_Parallel8::Bus_Parallel8(int8_t cs, int8_t dc, int8_t wr, int8_t rd,
                             int8_t d0, int8_t d1, int8_t d2, int8_t d3,
                             int8_t d4, int8_t d5, int8_t d6, int8_t d7)
    : _cs(cs), _dc(dc), _wr(wr), _rd(rd)
    , _d0(d0), _d1(d1), _d2(d2), _d3(d3)
    , _d4(d4), _d5(d5), _d6(d6), _d7(d7)
    , _freq(10000000) {}

Bus_Parallel8::~Bus_Parallel8() {}

bool Bus_Parallel8::begin() {
    const int8_t pins[] = {_cs, _dc, _wr, _d0, _d1, _d2, _d3, _d4, _d5, _d6, _d7};
    for (int8_t pin : pins) {
        if (pin < 0) return false;
        pinMode(pin, OUTPUT);
    }
    if (_rd >= 0) pinMode(_rd, OUTPUT);
    digitalWrite(_cs, HIGH);
    digitalWrite(_dc, HIGH);
    digitalWrite(_wr, HIGH);
    if (_rd >= 0) digitalWrite(_rd, HIGH);
    return true;
}

void Bus_Parallel8::end() { if (_writing) endWrite(); }
void Bus_Parallel8::beginWrite() {
    if (!_writing) { digitalWrite(_cs, LOW); _writing = true; }
}
void Bus_Parallel8::endWrite() {
    if (_writing) { digitalWrite(_cs, HIGH); _writing = false; }
}
void Bus_Parallel8::writeCommand(uint8_t command) {
    const bool standalone = !_writing;
    if (standalone) beginWrite();
    digitalWrite(_dc, LOW);
    writeByte(command);
    digitalWrite(_dc, HIGH);
    if (standalone) endWrite();
}
void Bus_Parallel8::writeData(uint8_t data) {
    const bool standalone = !_writing;
    if (standalone) beginWrite();
    digitalWrite(_dc, HIGH);
    writeByte(data);
    if (standalone) endWrite();
}
void Bus_Parallel8::writeData(const uint8_t* data, size_t len) {
    if (!data) return;
    const bool standalone = !_writing;
    if (standalone) beginWrite();
    digitalWrite(_dc, HIGH);
    for (size_t i = 0; i < len; ++i) writeByte(data[i]);
    if (standalone) endWrite();
}
void Bus_Parallel8::beginRead() {
    beginWrite();
    setDataDirection(INPUT);
}
void Bus_Parallel8::endRead() {
    setDataDirection(OUTPUT);
    endWrite();
}
uint8_t Bus_Parallel8::readData() {
    if (_rd < 0) return 0;
    digitalWrite(_rd, LOW);
    uint8_t value = 0;
    const int8_t pins[] = {_d0, _d1, _d2, _d3, _d4, _d5, _d6, _d7};
    for (uint8_t bit = 0; bit < 8; ++bit)
        if (digitalRead(pins[bit])) value |= static_cast<uint8_t>(1U << bit);
    digitalWrite(_rd, HIGH);
    return value;
}
void Bus_Parallel8::setFrequency(uint32_t freq) { _freq = freq; }

void Bus_Parallel8::setDataDirection(uint8_t mode) {
    const int8_t pins[] = {_d0, _d1, _d2, _d3, _d4, _d5, _d6, _d7};
    for (int8_t pin : pins) pinMode(pin, mode);
}

void Bus_Parallel8::writeByte(uint8_t value) {
    const int8_t pins[] = {_d0, _d1, _d2, _d3, _d4, _d5, _d6, _d7};
    for (uint8_t bit = 0; bit < 8; ++bit)
        digitalWrite(pins[bit], (value & (1U << bit)) ? HIGH : LOW);
    digitalWrite(_wr, LOW);
    digitalWrite(_wr, HIGH);
}
