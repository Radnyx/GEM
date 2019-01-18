#include <SDL2/SDL.h>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "cpu.h"
#include "gpu.h"
#include "dis.h"
#include "apu.h"


char* readFileBytes(const char *name, uint32_t* length)
{
    std::ifstream fl(name);
    if (!fl) return nullptr;
    fl.seekg(0, std::ios::end);
    size_t len = fl.tellg();
    if (length) *length = len;
    char *ret = new char[len];
    fl.seekg(0, std::ios::beg);
    fl.read(ret, len);
    fl.close();
    return ret;
}

int main(int argc, char* argv[])
{
    /*if (argc <= 2) {
        std::cout << "Please supply the paths to (1) the gameboy boot ROM and (2) a game to play." << std::endl;
        return 1;
    }*/

    SDL_Init(SDL_INIT_EVERYTHING);
    Disassembler::init();

    CPU::debugger.init();
    GPU::init();

    CPU::initBootROM("roms/boot.gb");
    if(!CPU::init("roms/" "link's awakening" ".gb"))
    {

        APU::init();
        CPU::run();
    } else {
        SDL_CloseAudio();
        SDL_Quit();
        return 1;
    }

    SDL_CloseAudio();
    SDL_Quit();
    return 0;
}
