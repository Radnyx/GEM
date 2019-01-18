#include "mbc.h"
#include "cpu.h"

void nrom_write(MBC* mbc, uint16_t loc, uint8_t value) { }

void mbc1_write(MBC* mbc, uint16_t loc, uint8_t value)
{
    switch (loc >> 12) {
    case 0: case 1:
            mbc->enableram = ((value & 0x0F) == 0xA);
    break;
    case 2: case 3: {
        uint8_t bank = value & 0x1F;
        CPU::setBank(bank);
    } break;
    case 4: case 5:
        if (mbc->mode == 1)
        {

        }
        else
        {

        }
    break;
    case 6: case 7:
        // RAM Mode 0 = 16/8
        // RAM Mode 1 = 4/32
        mbc->mode = value & 1;
    break;
    }
}


MBC nrom = { 0, 0, 0, false, nrom_write };
MBC mbc1 = { 0, 0, 0, false, mbc1_write };
