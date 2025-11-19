# MIPS Processor Simulator

A comprehensive MIPS processor simulator written in C++ that assembles and executes MIPS assembly language programs. The simulator includes a full implementation of the MIPS instruction set with cache support (LFU cache) for both instructions and data.

## Features

- **Complete MIPS Instruction Set Support**:
  - Arithmetic: `add`, `addi`, `sub`, `mult`, `div`, `mflo`, `mfhi`
  - Logical: `and`, `or`, `slt`
  - Memory: `lw`, `sw`, `la`, `lwc1`, `swc1`
  - Control Flow: `beq`, `j`, `jr`, `jal`
  - System: `syscall`, `li`, `move`
  - Floating Point: `add.s`, `lwc1`, `swc1`

- **Memory Management**:
  - Separate `.data` and `.text` sections
  - Support for `.word`, `.asciiz`, and `.float` directives
  - 4KB memory address space
  - Little-endian memory organization

- **Cache Implementation**:
  - LFU (Least Frequently Used) cache replacement policy
  - Separate instruction and data caches
  - Configurable cache capacity
  - Cache hit/miss statistics

- **Register Set**:
  - 32 general-purpose registers
  - 32 floating-point registers
  - Special registers: HI, LO, PC (Program Counter)

- **Debugging Features**:
  - Detailed execution trace
  - Binary and hexadecimal output
  - Register state display
  - Memory dump
  - Control signal visualization

## Project Structure

```
.
├── MIPS_sim.cpp             # Main processor implementation
├── cache.h                  # LFU cache header file
├── cache.cpp                # LFU cache implementation
├── test_code_1_mips_sim.asm # Sample MIPS assembly program
└── README.md                # This file
```

## Requirements

- C++11 compatible compiler (g++, clang++)
- Standard C++ libraries

## Compilation

To compile the simulator, use the following command:

```bash
g++ -std=c++11 -o mips_simulator MIPS_sim.cpp cache.cpp -include sstream
```

Or simply:

```bash
g++ -std=c++11 -o mips_simulator MIPS_sim.cpp cache.cpp
```

Note: If you encounter errors related to `std::istringstream`, use the first command with the `-include sstream` flag.

## Usage

### Running the Simulator

The simulator reads a MIPS assembly file named `test_code_1_mips_sim.asm` by default:

```bash
./mips_simulator
```

### Assembly File Format

Your MIPS assembly file should follow this structure:

```assembly
.data
variable_name: .word value
string_var: .asciiz "text"
float_var: .float 3.14

.text
start:
    # Your MIPS instructions here
    addi $t0, $zero, 5
    # ...
    syscall
```

### Example Program

The included `test_code_1_mips_sim.asm` demonstrates a loop that accumulates the sum of two values:

```assembly
.data
val1: .word 5
val2: .word 10
result: .word 0

.text
start:
    addi $t7, $zero, 0        # Loop counter i = 0
    addi $t8, $zero, 5        # Loop limit 5

loop:
    lw $t0, val1              # Load val1
    lw $t1, val2              # Load val2
    add $t2, $t0, $t1         # val1 + val2
    lw $t3, result            # Load previous result
    add $t2, $t2, $t3         # Add new sum to previous result
    sw $t2, result            # Store accumulated result
    addi $t7, $t7, 1          # i++
    beq $t7, $t8, print       # If i == 5, jump to print
    j loop                    # Else continue loop

print:
    lw $a0, result            # Load accumulated result to $a0
    addi $v0, $zero, 1        # Syscall code for print integer
    syscall                   # Print the result

exit:
    addi $v0, $zero, 10       # Syscall code for exit
    syscall                   # Exit program
```

Expected output: 75 (5+10 accumulated 5 times)

## Syscall Support

The simulator supports the following syscall codes:

| Code | Service | Arguments |
|------|---------|-----------|
| 1    | Print integer | `$a0` = integer to print |
| 10   | Exit program | None |

## Output

The simulator provides detailed execution information:

1. **Assembly Instructions**: Shows each instruction being assembled
2. **Machine Code**: Displays the 32-bit binary representation
3. **Execution Trace**: 
   - Instruction fetch (with cache statistics)
   - Control signals
   - Register values before and after
   - Memory operations
   - Program counter updates
4. **Final State**:
   - Complete memory dump
   - Final register values in hex and decimal

## Cache Statistics

The simulator displays cache operations:
- `Cache HIT`: Successful cache access
- `Cache MISS`: Cache miss, data fetched from memory
- `Cache PUT`: New entry added to cache
- `Cache EVICT`: Entry removed due to capacity

## Architecture Details

### Memory Map

- **Data Section**: `0x0000` - `0x00FF` (256 bytes)
- **Text Section**: `0x0100` - `0x0FFF` (3840 bytes)

### Register Names

- `$zero` (R0): Constant 0
- `$at` (R1): Assembler temporary
- `$v0-$v1` (R2-R3): Function return values
- `$a0-$a3` (R4-R7): Function arguments
- `$t0-$t9` (R8-R15, R24-R25): Temporary registers
- `$s0-$s7` (R16-R23): Saved registers
- `$k0-$k1` (R26-R27): Kernel registers
- `$gp` (R28): Global pointer
- `$sp` (R29): Stack pointer
- `$fp` (R30): Frame pointer
- `$ra` (R31): Return address

## Limitations

- Fixed memory size (4KB)
- Limited syscall support
- No pipeline simulation
- No hazard detection
- Single-threaded execution only

## Contributing

Contributions are welcome! Feel free to:
- Report bugs
- Suggest new features
- Submit pull requests
- Improve documentation

## Author

Created by [rakshitx1](https://github.com/rakshitx1)

## License

This project is available for educational purposes.
