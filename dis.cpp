#include "dis.h"
#include <iostream>
#include <map>
#include "cpu.h"
#include <sstream>
#include <iomanip>


/* This makes use of the string 'replace' function
    that searches for the instance of 'from' and, given the length of it, copies
    in string 'to'. */
void replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos) return;
    str.replace(start_pos, from.length(), to);
}

std::string toHexString(uint32_t value, int w)
{
    std::stringstream ss;
    ss << std::setw(w) << std::setfill('0') << std::hex << std::uppercase << value;
    return ss.str();
}

namespace Disassembler
{
    std::map<uint8_t, std::string> opcodes;

    void init()
    {
        /** In retrospect, I should have just created a file instead of creating
            this mess, or at least a big string constant....
            Also wow I didn't even disassemble the CB instructions!
         */
        // %b is replaced with a byte
        // %d is replaced with a word
        opcodes[0x00] = "NOP";
        opcodes[0x01] = "LD BC, %w";
        opcodes[0x02] = "LD (BC), A";
        opcodes[0x03] = "INC BC";
        opcodes[0x04] = "INC B";
        opcodes[0x05] = "DEC B";
        opcodes[0x06] = "LD B, %b";
        opcodes[0x07] = "RLCA";
        opcodes[0x08] = "LD (%w), SP";
        opcodes[0x09] = "ADD HL, BC";
        opcodes[0x0A] = "LD A, (BC)";
        opcodes[0x0B] = "DEC BC";
        opcodes[0x0C] = "INC C";
        opcodes[0x0D] = "DEC C";
        opcodes[0x0E] = "LD C, %b";
        opcodes[0x0F] = "RRCA";
        opcodes[0x10] = "STOP";
        opcodes[0x11] = "LD DE, %w";
        opcodes[0x12] = "LD (DE), A";
        opcodes[0x13] = "INC DE";
        opcodes[0x14] = "INC D";
        opcodes[0x15] = "DEC D";
        opcodes[0x16] = "LD D, %b";
        opcodes[0x17] = "RLA";
        opcodes[0x18] = "JR %b";
        opcodes[0x19] = "ADD HL, DE";
        opcodes[0x1A] = "LD A, (DE)";
        opcodes[0x1B] = "DEC DE";
        opcodes[0x1C] = "INC E";
        opcodes[0x1D] = "DEC E";
        opcodes[0x1E] = "LD E, %b";
        opcodes[0x1F] = "RRA";
        opcodes[0x20] = "JR NZ, %b";
        opcodes[0x21] = "LD HL, %w";
        opcodes[0x22] = "LD (HL+), A";
        opcodes[0x23] = "INC HL";
        opcodes[0x24] = "INC H";
        opcodes[0x25] = "DEC H";
        opcodes[0x26] = "LD H, %b";
        opcodes[0x27] = "DAA";
        opcodes[0x28] = "JR Z, %b";
        opcodes[0x29] = "ADD HL, HL";
        opcodes[0x2A] = "LD A, (HL+)";
        opcodes[0x2B] = "DEC HL";
        opcodes[0x2C] = "INC L";
        opcodes[0x2D] = "DEC L";
        opcodes[0x2E] = "LD L, %b";
        opcodes[0x2F] = "CPL";
        opcodes[0x30] = "JR NC, %b";
        opcodes[0x31] = "LD SP, %w";
        opcodes[0x32] = "LD (HL-), A";
        opcodes[0x33] = "INC SP";
        opcodes[0x34] = "INC (HL)";
        opcodes[0x35] = "DEC (HL)";
        opcodes[0x36] = "LD (HL), %b";
        opcodes[0x37] = "SCF";
        opcodes[0x38] = "JR C, %b";
        opcodes[0x39] = "ADD HL, SP";
        opcodes[0x3A] = "LD A, (HL-)";
        opcodes[0x3B] = "DEC SP";
        opcodes[0x3C] = "INC A";
        opcodes[0x3D] = "DEC A";
        opcodes[0x3E] = "LD A, %b";
        opcodes[0x3F] = "CCF";
        opcodes[0x40] = "LD B, B";
        opcodes[0x41] = "LD B, C";
        opcodes[0x42] = "LD B, D";
        opcodes[0x43] = "LD B, E";
        opcodes[0x44] = "LD B, H";
        opcodes[0x45] = "LD B, L";
        opcodes[0x46] = "LD B, (HL)";
        opcodes[0x47] = "LD B, A";
        opcodes[0x48] = "LD C, B";
        opcodes[0x49] = "LD C, C";
        opcodes[0x4A] = "LD C, D";
        opcodes[0x4B] = "LD C, E";
        opcodes[0x4C] = "LD C, H";
        opcodes[0x4D] = "LD C, L";
        opcodes[0x4E] = "LD C, (HL)";
        opcodes[0x4F] = "LD C, A";
        opcodes[0x50] = "LD D, B";
        opcodes[0x51] = "LD D, C";
        opcodes[0x52] = "LD D, D";
        opcodes[0x53] = "LD D, E";
        opcodes[0x54] = "LD D, H";
        opcodes[0x55] = "LD D, L";
        opcodes[0x56] = "LD D, (HL)";
        opcodes[0x57] = "LD D, A";
        opcodes[0x58] = "LD E, B";
        opcodes[0x59] = "LD E, C";
        opcodes[0x5A] = "LD E, D";
        opcodes[0x5B] = "LD E, E";
        opcodes[0x5C] = "LD E, H";
        opcodes[0x5D] = "LD E, L";
        opcodes[0x5E] = "LD E, (HL)";
        opcodes[0x5F] = "LD E, A";
        opcodes[0x60] = "LD H, B";
        opcodes[0x61] = "LD H, C";
        opcodes[0x62] = "LD H, D";
        opcodes[0x63] = "LD H, E";
        opcodes[0x64] = "LD H, H";
        opcodes[0x65] = "LD H, L";
        opcodes[0x66] = "LD H, (HL)";
        opcodes[0x67] = "LD H, A";
        opcodes[0x68] = "LD L, B";
        opcodes[0x69] = "LD L, C";
        opcodes[0x6A] = "LD L, D";
        opcodes[0x6B] = "LD L, E";
        opcodes[0x6C] = "LD L, H";
        opcodes[0x6D] = "LD L, L";
        opcodes[0x6E] = "LD L, (HL)";
        opcodes[0x6F] = "LD L, A";
        opcodes[0x70] = "LD (HL), B";
        opcodes[0x71] = "LD (HL), C";
        opcodes[0x72] = "LD (HL), D";
        opcodes[0x73] = "LD (HL), E";
        opcodes[0x74] = "LD (HL), H";
        opcodes[0x75] = "LD (HL), L";
        opcodes[0x76] = "HALT";
        opcodes[0x77] = "LD (HL), A";
        opcodes[0x78] = "LD A, B";
        opcodes[0x79] = "LD A, C";
        opcodes[0x7A] = "LD A, D";
        opcodes[0x7B] = "LD A, E";
        opcodes[0x7C] = "LD A, H";
        opcodes[0x7D] = "LD A, L";
        opcodes[0x7E] = "LD A, (HL)";
        opcodes[0x7F] = "LD A, A";
        opcodes[0x80] = "ADD A, B";
        opcodes[0x81] = "ADD A, C";
        opcodes[0x82] = "ADD A, D";
        opcodes[0x83] = "ADD A, E";
        opcodes[0x84] = "ADD A, H";
        opcodes[0x85] = "ADD A, L";
        opcodes[0x86] = "ADD A, (HL)";
        opcodes[0x87] = "ADD A, A";
        opcodes[0x88] = "ADC A, B";
        opcodes[0x89] = "ADC A, C";
        opcodes[0x8A] = "ADC A, D";
        opcodes[0x8B] = "ADC A, E";
        opcodes[0x8C] = "ADC A, H";
        opcodes[0x8D] = "ADC A, L";
        opcodes[0x8E] = "ADC A, (HL)";
        opcodes[0x8F] = "ADC A, A";
        opcodes[0x90] = "SUB B";
        opcodes[0x91] = "SUB C";
        opcodes[0x92] = "SUB D";
        opcodes[0x93] = "SUB E";
        opcodes[0x94] = "SUB H";
        opcodes[0x95] = "SUB L";
        opcodes[0x96] = "SUB (HL)";
        opcodes[0x97] = "SUB A";
        opcodes[0x98] = "SBC A, B";
        opcodes[0x99] = "SBC A, C";
        opcodes[0x9A] = "SBC A, D";
        opcodes[0x9B] = "SBC A, E";
        opcodes[0x9C] = "SBC A, H";
        opcodes[0x9D] = "SBC A, L";
        opcodes[0x9E] = "SBC A, (HL)";
        opcodes[0x9F] = "SBC A, A";
        opcodes[0xA0] = "AND B";
        opcodes[0xA1] = "AND C";
        opcodes[0xA2] = "AND D";
        opcodes[0xA3] = "AND E";
        opcodes[0xA4] = "AND H";
        opcodes[0xA5] = "AND L";
        opcodes[0xA6] = "AND (HL)";
        opcodes[0xA7] = "AND A";
        opcodes[0xA8] = "XOR B";
        opcodes[0xA9] = "XOR C";
        opcodes[0xAA] = "XOR D";
        opcodes[0xAB] = "XOR E";
        opcodes[0xAC] = "XOR H";
        opcodes[0xAD] = "XOR L";
        opcodes[0xAE] = "XOR (HL)";
        opcodes[0xAF] = "XOR A";
        opcodes[0xB0] = "OR B";
        opcodes[0xB1] = "OR C";
        opcodes[0xB2] = "OR D";
        opcodes[0xB3] = "OR E";
        opcodes[0xB4] = "OR H";
        opcodes[0xB5] = "OR L";
        opcodes[0xB6] = "OR (HL)";
        opcodes[0xB7] = "OR A";
        opcodes[0xB8] = "CP B";
        opcodes[0xB9] = "CP C";
        opcodes[0xBA] = "CP D";
        opcodes[0xBB] = "CP E";
        opcodes[0xBC] = "CP H";
        opcodes[0xBD] = "CP L";
        opcodes[0xBE] = "CP (HL)";
        opcodes[0xBF] = "CP A";
        opcodes[0xC0] = "RET NZ";
        opcodes[0xC1] = "POP BC";
        opcodes[0xC2] = "JP NZ, %w";
        opcodes[0xC3] = "JP %w";
        opcodes[0xC4] = "CALL NZ, %w";
        opcodes[0xC5] = "PUSH BC";
        opcodes[0xC6] = "ADD A, %b";
        opcodes[0xC7] = "RST 00";
        opcodes[0xC8] = "RET Z";
        opcodes[0xC9] = "RET";
        opcodes[0xCA] = "JP Z, %w";
        //      0xCB
        opcodes[0xCC] = "CALL Z, %w";
        opcodes[0xCD] = "CALL %w";
        opcodes[0xCE] = "ADC A, %b";
        opcodes[0xCF] = "RST 08";
        opcodes[0xD0] = "RET NC";
        opcodes[0xD1] = "POP DE";
        opcodes[0xD2] = "JP NC, %w";
        opcodes[0xD3] = "---";
        opcodes[0xD4] = "CALL NC, %w";
        opcodes[0xD5] = "PUSH DE";
        opcodes[0xD6] = "SUB A, %b";
        opcodes[0xD7] = "RST 10";
        opcodes[0xD8] = "RET C";
        opcodes[0xD9] = "RETI";
        opcodes[0xDA] = "JP C, %w";
        opcodes[0xDB] = "---";
        opcodes[0xDC] = "CALL C, %w";
        opcodes[0xDD] = "---";
        opcodes[0xDE] = "SBC A, %b";
        opcodes[0xDF] = "RST 18";
        opcodes[0xE0] = "LD (FF00 + %b), A";
        opcodes[0xE1] = "POP HL";
        opcodes[0xE2] = "LD (C), A";
        opcodes[0xE3] = "---";
        opcodes[0xE4] = "---";
        opcodes[0xE5] = "PUSH HL";
        opcodes[0xE6] = "AND %b";
        opcodes[0xE7] = "RST 20";
        opcodes[0xE8] = "ADD SP, %b";
        opcodes[0xE9] = "JP HL";
        opcodes[0xEA] = "LD (%w), A";
        opcodes[0xEB] = "---";
        opcodes[0xEC] = "---";
        opcodes[0xED] = "---";
        opcodes[0xEE] = "XOR %b";
        opcodes[0xEF] = "RST 28";
        opcodes[0xF0] = "LD A, (FF00 + %b)";
        opcodes[0xF1] = "POP AF";
        opcodes[0xF2] = "LD A, (C)";
        opcodes[0xF3] = "DI";
        opcodes[0xF4] = "---";
        opcodes[0xF5] = "PUSH AF";
        opcodes[0xF6] = "OR %b";
        opcodes[0xF7] = "RST 30";
        opcodes[0xF8] = "LD HL, SP + %b";
        opcodes[0xF9] = "LD SP, HL";
        opcodes[0xFA] = "LD A, (%w)";
        opcodes[0xFB] = "EI";
        opcodes[0xFC] = "---";
        opcodes[0xFD] = "---";
        opcodes[0xFE] = "CP %b";
        opcodes[0xFF] = "RST 38";

    }

    std::vector<Instruction> disassembleRAM(uint32_t pos, uint32_t end)
    {
        std::vector<Instruction> instructions;

        while(pos < end)
        {
            uint32_t start = pos; // Start of instruction
            std::vector<uint16_t> bytes; // Storage of existing bytes

            std::string op = opcodes[CPU::read(pos)];
            bytes.push_back(CPU::read(pos++));

            int32_t getb = op.find("%b");
            int32_t getw = op.find("%w");
            uint8_t byte;
            uint16_t word;

            #define dis_get_byte() { byte = CPU::read(pos++); \
                bytes.push_back(byte); }

            #define dis_get_word() {word = CPU::read(pos) | (CPU::read(pos + 1) << 8);\
                pos += 2; bytes.push_back(word & 0xFF); bytes.push_back(word >> 8);}

            if (getb == -1 && getw != -1) dis_get_word()
            else if (getw == -1 && getb != -1) dis_get_byte()
            else if (getb < getw)
            {
                dis_get_byte();
                dis_get_word();
            }
            else if (getb > getw)
            {
                dis_get_word();
                dis_get_byte();
            }


            #undef dis_get_byte
            #undef dis_get_word

            Instruction instr;
            instr.pos = start;
            instr.bytes = bytes;

            replace(op, "%b", toHexString(byte, 2));
            replace(op, "%w", toHexString(word, 4));
            instr.instr = op;

            instructions.push_back(instr);
        }

        return instructions;
    }
}
