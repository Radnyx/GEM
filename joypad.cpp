#include "joypad.h"
#include <SDL2/SDL.h>
#include "cpu.h"
#include <iostream>
#include "gpu.h"

namespace Joypad
{
    bool mousedown = false;
    bool mouseup = false;
    bool control = false;

    bool pressed[512];

    std::deque<char> typeStack;

    void update()
    {
        mouseup = false;
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                CPU::quit();
                break;

            case SDL_KEYDOWN:
                {
                // Control type stack size
                if (typeStack.size() >= 256) typeStack.pop_front();

                SDL_Keycode key = event.key.keysym.sym;

                // Typing buffer
                if (key < 256) {
                    // Shifting
                    if(event.key.keysym.mod & KMOD_SHIFT) {
                        if (isalpha(key)) key -= 32;
                        switch (key)
                        {
                            case '-': key = '_'; break;
                        }
                    }
                    typeStack.push_back(key);
                }

                if (control && !CPU::tasplayer.isRunning())
                    pressed[event.key.keysym.scancode] = true;
                }
                break;

            case SDL_KEYUP:
                if (control && !CPU::tasplayer.isRunning())
                    pressed[event.key.keysym.scancode] = false;
                break;

            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
                        if (event.window.windowID == CPU::debugger.getWindowID())
                        {
                            CPU::debugger.close();
                        }
                        else if (event.window.windowID == GPU::getWindowID())
                        {
                            CPU::quit();
                        }
                        break;

                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        if (event.window.windowID == GPU::getWindowID())
                            control = true;
                        break;
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        if (event.window.windowID == GPU::getWindowID())
                            control = false;
                        break;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                mousedown = true;
                break;

            case SDL_MOUSEBUTTONUP:
                mouseup = true;
                mousedown = false;
                break;
            }


        }
    }

    uint8_t getButtons()
    {
        return (!pressed[SDL_SCANCODE_X]) |
                (!pressed[SDL_SCANCODE_Z] << 1) |
                (!pressed[SDL_SCANCODE_RSHIFT] << 2) |
                (!pressed[SDL_SCANCODE_RETURN] << 3);
    }

    uint8_t getDirections()
    {
        uint8_t value = (!pressed[SDL_SCANCODE_RIGHT]) |
                (!pressed[SDL_SCANCODE_LEFT] << 1) |
                (!pressed[SDL_SCANCODE_UP] << 2) |
                (!pressed[SDL_SCANCODE_DOWN] << 3);
        if (pressed[SDL_SCANCODE_RIGHT] && pressed[SDL_SCANCODE_LEFT])
            value |= 0x3;
        if (pressed[SDL_SCANCODE_UP] && pressed[SDL_SCANCODE_DOWN])
            value |= 0xC;
        return  value;
    }
}
