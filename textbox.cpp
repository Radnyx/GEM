#include "textbox.h"
#include "joypad.h"
#include "debug.h"
#include <SDL2/SDL.h>
#include "cpu.h"

TextBox::TextBox(int32_t x, int32_t y, uint32_t w, uint32_t h)
: Component(x, y)
{
    width = w;
    height = h;
}

void TextBox::update(Debugger* debugger)
{
    int32_t mousex, mousey;
    SDL_GetMouseState(&mousex, &mousey);

    if ((Joypad::mousedown || Joypad::mouseup) &&
        (mousex > x && mousey > y && mousex <= x + (int)width && mousey <= y + (int)height))
    {
        if (Joypad::mouseup) {
            pressed = true;
            Joypad::typeStack.clear();
        }
    }
    else if (Joypad::mousedown) {
        pressed = false;
    }

    // Write and control text from key input
    if (pressed)
    {
        while (!Joypad::typeStack.empty())
        {
            char c = Joypad::typeStack.back();
            if (text.size() < limit)
            {
                bool check = false;
                if (hex)
                    check = isxdigit(c);
                else
                    check = c >= 32 && c < 127;
                if (check)
                    text += c;
            }

            if (c == SDLK_BACKSPACE)
                text = text.substr(0,text.size()-1);

            if (c == SDLK_RETURN)
                onenter(this, debugger);

            Joypad::typeStack.pop_back();
        }
    }
}

void TextBox::draw(Debugger* debugger)
{
    debugger->drawRect(0x3D5C5C, x, y, width, height);
    debugger->fillRect(pressed ? 0xFFFFFF : 0xD7D7D7, x + 1, y + 1, width - 2, height - 2);

    uint32_t start = 0;
    uint32_t end = text.size();
    if (text.size() >= width / 9) {
        start = text.size() - (width / 9) + 1;
        end -= start;
    }
    std::string sub = text.substr(start, end);
    debugger->write(sub.c_str(), pressed ? 0x0 : 0x202020, x + 4, y + 4);

    if (pressed && CPU::frameticks / 30 % 2)
        debugger->drawChar('_', 0x0, x + 4 + (start+end) * 9, y + 4);
}

void TextBox::setLimit(uint32_t lim)
{
    limit = lim;
}

void TextBox::setText(std::string text)
{
    this->text = text;
}

void TextBox::setHex(bool b)
{
    hex = b;
}
