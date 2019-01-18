#include "cpu.h"
#include <SDL2/SDL.h>
#include <iostream>
#include "gpu.h"
#include "joypad.h"
#include <stdio.h>
#include "mbc.h"
#include "apu.h"
#include <fstream>

namespace CPU
{
    std::string romtitle;
    const char* bootdir;
    uint8_t currentROMBank = 0;

    Debugger debugger;
    bool opendebugger = true;

    TAS tasplayer;

    // Registers
    uint8_t A, F,
            B, C,
            D, E,
            H, L;

    // Stack Pointer
    uint16_t SP;
    // Program Counter
    uint16_t PC;


    bool runningBootROM = false;

    uint8_t* ROM = nullptr;
    uint8_t* CART_ROM;

    uint8_t RAM[0x10000];
    uint8_t* EXTERNAL_RAM = nullptr;
    uint8_t* RAM_BANK;

    bool accessOAM = true;
    bool accessVRAM = true;

    // Interrupt enable
    bool ienable = true;
    int pendingIEnable = 0, pendingIDisable = 0;

    bool halted = false;
    bool haltskip = false;

    // Memory Bank Controller
    MBC mbc = nrom;

    bool running = true;

    // Steps through the program one
    // instruction at a time
    bool stepmode = false;
    int takestep = 0;

    // Clock cycles
    uint32_t cycles = 0;

    // DIV timer cycles
    uint32_t divcycles = 0;

    // TIMA cycles
    uint32_t timercycles = 0;

    // 60 fps count
    uint32_t frameticks = 0;


    uint8_t getInput(uint8_t val)
    {
        if (!(val & 0x20))
            return (val & 0xF0) | Joypad::getButtons();

        if (!(val & 0x10))
            return (val & 0xF0) | Joypad::getDirections();

        return 0xFF;
    }

    // Read a byte from a memory location
    uint8_t read(uint16_t loc)
    {
        if (loc >= 0x4000 && loc <= 0x7FFF) {
            return CART_ROM[loc - 0x4000];
        }
        else if (loc >= 0x8000 && loc < 0xA000)
        {
            if (accessVRAM)
                return RAM[loc];
            else
                return 0xFF;
        }
        else if (loc >= 0xFE00 && loc < 0xFEA0)
        {
            if (accessOAM)
                return RAM[loc];
            else
                return 0;
        }
        else if (loc >= 0xA000 && loc < 0xBFFF)
        {
            if (mbc.enableram)
            {
                return (RAM_BANK[loc - 0xA000]);
            }
        }
        else if (loc >= 0xE000 && loc <= 0xFDFF)
            return RAM[loc - 0x2000];
        else if (loc >= 0xFF00)
        {
            switch(loc & 0xFF)
            {
                case IO_LY:
                    if (CPU::RAM[IO_LCDC] & (1 << 7))
                        return RAM[IO_LY];
                    else
                        return 0;
                case IO_DMA: return 0;
                case IO_NR13: return 0;
                case IO_NR14: return RAM[loc] & 0x40;
                case IO_NR23: return 0;
                case IO_NR24: return RAM[loc] & 0x40;
                case IO_NR30: return RAM[loc] & 0x80;
                case IO_NR33: return 0;
                case IO_NR34: return RAM[loc] & 0x40;
                case IO_NR44: return RAM[loc] & 0x40;

                default: return RAM[loc];
            }
        }
        return RAM[loc];
    }

    // Write a byte to a memory location
    void write(uint16_t loc, uint8_t byte)
    {
        // NEEDS MBC SUPPORT
        if (loc < 0x8000) {
            mbc.write(&mbc, loc, byte);
            return;
        }
        else if (loc >= 0x8000 && loc < 0xA000)
        {
            if (accessVRAM)
                RAM[loc] = byte;
        }
        else if (loc >= 0xFE00 && loc < 0xFEA0)
        {
            if (accessOAM)
                RAM[loc] = byte;
        }
        else if (loc >= 0xA000 && loc < 0xBFFF)
        {
            if (mbc.enableram)
            {
                RAM_BANK[loc - 0xA000] = byte;
            }
        }
        else if (loc >= 0xE000 && loc <= 0xFDFF)
            RAM[loc - 0x2000] = byte;

        else if (loc >= 0xFF00)
        {
            switch(loc)
            {
                case IO_P1: {
                    uint8_t oldP1 = RAM[IO_P1];
                    byte = (byte & 0xF0) | (oldP1 & ~0xF0);
                    byte = getInput(byte);
                    if ((byte != oldP1) && ((byte & 0x0F) != 0x0F))
                        RAM[IO_IF] |= INTERRUPT_HILO;
                    RAM[IO_P1] = byte;
                    } break;

                case IO_LY: /* LY is reset when written to */
                case IO_DIV: RAM[loc] = 0; break; /* DIV is reset when written to */
                case IO_STAT: RAM[loc] =(byte & ~0x07) | (RAM[loc] & 0x07);

                /* SOUND REGISTERS*/
                case IO_NR10:
                    if (!APU::poweron) break;
                    APU::freqSweepTime = (byte >> 4) & 3;
                    APU::freqSweepDirection = byte & 0x8;
                    APU::freqSweepShift = byte & 0x7;
                    RAM[loc] = byte;
                    break;
                case IO_NR11:
                    if (!APU::poweron) break;
                    APU::channel[0].length = byte & 0x3F;
                    APU::channel[0].lengthTimer
                        = (64 - APU::channel[0].length) * (1/256.f) * 4194304;
                    APU::duty1 = APU::dutyCycles[byte >> 6];
                    RAM[loc] = byte;
                    break;
                case IO_NR12:
                    if (!APU::poweron) break;
                    APU::channel[0].volume = byte >> 4;
                    APU::channel[0].volumeSweep = byte & 0x7;
                    APU::channel[0].volumeDirection = byte & 0x8;
                    RAM[loc] = byte;
                    break;
                case IO_NR13:
                    if (!APU::poweron) break;
                    APU::channel[0].freq &= 0xFF00;
                    APU::channel[0].freq |= byte;
                    RAM[loc] = byte;
                    break;
                case IO_NR14:
                    if (!APU::poweron) break;
                    APU::channel[0].restart = byte & 0x80;
                    APU::channel[0].uselength = byte & 0x40;
                    APU::channel[0].freq &= 0xF8FF;
                    APU::channel[0].freq |= int(byte & 0x7) << 8;

                    APU::channel[0].length = RAM[IO_NR11] & 0x3F;
                    APU::channel[0].lengthTimer
                        = (64 - APU::channel[0].length) * (1/256.f) * 4194304;

                    if (byte & 0x80)
                    {
                        RAM[IO_NR52] |= 1;

                        APU::channel[0].volume = RAM[IO_NR12] >> 4;
                        APU::freqSweepTime = (RAM[IO_NR10] >> 4) & 3;

                        if (!(APU::freqSweepTime || APU::freqSweepShift))
                            RAM[IO_NR52] &= ~1;

                        if (APU::freqSweepShift && APU::freqSweepTime)
                            APU::calcFreqSweep();

                    }
                    RAM[loc] = byte;
                    break;

                case IO_NR21:
                    if (!APU::poweron) break;
                    APU::channel[1].length = byte & 0x3F;
                    APU::channel[1].lengthTimer
                        = (64 - APU::channel[1].length) * (1/256.f) * 4194304;
                    APU::duty2 = APU::dutyCycles[byte >> 6];
                    RAM[loc] = byte;
                    break;
                case IO_NR22:
                    if (!APU::poweron) break;
                    APU::channel[1].volume = byte >> 4;
                    APU::channel[1].volumeSweep = byte & 0x7;
                    APU::channel[1].volumeDirection = byte & 0x8;
                    RAM[loc] = byte;
                    break;
                case IO_NR23:
                    if (!APU::poweron) break;
                    APU::channel[1].freq &= 0xFF00;
                    APU::channel[1].freq |= byte;
                    RAM[loc] = byte;
                    break;
                case IO_NR24:
                    if (!APU::poweron) break;
                    APU::channel[1].restart = byte & 0x80;
                    APU::channel[1].uselength = byte & 0x40;
                    APU::channel[1].freq &= 0xF8FF;
                    APU::channel[1].freq |= int(byte & 0x7) << 8;

                    APU::channel[1].length = RAM[IO_NR21] & 0x3F;
                    APU::channel[1].lengthTimer
                        = (64 - APU::channel[1].length) * (1/256.f) * 4194304;

                    if (byte & 0x80)
                    {
                        RAM[IO_NR52] |= 2;
                        APU::channel[1].volume = RAM[IO_NR22] >> 4;
                    }
                    RAM[loc] = byte;
                    break;

                /* Free Wave Channel */
                case IO_NR30:
                    if (!APU::poweron) break;
                    APU::playwave = byte & 0x80;
                    RAM[loc] = byte;
                    break;
                case IO_NR31:
                    if (!APU::poweron) break;
                    APU::channel[2].length = byte;
                    APU::channel[2].lengthTimer
                        = (256 - APU::channel[2].length) * (1/256.f) * 4194304;
                    RAM[loc] = byte;
                    break;
                case IO_NR32:
                    if (!APU::poweron) break;
                    switch((byte >> 5) & 0x3)
                    {
                        case 0:  APU::channel[2].volume = 0; break;
                        case 1:  APU::channel[2].volume = 4; break;
                        case 2:  APU::channel[2].volume = 2; break;
                        case 3:  APU::channel[2].volume = 1; break;
                    }
                    RAM[loc] = byte;
                    break;
                case IO_NR33:
                    if (!APU::poweron) break;
                    APU::channel[2].freq &= 0xFF00;
                    APU::channel[2].freq |= byte;
                    RAM[loc] = byte;
                    break;
                case IO_NR34:
                    if (!APU::poweron) break;
                    APU::channel[2].restart = byte & 0x80;
                    APU::channel[2].uselength = byte & 0x40;
                    APU::channel[2].freq &= 0xF8FF;
                    APU::channel[2].freq |= int(byte & 0x7) << 8;

                    if (byte & 0x80)
                    {
                        RAM[IO_NR52] |= 4;

                        APU::channel[2].length = RAM[IO_NR31];
                        APU::channel[2].lengthTimer
                        = (256 - APU::channel[2].length) * (1/256.f) * 4194304;


                        switch((RAM[IO_NR32] >> 5) & 0x3)
                        {
                            case 0:  APU::channel[2].volume = 0; break;
                            case 1:  APU::channel[2].volume = 4; break;
                            case 2:  APU::channel[2].volume = 2; break;
                            case 3:  APU::channel[2].volume = 1; break;
                        }
                    }
                    RAM[loc] = byte;
                    break;

                /* Nose Channel */
                case IO_NR41: // Channel 4 Sound Length
                    if (!APU::poweron) break;
                    APU::channel[3].length = byte & 0x3F;
                    APU::channel[3].lengthTimer
                        = (64 - APU::channel[3].length) * (1/256.f) * 4194304;
                    RAM[loc] = byte;
                    break;
                case IO_NR42: // Channel 4 Volume Envelope
                    if (!APU::poweron) break;
                    APU::channel[3].volume = byte >> 4;
                    APU::channel[3].volumeSweep = byte & 0x7;
                    APU::channel[3].volumeDirection = byte & 0x8;
                    RAM[loc] = byte;
                    break;
                case IO_NR43: // Channel 4 Polynomial Counter
                    if (!APU::poweron) break;
                    APU::shiftClockFreq = (byte >> 4);
                    APU::counterStepWidth = (byte & 0x8);
                    APU::divRatio = (byte & 0x7);

                    {
                        double r = APU::divRatio;
                        if (r == 0) r = 0.5d;
                        APU::noiseScaler = 524288.d / r / (double)(1 << (APU::shiftClockFreq+1));
                    }

                    RAM[loc] = byte;
                    break;
                case IO_NR44: // Channel 4 Counter/Consecutive; Intial
                    if (!APU::poweron) break;
                    APU::channel[3].restart = byte & 0x80;
                    APU::channel[3].uselength = byte & 0x40;

                    if (byte & 0x80)
                    {
                        RAM[IO_NR52] |= 8;

                        APU::channel[3].length = RAM[IO_NR41] & 0x3F;
                        APU::channel[3].lengthTimer
                        = (64 - APU::channel[3].length) * (1/256.f) * 4194304;

                        APU::channel[3].volume = RAM[IO_NR42] >> 4;
                        APU::lfsr = ~0;
                    }
                    RAM[loc] = byte;
                    break;

                /* Sound Control Registers */
                case IO_NR50: // Channel control / ON-OFF/ Volume
                    if (!APU::poweron) break;
                    APU::solevel_1 = (byte & 7) / 7.f;
                    APU::solevel_2 = ((byte >> 4) & 7) / 7.f;
                    RAM[loc] = byte;
                    break;

                case IO_NR51:
                    if (!APU::poweron) break;
                    RAM[loc] = byte;
                    break;

                // IO_NR51 (Sound Output selection)
                // is determined during mixing
                case IO_NR52: // Sound on/off
                    APU::poweron = byte & 0x80;
                    RAM[loc] &= 0x7F;
                    RAM[loc] |= APU::poweron << 7;

                    if (!APU::poweron)
                    {
                        for (int i = IO_NR10; i < IO_NR51; i++)
                            CPU::RAM[i] = 0;
                    }
                    break;

                /* DMA Transfer */
                case IO_DMA: {
                    RAM[loc] = byte;
                    uint16_t start = byte << 8;
                    for (uint16_t i = 0; i < 0xA0; i++)
                    {
                        RAM[OAM + i] = read(start + i);
                    }
                    } break;



                default: RAM[loc] = byte; break;
            }
        }
        else RAM[loc] = byte;
    }

    // Write a word to a memory location
    inline void writeWord(uint16_t loc, uint16_t word)
    {
        write(loc, word & 0xFF);
        write(loc + 1, word >> 8);
    }

    // Read the next byte at PC
    inline uint8_t getImmediateByte()
    {
        return read(PC++);
    }

    // Read the next word at PC
    inline uint16_t getImmediateWord()
    {
        PC += 2;
        return (read(PC - 1) << 8) | read(PC - 2);
    }

    // Update cycles
    inline void tick(uint32_t t)
    {
        cycles += t;
        divcycles += t;
        if (RAM[IO_TAC] & 4) timercycles += t;
        GPU::rendercycles += t;

        for (int i = 0; i < 4; i++) {
            APU::channel[i].envelopeTimer += t;
            APU::channel[i].lengthTimer -= t;
        }
        APU::noiseFreqTimer += t;
        APU::freqSweepTimer += t;
    }

    #define FUNC_FLAG(name, flag) \
        inline void set## name ##Flag(bool b) \
        { if (b) F |= flag; else F &= ~flag; }
    FUNC_FLAG(Zero,       ZERO_FLAG);
    FUNC_FLAG(Subtract,   SUBTRACT_FLAG);
    FUNC_FLAG(HalfCarry,  HALF_CARRY_FLAG);
    FUNC_FLAG(Carry,      CARRY_FLAG);
    #undef FUNC_FLAG

    // Register pair AF
    uint16_t AF()
    {
        return (A << 8) | F;
    }

    // Register pair BC
    uint16_t BC()
    {
        return (B << 8) | C;
    }

    // Register pair DE
    uint16_t DE()
    {
        return (D << 8) | E;
    }

    // Register pair HL
    uint16_t HL()
    {
        return (H << 8) | L;
    }

    // Change HL's value
    inline void setHL(uint16_t word)
    {
        H = word >> 8;
        L = word;
    }

    inline void add(uint8_t value)
    {
        uint16_t result = (uint16_t)A + (uint16_t)value;
        setSubtractFlag(false);
        setHalfCarryFlag(((A & 0xF) + (value & 0xF)) > 0x0F);
        setCarryFlag(result > 0xFF);
        A = result;
        setZeroFlag(A == 0);
    }

    inline void adc(uint8_t value)
    {
        uint16_t result = (uint16_t)A + (uint16_t)value + (uint16_t)getCarry();
        setSubtractFlag(false);
        setHalfCarryFlag(((A & 0xF) + (value & 0xF) + getCarry()) & 0x10);
        setCarryFlag(result & 0x100);
        A = result;
        setZeroFlag(A == 0);
    }

    inline void sub(uint8_t value)
    {
        uint16_t result = (uint16_t)A - (uint16_t)value;
        setSubtractFlag(true);
        setHalfCarryFlag((result & 0xF) > (A & 0xF));
        setCarryFlag(result > 0xFF);
        A = result;
        setZeroFlag(A == 0);
    }

    inline void sbc(uint8_t value)
    {
        uint16_t result = (uint16_t)A - (uint16_t)value - getCarry();
        setSubtractFlag(true);
        setHalfCarryFlag((A^value^(result & 0xFF)) & 0x10);
        setCarryFlag(result > 0xFF);
        A = result;
        setZeroFlag(A == 0);
    }

    inline void cp(uint8_t value)
    {
        uint16_t result = (uint16_t)A - value;
        setZeroFlag((uint8_t)result == 0);
        setSubtractFlag(true);
        setHalfCarryFlag((result & 0xF) > (A & 0xF));
        setCarryFlag(result > 0xFF);
    }

    inline void inc(uint8_t& a)
    {
        uint8_t result = a + 1;
        setZeroFlag(result == 0);
        setSubtractFlag(false);
        setHalfCarryFlag(((a & 0xF) + (1 & 0xF)) & 0x10);
        a = result;
    }

    inline void dec(uint8_t& a)
    {
        uint8_t result = a - 1;
        setZeroFlag(result == 0);
        setSubtractFlag(true);
        setHalfCarryFlag(((a & 0xF) - (1 & 0xF)) & 0x10);
        a = result;
    }

    inline void addWord(uint16_t value)
    {
        uint32_t hl = HL();
        uint32_t result = hl + (uint32_t)value;
        setCarryFlag(result > 0xFFFF);
        setHalfCarryFlag(((hl & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF);
        setSubtractFlag(false);
        setHL(result);
    }

    // Increment register pair
    inline void incPair(uint8_t& h, uint8_t& l)
    {
        l++;
        if (l == 0) h++;
    }

    // Decrement register pair
    inline void decPair(uint8_t& h, uint8_t& l)
    {
        l--;
        if (l == 0xFF) h--;
    }

    inline void pushWord(uint16_t val)
    {
        SP -= 2;
        writeWord(SP, val);
    }

    inline uint16_t popWord()
    {
        uint16_t val = read(SP) | (read(SP + 1) << 8);
        SP += 2;
        return val;
    }

    // Jump relative IF
    inline void JRIF(bool b)
    {
        int8_t val = getImmediateByte();
        if (b)
        {
            PC += (int16_t)val;
            tick(4);
        }
    }

    // Return IF
    inline void RETIF(bool b)
    {
        if (b)
        {
            PC = popWord();
            tick(12);
        }
    }

    // Jump IF
    inline void JPIF(bool b)
    {
        uint16_t val = getImmediateWord();
        if (b)
        {
            PC = val;
            tick(4);
        }
    }

    // Call IF
    inline void CALLIF(bool b)
    {
        uint16_t val = getImmediateWord();
        if (b)
        {
            pushWord(PC);
            PC = val;
            tick(12);
        }
    }

    inline void RST(uint8_t pos)
    {
        pushWord(PC);
        PC = 0x0000 + pos;
    }

    inline uint8_t getReg(uint8_t r)
    {
        switch(r)
        {
            case 0: return B;
            case 1: return C;
            case 2: return D;
            case 3: return E;
            case 4: return H;
            case 5: return L;
            case 6: tick(4); return read(HL());
            case 7: return A;
        }
        return 0;
    }

    // STOP
    inline void stop()
    {

    }

    // Enable interrupts
    inline void EI()
    {
        pendingIEnable = 1;
    }

    // Disable interrupts
    inline void DI()
    {
        pendingIDisable = 1;
    }

    // Rotate left
    inline void rlc(uint8_t& reg)
    {
        setCarryFlag(reg >> 7);
        reg = (reg >> 7) | (reg << 1);
        setZeroFlag(reg == 0);
        setSubtractFlag(false);
        setHalfCarryFlag(false);
    }

    // Rotate right
    inline void rrc(uint8_t& reg)
    {
        setCarryFlag(reg & 1);
        reg = (reg >> 1) | (reg << 7);
        setZeroFlag(reg == 0);
        setSubtractFlag(false);
        setHalfCarryFlag(false);
    }

    // Rotate left through carry
    inline void rl(uint8_t& reg)
    {
        uint8_t temp = reg & 0x80;
        reg = (reg << 1) | getCarry();
        setCarryFlag(temp);
        setZeroFlag(reg == 0);
        setSubtractFlag(false);
        setHalfCarryFlag(false);
    }

    // Rotate right through carry
    inline void rr(uint8_t& reg)
    {
        uint8_t temp = reg & 1;
        reg = (getCarry() << 7) | (reg >> 1);
        setCarryFlag(temp);
        setZeroFlag(reg == 0);
        setSubtractFlag(false);
        setHalfCarryFlag(false);
    }

    // Shift left into Carry
    inline void sla(uint8_t& reg)
    {
        setCarryFlag(reg & 0x80);
        reg <<= 1;
        setZeroFlag(reg == 0);
    }

    // Shift right into Carry
    inline void sra(uint8_t& reg)
    {
        setCarryFlag(reg & 0x1);
        reg >>= 1;
        setZeroFlag(reg == 0);
    }

    inline void swap(uint8_t& reg)
    {
        reg = (reg >> 4) | (reg << 4);
        setZeroFlag(reg == 0);
        setSubtractFlag(false);
        setHalfCarryFlag(false);
        setCarryFlag(false);
    }

    // Shift right into Carry
    inline void srl(uint8_t& reg)
    {
        setCarryFlag(reg & 1);
        reg >>= 1;
        setZeroFlag(reg == 0);
        setSubtractFlag(false);
        setHalfCarryFlag(false);
    }

    void printStatus()
    {
        printf("PC : %4X\n", PC);
        printf("SP : %4X\n", SP);
        printf("AF : %4X\n", AF());
        printf("BC : %4X\n", BC());
        printf("DE : %4X\n", DE());
        printf("HL : %4X\n", HL());
    }

    void initBootROM(const char* filename)
    {
        runningBootROM = true;

        if (!bootdir) bootdir = filename;
        static uint8_t* bootrom = (uint8_t*)readFileBytes(filename);
        for (int i = 0; i <= 0xFF; i++) {
            RAM[i] = bootrom[i];
        }

        PC = 0x00;
    }

    // Initialize the CPU
    int init(const char* filename)
    {
        /* Retrieve the rom from the file if it exists */
        uint8_t* newrom = (uint8_t*)readFileBytes(filename);
        if (newrom == nullptr)
        {
            std::cout << "Error (File \"" << filename << "\" not found)";
            return 1;
        }

        if (ROM) {
            delete[] ROM;
            ROM = nullptr;
        }

        ROM = newrom;

        GPU::raise();
        hardreset();

        return 0;
    }

    /* Initialize cartridge specific things */
    void hardreset()
    {
        if (EXTERNAL_RAM) {
            delete[] EXTERNAL_RAM;
            EXTERNAL_RAM = nullptr;
        }

        romtitle = "";

        /* Title */
        for (int i = 0x134; i < 0x142 && ROM[i] != 0; i++)
            romtitle += (char)ROM[i];

        /* Get cart info */
        switch (ROM[0x147])
        {

        case 0x01:
            mbc = mbc1;
            break;

        case 0x03:
            mbc = mbc1;
            break;
        }

        switch(ROM[0x148])
        {
            case 0: mbc.rombanks = 2; break;
            case 1: mbc.rombanks = 4; break;
            case 2: mbc.rombanks = 8; break;
            case 3: mbc.rombanks = 16; break;
            case 4: mbc.rombanks = 32; break;
            case 5: mbc.rombanks = 64; break;
            case 6: mbc.rombanks = 128; break;
        }

        mbc.ramsize = ROM[0x149];
        if (mbc.ramsize != 0)
        {
            uint32_t kb_ramsize = 0;
            if (mbc.ramsize == 1)
                kb_ramsize = 0x800;
            else if (mbc.ramsize == 2)
                kb_ramsize = 0x2000;
            else if (mbc.ramsize == 3)
                kb_ramsize = 0x8000;
            else if (mbc.ramsize == 4)
                kb_ramsize = 0x20000;

            EXTERNAL_RAM = new uint8_t[kb_ramsize];
            RAM_BANK = EXTERNAL_RAM;
        }

        reset();
    }

    /* Initialize variables */
    void reset()
    {
        tasplayer.stop();

        ienable = true;
        halted = false;
        haltskip = false;
        cycles = 0;
        divcycles = 0;
        timercycles = 0;
        frameticks = 0;

        for (int i = 0x8000; i <= 0xFFFF; i++)
            RAM[i] = rand();
        for (int i = 0xA000; i < 0xC000; i++)
            RAM[i] = 0xFF;
        for (int i = 0xFF00; i < 0xFF80; i++)
            RAM[i] = 0xFF;

        PC = 0x0100;
        SP = 0xFFFE;

        A = 0x01;
        F = 0xB0;
        B = 0x00;
        C = 0x13;
        D = 0x00;
        E = 0xD8;
        H = 0x01;
        L = 0x4D;
        RAM[0xFF05] = 0x00;
        RAM[0xFF06] = 0x00;
        RAM[0xFF07] = 0x00;
        RAM[0xFF10] = 0x80;
        RAM[0xFF11] = 0xBF;
        RAM[0xFF12] = 0xF3;
        RAM[0xFF14] = 0xBF;
        RAM[0xFF16] = 0x3F;
        RAM[0xFF17] = 0x00;
        RAM[0xFF19] = 0xBF;
        RAM[0xFF1A] = 0x7F;
        RAM[0xFF1B] = 0xFF;
        RAM[0xFF1C] = 0x9F;
        RAM[0xFF1E] = 0xBF;
        RAM[0xFF20] = 0xFF;
        RAM[0xFF21] = 0x00;
        RAM[0xFF22] = 0x00;
        RAM[0xFF23] = 0xBF;
        RAM[0xFF24] = 0x77;
        RAM[0xFF25] = 0xF3;
        RAM[0xFF26] = 0xF1;
        RAM[0xFF40] = 0x91;
        RAM[0xFF42] = 0x00;
        RAM[0xFF43] = 0x00;
        RAM[0xFF45] = 0x00;
        RAM[0xFF47] = 0xFC;
        RAM[0xFF48] = 0xFF;
        RAM[0xFF49] = 0xFF;
        RAM[0xFF4A] = 0x00;
        RAM[0xFF4B] = 0x00;
        RAM[0xFFFF] = 0x00;

        // Copy ROM Bank #0 into RAM
        for(uint32_t i = 0; i < 0x4000; i++)
            RAM[i] = ROM[i];

        /* Points to ROM Bank # 1 */
        CART_ROM = ROM + 0x4000;

        initBootROM(bootdir);
    }

    void setBank(uint8_t bank)
    {
        if (bank == 0) bank = 1;
        currentROMBank = bank;
        if (bank > mbc.rombanks)
            std::cout << "Error: RAM Bank #" << (int)bank << " does not exist!" << std::endl;
        CART_ROM = ROM + 0x4000 * bank;
    }

    void prefixCB();


    // Execute a single instructiOn
    void step()
    {
        if (runningBootROM && PC == 0x100)
        {
            runningBootROM = false;
            for (int i = 0; i <= 0xFF; i++)
                RAM[i] = ROM[i];
            return;
        }

        uint8_t temp;
        uint16_t temp16;

        uint8_t opcode = getImmediateByte();

        // If interrupts are disabled and a HALT
        // is executed, then stop updating the PC
        // for ONE instruction.
        if (haltskip)
        {
            PC--;
            haltskip = false;
        }

        switch(opcode)
        {
            /* NOP */
            case 0x00: tick(4); break;

            /* LD BC, d16 */
            case 0x01:
                C = getImmediateByte();
                B = getImmediateByte();
                tick(12);
                break;

            /* LD (BC), A */
            case 0x02:
                write(BC(), A);
                tick(8);
                break;

            /* INC BC */
            case 0x03:
                incPair(B, C);
                tick(8);
                break;

            /* INC B */
            case 0x04:
                inc(B);
                tick(4);
                break;

            /* DEC B */
            case 0x05:
                dec(B);
                tick(4);
                break;

            /* LD B, d8 */
            case 0x06:
                B = getImmediateByte();
                tick(8);
                break;

            /* RLCA */
            case 0x07:
                setCarryFlag(A >> 7);
                A = (A >> 7) | (A << 1);
                setZeroFlag(false);
                setSubtractFlag(false);
                setHalfCarryFlag(false);
                tick(4);
                break;

            /* LD (a16), SP */
            case 0x08:
                writeWord(getImmediateWord(), SP);
                tick(20);
                break;

            /* ADD HL, BC */
            case 0x09:
                addWord(BC());
                tick(8);
                break;

            /* LD A, (BC) */
            case 0x0A:
                A = read(BC());
                tick(8);
                break;

            /* DEC BC */
            case 0x0B:
                decPair(B, C);
                tick(8);
                break;

            /* INC C */
            case 0x0C:
                inc(C);
                tick(4);
                break;

            /* DEC C */
            case 0x0D:
                dec(C);
                tick(4);
                break;

            /* LD C, d8 */
            case 0x0E:
                C = getImmediateByte();
                tick(8);
                break;

            /* RRCA */
            case 0x0F:
                setCarryFlag(A & 1);
                A = (A << 7) | (A >> 1);
                setZeroFlag(false);
                setSubtractFlag(false);
                setHalfCarryFlag(false);
                tick(4);
                break;

            /* STOP 0 */
            case 0x10:
                stop();
                tick(4);
                break;

            /* LD DE, d16 */
            case 0x11:
                E = getImmediateByte();
                D = getImmediateByte();
                tick(12);
                break;

            /* LD (DE), A */
            case 0x12:
                write(DE(), A);
                tick(8);
                break;

            /* INC DE */
            case 0x13:
                incPair(D, E);
                tick(8);
                break;

            /* INC D */
            case 0x14:
                inc(D);
                tick(4);
                break;

            /* DEC D */
            case 0x15:
                dec(D);
                tick(4);
                break;

            /* LD D, d8 */
            case 0x16:
                D = getImmediateByte();
                tick(8);
                break;

            /* RLA */
            case 0x17:
                temp = A >> 7;
                A = getCarry() | (A << 1);
                setCarryFlag(temp);
                setZeroFlag(false);
                setSubtractFlag(false);
                setHalfCarryFlag(false);
                tick(4);
                break;

            /* JR r8 */
            case 0x18:
                temp = getImmediateByte();
                PC += (int16_t)int8_t(temp);
                tick(12);
                break;

            /* ADD HL, DE */
            case 0x19:
                addWord(DE());
                tick(8);
                break;

            /* LD A, (DE) */
            case 0x1A:
                A = read(DE());
                tick(8);
                break;

            /* DEC DE */
            case 0x1B:
                decPair(D, E);
                tick(8);
                break;

            /* INC E */
            case 0x1C:
                inc(E);
                tick(4);
                break;

            /* DEC E */
            case 0x1D:
                dec(E);
                tick(4);
                break;

            /* LD E, d8 */
            case 0x1E:
                E = getImmediateByte();
                tick(8);
                break;

            /* RRA */
            case 0x1F:
                temp = A & 1;
                A = ((int)getCarry() << 7) | (A >> 1);
                setCarryFlag(temp);
                setZeroFlag(false);
                setSubtractFlag(false);
                setHalfCarryFlag(false);
                tick(4);
                break;

            /* JR NZ, r8 */
            case 0x20:
                JRIF(!getZero());
                tick(8);
                break;

            /* LD HL, d16 */
            case 0x21:
                L = getImmediateByte();
                H = getImmediateByte();
                tick(12);
                break;

            /* LD (HL+), A */
            case 0x22:
                write(HL(), A);
                incPair(H, L);
                tick(8);
                break;

            /* INC HL */
            case 0x23:
                incPair(H, L);
                tick(8);
                break;

            /* INC H */
            case 0x24:
                inc(H);
                tick(4);
                break;

            /* DEC H */
            case 0x25:
                dec(H);
                tick(4);
                break;

            /* LD H, d8 */
            case 0x26:
                H = getImmediateByte();
                tick(8);
                break;

            /* DAA */
            case 0x27:
                temp16 = A;
                if (!getSubtract())
                {
                    if ((temp16 & 0x0F) > 0x09 || getHalfCarry()) temp16 += 0x06;
                    if (temp16 > 0x9F || getCarry()) temp16 += 0x60;
                }
                else
                {
                    if (getHalfCarry())
                        temp16 = (temp16 - 6) & 0xFF;
                    if (getCarry())
                        temp16 -= 0x60;
                }
                if (temp16 > 0xFF)
                    setCarryFlag(true);
                temp16 &= 0xFF;

                setZeroFlag(temp16 == 0);
                setHalfCarryFlag(false);
                A = temp16;
                tick(4);
                break;

            /* JR Z, r8 */
            case 0x28:
                JRIF(getZero());
                tick(8);
                break;

            /* ADD HL, HL */
            case 0x29:
                addWord(HL());
                tick(8);
                break;

            /* LD A, (HL+) */
            case 0x2A:
                A = read(HL());
                incPair(H, L);
                tick(8);
                break;

            /* DEC HL */
            case 0x2B:
                decPair(H, L);
                tick(8);
                break;

            /* INC L */
            case 0x2C:
                inc(L);
                tick(4);
                break;

            /* DEC L */
            case 0x2D:
                dec(L);
                tick(4);
                break;

            /* LD L, d8 */
            case 0x2E:
                L = getImmediateByte();
                tick(8);
                break;

            /* CPL */
            case 0x2F:
                A = ~A;
                setSubtractFlag(true);
                setHalfCarryFlag(true);
                tick(4);
                break;

            /* JR NC, r8 */
            case 0x30:
                JRIF(!getCarry());
                tick(8);
                break;

            /* LD SP, d16 */
            case 0x31:
                SP = getImmediateWord();
                tick(12);
                break;

            /* LD (HL-), A */
            case 0x32:
                write(HL(), A);
                decPair(H, L);
                tick(8);
                break;

            /* INC SP */
            case 0x33:
                SP++;
                tick(8);
                break;

            /* INC (HL) */
            case 0x34:
                temp16 = HL();
                temp = read(temp16);
                inc(temp);
                write(temp16, temp);
                tick(12);
                break;

            /* DEC (HL) */
            case 0x35:
                temp16 = HL();
                temp = read(temp16);
                dec(temp);
                write(temp16, temp);
                tick(12);
                break;

            /* LD (HL), d8 */
            case 0x36:
                write(HL(), getImmediateByte());
                tick(12);
                break;

            /* SCF */
            case 0x37:
                setCarryFlag(true);
                setSubtractFlag(false);
                setHalfCarryFlag(false);
                tick(4);
                break;

            /* JR C, r8 */
            case 0x38:
                JRIF(getCarry());
                tick(8);
                break;

            /* ADD HL, SP */
            case 0x39:
                addWord(SP);
                tick(8);
                break;

            /* LD A, (HL-) */
            case 0x3A:
                A = read(HL());
                decPair(H, L);
                tick(8);
                break;

            /* DEC SP */
            case 0x3B:
                SP--;
                tick(8);
                break;

            /* INC A */
            case 0x3C:
                inc(A);
                tick(4);
                break;

            /* DEC A */
            case 0x3D:
                dec(A);
                tick(4);
                break;

            /* LD A, d8 */
            case 0x3E:
                A = getImmediateByte();
                tick(8);
                break;

            /* CCF */
            case 0x3F:
                setCarryFlag(!getCarry());
                setSubtractFlag(false);
                setHalfCarryFlag(false);
                tick(4);
                break;

            /* LD B, x */
            case 0x40: case 0x41: case 0x42: case 0x43:
            case 0x44: case 0x45: case 0x46: case 0x47:
                B = getReg(opcode & 7);
                tick(4);
                break;

            /* LD C, x */
            case 0x48: case 0x49: case 0x4A: case 0x4B:
            case 0x4C: case 0x4D: case 0x4E: case 0x4F:
                C = getReg(opcode & 7);
                tick(4);
                break;

            /* LD D, x */
            case 0x50: case 0x51: case 0x52: case 0x53:
            case 0x54: case 0x55: case 0x56: case 0x57:
                D = getReg(opcode & 7);
                tick(4);
                break;

            /* LD E, x */
            case 0x58: case 0x59: case 0x5A: case 0x5B:
            case 0x5C: case 0x5D: case 0x5E: case 0x5F:
                E = getReg(opcode & 7);
                tick(4);
                break;

            /* LD H, x */
            case 0x60: case 0x61: case 0x62: case 0x63:
            case 0x64: case 0x65: case 0x66: case 0x67:
                H = getReg(opcode & 7);
                tick(4);
                break;

            /* LD L, x */
            case 0x68: case 0x69: case 0x6A: case 0x6B:
            case 0x6C: case 0x6D: case 0x6E: case 0x6F:
                L = getReg(opcode & 7);
                tick(4);
                break;

            /* LD (HL), x */
            case 0x70: case 0x71: case 0x72: case 0x73:
            case 0x74: case 0x75: case 0x77:
                write(HL(), getReg(opcode & 7));
                tick(8);
                break;

            /* HALT */
            case 0x76:
                if (ienable)
                    halted = true;
                else
                    haltskip = true;
                tick(4);
                break;

            /* LD A, x */
            case 0x78: case 0x79: case 0x7A: case 0x7B:
            case 0x7C: case 0x7D: case 0x7E: case 0x7F:
                A = getReg(opcode & 7);
                tick(4);
                break;

            /* ADD A, x */
            case 0x80: case 0x81: case 0x82: case 0x83:
            case 0x84: case 0x85: case 0x86: case 0x87:
                add(getReg(opcode & 7));
                tick(4);
                break;

            /* ADC A, x */
            case 0x88: case 0x89: case 0x8A: case 0x8B:
            case 0x8C: case 0x8D: case 0x8E: case 0x8F:
                adc(getReg(opcode & 7));
                tick(4);
                break;

            /* SUB x */
            case 0x90: case 0x91: case 0x92: case 0x93:
            case 0x94: case 0x95: case 0x96: case 0x97:
                sub(getReg(opcode & 7));
                tick(4);
                break;

            /* SBC x */
            case 0x98: case 0x99: case 0x9A: case 0x9B:
            case 0x9C: case 0x9D: case 0x9E: case 0x9F:
                sbc(getReg(opcode & 7));
                tick(4);
                break;

            /* AND x */
            case 0xA0: case 0xA1: case 0xA2: case 0xA3:
            case 0xA4: case 0xA5: case 0xA6: case 0xA7:
                A &= getReg(opcode & 7);
                setZeroFlag(A == 0);
                setSubtractFlag(false);
                setHalfCarryFlag(true);
                setCarryFlag(false);
                tick(4);
                break;

            /* XOR x */
            case 0xA8: case 0xA9: case 0xAA: case 0xAB:
            case 0xAC: case 0xAD: case 0xAE: case 0xAF:
                A ^= getReg(opcode & 7);
                setZeroFlag(A == 0);
                setSubtractFlag(false);
                setHalfCarryFlag(false);
                setCarryFlag(false);
                tick(4);
                break;

            /* OR x */
            case 0xB0: case 0xB1: case 0xB2: case 0xB3:
            case 0xB4: case 0xB5: case 0xB6: case 0xB7:
                A |= getReg(opcode & 7);
                setZeroFlag(A == 0);
                setSubtractFlag(false);
                setHalfCarryFlag(false);
                setCarryFlag(false);
                tick(4);
                break;

            /* CP x */
            case 0xB8: case 0xB9: case 0xBA: case 0xBB:
            case 0xBC: case 0xBD: case 0xBE: case 0xBF:
                cp(getReg(opcode & 7));
                tick(4);
                break;

            /* RET NZ */
            case 0xC0:
                RETIF(!getZero());
                tick(8);
                break;

            /* POP BC */
            case 0xC1:
                temp16 = popWord();
                B = temp16 >> 8;
                C = temp16;
                tick(12);
                break;

            /* JP NZ, a16 */
            case 0xC2:
                JPIF(!getZero());
                tick(12);
                break;

            /* JP a16 */
            case 0xC3:
                PC = getImmediateWord();
                tick(16);
                break;

            /* CALL NZ, a16 */
            case 0xC4:
                CALLIF(!getZero());
                tick(12);
                break;

            /* PUSH BC */
            case 0xC5:
                pushWord(BC());
                tick(16);
                break;

            /* ADD A, d8 */
            case 0xC6:
                add(getImmediateByte());
                tick(8);
                break;

            /* RST 00H */
            case 0xC7:
                RST(0x00);
                tick(16);
                break;

            /* RET Z */
            case 0xC8:
                RETIF(getZero());
                tick(8);
                break;

            /* RET */
            case 0xC9:
                PC = popWord();
                tick(16);
                break;

            /* JP Z, a16 */
            case 0xCA:
                JPIF(getZero());
                tick(12);
                break;

            /* PREFIX CB */
            case 0xCB:
                prefixCB();
                tick(4);
                return;

            /* CALL Z, a16 */
            case 0xCC:
                CALLIF(getZero());
                tick(12);
                break;

            /* CALL a16 */
            case 0xCD:
                pushWord(PC + 2);
                PC = getImmediateWord();
                tick(24);
                break;

            /* ADC A, d8 */
            case 0xCE:
                adc(getImmediateByte());
                tick(8);
                break;

            /* RST 08H */
            case 0xCF:
                RST(8);
                tick(16);
                break;

            /* RET NC */
            case 0xD0:
                RETIF(!getCarry());
                tick(8);
                break;

            /* POP DE */
            case 0xD1:
                temp16 = popWord();
                D = temp16 >> 8;
                E = temp16;
                tick(12);
                break;

            /* JP NC, a16 */
            case 0xD2:
                JPIF(!getCarry());
                tick(12);
                break;

            /* CALL NC, 16 */
            case 0xD4:
                CALLIF(!getCarry());
                tick(12);
                break;

            /* PUSH DE */
            case 0xD5:
                pushWord(DE());
                tick(16);
                break;

            /* SUB d8 */
            case 0xD6:
                sub(getImmediateByte());
                tick(8);
                break;

            /* RST 10H */
            case 0xD7:
                RST(0x10);
                tick(16);
                break;

            /* RET C */
            case 0xD8:
                RETIF(getCarry());
                tick(8);
                break;

            /* RETI */
            case 0xD9:
                PC = popWord();
                ienable = true;
                tick(16);
                break;

            /* JP C, a16 */
            case 0xDA:
                JPIF(getCarry());
                tick(12);
                break;

            /* CALL C, a16 */
            case 0xDC:
                CALLIF(getCarry());
                tick(12);
                break;

            /* SBC A, d8 */
            case 0xDE:
                sbc(getImmediateByte());
                tick(8);
                break;

            /* RST 18H */
            case 0xDF:
                RST(0x18);
                tick(16);
                break;

            /* LDH (a8), A */
            case 0xE0:
                write(0xFF00 + getImmediateByte(), A);
                tick(12);
                break;

            /* POP HL */
            case 0xE1:
                temp16 = popWord();
                H = temp16 >> 8;
                L = temp16;
                tick(12);
                break;

            /* LD (C), A */
            case 0xE2:
                write(0xFF00 + C, A);
                tick(8);
                break;

            /* PUSH HL */
            case 0xE5:
                pushWord(HL());
                tick(16);
                break;

            /* AND d8 */
            case 0xE6:
                A &= getImmediateByte();
                setZeroFlag(A == 0);
                setSubtractFlag(false);
                setHalfCarryFlag(true);
                setCarryFlag(false);
                tick(8);
                break;

            /* RST 20H */
            case 0xE7:
                RST(0x20);
                tick(16);
                break;

            /* ADD SP, r8 */
            case 0xE8:
                temp16 = int16_t(int8_t(getImmediateByte()));
                setZeroFlag(false);
                setSubtractFlag(false);
                setHalfCarryFlag(((SP & 0x0F) + (temp16 & 0x0F)) > 0x0F);
                setCarryFlag(((SP & 0xFF) + (temp16 & 0xFF)) > 0xFF);
                SP += temp16;
                tick(16);
                break;

            /* JP (HL) */
            case 0xE9:
                PC = HL();
                tick(4);
                break;

            /* LD (a16), A */
            case 0xEA:
                write(getImmediateWord(), A);
                tick(16);
                break;

            /* XOR d8 */
            case 0xEE:
                A ^= getImmediateByte();
                setZeroFlag(A == 0);
                setSubtractFlag(false);
                setHalfCarryFlag(false);
                setCarryFlag(false);
                tick(8);
                break;

            /* RST 28H */
            case 0xEF:
                RST(0x28);
                tick(16);
                break;

            /* LDH A, (a8) */
            case 0xF0:
                A = read(0xFF00 + getImmediateByte());
                tick(12);
                break;

            /* POP AF */
            case 0xF1:
                temp16 = popWord();
                A = temp16 >> 8;
                F = temp16 & 0xF0;
                tick(12);
                break;

            /* LD A, (C) */
            case 0xF2:
                A = read(0xFF00 + C);
                tick(8);
                break;

            /* DI */
            case 0xF3:
                DI();
                break;

            /* PUSH AF */
            case 0xF5:
                pushWord(AF());
                tick(16);
                break;

            /* OR d8 */
            case 0xF6:
                A |= getImmediateByte();
                setZeroFlag(A == 0);
                setSubtractFlag(false);
                setHalfCarryFlag(false);
                setCarryFlag(false);
                tick(8);
                break;

            /* RST 30H */
            case 0xF7:
                RST(0x30);
                tick(16);
                break;

            /* LD HL, SP + r8 */
            case 0xF8:
                temp16 = int16_t(int8_t(getImmediateByte()));
                setHL(SP + temp16);
                setHalfCarryFlag(((SP & 0x0F) + (temp16 & 0x0F)) > 0x0F);
                setCarryFlag(((SP & 0xFF) + (temp16 & 0xFF)) > 0xFF);
                setZeroFlag(false);
                setSubtractFlag(false);
                tick(12);
                break;

            /* LD SP, HL */
            case 0xF9:
                SP = HL();
                tick(8);
                break;

            /* LD A, (a16) */
            case 0xFA:
                A = read(getImmediateWord());
                tick(16);
                break;

            /* EI */
            case 0xFB:
                EI();
                tick(4);
                break;

            /* CP d8 */
            case 0xFE:
                cp(getImmediateByte());
                tick(8);
                break;

            /* RST 38H */
            case 0xFF:
                RST(0x38);
                tick(16);
                break;

            default:
                std::cout << "Error (Invalid Opcode): " << std::hex << (int)opcode << std::endl;
                printStatus();
                break;
        }
    }

    void prefixCB()
    {
        uint8_t temp;
        uint16_t temp16;

        uint8_t opcode = getImmediateByte();
        switch(opcode)
        {

        /* RLC B */
        case 0x00: rlc(B); tick(8); break;

        /* RLC C */
        case 0x01: rlc(C); tick(8); break;

        /* RLC D */
        case 0x02: rlc(D); tick(8); break;

        /* RLC E */
        case 0x03: rlc(E); tick(8); break;

        /* RLC (HL) */
        case 0x06:
            temp = read(HL());
            rlc(temp);
            write(HL(), temp);
            tick(16);
            break;

        /* RLC A */
        case 0x07: rlc(A); tick(8); break;

        /* RRC B */
        case 0x08: rrc(B); tick(8); break;

        /* RRC C */
        case 0x09: rrc(C); tick(8); break;

        /* RRC E */
        case 0x0B: rrc(E); tick(8); break;

        /* RRC L */
        case 0x0D: rrc(L); tick(8); break;

        /* RRC (HL) */
        case 0x0E:
            temp = read(HL());
            rrc(temp);
            write(HL(), temp);
            tick(16);
            break;

        /* RL B */
        case 0x10: rl(B); tick(8); break;

        /* RL C */
        case 0x11: rl(C); tick(8); break;

        /* RL D */
        case 0x12: rl(D); tick(8); break;

        /* RL E */
        case 0x13: rl(E); tick(8); break;

        /* RL H */
        case 0x14: rl(H); tick(8); break;

        /* RL L */
        case 0x15: rl(L); tick(8); break;

        /* RL (HL) */
        case 0x16:
            temp = read(HL());
            rl(temp);
            write(HL(), temp);
            tick(16);
            break;

        /* RL A */
        case 0x17: rl(A); tick(8); break;

        /* RR B */
        case 0x18: rr(B); tick(8); break;

        /* RR C */
        case 0x19: rr(C); tick(8); break;

        /* RR D */
        case 0x1A: rr(D); tick(8); break;

        /* RR E */
        case 0x1B: rr(E); tick(8); break;

        /* RR H */
        case 0x1C: rr(H); tick(8); break;

         /* RR L */
        case 0x1D: rr(L); tick(8); break;

         /* RR A */
        case 0x1F: rr(A); tick(8); break;

        /* SLA B */
        case 0x20: sla(B); tick(8); break;

        /* SLA C */
        case 0x21: sla(C); tick(8); break;

        /* SLA D */
        case 0x22: sla(D); tick(8); break;

        /* SLA E */
        case 0x23: sla(E); tick(8); break;

        /* SLA H */
        case 0x24: sla(H); tick(8); break;

        /* SLA L */
        case 0x25: sla(L); tick(8); break;

        /* SLA (HL) */
        case 0x26:
            temp = read(HL());
            sla(temp);
            write(HL(), temp);
            tick(16);
            break;

        /* SLA A */
        case 0x27: sla(A); tick(8); break;

        /* SRA B */
        case 0x28: sra(B); tick(8); break;

        /* SRA C */
        case 0x29: sra(C); tick(8); break;

        /* SRA D */
        case 0x2A: sra(D); tick(8); break;

        /* SRA E */
        case 0x2B: sra(E); tick(8); break;

        /* SRA (HL) */
        case 0x2E:
            temp = read(HL());
            sra(temp);
            write(HL(), temp);
            tick(16);
            break;

        /* SRA A */
        case 0x2F: sra(A); tick(8); break;

        /* SWAP B */
        case 0x30: swap(B); tick(8); break;

        /* SWAP C */
        case 0x31: swap(C); tick(8); break;

        /* SWAP E */
        case 0x33: swap(E); tick(8); break;

        /* SWAP L */
        case 0x35: swap(L); tick(8); break;

        /* SWAP (HL)*/
        case 0x36:
            temp = read(HL());
            swap(temp);
            write(HL(), temp);
            tick(16);
            break;

        /* SWAP A */
        case 0x37: swap(A); tick(8); break;


        /* SRL B */
        case 0x38: srl(B); tick(8); break;

        /* SRL C */
        case 0x39: srl(C); tick(8); break;

        /* SRL D */
        case 0x3A: srl(D); tick(8); break;

        /* SRL E */
        case 0x3B: srl(E); tick(8); break;

        /* SRL H */
        case 0x3C: srl(H); tick(8); break;

        /* SRL L */
        case 0x3D: srl(L); tick(8); break;

        /* SRL A*/
        case 0x3F: srl(A); tick(8); break;

        /* BIT 0, x */
        case 0x40: case 0x41: case 0x42: case 0x43:
        case 0x44: case 0x45: case 0x46: case 0x47:
            setZeroFlag((getReg(opcode & 7) & (1 << 0)) == 0);
            setSubtractFlag(false);
            setHalfCarryFlag(true);
            tick(8);
            if (opcode == 0x76) tick(4); // Extra 4 ticks for [HL]
            break;

        /* BIT 1, x */
        case 0x48: case 0x49: case 0x4A: case 0x4B:
        case 0x4C: case 0x4D: case 0x4E: case 0x4F:
            setZeroFlag((getReg(opcode & 7) & (1 << 1)) == 0);
            setSubtractFlag(false);
            setHalfCarryFlag(true);
            tick(8);
            if (opcode == 0x6E) tick(4); // Extra 4 ticks for [HL]
            break;

        /* BIT 2, x */
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57:
            setZeroFlag((getReg(opcode & 7) & (1 << 2)) == 0);
            setSubtractFlag(false);
            setHalfCarryFlag(true);
            tick(8);
            if (opcode == 0x76) tick(4); // Extra 4 ticks for [HL]
            break;

        /* BIT 3, x */
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:
            setZeroFlag((getReg(opcode & 7) & (1 << 3)) == 0);
            setSubtractFlag(false);
            setHalfCarryFlag(true);
            tick(8);
            if (opcode == 0x6E) tick(4); // Extra 4 ticks for [HL]
            break;

        /* BIT 4, x */
        case 0x60: case 0x61: case 0x62: case 0x63:
        case 0x64: case 0x65: case 0x66: case 0x67:
            setZeroFlag((getReg(opcode & 7) & (1 << 4)) == 0);
            setSubtractFlag(false);
            setHalfCarryFlag(true);
            tick(8);
            if (opcode == 0x76) tick(4); // Extra 4 ticks for [HL]
            break;


        /* BIT 5, x */
        case 0x68: case 0x69: case 0x6A: case 0x6B:
        case 0x6C: case 0x6D: case 0x6E: case 0x6F:
            setZeroFlag((getReg(opcode & 7) & (1 << 5)) == 0);
            setSubtractFlag(false);
            setHalfCarryFlag(true);
            tick(8);
            if (opcode == 0x6E) tick(4); // Extra 4 ticks for [HL]
            break;

        /* BIT 6, x */
        case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x76: case 0x77:
            setZeroFlag((getReg(opcode & 7) & (1 << 6)) == 0);
            setSubtractFlag(false);
            setHalfCarryFlag(true);
            tick(8);
            if (opcode == 0x76) tick(4); // Extra 4 ticks for [HL]
            break;

        /* BIT 7, x */
        case 0x78: case 0x79: case 0x7A: case 0x7B:
        case 0x7C: case 0x7D: case 0x7E: case 0x7F:
            setZeroFlag((getReg(opcode & 7) & (1 << 7)) == 0);
            setSubtractFlag(false);
            setHalfCarryFlag(true);
            tick(8);
            if (opcode == 0x7E) tick(4); // Extra 4 ticks for [HL]
            break;

        /* RES 0, B */
        case 0x80:
            B = B & ~1;
            tick(8);
            break;

        /* RES 0, (HL) */
        case 0x86:
            temp16 = HL();
            write(temp16, read(temp16) & ~1);
            tick(16);
            break;

        /* RES 0, A */
        case 0x87:
            A = A & ~1;
            tick(8);
            break;

        /* RES 1, B */
        case 0x88:
            B = B & ~(1 << 1);
            tick(8);
            break;

        /* RES 1, (HL) */
        case 0x8E:
            temp16 = HL();
            write(temp16, read(temp16) & ~(1 << 1));
            tick(16);
            break;

        /* RES 1, A */
        case 0x8F:
            A = A & ~(1 << 1);
            tick(8);
            break;

        /* RES 2, B */
        case 0x90:
            B = B & ~(1 << 2);
            tick(8);
            break;

        /* RES 2, H */
        case 0x94:
            H = H & ~(1 << 2);
            tick(8);
            break;

        /* RES 2, L */
        case 0x95:
            L = L & ~(1 << 2);
            tick(8);
            break;

        /* RES 2, (HL) */
        case 0x96:
            temp16 = HL();
            write(temp16, read(temp16) & ~(1 << 2));
            tick(16);
            break;

        /* RES 2, A */
        case 0x97:
            A = A & ~(1 << 2);
            tick(8);
            break;

        /* RES 3, B */
        case 0x98:
            B = B & ~(1 << 3);
            tick(8);
            break;

        /* RES 3, (HL) */
        case 0x9E:
            temp16 = HL();
            write(temp16, read(temp16) & ~(1 << 3));
            tick(16);
            break;

        /* RES 3, A */
        case 0x9F:
            A = A & ~(1 << 3);
            tick(8);
            break;

        /* RES 4, B */
        case 0xA0:
            B = B & ~(1 << 4);
            tick(8);
            break;

        /* RES 4, (HL) */
        case 0xA6:
            temp16 = HL();
            write(temp16, read(temp16) & ~(1 << 4));
            tick(16);
            break;

        /* RES 4, A */
        case 0xA7:
            A = A & ~(1 << 4);
            tick(8);
            break;

        /* RES 5, B */
        case 0xA8:
            B = B & ~(1 << 5);
            tick(8);
            break;

        /* RES 5, (HL) */
        case 0xAE:
            temp16 = HL();
            write(temp16, read(temp16) & ~(1 << 5));
            tick(16);
            break;

        /* RES 5, A */
        case 0xAF:
            A = A & ~(1 << 5);
            tick(8);
            break;

        /* RES 6, B */
        case 0xB0:
            B = B & ~(1 << 6);
            tick(8);
            break;

        /* RES 6, H */
        case 0xB4:
            H = H & ~(1 << 6);
            tick(8);
            break;

        /* RES 6, (HL) */
        case 0xB6:
            temp16 = HL();
            write(temp16, read(temp16) & ~(1 << 6));
            tick(16);
            break;

        /* RES 6, A */
        case 0xB7:
            A = A & ~(1 << 6);
            tick(8);
            break;

        /* RES 7, B */
        case 0xB8:
            B = B & ~(1 << 7);
            tick(8);
            break;

        /* RES 7, C */
        case 0xB9:
            C = C & ~(1 << 7);
            tick(8);
            break;

        /* RES 7, D */
        case 0xBA:
            D = D & ~(1 << 7);
            tick(8);
            break;

        /* RES 7, E */
        case 0xBB:
            E = E & ~(1 << 7);
            tick(8);
            break;

        /* RES 7, H */
        case 0xBC:
            H = H & ~(1 << 7);
            tick(8);
            break;

        /* RES 7, L */
        case 0xBD:
            L = L & ~(1 << 7);
            tick(8);
            break;

        /* RES 7, (HL) */
        case 0xBE:
            temp16 = HL();
            write(temp16, read(temp16) & ~(1 << 7));
            tick(16);
            break;

        /* RES 7, A */
        case 0xBF:
            A = A & ~(1 << 7);
            tick(8);
            break;

        /* SET 0, B */
        case 0xC0:
            B |= (1 << 0);
            tick(8);
            break;

        /* SET 0, C */
        case 0xC1:
            C |= (1 << 0);
            tick(8);
            break;

        /* SET 0, D */
        case 0xC2:
            D |= (1 << 0);
            tick(8);
            break;

        /* SET 0, L */
        case 0xC5:
            L |= (1 << 0);
            tick(8);
            break;

        /* SET 0, (HL) */
        case 0xC6:
            temp16 = HL();
            write(temp16, read(temp16) | (1 << 0));
            tick(16);
            break;

        /* SET 0, A */
        case 0xC7:
            A |= (1 << 0);
            tick(8);
            break;

        /* SET 1, B */
        case 0xC8:
            B |= (1 << 1);
            tick(8);
            break;

        /* SET 1, E */
        case 0xCB:
            E |= (1 << 1);
            tick(8);
            break;

        /* SET 1, (HL) */
        case 0xCE:
            temp16 = HL();
            write(temp16, read(temp16) | (1 << 1));
            tick(16);
            break;

        /* SET 1, A */
        case 0xCF:
            A |= (1 << 1);
            tick(8);
            break;

        /* SET 2, B */
        case 0xD0:
            B |= (1 << 2);
            tick(8);
            break;

        /* SET 2, L */
        case 0xD5:
            L |= (1 << 2);
            tick(8);
            break;

        /* SET 2, (HL) */
        case 0xD6:
            temp16 = HL();
            write(temp16, read(temp16) | (1 << 2));
            tick(16);
            break;

        /* SET 2, A */
        case 0xD7:
            A |= (1 << 2);
            tick(8);
            break;

        /* SET 3, B */
        case 0xD8:
            B |= (1 << 3);
            tick(8);
            break;

        /* SET 3, L */
        case 0xDD:
            L |= (1 << 3);
            tick(8);
            break;

        /* SET 3, (HL) */
        case 0xDE:
            temp16 = HL();
            write(temp16, read(temp16) | (1 << 3));
            tick(16);
            break;

        /* SET 3, A */
        case 0xDF:
            A |= (1 << 3);
            tick(8);
            break;

        /* SET 4, B */
        case 0xE0:
            B |= (1 << 4);
            tick(8);
            break;

        /* SET 4, D */
        case 0xE2:
            D |= (1 << 4);
            tick(8);
            break;

        /* SET 4, (HL) */
        case 0xE6:
            temp16 = HL();
            write(temp16, read(temp16) | (1 << 4));
            tick(16);
            break;

        /* SET 4, A */
        case 0xE7:
            A |= (1 << 4);
            tick(8);
            break;

        /* SET 5, (HL) */
        case 0xEE:
            temp16 = HL();
            write(temp16, read(temp16) | (1 << 5));
            tick(16);
            break;

        /* SET 5, A */
        case 0xEF:
            A |= (1 << 5);
            tick(8);
            break;

        /* SET 6, B */
        case 0xF0:
            B |= (1 << 6);
            tick(8);
            break;

        /* SET 6, C */
        case 0xF1:
            C |= (1 << 6);
            tick(8);
            break;

        /* SET 6, D */
        case 0xF2:
            D |= (1 << 6);
            tick(8);
            break;

        /* SET 6, E */
        case 0xF3:
            E |= (1 << 6);
            tick(8);
            break;

        /* SET 6, H */
        case 0xF4:
            H |= (1 << 6);
            tick(8);
            break;

        /* SET 6, L */
        case 0xF5:
            L |= (1 << 6);
            tick(8);
            break;

        /* SET 6, (HL) */
        case 0xF6:
            temp16 = HL();
            write(temp16, read(temp16) | (1 << 6));
            tick(16);
            break;

        /* SET 6, A */
        case 0xF7:
            A |= (1 << 6);
            tick(8);
            break;

        /* SET 7, B */
        case 0xF8:
            B |= (1 << 7);
            tick(8);
            break;

        /* SET 7, C */
        case 0xF9:
            C |= (1 << 7);
            tick(8);
            break;

        /* SET 7, D */
        case 0xFA:
            D |= (1 << 7);
            tick(8);
            break;

        /* SET 7, E */
        case 0xFB:
            E |= (1 << 7);
            tick(8);
            break;

        /* SET 7, H */
        case 0xFC:
            H |= (1 << 7);
            tick(8);
            break;

        /* SET 7, L */
        case 0xFD:
            L |= (1 << 7);
            tick(8);
            break;

        /* SET 7, (HL) */
        case 0xFE:
            temp16 = HL();
            write(temp16, read(temp16) | (1 << 7));
            tick(16);
            break;

        /* SET 7, A */
        case 0xFF:
            A |= (1 << 7);
            tick(8);
            break;



        default:
            std::cout << std::hex << "CB : " << (int) opcode << " at " << PC << std::endl;
        }
    }

    void interrupt(uint16_t pos)
    {
        halted = false;
        pushWord(PC);
        PC = pos;
        ienable = false;
    }

    void handleInterrupt()
    {

        uint8_t intr = CPU::RAM[IO_IE] & CPU::RAM[IO_IF];
        if (!intr) return;
        tick(5);

        if (intr & INTERRUPT_VBLANK)
        {
            RAM[IO_IF] &= ~INTERRUPT_VBLANK;
            interrupt(0x40);
        }
        else if (intr & INTERRUPT_LCDC)
        {
            RAM[IO_IF] &= ~INTERRUPT_LCDC;
            interrupt(0x48);
        }
        else if (intr & INTERRUPT_TIMER)
        {
            RAM[IO_IF] &= ~INTERRUPT_TIMER;
            interrupt(0x50);
        }
        else if (intr & INTERRUPT_HILO)
        {
            RAM[IO_IF] &= ~INTERRUPT_HILO;
            interrupt(0x60);
        }
    }

    void exec(uint32_t maxcycles)
    {
        const uint32_t timerSpeeds[4] = { 1024, 16, 64, 256 };
        while(cycles < maxcycles)
        {
            GPU::step();

            // Step Mode control
            if (stepmode)
            {
                if (!takestep)
                {
                    return;
                } else takestep--;
            }

            APU::step();

            if (halted)
            {
                tick(4);
            }
            else
            {
                step();
                if (pendingIEnable)
                {
                    if (pendingIEnable == 2)
                    {
                        ienable = true;
                        pendingIEnable = 0;
                    }
                    else pendingIEnable++;
                }

                if (pendingIDisable)
                {
                    if (pendingIDisable == 2)
                    {
                        ienable = false;
                        pendingIDisable = 0;
                    }
                    else pendingIDisable++;
                }

                /* If the "don't run-until flag" isn't set,
                which means we're running until PC equals
                debugger.rununtil & 0xFFFF, enable stepmode
                until we reach it. OR in the stop-running-until
                flag and return, paused. */
                if (!debugger.closed && (debugger.rununtil & 0x10000) == 0)
                {
                    if (PC == debugger.rununtil)
                    {
                        stepmode = true;
                        debugger.rununtil |= 0x10000;
                        return;
                    }
                }
            }

            if (ienable)
                handleInterrupt();

            // Update DIV timer
            if (divcycles >= 256)
            {
                RAM[IO_DIV]++;
                divcycles = 0;
            }

            // Update timer
            if ((RAM[IO_TAC] & 4) && timercycles >= timerSpeeds[RAM[IO_TAC] & 3])
            {
                RAM[IO_TIMA]++;
                timercycles = 0;
                if (RAM[IO_TIMA] == 0)
                {
                    RAM[IO_TIMA] = RAM[IO_TMA];
                    RAM[IO_IF] |= INTERRUPT_TIMER;
                }
            }

        }
        cycles -= maxcycles;
    }

    void run()
    {

        //uint32_t sec = SDL_GetTicks();
        while (running)
        {
            uint32_t time = SDL_GetTicks();

            exec(69905);

            Joypad::update();

            if (!debugger.closed)
            {
                debugger.update();
                debugger.draw();
            }

            if (Joypad::pressed[SDL_SCANCODE_ESCAPE])
            {

                Joypad::pressed[SDL_SCANCODE_ESCAPE] = false;
                if (debugger.closed)
                    debugger.init();
                else
                    debugger.close();

            }



            frameticks++;
            while(SDL_GetTicks() - time < 16);

            /*
            if (SDL_GetTicks() - sec >= 1000) {
                sec = SDL_GetTicks();
            }
            */
        }
    }

    void quit()
    {
        running = false;
    }
}
