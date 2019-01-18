#ifndef CPU_H
#define CPU_H
#include <stdint.h>
#include "debug.h"
#include "tas.h"
#include <iostream>

const int ZERO_FLAG         = 0x80;
const int SUBTRACT_FLAG     = 0x40;
const int HALF_CARRY_FLAG   = 0x20;
const int CARRY_FLAG        = 0x10;

const uint16_t OAM          = 0xFE00;

const uint16_t IO_P1        = 0xFF00;
const uint16_t IO_DIV       = 0xFF04;
const uint16_t IO_TIMA      = 0xFF05;
const uint16_t IO_TMA       = 0xFF06;
const uint16_t IO_TAC       = 0xFF07;
const uint16_t IO_IF        = 0xFF0F;
const uint16_t IO_NR10      = 0xFF10;
const uint16_t IO_NR11      = 0xFF11;
const uint16_t IO_NR12      = 0xFF12;
const uint16_t IO_NR13      = 0xFF13;
const uint16_t IO_NR14      = 0xFF14;
const uint16_t IO_NR21      = 0xFF16;
const uint16_t IO_NR22      = 0xFF17;
const uint16_t IO_NR23      = 0xFF18;
const uint16_t IO_NR24      = 0xFF19;
const uint16_t IO_NR30      = 0xFF1A;
const uint16_t IO_NR31      = 0xFF1B;
const uint16_t IO_NR32      = 0xFF1C;
const uint16_t IO_NR33      = 0xFF1D;
const uint16_t IO_NR34      = 0xFF1E;
const uint16_t IO_NR41      = 0xFF20;
const uint16_t IO_NR42      = 0xFF21;
const uint16_t IO_NR43      = 0xFF22;
const uint16_t IO_NR44      = 0xFF23;
const uint16_t IO_NR50      = 0xFF24;
const uint16_t IO_NR51      = 0xFF25;
const uint16_t IO_NR52      = 0xFF26;
const uint16_t IO_LCDC      = 0xFF40;
const uint16_t IO_STAT      = 0xFF41;
const uint16_t IO_SCY       = 0xFF42;
const uint16_t IO_SCX       = 0xFF43;
const uint16_t IO_LY        = 0xFF44;
const uint16_t IO_LYC       = 0xFF45;
const uint16_t IO_DMA       = 0xFF46;
const uint16_t IO_BGP       = 0xFF47;
const uint16_t IO_OBP0      = 0xFF48;
const uint16_t IO_OBP1      = 0xFF49;
const uint16_t IO_WY        = 0xFF4A;
const uint16_t IO_WX        = 0xFF4B;
const uint16_t IO_IE        = 0xFFFF;

const uint8_t INTERRUPT_VBLANK  = 0x1;
const uint8_t INTERRUPT_LCDC    = 0x2;
const uint8_t INTERRUPT_TIMER   = 0x4;

const uint8_t INTERRUPT_HILO    = 0x10;

class Debugger;

namespace CPU
{
    extern std::string romtitle;
    extern uint8_t currentROMBank;

    extern Debugger debugger;
    extern bool opendebugger;

    extern TAS tasplayer;

    extern uint8_t  A, F,
                    B, C,
                    D, E,
                    H, L;

    extern uint16_t SP;
    extern uint16_t PC;

    extern bool runningBootROM;

    extern uint8_t RAM[];

    extern bool stepmode;
    extern int takestep;

    extern bool accessOAM;
    extern bool accessVRAM;

    /* These 4 timers get saved in save states */
    extern uint32_t cycles;
    extern uint32_t divcycles;
    extern uint32_t timercycles;
    extern uint32_t frameticks;

    void doVBlank();

    uint16_t AF();
    uint16_t BC();
    uint16_t DE();
    uint16_t HL();

    inline bool getZero() { return F & ZERO_FLAG; }
    inline bool getSubtract() { return F & SUBTRACT_FLAG; }
    inline bool getHalfCarry() { return F & HALF_CARRY_FLAG; }
    inline bool getCarry() { return F & CARRY_FLAG; }

    uint8_t read(uint16_t loc);
    void write(uint16_t loc, uint8_t byte);

    void setBank(uint8_t bank);

    extern uint32_t frameticks;

    void initBootROM(const char* filename);
    // Return 1 on bad filename
    int init(const char* filename);
    void hardreset();
    void reset();

    void run();

    void quit();
}

char* readFileBytes(const char *name, uint32_t* len = nullptr);

#endif // CPU_H
