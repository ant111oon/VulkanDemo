#pragma once

#include <random>


namespace math
{
    float RndFloat(float min, float max);

    int8_t  RndInt8(int8_t min, int8_t max);
    uint8_t RndUInt8(uint8_t min, uint8_t max);

    int16_t  RndInt16(int16_t min, int16_t max);
    uint16_t RndUInt16(uint16_t min, uint16_t max);

    int32_t  RndInt32(int32_t min, int32_t max);
    uint32_t RndUInt32(uint32_t min, uint32_t max);
}