#ifndef DIS_H
#define DIS_H
#include <iostream>
#include <vector>

struct Instruction
{
    uint32_t pos;
    std::string instr;
    // List of bytes included in the instruction
    std::vector<uint16_t> bytes;
};

namespace Disassembler
{
    void init();
    std::vector<Instruction> disassembleRAM(uint32_t start, uint32_t end);
}

#endif
