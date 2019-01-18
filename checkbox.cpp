#include "checkbox.h"
#include "debug.h"
#include <SDL2/SDL.h>
#include "joypad.h"

CheckBox::CheckBox(int32_t x, int32_t y)
: Component(x, y)
{
    this->x = x;
    this->y = y;
    oncheck = nullptr;
}

void CheckBox::update(Debugger* debugger)
{
    int32_t mousex, mousey;
    SDL_GetMouseState(&mousex, &mousey);

    if ((Joypad::mousedown || Joypad::mouseup) &&
        (mousex > x && mousey > y && mousex <= x + 17 && mousey <= y + 18))
    {
        pressed = true;

        if (Joypad::mouseup) {
            if (check)
                *check = !(*check);
            if (oncheck)
                oncheck(debugger);
        }

    } else pressed = false;
}

void CheckBox::draw(Debugger* debugger)
{
    debugger->drawRect(pressed ? 0xF0F0F0 : 0x3D5C5C, x, y, 17, 18);
    debugger->fillRect(pressed ? 0x090923 : 0x0F0F3B, x + 1, y + 1, 15, 16);

    if (*check)
    {
        debugger->drawChar((char)4, 0xE6E600, x + 4, y + 2);
    }
}
