#include "pch.h"

#include "random.h"


namespace math
{
    static std::mt19937_64& GetRndGenerator()
    {
        static std::random_device rndDevice;
        static std::mt19937_64 generator(rndDevice());

        return generator;
    }


    float RndFloat(float min, float max)
    {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(GetRndGenerator());
    }


    int8_t RndInt8(int8_t min, int8_t max)
    {
        return static_cast<int8_t>(RndInt32(min, max));
    }


    uint8_t RndUInt8(uint8_t min, uint8_t max)
    {
        return static_cast<uint8_t>(RndUInt32(min, max));
    }


    int16_t RndInt16(int16_t min, int16_t max)
    {
        std::uniform_int_distribution<int16_t> dist(min, max);
        return dist(GetRndGenerator());
    }


    uint16_t RndUInt16(uint16_t min, uint16_t max)
    {
        std::uniform_int_distribution<uint16_t> dist(min, max);
        return dist(GetRndGenerator());
    }


    int32_t RndInt32(int32_t min, int32_t max)
    {
        std::uniform_int_distribution<int32_t> dist(min, max);
        return dist(GetRndGenerator());
    }


    uint32_t RndUInt32(uint32_t min, uint32_t max)
    {
        std::uniform_int_distribution<uint32_t> dist(min, max);
        return dist(GetRndGenerator());
    }
}