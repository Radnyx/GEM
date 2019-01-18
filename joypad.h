#ifndef JOYPAD_H
#define JOYPAD_H
#include <deque>
#include <stdint.h>

namespace Joypad
{
    extern bool mousedown;
    extern bool mouseup;
    extern bool control;

    extern bool pressed[512];

    extern std::deque<char> typeStack;

    void update();

    uint8_t getButtons();
    uint8_t getDirections();
}

#endif // JOYPAD_H
