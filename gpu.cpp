#include "gpu.h"
#include <SDL2/SDL.h>
#include <iostream>
#include "cpu.h"
#include "joypad.h"

namespace GPU
{
    uint32_t rendercycles = 0;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* videoTexture = nullptr;

    uint32_t* pixels = nullptr;

    uint32_t scale = 2;

    uint32_t palette[4] = { 0xFFEFCE, 0xDE944A, 0xAD2921, 0x311852 };

    /* Line specific data for sprite drawing and priorities */
    uint8_t bgpixels[160];
    uint8_t spritey[144][160];

    void init()
    {
        window = SDL_CreateWindow("GEM Gameboy Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  160 * scale, 144 * scale, 0);

        renderer = SDL_CreateRenderer(window, -1, 0);

        // Scale info
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
        SDL_RenderSetLogicalSize(renderer, 160, 144);

        videoTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         160, 144);

        pixels = new uint32_t[160 * 144];

    }

    inline uint32_t getPaletteIndex(uint16_t loc, uint8_t col)
    {
        return (CPU::RAM[loc] >> (col << 1)) & 0x03;
    }

    void drawSprites(uint8_t line)
    {
        if (line >= 144) return;

        int objSize = (bool)(CPU::RAM[IO_LCDC] & 0x4);
        objSize++;

        for (int s = 0; s < 40; s++)
        {
            uint16_t spr_idx = s * 4 + OAM;
            uint8_t y = CPU::RAM[spr_idx];
            if (y >= 144 + 16) continue;

            uint8_t x = CPU::RAM[spr_idx + 1] - 8;
            uint8_t tile = CPU::RAM[spr_idx + 2];
            if (objSize == 2) tile &= ~1;
            uint8_t flags = CPU::RAM[spr_idx + 3];

            bool priority = flags & 0x80;
            bool flipv = flags & 0x40;
            bool fliph = flags & 0x20;
            bool pal = flags & 0x10;

            if (flipv && objSize == 2) tile++;

            if (objSize == 1) {
                if (line < y - 16 || line >= y - 8) continue;
            }
            else if (objSize == 2) {
                if (line < y - 16 || line >= y) continue;
                if (line >= y - 8) {
                    if (flipv) tile--;
                    else tile++;
                }
            }

            uint16_t datapos = (0x8000 + tile * 16);
            uint32_t yline = y - line - 1;
            uint32_t offs = (yline % 8);

            if (!flipv) offs = 7 - offs;
            offs <<= 1;
            datapos += offs;

            for (int i = 0; i < 8; i++)
            {
                uint32_t xp = i;
                if (!fliph) xp = 7 - i;

                uint8_t col =  (((CPU::RAM[datapos + 1] >> (xp)) & 1) << 1) |
                                ((CPU::RAM[datapos + 0] >> (xp)) & 1);

                if (col != 0) {
                    uint8_t xx = x + i;
                    if (xx < 0 || xx >= 160 || (priority && bgpixels[xx] != 0)
                        || spritey[line][xx] >= y) continue;

                    pixels[xx + line * 160] = palette[getPaletteIndex(IO_OBP0 + pal, col)];
                    spritey[line][xx] = y;
                }
            }
        }
    }

    void renderLine(uint8_t line)
    {
        if (line >= 144) return;

        uint16_t bgTilemap = (CPU::RAM[IO_LCDC] & 8) ? 0x9C00 : 0x9800;
        uint16_t windowTilemap = (CPU::RAM[IO_LCDC] & 0x40) ? 0x9C00 : 0x9800;

        bool showWindow = (CPU::RAM[IO_LCDC] & 0x20);

        bool dataSigned = !(CPU::RAM[IO_LCDC] & 0x10);

        uint8_t sx = CPU::RAM[IO_SCX];
        uint8_t sy = CPU::RAM[IO_SCY];

        uint8_t wx = CPU::RAM[IO_WX] - 7;
        uint8_t wy = CPU::RAM[IO_WY];

        for (int i = 0; i < 160; i++)
        {
            spritey[line][i] = 0;
            uint16_t bgTilepos = (((i + sx) >> 3) & 31) + (((line + sy) >> 3) & 31) * 32;
            uint16_t windowTilepos = (((i - wx) >> 3) & 31) + (((line - wy) >> 3) & 31) * 32;

            uint8_t bgTile = CPU::RAM[bgTilemap + bgTilepos];
            uint8_t windowTile = CPU::RAM[windowTilemap + windowTilepos];

            uint16_t bgDatapos, windowDatapos;
            if (dataSigned) {
                bgDatapos = (0x9000 + ((int8_t)bgTile) * 16);
                windowDatapos = (0x9000 + ((int8_t)windowTile) * 16);
            }
            else {
                bgDatapos = (0x8000 + (bgTile * 16));
                windowDatapos = (0x8000 + (windowTile * 16));
            }

            bgDatapos += ((line + sy) & 7) << 1;
            windowDatapos += ((line + wy) & 7) << 1;

            uint8_t txp = ((i + sx) & 7);
            uint8_t wxp = ((i - wx) & 7);

            uint8_t bgCol = (((CPU::RAM[bgDatapos + 1]     >> (7 - txp)) & 1) << 1) |
                            ((CPU::RAM[bgDatapos + 0] >> (7 - txp)) & 1);

            uint8_t windowCol = (((CPU::RAM[windowDatapos + 1] >> (7 - wxp)) & 1) << 1) |
                            ((CPU::RAM[windowDatapos + 0] >> (7 - wxp)) & 1);

            uint32_t pos = i + line * 160;
            bgpixels[i] = bgCol;
            pixels[pos] = palette[getPaletteIndex(IO_BGP, bgCol)];
            if (showWindow && line >= wy) {
                bgpixels[i] = windowCol;
                pixels[pos] = palette[getPaletteIndex(IO_BGP, windowCol)];
            }

        }



    }

    inline void checkCoincidence()
    {
        if (CPU::RAM[IO_LY] == CPU::RAM[IO_LYC])
        {
            CPU::RAM[IO_STAT] |= 0x04;
            if (CPU::RAM[IO_STAT] & (1 << 6))
                CPU::RAM[IO_IF] |= INTERRUPT_LCDC;
        }
        else
            CPU::RAM[IO_STAT] &= ~0x04;
    }

    bool pendingVBlank = false;
    void step()
    {
        bool LCDenabled = CPU::RAM[IO_LCDC] & (1 << 7);

        uint8_t mode = CPU::RAM[IO_STAT] & 3;
        switch(mode)
        {
        case 0x00: // HBlank
            CPU::accessVRAM = true;
            if (rendercycles >= 204)
            {
                rendercycles -= 204;
                CPU::RAM[IO_LY]++;

                checkCoincidence();

                if (CPU::RAM[IO_LY] == 144)
                {
                    // Mode -> 1
                    CPU::RAM[IO_STAT] &= ~0x03;
                    CPU::RAM[IO_STAT] |= 0x01;

                    //CPU::RAM[IO_IF] |= INTERRUPT_VBLANK;
                    pendingVBlank = true;
                    refresh();


                    if (LCDenabled)
                    {
                        if (CPU::tasplayer.isRunning())
                        {
                            CPU::tasplayer.step();
                        }
                    }
                }
                else
                {
                    // Mode -> 2
                    CPU::RAM[IO_STAT] &= ~0x03;
                    CPU::RAM[IO_STAT] |= 0x02;

                    if (LCDenabled && (CPU::RAM[IO_STAT] & (1 << 5)))
                        CPU::RAM[IO_IF] |= INTERRUPT_LCDC;
                }

                if (LCDenabled)
                    renderLine(CPU::RAM[IO_LY] - 1);

                if ((CPU::RAM[IO_LCDC] & 0x2))
                    drawSprites(CPU::RAM[IO_LY] - 1);
            }
            break;
        case 0x01:
            CPU::accessVRAM = true;
            if (pendingVBlank && rendercycles >= 24)
            {
                if (LCDenabled)
                {
                    CPU::RAM[IO_IF] |= INTERRUPT_VBLANK;
                    if (CPU::RAM[IO_STAT] & (1 << 4))
                        CPU::RAM[IO_IF] |= INTERRUPT_LCDC;
                }
                pendingVBlank = false;
            }
            if (rendercycles >= 456)
            {
                rendercycles -= 456;

                if (CPU::RAM[IO_LY] == 153)
                {
                    CPU::RAM[IO_LY] = 0;

                    // Mode -> 2
                    CPU::RAM[IO_STAT] &= ~0x03;
                    CPU::RAM[IO_STAT] |= 0x02;

                    if (LCDenabled && (CPU::RAM[IO_STAT] & (1 << 5)))
                        CPU::RAM[IO_IF] |= INTERRUPT_LCDC;

                }
                else CPU::RAM[IO_LY]++;

                checkCoincidence();
            }
            break;
        case 0x02:
            if (rendercycles >= 80)
            {
                rendercycles -= 80;
                // Mode -> 3
                CPU::RAM[IO_STAT] &= ~0x03;
                CPU::RAM[IO_STAT] |= 0x03;
            }
        case 0x03:
           // CPU::accessVRAM = false;

            if (rendercycles >= 172)
            {
                rendercycles -= 172;
                // Mode -> 0
                CPU::RAM[IO_STAT] &= ~0x03;

                if (LCDenabled && (CPU::RAM[IO_STAT] & (1 << 3)))
                    CPU::RAM[IO_IF] |= INTERRUPT_LCDC;
            }
        }


        if (!LCDenabled)
        {
            CPU::accessOAM = true;
            CPU::accessVRAM = true;
        }
    }

    void refresh()
    {
        SDL_UpdateTexture(videoTexture, nullptr, pixels, 160 * sizeof(uint32_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, videoTexture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    uint32_t getWindowID()
    {
        return SDL_GetWindowID(window);
    }

    void raise()
    {
        SDL_RaiseWindow(window);
        CPU::debugger.loseFocus();
    }

}
