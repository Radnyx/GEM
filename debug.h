#ifndef DEBUG_H
#define DEBUG_H
#include <stdint.h>
#include <vector>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class Component;
class Button;

class Debugger
{
private:

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* videoTexture = nullptr;

    std::vector<Component*> components;

    Button* button_step_count;
    Button* button_run_until;

    void drawRegs(int32_t, int32_t);
    void drawMemory(int32_t, int32_t);
    void drawDisassembly(int32_t, int32_t);
public:
    bool closed;

    uint32_t* pixels;

    int32_t memorystart = 0x0000;
    int32_t stepcount = 1;

    uint32_t rununtil = 0x10000;

    Debugger();

    void init();

    void update();

    void draw();

    void fillRect(uint32_t col, int32_t x, int32_t y, uint32_t w, uint32_t h);
    void drawRect(uint32_t col, int32_t x, int32_t y, uint32_t w, uint32_t h);
    void drawChar(char c, uint32_t col, int32_t x, int32_t y);
    void write(std::string text, uint32_t col, int32_t x, int32_t y);

    void loseFocus();
    uint32_t getWindowID();

    void close();
};

#endif // DEBUG_H
