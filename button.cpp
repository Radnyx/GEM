#include "button.h"
#include "debug.h"
#include <string.h>
#include <SDL2/SDL.h>
#include <iostream>
#include "joypad.h"
#include "cpu.h"

Button::Button(std::string text, int32_t x, int32_t y, int32_t w, int32_t h)
: Component(x, y)
{
    this->text = text;
    width = w;
    height = h;
}

void Button::update(Debugger* debugger)
{
    int32_t mousex, mousey;
    SDL_GetMouseState(&mousex, &mousey);

    if ((Joypad::mousedown || Joypad::mouseup) &&
        (mousex > x && mousey > y && mousex <= x + width && mousey <= y + height))
    {
        pressed = true;

        if (Joypad::mouseup)
            onclick(debugger);

    } else pressed = false;
}

void Button::draw(Debugger* debugger)
{
    debugger->drawRect(pressed ? 0xF0F0F0 : 0x3D5C5C, x, y, width, height);
    debugger->fillRect(pressed ? 0x090923 : 0x0F0F3B, x + 1, y + 1, width - 2, height - 2);
    debugger->write(text.c_str(), pressed ? 0x363636 : 0xF0F0F0, x + (width / 2) - (text.size() * 9) / 2, y + height / 2 - 7);
}

void Button::setText(std::string text)
{
    this->text = text;
}
