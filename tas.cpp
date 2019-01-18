#include "tas.h"
#include <iostream>
#include "cpu.h"
#include "joypad.h"
#include <SDL2/SDL.h>
#include "gpu.h"

int TAS::loadVBM(const char* filename)
{

    uint8_t* read = (uint8_t*)readFileBytes(filename, &length);
    if (!read)
    {
        std::cout << "Error (File \"" << filename << "\" not found)";
        return 1;
    }

    if (data)
    {
        delete[] data;
        data = nullptr;
    }

    data = read;
    index = 0;

    if (getInt() != 0x1A4D4256)
    {
        std::cout << "Error (File \"" << filename << "\" has invalid VBM format)";
        return 1;
    }
    getInt(); // Major version number, must be "1"
    getInt(); // Movie movie "uid" - identifies the movie-savestate relationship, also used as the recording time in Unix epoch format
    frames = getInt(); // Number of frames
    getInt(); // Rerecord count
    getByte(); // Movie start flags
    // bit 0: if 1, movie starts from embedded quicksave snapshot
    // bit 1: if 1, movie starts from reset with embedded SRAM
    // if both bits are 1, invalid
    getByte(); // Controller flags
    // bit 0, controller 1 in use. etc.
    getByte(); // System flags
    // bit 0: if 1, for GBA
    // bit 1: if 1, for GBC
    // bit 2: if 1, for SGB
    // if 3 bits are 0, for GB
    getByte(); // Emulator options

    getInt(); //
    getInt(); //
    getInt();
    getInt(); // ROM name
    getInt();
    getInt();
    getByte(); // minor version/revision number of current VBM version
    getByte(); // Internal CRC of ROM used while recording
    getByte(); getByte(); // ROM checksum
    getInt(); // Game code of ROM
    uint32_t offset = getInt(); // Offset to savestate or SRAM
    uint32_t control = getInt(); // offset to controller data inside file

    // After the header is 192 bytes of text.The first 64 of these 192 bytes are for the
    // author's name (or names). The following 128 bytes are for a description of the
    // movie. Both parts must be null terminated.
    for (int i = 0; i < 192; i++)
        getByte();

    index = control;
    CPU::hardreset();
    GPU::raise();

    running = true;
    return 0;
}

void TAS::step()
{
    if (CPU::runningBootROM) return;

    uint8_t pad = getByte();
    uint8_t special = getByte();

    Joypad::pressed[SDL_SCANCODE_X]      = pad & 0x01;
    Joypad::pressed[SDL_SCANCODE_Z]      = pad & 0x02;
    Joypad::pressed[SDL_SCANCODE_RSHIFT] = pad & 0x04;
    Joypad::pressed[SDL_SCANCODE_RETURN] = pad & 0x08;
    Joypad::pressed[SDL_SCANCODE_RIGHT]  = pad & 0x10;
    Joypad::pressed[SDL_SCANCODE_LEFT]   = pad & 0x20;
    Joypad::pressed[SDL_SCANCODE_UP]     = pad & 0x40;
    Joypad::pressed[SDL_SCANCODE_DOWN]   = pad & 0x80;

    if (special & 0x08) CPU::reset();

}

void TAS::stop()
{
    running = false;
}

bool TAS::isRunning()
{
    return running;
}

uint8_t TAS::getByte()
{
    if (index >= length)
    {
        running = false;
        std::cout << "TAS has ended" << std::endl;
        return 0;
    }
    return data[index++];
}

uint32_t TAS::getInt()
{
    uint32_t value = 0;
    value |= getByte();
    value |= getByte() << 8;
    value |= getByte() << 16;
    value |= getByte() << 24;
    return value;
}
