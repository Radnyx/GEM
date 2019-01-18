#include "debug.h"
#include <SDL2/SDL.h>
#include "cpu.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip>
#include "lodepng.h"
#include "button.h"
#include "checkbox.h"
#include "textbox.h"
#include "dis.h"
#include "apu.h"
#include "gpu.h"
#include "joypad.h"
#include <fstream>

const uint32_t width = 640;
const uint32_t height = 400;

std::vector<uint32_t> charmap;

std::stringstream ss;
void enableHex(int w)
{
    ss << std::hex << std::uppercase << std::setw(w) << std::setfill('0');
}

// Get memory name for the given position
const char* getMemoryName(uint16_t pos)
{
    if (pos < 0x100)       return "RST ";
    else if (pos < 0x150)  return "HEAD";
    else if (pos < 0x4000) return "ROM0";
    else if (pos < 0x8000) return "RMBK";
    else if (pos < 0x9C00) return "BGM1";
    else if (pos < 0xA000) return "BGM2";
    else if (pos < 0xC000) return "CRAM";
    else if (pos < 0xE000) return "IRAM";
    else if (pos < 0xFE00) return "ECHO";
    else if (pos < 0xFFA0) return "OAM ";
    else if (pos < 0xFF80) return "I/O ";
    else if (pos < 0xFFFF) return "ZPAG";
    else return "";
}

Debugger::Debugger()
{

    /* Load charmap */
    if (charmap.size() == 0)
    {
        uint32_t len = 288 * 112;
        uint32_t w, h;
        std::vector<unsigned char> image; //the raw pixels
        lodepng::decode(image, w, h, "charmap.png");
        charmap.resize(len);
        uint8_t* rawpix = (uint8_t*)((void*)&charmap[0]); // Raw data in bytes to be copied to the pixel array
        for(uint32_t i = 0; i < image.size(); i++) {
            // Fix the BGR to RGB issue
            if(i % 4 == 0) rawpix[i] = image[i+2];
            else if(i % 4 == 2) rawpix[i] = image[i-2];
            else rawpix[i] = image[i];
        }
        image.clear();
    }

    // Make Window Componentss

    // Do one CPU step
    Button* button_step = new Button("step", 6, 168, 64, 18);
    button_step->onclick = [](Debugger* debugger)
                            {
                                CPU::stepmode = true;
                                CPU::takestep = 1;
                                // Override rununtil
                                debugger->rununtil |= 0x10000;
                            };
    components.push_back(button_step);

    // Step for the number of specified steps
    button_step_count = new Button("step x1", 6, 210, 108, 18);
    button_step_count->onclick = [](Debugger* debugger)
                            {
                                CPU::stepmode = true;
                                CPU::takestep = debugger->stepcount;
                            };
    components.push_back(button_step_count);

    // Set the arbitrary step count
    TextBox* step_count_enter = new TextBox(6, 232, 108, 18);
    step_count_enter->setText("1");
    step_count_enter->setLimit(5);
    step_count_enter->onenter = [](TextBox* self, Debugger* debugger)
                            {
                                std::stringstream ss;
                                ss << std::hex << self->text;
                                ss >> debugger->stepcount;
                                ss.str(std::string());
                                ss.clear();

                                ss << "step x" << debugger->stepcount;
                                debugger->button_step_count->setText(ss.str());
                            };
    components.push_back(step_count_enter);

    // Run until the specified location
    button_run_until = new Button("run to ?", 6, 276, 108, 18);
    button_run_until->onclick = [](Debugger* debugger)
                            {
                                CPU::stepmode = true;
                                debugger->rununtil &= 0xFFFF;
                            };
    components.push_back(button_run_until);

    // Set the arbitrary step count
    TextBox* run_until_enter = new TextBox(6, 298, 108, 18);
    run_until_enter->setLimit(4);
    run_until_enter->onenter = [](TextBox* self, Debugger* debugger)
                            {
                                std::stringstream ss;
                                ss << std::hex << self->text;
                                ss >> debugger->rununtil;
                                ss.str(std::string());
                                ss.clear();

                                debugger->rununtil |= 0x10000;

                                ss << "run to " << (debugger->rununtil & 0xFFFF);
                                debugger->button_run_until->setText(ss.str());
                            };
    components.push_back(run_until_enter);

    // Toggle CPU instruction stepping
    CheckBox* toggle_step = new CheckBox(74, 168);
    toggle_step->check = &CPU::stepmode;
    toggle_step->oncheck = [](Debugger* debugger)
                            {
                                // Override run until
                                debugger->rununtil |= 0x10000;
                            };
    components.push_back(toggle_step);

    // Toggle CPU instruction stepping
    Button* button_reset = new Button("\x0F", 94, 168, 20, 18);
    button_reset->onclick = [](Debugger* debugger)
                            {
                                CPU::reset();
                                CPU::stepmode = true;
                                // Override run until
                                debugger->rununtil |= 0x10000;
                            };
    components.push_back(button_reset);

    // Scroll the memory window up 16 bytes
    Button* scroll_mem_up = new Button("\x1E", 618, 172, 12, 20);
    scroll_mem_up->onclick = [](Debugger* debugger)
                                {
                                    debugger->memorystart -= 16;
                                    if (debugger->memorystart < 0)
                                        debugger->memorystart = 0;
                                };
    components.push_back(scroll_mem_up);

    // Scroll the memory window down 16 bytes
    Button* scroll_mem_down = new Button("\x1F", 618, 372, 12, 20);
    scroll_mem_down->onclick = [](Debugger* debugger)
                                {
                                    debugger->memorystart += 16;
                                };
    components.push_back(scroll_mem_down);

    // Enter the position of the memory start for the memory viewer
    TextBox* memory_start_enter = new TextBox(62, 376, 50, 20);
    memory_start_enter->setLimit(4);
    memory_start_enter->setText("0000");
    memory_start_enter->onenter = [](TextBox* self, Debugger* debugger)
                                {
                                    std::stringstream ss;
                                    ss << std::hex << self->text;
                                    ss >> debugger->memorystart;
                                };
    components.push_back(memory_start_enter);

    /* Toggle sound channels */
    CheckBox* toggle_pulse1 = new CheckBox(612, 8);
    toggle_pulse1->check = &APU::channelEnabled[0];
    components.push_back(toggle_pulse1);

    CheckBox* toggle_pulse2 = new CheckBox(612, 32);
    toggle_pulse2->check = &APU::channelEnabled[1];
    components.push_back(toggle_pulse2);

    CheckBox* toggle_wave = new CheckBox(612, 56);
    toggle_wave->check =   &APU::channelEnabled[2];
    components.push_back(toggle_wave);

    CheckBox* toggle_noise = new CheckBox(612, 80);
    toggle_noise->check =  &APU::channelEnabled[3];
    components.push_back(toggle_noise);

    /* Enter rom to load */
    TextBox* load_enter = new TextBox(472, 144, 158, 18);
    load_enter->setLimit(255);
    load_enter->setHex(false);
    load_enter->onenter = [](TextBox* self, Debugger* debugger)
                            {
                                CPU::init(self->text.c_str());
                            };
    components.push_back(load_enter);

    /* Enter TAS to load */
    TextBox* tas_enter = new TextBox(472, 120, 108, 18);
    tas_enter->setLimit(255);
    tas_enter->setHex(false);
    tas_enter->onenter = [](TextBox* self, Debugger* debugger)
                            {
                                CPU::tasplayer.loadVBM(self->text.c_str());
                            };
    components.push_back(tas_enter);

    Button* tas_stop = new Button("Stop", 582, 120, 48, 18);
    tas_stop->onclick = [](Debugger* debugger)
                            {
                                CPU::tasplayer.stop();
                            };
    components.push_back(tas_stop);

    #define OSTREAM_WRITE_U32(fout, value) \
            fout.put(value >> 24); \
            fout.put(value >> 16); \
            fout.put(value >> 8); \
            fout.put(value);

    /* Save states */
    Button* save_state = new Button("Save State", 426, 6, 108, 18);
    save_state->onclick = [](Debugger* debugger)
                            {
                                std::ofstream state;
                                std::string title = "states/" + CPU::romtitle + ".sav";
                                state.open(title, std::ios::binary | std::ios::out);
                                state.put(CPU::currentROMBank);
                                OSTREAM_WRITE_U32(state, CPU::cycles);
                                OSTREAM_WRITE_U32(state, CPU::divcycles);
                                OSTREAM_WRITE_U32(state, CPU::timercycles);
                                OSTREAM_WRITE_U32(state, CPU::frameticks);
                                for(int i = 0; i < 4; i++)
                                    state.write((char*)&APU::channel[i], sizeof(Channel));
                                state.put(CPU::PC >> 8);
                                state.put(CPU::PC);
                                state.put(CPU::SP >> 8);
                                state.put(CPU::SP);
                                state.put(CPU::A);
                                state.put(CPU::F);
                                state.put(CPU::B);
                                state.put(CPU::C);
                                state.put(CPU::D);
                                state.put(CPU::E);
                                state.put(CPU::H);
                                state.put(CPU::L);
                                state.write((char*)CPU::RAM, 0x10000);
                                state.close();
                            };
    components.push_back(save_state);

    #undef OSTREAM_WRITE_U32

    #define OSTREAM_READ_U32(fin, value) \
            value = fin.get() << 24; \
            value |= fin.get() << 16; \
            value |= fin.get() << 8; \
            value |= fin.get(); \

    Button* load_state = new Button("Load State", 426, 32, 108, 18);
    load_state->onclick = [](Debugger* debugger)
                            {
                                CPU::hardreset();
                                std::ifstream state;
                                std::string title = "states/" + CPU::romtitle + ".sav";
                                state.open(title, std::ios::binary | std::ios::in);
                                CPU::setBank(state.get());
                                OSTREAM_READ_U32(state, CPU::cycles);
                                OSTREAM_READ_U32(state, CPU::divcycles);
                                OSTREAM_READ_U32(state, CPU::timercycles);
                                OSTREAM_READ_U32(state, CPU::frameticks);
                                for(int i = 0; i < 4; i++)
                                    state.read((char*)&APU::channel[i], sizeof(Channel));
                                CPU::PC = state.get() << 8;
                                CPU::PC |= state.get();
                                CPU::SP = state.get() << 8;
                                CPU::SP |= state.get();
                                CPU::A = state.get();
                                CPU::F = state.get();
                                CPU::B = state.get();
                                CPU::C = state.get();
                                CPU::D = state.get();
                                CPU::E = state.get();
                                CPU::H = state.get();
                                CPU::L = state.get();
                                state.read((char*)CPU::RAM, 0x10000);
                                state.close();
                                GPU::raise();
                            };
    components.push_back(load_state);
    #undef OSTREAM_READ_U32

}

// Create the window
void Debugger::init()
{
    window = SDL_CreateWindow("Debugger", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  width, height, 0);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_RenderSetLogicalSize(renderer, width, height);

    videoTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        width, height);

    pixels = new uint32_t[width * height];
    closed = false;
}

// Update logic information
void Debugger::update()
{
    if (!Joypad::control) {
        for (uint32_t i = 0; i < components.size(); i++)
            components[i]->update(this);
    }

    if (memorystart > 0xFF20) memorystart = 0xFF20;

    if ((rununtil & 0x10000) == 0)
    {
        CPU::stepmode = false;
    }
}

// Update graphical information
void Debugger::draw()
{

    fillRect(0x363636, 618, 192, 12, 180);
    drawRegs(6, 14);
    drawMemory(120, 168);
    drawDisassembly(120, 6);

    write("Step Count:", 0xD7D7D7, 8, 192);
    write("Run Until:", 0xD7D7D7, 8, 258);
    write("Addr:", 0xD7D7D7, 8, 380);

    write("Sqr 1", 0xD7D7D7, 562, 10);
    write("Sqr 2", 0xD7D7D7, 562, 34);
    write("Wave", 0xD7D7D7,  562, 58);
    write("Noise", 0xD7D7D7, 562, 82);

    write("TAS:", 0xD7D7D7, 428, 122);
    write("Load:", 0xD7D7D7, 428, 146);


    for (uint32_t i = 0; i < components.size(); i++)
        components[i]->draw(this);

    SDL_UpdateTexture(videoTexture, nullptr, pixels, width * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, videoTexture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

// Draw disassembly information at the current PC
void Debugger::drawDisassembly(int32_t x, int32_t y)
{
    drawRect(0x3D5C5C, x, y, 300, 156);
    fillRect(0x0F0F3B, x + 1, y + 1, 298, 154);
    fillRect(0xEFEFEF, x + 6, y + 6, 288, 144);

    // Disassemble 12 bytes around to get an accurate scope of disassembled code
    int back = CPU::PC - 12;
    if (back < 0) back = 0;
    std::vector<Instruction> disassembly = Disassembler::disassembleRAM(back, CPU::PC + 12);
    // Find the instruction that's located at PC
    uint32_t pcInstr;
    for (uint32_t i = 0; i < disassembly.size(); i++)
    {
        if (CPU::PC == disassembly[i].pos)
        {
            pcInstr = i;
            break;
        }
    }

    int32_t start = pcInstr - 3;
    if (start < 0) start = 0;
    // Loop and draw the disassembled opcodes for the previous and next 3 instructions
    for (uint32_t op = start, i = 0; op <= (uint32_t)start + 6; op++, i++)
    {
        if (op >= disassembly.size()) continue;

        Instruction* instr = &disassembly[op];
        bool atPC = CPU::PC == instr->pos;
        int32_t yp = y + 10 + i * 20;

        // Highlight current instruction
        if (atPC)
            fillRect(0xB7B7C4, x + 6, yp - 4, 288, 20);

        // Draw address
        enableHex(4);
        ss << instr->pos << " ";
        write(ss.str(), atPC ? 0x000000 : 0x808080, x + 10, yp+1);
        write(ss.str(), atPC ? 0x661D1D : 0x000000, x + 10, yp);
        ss.str("");

        // Draw opcode bytes
        for (uint32_t b = 0; b < instr->bytes.size(); b++) {
            enableHex(2);
            ss << instr->bytes[b];
            write(ss.str(), 0x0, x + 54 + b * 22, yp);
            ss.str("");
        }

        // Draw instruction
        write(instr->instr, 0x0, x + 14 + 14 * 8, yp);
    }
}

void Debugger::drawMemory(int32_t x, int32_t y)
{
    drawRect(0x3D5C5C, x, y, 490, 228);
    fillRect(0x0F0F3B, x + 1, y + 1, 488, 226);

    uint32_t bytewidth = 16;

    for (int32_t i = memorystart; i < memorystart + 224; i++)
    {
        enableHex(4);

        // Draw memory location and vertical bar
        if (i % (bytewidth) == memorystart % bytewidth)
        {
            // Draw memory name
            int yp = y + 4 + ((i / bytewidth) * 16) - memorystart + memorystart % bytewidth;
            ss << getMemoryName(i);
            write(ss.str(), 0xF0F0F0, x + 4, yp);
            ss.str("");
            drawChar('|', 0x3D5C5C, x + 42, yp);

            enableHex(4);

            // Draw location
            ss << i;
            write(ss.str(), 0xF0F0F0, x + 54, yp);
            ss.str("");
            drawChar('|', 0x3D5C5C, x + 40 + 54, yp);
        }

        // Draw bytes
        enableHex(2);
        if (i < 0x8000 || (i >= 0xA000 && i < 0xFE00))
            ss << (int)CPU::read(i);
        else
            ss << (int)CPU::RAM[i];

        uint32_t bytecol = 0xF0F0F0;
        if (CPU::PC == i) bytecol = 0xA1A100;
        else if (CPU::SP == i) bytecol = 0xCC297A;

        write(ss.str(), bytecol, x + 106 + ((i-memorystart) % bytewidth) * 24,
              y + 4 + 16 * ((i-memorystart) / bytewidth));
        ss.str("");
    }

}

// Write register information
void Debugger::drawRegs(int32_t x, int32_t y)
{
    int off = 0;

    drawRect(0x3D5C5C, x, y - 8, 108, 156);
    fillRect(0x0F0F3B, x + 1, y - 7, 106, 154);

    // Draw PC
    enableHex(4);
    ss << CPU::PC;
    write("PC :", 0xA1A100, x + 9, y + 14 * off);
    write(ss.str(), CPU::PC >= 0x8000 ? 0x8F0000 : 0xF0F0F0, x + 6 * 9, y + 14 * off++);
    ss.str("");
    enableHex(4);

    // Draw SP
    ss << CPU::SP;
    write("SP :", 0xCC297A, x + 9, y + 14 * off);
    write(ss.str(), 0xF0F0F0, x + 6 * 9, y + 14 * off++);
    ss.str("");
    enableHex(4);


    #define DRAW_R(reg) \
        ss << CPU:: reg (); \
        write(#reg " :", 0x006699, x + 9, y + 14 * off); \
        write(ss.str(), 0xF0F0F0, x + 6 * 9, y + 14 * off++); \
        ss.str(""); \
        ss << std::hex << std::setw(4) << std::setfill('0'); // \('O')/
    DRAW_R(AF);
    DRAW_R(BC);
    DRAW_R(DE);
    DRAW_R(HL);
    #undef DRAW_R

    write("zero", (CPU::getZero() ? 0xE6E600 : 0x363636), x + 9, y + 14 * off++);
    write("subtract", (CPU::getSubtract() ? 0xE6E600 : 0x363636), x + 9, y + 14 * off++);
    write("half carry", (CPU::getHalfCarry() ? 0xE6E600 : 0x363636), x + 9, y + 14 * off++);
    write("carry", (CPU::getCarry() ? 0xE6E600 : 0x363636), x + 9, y + 14 * off++);
}

// Fill a rectangle of color
void Debugger::fillRect(uint32_t col, int32_t xo, int32_t yo, uint32_t w, uint32_t h)
{
    if (!pixels) return;

    uint32_t xow = xo + w;
    uint32_t yoh = yo + h;
    for (uint32_t y = yo; y < yoh; y++)
    {
        if (y < 0) continue;
        if (y >= height) break;
        for (uint32_t x = xo; x < xow; x++)
        {
            if (x < 0) continue;
            if (x >= width) break;
            pixels[x + y * width] = col;
        }
    }
}

// Draw a rectangle of color
void Debugger::drawRect(uint32_t col, int32_t xo, int32_t yo, uint32_t w, uint32_t h)
{
    fillRect(col, xo, yo, w, 1);
    fillRect(col, xo, yo, 1, h);
    fillRect(col, xo + w-1, yo, 1, h);
    fillRect(col, xo, yo + h-1, w, 1);
}

// Draw a single character
void Debugger::drawChar(char c, uint32_t col, int32_t xo, int32_t yo)
{
    if (!pixels) return;

    uint32_t xow = xo + 9;
    uint32_t yoh = yo + 14;
    for (uint32_t y = yo; y < yoh; y++)
    {
        if (y < 0) continue;
        if (y >= height) break;

        uint32_t ty = (y - yo);
        for (uint32_t x = xo; x < xow; x++)
        {
            if (x < 0) continue;
            if (x >= width) break;

            uint32_t tx = (x - xo);
            if (charmap[(tx + ((uint8_t)c % 32 * 9)) + (ty + ((uint8_t)c / 32 * 14)) * 288] == 0xFF000000)
                pixels[x + y * width] = col;
            else
              ;//  pixels[x + y * width] = 0;
        }
    }
}

// Write text onto the screen
void Debugger::write(std::string text, uint32_t col, int32_t x, int32_t y)
{
    for (int i = 0; text[i] != 0; i++)
    {
        drawChar(text[i], col, x + i * 9, y);
    }
}

void Debugger::loseFocus()
{
    for (uint32_t i = 0; i < components.size(); i++)
    {
        components[i]->pressed = false;
    }
}

uint32_t Debugger::getWindowID()
{
    return SDL_GetWindowID(window);
}

void Debugger::close()
{
    closed = true;
    SDL_DestroyWindow(window);
    SDL_DestroyTexture(videoTexture);
    SDL_DestroyRenderer(renderer);
    delete[] pixels;
    pixels = nullptr;
    window = nullptr;
    videoTexture = nullptr;
    renderer = nullptr;
}
