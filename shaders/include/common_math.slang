__exported import half_utils;


bool IsFlagSet(uint value, uint mask)
{
    return (value & mask) != 0;
}


bool IsBitSet(uint value, uint bit)
{
    return IsFlagSet(value, uint(1) << bit);
}