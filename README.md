# Tera32

A small RISC-V emulator written in C. Tera32 implements the **RV32I** base integer instruction set, runs flat binaries on a single hart, and provides a minimal Linux-style syscall interface (`write` and `exit`) so bare-metal programs can print and return an exit code.

It is built for learning: the whole CPU lives in one readable file (`src/cpu.c`), and a built-in test suite checks every instruction group.

## Features

- Full RV32I user-level instruction set (see [Supported instructions](#supported-instructions))
- Flat, little-endian, 64 KiB memory
- `ecall`-based `write` and `exit`, so programs can produce output and a process exit code
- Built-in self-tests that run when no program is given
- Example program and a Makefile to cross-compile it

## Build

```sh
make
```

This produces the `tera32` binary in the project root. It needs only a C compiler.

## Usage

```sh
./tera32                  # run the built-in self-tests
./tera32 program.bin      # load a flat binary at address 0 and run it
```

When running a program, Tera32's own exit code is the program's exit code, so you can check it from the shell:

```sh
./tera32 src/prog/hello.bin
echo $?
```

The example program prints a message, sums 1 through 10 in a loop, and exits with the result (`55`).

## Writing your own programs

Programs are assembled for bare-metal RV32I and converted to a flat binary. You need a RISC-V cross toolchain (for example `riscv64-unknown-elf-gcc`).

The Makefile in `src/prog/` shows the full recipe:

```sh
cd src/prog
make
```

which is equivalent to:

```sh
riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib -static \
    -Wl,-Ttext=0x0 -Wl,--no-relax hello.s -o hello.elf
riscv64-unknown-elf-objcopy -O binary hello.elf hello.bin
```

Notes on the flags:

- `-march=rv32i -mabi=ilp32` limits the compiler to the base ISA, with no multiply/divide, compressed, or float instructions.
- `-Wl,-Ttext=0x0` links the code at address 0, which is where Tera32 loads and starts execution.
- `-Wl,--no-relax` stops the linker from using `gp`-relative addressing, since Tera32 does not initialise `gp`.
- `objcopy -O binary` strips the ELF wrapper. Tera32 loads raw bytes, not ELF files.

Override the toolchain prefix with `make CROSS=riscv32-unknown-elf-` if yours is named differently.

## Machine model

| Property | Value |
|---|---|
| ISA | RV32I |
| Harts | 1 |
| Registers | `x0`–`x31` (`x0` is hardwired to zero) |
| Endianness | Little |
| Memory | 64 KiB flat RAM, `0x0000`–`0xFFFF` |
| Load address | `0x00000000` |
| Reset PC | `0x00000000` |
| Initial `sp` (`x2`) | `0x00010000` (top of memory, grows down) |

Misaligned loads and stores are allowed. Out-of-range loads return 0 and out-of-range stores are dropped, both with a message on stderr.

## Syscalls

Tera32 uses the Linux RISC-V convention: the syscall number goes in `a7`, arguments in `a0`–`a2`, and the result comes back in `a0`.

| `a7` | Name | Arguments | Behaviour |
|---|---|---|---|
| `64` | `write` | `a0` = fd, `a1` = buffer, `a2` = length | Writes to stdout (fd 1) or stderr (fd 2). Returns the byte count, or `-1` for a bad fd or out-of-range buffer. |
| `93` | `exit` | `a0` = exit code | Halts the CPU and sets the emulator's exit code. |

Any other syscall number prints an error and halts. `ebreak` also halts.

## Supported instructions

| Group | Instructions |
|---|---|
| Upper immediate | `lui`, `auipc` |
| Jumps | `jal`, `jalr` |
| Branches | `beq`, `bne`, `blt`, `bge`, `bltu`, `bgeu` |
| Loads | `lb`, `lh`, `lw`, `lbu`, `lhu` |
| Stores | `sb`, `sh`, `sw` |
| ALU immediate | `addi`, `slti`, `sltiu`, `xori`, `ori`, `andi`, `slli`, `srli`, `srai` |
| ALU register | `add`, `sub`, `sll`, `slt`, `sltu`, `xor`, `srl`, `sra`, `or`, `and` |
| System | `ecall`, `ebreak`, `fence` (no-op) |

## Tests

Running `./tera32` with no arguments executes the self-tests in `src/main.c`:

1. **ALU and U-type**: every immediate and register ALU op, `lui`, `auipc`, writes to `x0` being ignored, and `cpu_fetch` being side-effect free
2. **Loads and stores**: sign vs. zero extension, byte stores into a word, negative offsets, and the split S-type immediate
3. **Control flow**: taken and not-taken `beq`/`bne`, a backward loop, `jal`, and `jalr`
4. **Signed vs. unsigned branches**: `blt`/`bge` against `bltu`/`bgeu` with `0xFFFFFFFF` and equal operands
5. **Syscalls**: `write` to stdout followed by `exit` with a non-zero code

It prints `all tests passed` on success and returns a non-zero exit code on any failure.

## Project layout

```
.
├── Makefile
├── src
│   ├── cpu.c        # fetch, decode, execute, memory access, syscalls
│   ├── cpu.h        # CPU struct and public API
│   ├── main.c       # self-tests and program loader
│   └── prog
│       ├── Makefile # cross-compile recipe
│       └── hello.s  # example program
└── tera32           # built binary
```

## How it works

The core loop is three steps, in `cpu_step`:

1. **Fetch** the 32-bit instruction at `pc`
2. **Advance** `pc` by 4
3. **Execute** it, then force `x0` back to zero

Because `pc` is advanced before execution, PC-relative instructions (`auipc`, `jal`, branches) compute their target from `pc - 4`, and `jal`/`jalr` store the already-incremented `pc` as the return address. Branch and jump immediates are reassembled from their scattered bit fields and sign-extended, and all arithmetic is done on `uint32_t` with explicit casts to `int32_t` wherever signed behaviour is needed.

## Limitations and ideas

- No CSR instructions (`Zicsr`), privilege modes, traps, or interrupts. CSR opcodes halt the CPU.
- No `M` (multiply/divide), `C` (compressed), or floating-point extensions.
- Unknown opcodes print a message and are skipped rather than raising an illegal-instruction trap.
- `cpu_fetch` does not bounds-check `pc`.
- Only flat binaries are loaded, with no ELF loader.
- Next steps: run compiled C programs (`sp` is already initialised for this), add `M`, and add traps.
