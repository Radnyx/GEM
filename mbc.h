#ifndef MBC_H
#define MBC_H
#include <stdint.h>

struct MBC
{
    int mode;
    int rombanks;
    int ramsize;
    bool enableram;
    void (*write)(MBC*,uint16_t loc, uint8_t value);
};

extern MBC nrom;
extern MBC mbc1;

#endif // MBC_H
