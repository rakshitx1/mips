#include <bitset>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "cache.h"

class MIPSprocessor  // Class for the processor
{
   public:
    std::unordered_map<std::string, uint32_t> symbolTable;  // To store variables from .data section
    std::vector<std::string> instructions;                  // Store instructions from .text section
    uint32_t dataMemoryStart;                               // Start address for .data section
    uint32_t currentDataAddress;                            // Current address for data section
    uint32_t PC;                                            // Start address for .text section
    uint32_t instructionSize;                               // Address of the last instruction
    uint8_t memoryAdd[0x1000];                              // Memory address
    LFUCache instructionCache;                              // Instruction cache
    LFUCache dataCache;                                     // Data cache
    uint32_t registers[32], floatRegisters[32], HI, LO;     // registers + HI and LO registers
    std::unordered_map<std::string, uint32_t> funcMap;      // Map for function name and address
    bool running;                                           // Variable to control the state of the processor

    // Control signals
    std::bitset<1> RegDst, Branch, ReadMem, MemtoReg, WriteMem, ALUSrc, WriteReg, ALUOp0, ALUOp1, Jump;

    // Function to read file and process .data and .text sections
    void readFile(const std::string& filename) {
        PC = 0x0100;  // Reset the program counter

        std::ifstream file(filename);
        std::string line;
        bool inDataSection = false;
        bool inTextSection = false;

        while (std::getline(file, line)) {
            // Trim leading and trailing whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') continue;

            std::istringstream iss(line);
            std::string token;
            iss >> token;

            if (token == ".data") {
                inDataSection = true;
                inTextSection = false;
                continue;
            } else if (token == ".text") {
                inTextSection = true;
                inDataSection = false;
                continue;
            }

            if (inTextSection) {
                if (!token.empty()) {
                    // If it's a label (ending with ':')
                    if (token.back() == ':') {
                        std::string label = token.substr(0, token.size() - 1);

                        // Store the address of the label
                        funcMap[label] = PC + 4;
                    } else {
                        instructions.push_back(line);  // Add instruction to the list
                        if (token == "lw" || token == "sw" || token == "la") {
                            PC += 8;  // Increment the program counter by 8 for lw, sw, and la instructions
                        } else {
                            PC += 4;  // Increment the program counter by 4 for other instructions
                        }
                    }
                }
            } else if (inDataSection) {
                processDataSection(line);  // Process the data section
            }
        }

        file.close();
    }

    // Function to process .data section
    void processDataSection(const std::string& line) {
        std::istringstream iss(line);
        std::string varName, directive, value;

        iss >> varName;      // variable name (e.g., num1:)
        varName.pop_back();  // remove the colon (:)
        iss >> directive;    // directive (e.g., .word)
        iss >> value;        // value (e.g., 5)

        if (directive == ".word") {
            // num1: .word 5 or numArray: .word 1, 2, 3, 4
            std::vector<int> intValues;
            std::string temp;
            std::istringstream valueStream(value);

            while (std::getline(valueStream, temp, ',')) {
                intValues.push_back(std::stoi(temp));
            }

            symbolTable[varName] = currentDataAddress;  // Store starting address

            for (int intValue : intValues) {
                std::memcpy(&memoryAdd[currentDataAddress], &intValue, sizeof(int));  // Copy integer as bytes
                currentDataAddress += 4;                                              // Increment by 4 bytes (word-aligned)
            }
        } else if (directive == ".asciiz") {
            std::string strValue = value;
            symbolTable[varName] = currentDataAddress;
            for (char c : strValue) {
                memoryAdd[currentDataAddress++] = static_cast<uint8_t>(c);  // Store each character
            }
            memoryAdd[currentDataAddress++] = 0;  // Null-terminate the string
        } else if (directive == ".float") {
            float floatValue = std::stof(value);
            std::memcpy(&memoryAdd[currentDataAddress], &floatValue, sizeof(float));  // Copy float as bytes
            symbolTable[varName] = currentDataAddress;                                // Store starting address
            currentDataAddress += 4;                                                  // Increment by 4 bytes (word-aligned)
        }
    }

    // Function to convert each instruction into 32-bit machine code
    void assembleInstructions() {
        PC = 0x0100;  // Reset the program counter

        for (const auto& instr : instructions) {
            std::cout << "Assembly: " << instr << std::endl;
            convertToMachineCode(instr);
        }

        instructionSize = PC - 4;  // Store the address of the last instruction
    }

    // Function to convert each instruction line to 32-bit machine code
    void convertToMachineCode(const std::string& instruction) {
        std::istringstream iss(instruction);
        std::string opcode, reg1, reg2, reg3;

        iss >> opcode;  // Get the operation (e.g., lw, add, etc.)

        if (opcode == "lw") {
            iss >> reg1 >> reg2;  // lw $t0, num1
            reg1.pop_back();      // Remove comma
            lwToMachineCode(reg1, reg2);
        } else if (opcode == "sw") {
            iss >> reg1 >> reg2;  // sw $t0, num1
            reg1.pop_back();      // Remove comma
            swToMachineCode(reg1, reg2);
        } else if (opcode == "add") {
            iss >> reg1 >> reg2 >> reg3;  // add $t2, $t0, $t1
            reg1.pop_back();              // Remove comma
            reg2.pop_back();              // Remove comma
            addToMachineCode(reg1, reg2, reg3);
        } else if (opcode == "sub") {
            iss >> reg1 >> reg2 >> reg3;  // sub $t2, $t0, $t1
            reg1.pop_back();              // Remove comma
            reg2.pop_back();              // Remove comma
            subToMachineCode(reg1, reg2, reg3);
        } else if (opcode == "mult") {
            iss >> reg1 >> reg2;  // mult $t2, $t0
            reg1.pop_back();      // Remove comma
            multToMachineCode(reg1, reg2);
        } else if (opcode == "div") {
            iss >> reg1 >> reg2;  // div $t2, $t0
            reg1.pop_back();      // Remove comma
            divToMachineCode(reg1, reg2);
        } else if (opcode == "mflo") {
            iss >> reg1;  // mflo $t0
            mfloToMachineCode(reg1);
        } else if (opcode == "mfhi") {
            iss >> reg1;  // mfhi $t0
            mfhiToMachineCode(reg1);
        } else if (opcode == "addi") {
            iss >> reg1 >> reg2 >> reg3;  // addi $t2, $t0, 5
            reg1.pop_back();              // Remove comma
            reg2.pop_back();              // Remove comma
            addiToMachineCode(reg1, reg2, reg3);
        } else if (opcode == "and") {
            iss >> reg1 >> reg2 >> reg3;  // and $t2, $t0, $t1
            reg1.pop_back();              // Remove comma
            reg2.pop_back();              // Remove comma
            andToMachineCode(reg1, reg2, reg3);
        } else if (opcode == "or") {
            iss >> reg1 >> reg2 >> reg3;  // or $t2, $t0, $t1
            reg1.pop_back();              // Remove comma
            reg2.pop_back();              // Remove comma
            orToMachineCode(reg1, reg2, reg3);
        } else if (opcode == "li") {
            iss >> reg1 >> reg2;  // li $v0, 1
            liToMachineCode(reg1, reg2);
        } else if (opcode == "move") {
            iss >> reg1 >> reg2;  // move $a0, $t2
            moveToMachineCode(reg1, reg2);
        } else if (opcode == "syscall") {
            syscallToMachineCode();
        } else if (opcode == "lwc1") {
            iss >> reg1 >> reg2;  // lwc1 $f0, num1
            reg1.pop_back();      // Remove comma
            lwc1ToMachineCode(reg1, reg2);
        } else if (opcode == "swc1") {
            iss >> reg1 >> reg2;  // swc1 $f0, num1
            reg1.pop_back();      // Remove comma
            swc1ToMachineCode(reg1, reg2);
        } else if (opcode == "add.s") {
            iss >> reg1 >> reg2 >> reg3;  // add.s $f0, $f1, $f2
            reg1.pop_back();              // Remove comma
            reg2.pop_back();              // Remove comma
            addSToMachineCode(reg1, reg2, reg3);
        } else if (opcode == "j") {
            iss >> reg1;  // j label
            jToMachineCode(reg1);
        } else if (opcode == "jr") {
            iss >> reg1;  // jr $ra
            jrToMachineCode(reg1);
        } else if (opcode == "jal") {
            iss >> reg1;  // jal label
            jalToMachineCode(reg1);
        } else if (opcode == "beq") {
            iss >> reg1 >> reg2 >> reg3;  // beq $t0, $t1, label
            reg1.pop_back();              // Remove comma
            reg2.pop_back();              // Remove comma
            beqToMachineCode(reg1, reg2, reg3);
        } else if (opcode == "la") {
            iss >> reg1 >> reg2;  // la $t0, num1
            reg1.pop_back();      // Remove comma
            laToMachineCode(reg1, reg2);
        } else if (opcode == "slt") {
            iss >> reg1 >> reg2 >> reg3;  // slt $t0, $t1, $t2
            reg1.pop_back();              // Remove comma
            reg2.pop_back();              // Remove comma
            sltToMachineCode(reg1, reg2, reg3);
        } else {
            std::cerr << "Error: Invalid instruction: " << opcode << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    // converts register name to number (e.g., $t0 -> 8)
    int regToNumber(const std::string& reg) {
        if (reg == "$zero") return 0;
        if (reg == "$at") return 1;
        if (reg == "$v0") return 2;
        if (reg == "$v1") return 3;
        if (reg == "$a0") return 4;
        if (reg == "$a1") return 5;
        if (reg == "$a2") return 6;
        if (reg == "$a3") return 7;
        if (reg == "$t0") return 8;
        if (reg == "$t1") return 9;
        if (reg == "$t2") return 10;
        if (reg == "$t3") return 11;
        if (reg == "$t4") return 12;
        if (reg == "$t5") return 13;
        if (reg == "$t6") return 14;
        if (reg == "$t7") return 15;
        if (reg == "$s0") return 16;
        if (reg == "$s1") return 17;
        if (reg == "$s2") return 18;
        if (reg == "$s3") return 19;
        if (reg == "$s4") return 20;
        if (reg == "$s5") return 21;
        if (reg == "$s6") return 22;
        if (reg == "$s7") return 23;
        if (reg == "$t8") return 24;
        if (reg == "$t9") return 25;
        if (reg == "$k0") return 26;
        if (reg == "$k1") return 27;
        if (reg == "$gp") return 28;
        if (reg == "$sp") return 29;
        if (reg == "$fp") return 30;
        if (reg == "$ra")
            return 31;
        else {
            exit(EXIT_FAILURE);
        }
    }

    // Converts number to register name (e.g., 8 -> $t0)
    std::string numberToReg(int reg) {
        switch (reg) {
            case 0:
                return "$zero";
            case 1:
                return "$at";
            case 2:
                return "$v0";
            case 3:
                return "$v1";
            case 4:
                return "$a0";
            case 5:
                return "$a1";
            case 6:
                return "$a2";
            case 7:
                return "$a3";
            case 8:
                return "$t0";
            case 9:
                return "$t1";
            case 10:
                return "$t2";
            case 11:
                return "$t3";
            case 12:
                return "$t4";
            case 13:
                return "$t5";
            case 14:
                return "$t6";
            case 15:
                return "$t7";
            case 16:
                return "$s0";
            case 17:
                return "$s1";
            case 18:
                return "$s2";
            case 19:
                return "$s3";
            case 20:
                return "$s4";
            case 21:
                return "$s5";
            case 22:
                return "$s6";
            case 23:
                return "$s7";
            case 24:
                return "$t8";
            case 25:
                return "$t9";
            case 26:
                return "$k0";
            case 27:
                return "$k1";
            case 28:
                return "$gp";
            case 29:
                return "$sp";
            case 30:
                return "$fp";
            case 31:
                return "$ra";
            default:
                return "";
        }
    }

    int floatRegisterToBinary(const std::string& reg) {
        if (reg[0] == 'f' && reg.size() > 1) {
            int regNum = std::stoi(reg.substr(1));
            if (regNum >= 0 && regNum <= 31) {
                return regNum;
            }
        }
        return -1;
    }

    // Convert instructions to 32-bit machine code
    void addToMachineCode(const std::string& reg1, const std::string& reg2, const std::string& reg3) {
        int rd = regToNumber(reg1);
        int rs = regToNumber(reg2);
        int rt = regToNumber(reg3);

        std::bitset<6> opcode(0);  // R-type opcode is 0
        std::bitset<5> rsBin(rs);
        std::bitset<5> rtBin(rt);
        std::bitset<5> rdBin(rd);
        std::bitset<5> shamt(0);         // Shift amount is 0 for add
        std::bitset<6> funct(0b100000);  // Function code for add

        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void addiToMachineCode(const std::string& reg1, const std::string& reg2, const std::string& imm) {
        int rt = regToNumber(reg1);
        int rs = regToNumber(reg2);
        int immediate = std::stoi(imm);

        std::bitset<6> opcode(0b001000);  // Opcode for addi
        std::bitset<5> rsBin(rs);
        std::bitset<5> rtBin(rt);
        std::bitset<16> immBin(immediate);

        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + immBin.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void lwToMachineCode(const std::string& reg1, const std::string& address) {
        int rt = regToNumber(reg1);  // Destination register
        int base, offset;
        std::string luiMachineCode;  // Initialize for storing lui machine code

        std::size_t openParen = address.find('(');
        std::size_t closeParen = address.find(')');

        if (openParen != std::string::npos && closeParen != std::string::npos) {
            // Extract offset and base register for format "offset(base)"
            try {
                offset = std::stoi(address.substr(0, openParen));  // Get the offset part
            } catch (const std::invalid_argument&) {
                std::cerr << "Error: Invalid offset value in address: " << address << std::endl;
                exit(EXIT_FAILURE);
            }

            std::string baseReg = address.substr(openParen + 1, closeParen - openParen - 1);
            base = regToNumber(baseReg);
        } else {
            // Otherwise, treat it as a variable name
            if (symbolTable.find(address) == symbolTable.end()) {
                std::cerr << "Error: Unknown variable '" << address << "'." << std::endl;
                exit(EXIT_FAILURE);
            }

            base = regToNumber("$at");             // Base register is $at ($1)
            int immediate = symbolTable[address];  // Full 32-bit address

            int upperImmediate = (immediate >> 16) & 0xFFFF;  // Upper 16 bits
            int lowerImmediate = immediate & 0xFFFF;          // Lower 16 bits

            // Create the lui instruction
            std::bitset<6> luiOpcode(0b001111);        // Opcode for 'lui'
            std::bitset<5> atBin(regToNumber("$at"));  // $at register
            std::bitset<16> upperImm(upperImmediate);

            luiMachineCode = luiOpcode.to_string() + std::bitset<5>(0).to_string() + atBin.to_string() + upperImm.to_string();

            // push back the machine code into memoryAdd array starting from PC
            memoryAdd[PC] = std::bitset<8>(std::stoi(luiMachineCode.substr(0, 8), nullptr, 2)).to_ulong();
            memoryAdd[PC + 1] = std::bitset<8>(std::stoi(luiMachineCode.substr(8, 8), nullptr, 2)).to_ulong();
            memoryAdd[PC + 2] = std::bitset<8>(std::stoi(luiMachineCode.substr(16, 8), nullptr, 2)).to_ulong();
            memoryAdd[PC + 3] = std::bitset<8>(std::stoi(luiMachineCode.substr(24, 8), nullptr, 2)).to_ulong();
            PC += 4;

            // Use lower 16 bits as offset for lw instruction
            offset = lowerImmediate;
        }

        // Ensure offset is a signed 16-bit number
        if (offset > 32767 || offset < -32768) {
            std::cerr << "Error: Offset value out of range for 16-bit signed integer." << std::endl;
            exit(EXIT_FAILURE);
        }

        // Create lw instruction
        std::bitset<6> opcode(0b100011);    // Opcode for lw
        std::bitset<5> baseBin(base);       // Base register binary value
        std::bitset<5> rtBin(rt);           // Destination register binary value
        std::bitset<16> offsetBin(offset);  // Offset value

        std::string lwMachineCode = opcode.to_string() + baseBin.to_string() + rtBin.to_string() + offsetBin.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(lwMachineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(lwMachineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(lwMachineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(lwMachineCode.substr(24, 8), nullptr, 2)).to_ulong();
        PC += 4;

        // Combine lui and lw machine code
        std::cout << "Machine Code: " << luiMachineCode << "\n"
                  << "              " << lwMachineCode << std::endl
                  << std::endl;
    }

    void swToMachineCode(const std::string& reg1, const std::string& address) {
        int rt = regToNumber(reg1);  // Source register (e.g., $t3)
        int base, offset;
        std::string luiMachineCode;  // Initialize for storing lui machine code

        std::size_t openParen = address.find('(');
        std::size_t closeParen = address.find(')');

        if (openParen != std::string::npos && closeParen != std::string::npos) {
            // Extract offset and base register for format "offset(base)"
            try {
                offset = std::stoi(address.substr(0, openParen));  // Get the offset part
            } catch (const std::invalid_argument&) {
                std::cerr << "Error: Invalid offset value in address: " << address << std::endl;
                exit(EXIT_FAILURE);
            }

            std::string baseReg = address.substr(openParen + 1, closeParen - openParen - 1);
            base = regToNumber(baseReg);
        } else {
            // Otherwise, treat it as a variable name
            if (symbolTable.find(address) == symbolTable.end()) {
                std::cerr << "Error: Unknown variable '" << address << "'." << std::endl;
                exit(EXIT_FAILURE);
            }

            base = regToNumber("$at");             // Base register is $at ($1)
            int immediate = symbolTable[address];  // Full 32-bit address

            int upperImmediate = (immediate >> 16) & 0xFFFF;  // Upper 16 bits
            int lowerImmediate = immediate & 0xFFFF;          // Lower 16 bits

            // Create the lui instruction
            std::bitset<6> luiOpcode(0b001111);        // Opcode for 'lui'
            std::bitset<5> atBin(regToNumber("$at"));  // $at register
            std::bitset<16> upperImm(upperImmediate);

            luiMachineCode = luiOpcode.to_string() + std::bitset<5>(0).to_string() + atBin.to_string() + upperImm.to_string();

            // push back the machine code into memoryAdd array starting from PC
            memoryAdd[PC] = std::bitset<8>(std::stoi(luiMachineCode.substr(0, 8), nullptr, 2)).to_ulong();
            memoryAdd[PC + 1] = std::bitset<8>(std::stoi(luiMachineCode.substr(8, 8), nullptr, 2)).to_ulong();
            memoryAdd[PC + 2] = std::bitset<8>(std::stoi(luiMachineCode.substr(16, 8), nullptr, 2)).to_ulong();
            memoryAdd[PC + 3] = std::bitset<8>(std::stoi(luiMachineCode.substr(24, 8), nullptr, 2)).to_ulong();
            PC += 4;

            // Use lower 16 bits as offset for sw instruction
            offset = lowerImmediate;
        }

        // Ensure offset is a signed 16-bit number
        if (offset > 32767 || offset < -32768) {
            std::cerr << "Error: Offset value out of range for 16-bit signed integer." << std::endl;
            exit(EXIT_FAILURE);
        }

        // Create sw instruction
        std::bitset<6> opcode(0b101011);    // Opcode for sw
        std::bitset<5> baseBin(base);       // Base register binary value
        std::bitset<5> rtBin(rt);           // Source register binary value
        std::bitset<16> offsetBin(offset);  // Offset value

        std::string swMachineCode = opcode.to_string() + baseBin.to_string() + rtBin.to_string() + offsetBin.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(swMachineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(swMachineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(swMachineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(swMachineCode.substr(24, 8), nullptr, 2)).to_ulong();
        PC += 4;

        // Combine lui and lw machine code
        std::cout << "Machine Code: " << luiMachineCode << "\n"
                  << "              " << swMachineCode << std::endl
                  << std::endl;
    }

    void subToMachineCode(const std::string& reg1, const std::string& reg2, const std::string& reg3) {
        int rd = regToNumber(reg1);
        int rs = regToNumber(reg2);
        int rt = regToNumber(reg3);

        std::bitset<6> opcode(0);  // R-type opcode is 0
        std::bitset<5> rsBin(rs);
        std::bitset<5> rtBin(rt);
        std::bitset<5> rdBin(rd);
        std::bitset<5> shamt(0);         // Shift amount is 0 for sub
        std::bitset<6> funct(0b100010);  // Function code for sub

        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void multToMachineCode(const std::string& reg1, const std::string& reg2) {
        int rs = regToNumber(reg1);
        int rt = regToNumber(reg2);

        std::bitset<6> opcode(0);        // R-type opcode is 0
        std::bitset<5> rsBin(rs);        // Source register 1
        std::bitset<5> rtBin(rt);        // Source register 2
        std::bitset<5> rdBin(0);         // Destination register (not used for mult)
        std::bitset<5> shamt(0);         // Shift amount is 0
        std::bitset<6> funct(0b011000);  // Function code for mult

        // Concatenate all parts to form the machine code
        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void divToMachineCode(const std::string& reg1, const std::string& reg2) {
        int rs = regToNumber(reg1);  // Source register
        int rt = regToNumber(reg2);  // Target register
        std::bitset<6> opcode(0);    // R-type opcode is 0
        std::bitset<5> rsBin(rs);
        std::bitset<5> rtBin(rt);
        std::bitset<5> rdBin(0);         // Destination register is 0 for div
        std::bitset<5> shamt(0);         // Shift amount is 0
        std::bitset<6> funct(0b011010);  // Function code for div

        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void mfloToMachineCode(const std::string& reg1) {
        int rd = regToNumber(reg1);      // Destination register
        std::bitset<6> opcode(0);        // R-type opcode is 0
        std::bitset<5> rsBin(0);         // No source register
        std::bitset<5> rtBin(0);         // No target register
        std::bitset<5> rdBin(rd);        // Destination register
        std::bitset<5> shamt(0);         // Shift amount is 0
        std::bitset<6> funct(0b010010);  // Function code for mflo

        // Concatenate to form machine code
        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void mfhiToMachineCode(const std::string& reg1) {
        int rd = regToNumber(reg1);      // Destination register
        std::bitset<6> opcode(0);        // R-type opcode is 0
        std::bitset<5> rsBin(0);         // No source register
        std::bitset<5> rtBin(0);         // No target register
        std::bitset<5> rdBin(rd);        // Destination register
        std::bitset<5> shamt(0);         // Shift amount is 0
        std::bitset<6> funct(0b010000);  // Function code for mfhi

        // Concatenate to form machine code
        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void andToMachineCode(const std::string& reg1, const std::string& reg2, const std::string& reg3) {
        int rd = regToNumber(reg1);
        int rs = regToNumber(reg2);
        int rt = regToNumber(reg3);

        std::bitset<6> opcode(0);  // R-type opcode is 0
        std::bitset<5> rsBin(rs);
        std::bitset<5> rtBin(rt);
        std::bitset<5> rdBin(rd);
        std::bitset<5> shamt(0);         // Shift amount is 0 for and
        std::bitset<6> funct(0b100100);  // Function code for and

        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void orToMachineCode(const std::string& reg1, const std::string& reg2, const std::string& reg3) {
        int rd = regToNumber(reg1);
        int rs = regToNumber(reg2);
        int rt = regToNumber(reg3);

        std::bitset<6> opcode(0);  // R-type opcode is 0
        std::bitset<5> rsBin(rs);
        std::bitset<5> rtBin(rt);
        std::bitset<5> rdBin(rd);
        std::bitset<5> shamt(0);         // Shift amount is 0 for or
        std::bitset<6> funct(0b100101);  // Function code for or

        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void liToMachineCode(const std::string& reg1, const std::string& value) {
        int immediate = std::stoi(value);
        int rt = regToNumber(reg1);

        std::bitset<6> opcode(0b001001);  // addi opcode
        std::bitset<5> rs(0);             // $zero register
        std::bitset<5> rtBin(rt);
        std::bitset<16> imm(immediate);

        std::string machineCode = opcode.to_string() + rs.to_string() + rtBin.to_string() + imm.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void moveToMachineCode(const std::string& reg1, const std::string& reg2) {
        int rd = regToNumber(reg1);
        int rs = regToNumber(reg2);

        std::bitset<6> opcode(0);  // R-type opcode is 0
        std::bitset<5> rsBin(rs);
        std::bitset<5> rtBin(0);
        std::bitset<5> rdBin(rd);
        std::bitset<5> shamt(0);         // Shift amount is 0 for add
        std::bitset<6> funct(0b100000);  // Function code for add

        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void syscallToMachineCode() {
        std::string machineCode = "00000000000000000000000000001100";  // Machine code for syscall

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void lwc1ToMachineCode(std::string reg1, std::string reg2) {
        int ft = floatRegisterToBinary(reg1);
        int base = regToNumber("$at");
        int offset = symbolTable[reg2];

        std::bitset<6> opcode(0b110001);  // Opcode for lwc1
        std::bitset<5> baseBin(base);
        std::bitset<5> ftBin(ft);
        std::bitset<16> offsetBin(offset);

        std::string machineCode = opcode.to_string() + baseBin.to_string() + ftBin.to_string() + offsetBin.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void swc1ToMachineCode(std::string reg1, std::string reg2) {
        int ft = floatRegisterToBinary(reg1);
        int base = regToNumber("$at");
        int offset = symbolTable[reg2];

        std::bitset<6> opcode(0b111001);  // Opcode for swc1
        std::bitset<5> baseBin(base);
        std::bitset<5> ftBin(ft);
        std::bitset<16> offsetBin(offset);

        std::string machineCode = opcode.to_string() + baseBin.to_string() + ftBin.to_string() + offsetBin.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void addSToMachineCode(std::string reg1, std::string reg2, std::string reg3) {
        int fd = floatRegisterToBinary(reg1);
        int fs = floatRegisterToBinary(reg2);
        int ft = floatRegisterToBinary(reg3);

        std::bitset<6> opcode(0);  // Opcode for add.s
        std::bitset<5> fsBin(fs);
        std::bitset<5> ftBin(ft);
        std::bitset<5> fdBin(fd);
        std::bitset<5> shamt(0);         // Shift amount is 0
        std::bitset<6> funct(0b000000);  // Function code for add.s

        std::string machineCode = opcode.to_string() + fsBin.to_string() + ftBin.to_string() + fdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void jToMachineCode(const std::string& label) {
        // Get the absolute address of the target
        uint32_t absoluteTarget = funcMap[label];

        // Calculate the jump target
        // We need the 4 most significant bits of (PC + 4) and the 26 least significant bits of the target address
        uint32_t pcNext = (PC + 4) & 0xF0000000;                                  // Get the 4 most significant bits of (PC + 4)
        uint32_t target = ((absoluteTarget & 0x0FFFFFFF) >> 2) | (pcNext >> 28);  // Get the 26 least significant bits and shift right by 2

        std::bitset<6> opcode(0b000010);  // Opcode for j
        std::bitset<26> targetBin(target);

        std::string machineCode = opcode.to_string() + targetBin.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void jrToMachineCode(const std::string& reg1) {
        int rs = regToNumber(reg1);

        std::bitset<6> opcode(0);  // R-type opcode is 0
        std::bitset<5> rsBin(rs);
        std::bitset<5> rtBin(0);         // No target register
        std::bitset<5> rdBin(0);         // No destination register
        std::bitset<5> shamt(0);         // Shift amount is 0
        std::bitset<6> funct(0b001000);  // Function code for jr

        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void jalToMachineCode(const std::string& label) {
        int target = funcMap[label] / 4;  // Calculate the target address

        std::bitset<6> opcode(0b000011);  // Opcode for jal
        std::bitset<26> targetBin(target);

        std::string machineCode = opcode.to_string() + targetBin.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void beqToMachineCode(const std::string& reg1, const std::string& reg2, const std::string& label) {
        int rs = regToNumber(reg1);
        int rt = regToNumber(reg2);
        int labelAddress = funcMap[label];                    // Get the address of the label
        int offset = (labelAddress - PC) / 4 - 1;             // Calculate the offset
        int16_t signedOffset = static_cast<int16_t>(offset);  // Ensure 16-bit signed representation

        std::bitset<6> opcode(0b000100);  // Opcode for beq
        std::bitset<5> rsBin(rs);
        std::bitset<5> rtBin(rt);
        std::bitset<16> offsetBin(static_cast<uint16_t>(signedOffset));  // Convert to bitset

        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + offsetBin.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void laToMachineCode(const std::string& reg1, const std::string& address) {
        int rt = regToNumber(reg1);
        int immediate = symbolTable[address];

        // Split the address into upper and lower 16 bits
        int upperImmediate = (immediate >> 16) & 0xFFFF;  // Upper 16 bits
        int lowerImmediate = immediate & 0xFFFF;          // Lower 16 bits

        // Construct lui instruction (load upper immediate)
        std::bitset<6> luiOpcode(0b001111);  // opcode for 'lui'
        std::bitset<5> rtBin(rt);            // register
        std::bitset<16> upperImm(upperImmediate);

        // Machine code for 'lui'
        std::string luiMachineCode = luiOpcode.to_string() + std::bitset<5>(0).to_string() + rtBin.to_string() + upperImm.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(luiMachineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(luiMachineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(luiMachineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(luiMachineCode.substr(24, 8), nullptr, 2)).to_ulong();

        PC += 4;

        // Construct addi/ori instruction (load lower immediate)
        std::bitset<6> addiOpcode(0b001000);  // opcode for 'addi' (could use ori instead)
        std::bitset<16> lowerImm(lowerImmediate);

        // Machine code for 'addi' (or 'ori')
        std::string addiMachineCode = addiOpcode.to_string() + rtBin.to_string() + rtBin.to_string() + lowerImm.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(addiMachineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(addiMachineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(addiMachineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(addiMachineCode.substr(24, 8), nullptr, 2)).to_ulong();

        PC += 4;

        // Combine the two machine codes
        std::cout << "Machine Code: " << luiMachineCode + "\n" + "              " + addiMachineCode << std::endl
                  << std::endl;
    }

    void sltToMachineCode(const std::string& reg1, const std::string& reg2, const std::string& reg3) {
        int rd = regToNumber(reg1);
        int rs = regToNumber(reg2);
        int rt = regToNumber(reg3);

        std::bitset<6> opcode(0);  // R-type opcode is 0
        std::bitset<5> rsBin(rs);
        std::bitset<5> rtBin(rt);
        std::bitset<5> rdBin(rd);
        std::bitset<5> shamt(0);         // Shift amount is 0 for slt
        std::bitset<6> funct(0b101010);  // Function code for slt

        std::string machineCode = opcode.to_string() + rsBin.to_string() + rtBin.to_string() + rdBin.to_string() + shamt.to_string() + funct.to_string();

        // push back the machine code into memoryAdd array starting from PC
        memoryAdd[PC] = std::bitset<8>(std::stoi(machineCode.substr(0, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 1] = std::bitset<8>(std::stoi(machineCode.substr(8, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 2] = std::bitset<8>(std::stoi(machineCode.substr(16, 8), nullptr, 2)).to_ulong();
        memoryAdd[PC + 3] = std::bitset<8>(std::stoi(machineCode.substr(24, 8), nullptr, 2)).to_ulong();

        std::cout << "Machine Code: " << machineCode << std::endl
                  << std::endl;
        PC += 4;
    }

    void executeInstructions() {
        running = true;
        PC = 0x0100;  // Start at address 0x0100

        while (running) {
            // Check if the program counter is within bounds of instructionSize
            if (PC > instructionSize) {
                std::cout << "-- program is finished running (dropped off bottom) --" << std::endl;
                running = false;
                break;
            }

            std::cout << "\n----------------------------------------" << std::endl;
            // Fetch the instruction from memory
            std::cout << "Instruction Cache:" << std::endl;
            uint32_t instruction = instructionCache.get(PC);
            if (instruction == UINT32_MAX) {  // Cache miss; fetch from RAM
                instruction = (static_cast<uint32_t>(memoryAdd[PC]) << 24) |
                              (static_cast<uint32_t>(memoryAdd[PC + 1]) << 16) |
                              (static_cast<uint32_t>(memoryAdd[PC + 2]) << 8) |
                              (static_cast<uint32_t>(memoryAdd[PC + 3]));
                instructionCache.put(PC, instruction);
            }

            std::cout << "Instruction: " << std::bitset<32>(instruction) << std::endl;
            std::cout << "Initial PC: " << PC << std::endl;

            // Extract opcode and set control signals
            uint8_t opcode = (instruction >> 26) & 0x3F;  // 6-bit opcode
            std::cout << "Opcode: " << std::bitset<8>(opcode) << std::endl;

            uint8_t funct = instruction & 0x3F;  // 6-bit function code
            setControlSignal(opcode, funct);

            // Extract register fields
            uint8_t rs = (instruction >> 21) & 0x1F;                        // Source register 1
            uint8_t rt = (instruction >> 16) & 0x1F;                        // Source register 2 or destination
            uint8_t rd = RegDst.test(0) ? (instruction >> 11) & 0x1F : rt;  // Destination register

            std::cout << "rs: " << numberToReg(rs) << ": " << std::bitset<32>(registers[rs]) << " ( " << registers[rs] << " )" << std::endl;
            std::cout << "rt: " << numberToReg(rt) << ": " << std::bitset<32>(registers[rt]) << " ( " << registers[rt] << " )" << std::endl;
            std::cout << "rd: " << numberToReg(rd) << ": " << std::bitset<32>(registers[rd]) << " ( " << registers[rd] << " )" << std::endl;

            // Read from the registers
            uint32_t readRegister1 = registers[rs];
            uint32_t readRegister2 = ALUSrc.test(0) ? (instruction & 0xFFFF) : registers[rt];

            // Perform ALU operation
            uint32_t ALUResult = ALUOperation(readRegister1, readRegister2, funct);
            std::cout << "ALUResult: " << std::bitset<32>(ALUResult) << std::endl;

            // Check if writeMem is 1 (sw case)
            if (WriteMem.test(0)) {
                int16_t offset = instruction & 0xFFFF;  // Extract the offset (sign-extended)
                if (offset & 0x8000) {                  // Handle sign-extension
                    offset |= 0xFFFF0000;
                }
                uint32_t address = readRegister1 + offset;  // Final memory address
                uint32_t value = registers[rt];             // The value from register[rt]

                // Print initial memory values
                std::cout << "Initial Memory Value:" << std::endl;
                printWord(address, memoryAdd);

                // Break the 32-bit value into 4 bytes and store them into the memory array
                std::cout << "Data Cache:" << std::endl;
                dataCache.put(address, value);        // Update cache
                memoryAdd[address] = (value) & 0xFF;  // Store the most significant byte
                memoryAdd[address + 1] = (value >> 8) & 0xFF;
                memoryAdd[address + 2] = (value >> 16) & 0xFF;
                memoryAdd[address + 3] = (value >> 24) & 0xFF;  // Store the least significant byte

                // Print Final memory values
                std::cout << "Final Memory Value:" << std::endl;
                printWord(address, memoryAdd);

                PC += 4;
                continue;
            }

            // If ReadMem is set, read from memory
            uint32_t memData = 0;
            if (ReadMem.test(0)) {
                // Extract the immediate value from the instruction
                uint32_t immediate = instruction & 0xFFFF;

                // Sign-extend the immediate to 32 bits
                uint32_t signExtendedAddress = (immediate & 0x8000) ? (immediate | 0xFFFF0000) : immediate;

                // Calculate the final address by adding to the base address (usually in rs)
                uint32_t baseAddress = registers[rs];  // Assuming rs is defined somewhere
                uint32_t address = baseAddress + signExtendedAddress;

                // Read memory using the calculated address
                memData = readMemory(address);

                std::cout << "Memory Data: " << std::bitset<32>(memData) << std::endl;
            }

            // If WriteReg is set, write the result to the rd register
            if (WriteReg.test(0) && !(Jump.test(0))) {
                std::cout << "Initial Register Values:" << std::endl;

                if (MemtoReg.test(0)) {
                    std::cout << numberToReg(rs) << ": " << std::bitset<32>(registers[rs]) << " ( " << registers[rs] << " )" << std::endl;
                    std::cout << numberToReg(rd) << ": " << std::bitset<32>(registers[rd]) << " ( " << registers[rd] << " )" << std::endl;

                    registers[rd] = memData;

                    std::cout << "Final Register Values:" << std::endl;
                    std::cout << numberToReg(rs) << ": " << std::bitset<32>(registers[rs]) << " ( " << registers[rs] << " )" << std::endl;
                    std::cout << numberToReg(rd) << ": " << std::bitset<32>(registers[rd]) << " ( " << registers[rd] << " )" << std::endl;
                } else {
                    std::cout << numberToReg(rs) << ": " << std::bitset<32>(registers[rs]) << " ( " << registers[rs] << " )" << std::endl;
                    std::cout << numberToReg(rt) << ": " << std::bitset<32>(registers[rt]) << " ( " << registers[rt] << " )" << std::endl;
                    std::cout << numberToReg(rd) << ": " << std::bitset<32>(registers[rd]) << " ( " << registers[rd] << " )" << std::endl;

                    registers[rd] = ALUResult;

                    std::cout << "Final Register Values:" << std::endl;
                    std::cout << numberToReg(rs) << ": " << std::bitset<32>(registers[rs]) << " ( " << registers[rs] << " )" << std::endl;
                    std::cout << numberToReg(rt) << ": " << std::bitset<32>(registers[rt]) << " ( " << registers[rt] << " )" << std::endl;
                    std::cout << numberToReg(rd) << ": " << std::bitset<32>(registers[rd]) << " ( " << registers[rd] << " )" << std::endl;
                }
            }

            // Branch and jump logic
            if (Branch.test(0) && ALUResult == 0) {
                PC += (instruction & 0xFFFF) << 2;
            } else if (Jump.test(0)) {
                // j case
                PC += 4;
                if (WriteReg.test(0)) {
                    // jal case
                    std::cout << "Initial Register Values:" << std::endl;
                    std::cout << numberToReg(31) << ": " << std::bitset<32>(registers[31]) << " ( " << registers[31] << " )" << std::endl;
                    registers[31] = PC;
                    std::cout << "Final Register Values:" << std::endl;
                    std::cout << numberToReg(31) << ": " << std::bitset<32>(registers[31]) << " ( " << registers[31] << " )" << std::endl;
                }
                uint32_t address = (instruction & 0x03FFFFFF) << 2;
                PC = (PC & 0xF0000000) | address;
                if (RegDst.test(0)) {
                    // jr case
                    PC = registers[31];
                }
                if (WriteReg.test(0)) {
                    // set PC for jal
                    PC = (instruction & 0x03FFFFFF) << 2;
                }
            } else {
                PC += 4;
            }

            // Handle syscall
            if (opcode == 0 && funct == 0x0C) {  // syscall detected
                uint32_t v0 = registers[2];      // get syscall code in $v0
                switch (v0) {
                    case 1:                                                                   // print integer
                        std::cout << "Syscall print integer: " << registers[4] << std::endl;  // $a0 = reg 4
                        break;
                    case 10:  // exit
                        std::cout << "Syscall exit called. Terminating program." << std::endl;
                        running = false;
                        break;
                    // add more syscalls as needed
                    default:
                        std::cerr << "Unknown syscall code: " << v0 << std::endl;
                        running = false;
                        break;
                }
            }

            std::cout << "Final PC: " << PC << std::endl;
        }
    }

    void setControlSignal(uint8_t opcode, uint8_t funct) {
        // Reset all signals
        RegDst.reset();
        WriteReg.reset();
        ALUSrc.reset();
        MemtoReg.reset();
        WriteMem.reset();
        ReadMem.reset();
        Branch.reset();
        Jump.reset();
        ALUOp1.reset();
        ALUOp0.reset();

        // Opcode binary format
        std::bitset<6> opcode_bin(opcode);
        bool r_format = opcode_bin.none();  // R-type instructions
        bool lw = opcode_bin == std::bitset<6>("100011");
        bool sw = opcode_bin == std::bitset<6>("101011");
        bool beq = opcode_bin == std::bitset<6>("000100");
        bool addi = opcode_bin == std::bitset<6>("001000");
        bool j = opcode_bin == std::bitset<6>("000010");
        bool jal = opcode_bin == std::bitset<6>("000011");

        // R-type instructions based on funct
        bool add = r_format && funct == 0b100000;
        bool sub = r_format && funct == 0b100010;
        bool jr = r_format && funct == 0b001000;
        bool syscall = r_format && funct == 0b1100;

        // Control signals
        RegDst = r_format;                         // Write to rd for R-type
        WriteReg = r_format || lw || addi || jal;  // Write to register
        ALUSrc = lw || sw || addi;                 // ALU source is immediate
        MemtoReg = lw;                             // Memory to register
        WriteMem = sw;                             // Write to memory
        ReadMem = lw;                              // Read from memory
        Branch = beq;                              // Branch if beq
        Jump = j || jal || jr;                     // Jump control
        ALUOp1 = r_format;                         // ALUOp1 for R-type
        ALUOp0 = beq;                              // ALUOp0 for beq

        // Special cases for ALUOp
        if (add || addi || lw || sw) {
            ALUOp1 = 0;
            ALUOp0 = 0;  // Addition
        } else if (sub || beq) {
            ALUOp1 = 0;
            ALUOp0 = 1;  // Subtraction
        } else if (r_format && !jr && !syscall) {
            ALUOp1 = 1;
            ALUOp0 = 0;
        }

        if (jal) {
            RegDst = 0;  // Write to $ra for jal
        }

        if (jr || syscall) {
            WriteReg = 0;  // Disable WriteReg for jr and syscall
        }

        // Print control signals for debugging
        std::cout << "RegDst: " << RegDst << "\n";
        std::cout << "WriteReg: " << WriteReg << "\n";
        std::cout << "ALUSrc: " << ALUSrc << "\n";
        std::cout << "MemtoReg: " << MemtoReg << "\n";
        std::cout << "WriteMem: " << WriteMem << "\n";
        std::cout << "ReadMem: " << ReadMem << "\n";
        std::cout << "Branch: " << Branch << "\n";
        std::cout << "ALUOp: " << ALUOp1 << ALUOp0 << "\n";
        std::cout << "Jump: " << Jump << "\n";
    }

    uint32_t ALUOperation(uint32_t readRegister1, uint32_t readRegister2, uint8_t funct) {
        if (ALUOp1 == 0 && ALUOp0 == 0) {
            // ADD operation (used for add, addi, lw, sw)
            return readRegister1 + readRegister2;
        } else if (ALUOp1 == 0 && ALUOp0 == 1) {
            // SUB operation (used for sub, beq)
            return readRegister1 - readRegister2;
        } else if (ALUOp1 == 1 && ALUOp0 == 0) {
            // R-type operations determined by funct
            switch (funct) {
                case 0x20:  // ADD
                    return readRegister1 + readRegister2;
                case 0x22:  // SUB
                    return readRegister1 - readRegister2;
                case 0x24:  // AND
                    return readRegister1 & readRegister2;
                case 0x25:  // OR
                    return readRegister1 | readRegister2;
                case 0x2A:  // SLT (Set on Less Than)
                    return (int32_t)readRegister1 < (int32_t)readRegister2 ? 1 : 0;
                case 0x08:  // jr case
                    return 0;
                case 0x0C:     // syscall
                    return 0;  // No ALU operation for syscall
                default:
                    std::cerr << "Unknown R-type ALU operation" << std::endl;
                    return 0;
            }
        } else {
            std::cerr << "Unknown ALUOp combination" << std::endl;
            return 0;
        }
    }

    uint32_t readMemory(uint32_t address) {
        std::cout << "Data Cache:" << std::endl;
        uint32_t data = dataCache.get(address);
        if (data == UINT32_MAX) {  // Cache miss
            data = (static_cast<uint32_t>(memoryAdd[address])) |
                   (static_cast<uint32_t>(memoryAdd[address + 1]) << 8) |
                   (static_cast<uint32_t>(memoryAdd[address + 2]) << 16) |
                   (static_cast<uint32_t>(memoryAdd[address + 3]) << 24);
            dataCache.put(address, data);
        }
        return data;
    }

    // function the print the part of memory where data is stored
    void printMemory() {
        std::cout << "\nMemory Contents:\n";
        std::cout << "Memory Address\tData (Hex)\t\tData (Binary)\n";
        for (std::size_t i = 0; i < 0x00FC; i += 4) {
            std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << i << ":\t\t";

            // Combine 4 bytes into a 32-bit word (little-endian)
            uint32_t word = 0;
            if (i < sizeof(memoryAdd)) {
                word |= memoryAdd[i];
            }
            if (i + 1 < sizeof(memoryAdd)) {
                word |= (memoryAdd[i + 1] << 8);
            }
            if (i + 2 < sizeof(memoryAdd)) {
                word |= (memoryAdd[i + 2] << 16);
            }
            if (i + 3 < sizeof(memoryAdd)) {
                word |= (memoryAdd[i + 3] << 24);
            }

            // Print the word as hexadecimal and binary
            std::cout << "0x" << std::hex << std::setw(8) << std::setfill('0') << word << "\t\t";
            std::cout << "0b" << std::bitset<32>(word) << std::endl;
        }
    }

    // Function to print word
    void printWord(uint32_t address, uint8_t* memoryAdd) {
        // Combine the 4 bytes into a single 32-bit integer
        uint32_t value = (memoryAdd[address]) |
                         (memoryAdd[address + 1] << 8) |
                         (memoryAdd[address + 2] << 16) |
                         (memoryAdd[address + 3] << 24);

        // Print the binary representation (32 bits) and its decimal value
        std::cout << "Address: " << std::bitset<32>(address) << " ( " << address << " ), Value: " << std::bitset<32>(value) << " ( " << value << " )" << std::endl;
    }

    // Print Register Values
    void printRegister() {
        std::cout << "\nRegister Values:\nRegister\t\tValue (Hex)\t\tValue (Decimal)\n";
        for (int i = 0; i < 32; i++) {
            if (i == 0) {
                std::cout << numberToReg(i) << "\t\t\t0x" << std::hex << std::setw(8) << std::setfill('0') << registers[i] << "\t\t" << std::dec << registers[i] << std::endl;
            } else {
                std::cout << numberToReg(i) << "\t\t\t\t0x" << std::hex << std::setw(8) << std::setfill('0') << registers[i] << "\t\t" << std::dec << registers[i] << std::endl;
            }
        }
    }

   public:
    MIPSprocessor()
        : instructionCache(12), dataCache(12) {
        symbolTable.clear();
        instructions.clear();
        funcMap.clear();
        std::fill(std::begin(memoryAdd), std::end(memoryAdd), 0);

        // Initialize the registers to 0
        for (int i = 0; i < 32; i++) {
            registers[i] = 0;
            floatRegisters[i] = 0;
        }
        HI = 0;
        LO = 0;

        dataMemoryStart = 0x0000;
        currentDataAddress = dataMemoryStart;
        PC = 0x0100;
        instructionSize = 0;
    }

    ~MIPSprocessor() {}
};

int main() {
    MIPSprocessor Processor;

    Processor.readFile("test_code_1_mips_sim.asm");
    Processor.assembleInstructions();
    Processor.executeInstructions();

    Processor.printMemory();
    Processor.printRegister();

    return EXIT_SUCCESS;
}
