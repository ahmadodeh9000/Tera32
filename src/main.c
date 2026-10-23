#include "cpu.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static int fails = 0;

/* ---------------- helpers ---------------- */

static void put32(CPU* c, uint32_t addr, uint32_t w) {
    for (int i = 0; i < 4; i++)
        c->mem[addr + i] = (w >> (8 * i)) & 0xFF;
}

// tiny encoders so some tests don't need hand-written hex
static uint32_t enc_i(uint32_t op, uint32_t f3, uint32_t rd, uint32_t rs1, int32_t imm) {
    return ((uint32_t)imm << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}
static uint32_t ADDI(uint32_t rd, uint32_t rs1, int32_t imm) {
    return enc_i(0x13, 0, rd, rs1, imm);
}
static uint32_t enc_b(uint32_t f3, uint32_t rs1, uint32_t rs2, int32_t imm) {
    uint32_t i = (uint32_t)imm;
    return (((i >> 12) & 1)  << 31) | (((i >> 5) & 0x3F) << 25) |
           (rs2 << 20) | (rs1 << 15) | (f3 << 12) |
           (((i >> 1) & 0xF) << 8)  | (((i >> 11) & 1)   << 7)  | 0x63;
}

// run until pc == end_pc; fail instead of hanging or running off memory
static void run_until(CPU* c, uint32_t end_pc) {
    for (int steps = 0; c->pc != end_pc; steps++) {
        if (steps >= 1000 || c->pc > MEM_SIZE - 4) {
            printf("FAIL: did not reach pc %08x (stuck at %08x)\n", end_pc, c->pc);
            fails++;
            return;
        }
        cpu_step(c);
    }
}

#define CHECK(reg, val)                                                     \
    do {                                                                    \
        if (c.regs[reg] != (uint32_t)(val)) {                               \
            printf("FAIL x%d: got %08x, expected %08x\n",                   \
                   reg, c.regs[reg], (uint32_t)(val));                      \
            fails++;                                                        \
        }                                                                   \
    } while (0)

/* ---------------- self-tests ---------------- */

static int run_self_tests(void) {
    static CPU c;

    // ================= Test 1: ALU + U-type =================
    cpu_reset(&c);

    static const uint32_t prog1[] = {
        0x00500093, // addi x1, x0, 5
        0x00708113, // addi x2, x1, 7
        0xFFF00193, // addi x3, x0, -1
        0x00900013, // addi x0, x0, 9   (must be ignored)
        0x0030C213, // xori  x4, x1, 3
        0x00A0E293, // ori   x5, x1, 10
        0x0040F313, // andi  x6, x1, 4
        0x0001A393, // slti  x7, x3, 0
        0xFFF03413, // sltiu x8, x0, -1
        0x00209493, // slli  x9, x1, 2
        0x0041D513, // srli  x10, x3, 4
        0x4041D593, // srai  x11, x3, 4
        0x00208633, // add  x12, x1, x2
        0x402086B3, // sub  x13, x1, x2
        0x0020F733, // and  x14, x1, x2
        0x0020E7B3, // or   x15, x1, x2
        0x0020C833, // xor  x16, x1, x2
        0x002098B3, // sll  x17, x1, x2
        0x0011A933, // slt  x18, x3, x1
        0x0011B9B3, // sltu x19, x3, x1
        0x4011DA33, // sra  x20, x3, x1
        0x0011DAB3, // srl  x21, x3, x1
        0x12345B37, // lui   x22, 0x12345
        0x00001B97, // auipc x23, 1        (at address 92)
        0x12345C37, // lui   x24, 0x12345
        0x678C0C13, // addi  x24, x24, 0x678
    };
    const int n1 = sizeof(prog1) / sizeof(prog1[0]);

    for (int i = 0; i < n1; i++) put32(&c, i * 4, prog1[i]);

    // fetch must be side-effect free
    c.pc = 0;
    assert(cpu_fetch(&c) == 0x00500093);
    assert(cpu_fetch(&c) == 0x00500093);
    assert(c.pc == 0);

    for (int i = 0; i < n1; i++) cpu_step(&c);

    CHECK(0, 0);
    CHECK(1, 5);
    CHECK(2, 12);
    CHECK(3, 0xFFFFFFFF);
    CHECK(4, 6);
    CHECK(5, 15);
    CHECK(6, 4);
    CHECK(7, 1);
    CHECK(8, 1);
    CHECK(9, 20);
    CHECK(10, 0x0FFFFFFF);
    CHECK(11, 0xFFFFFFFF);
    CHECK(12, 17);
    CHECK(13, 0xFFFFFFF9);
    CHECK(14, 4);
    CHECK(15, 13);
    CHECK(16, 9);
    CHECK(17, 0x5000);
    CHECK(18, 1);
    CHECK(19, 0);
    CHECK(20, 0xFFFFFFFF);
    CHECK(21, 0x07FFFFFF);
    CHECK(22, 0x12345000);
    CHECK(23, 0x105C);
    CHECK(24, 0x12345678);

    if (c.pc != (uint32_t)(n1 * 4)) {
        printf("FAIL test1 pc: got %08x, expected %08x\n", c.pc, n1 * 4);
        fails++;
    }

    // ================= Test 2: loads / stores =================
    cpu_reset(&c);

    static const uint32_t prog2[] = {
        0x20000093, // addi x1, x0, 0x200   base address
        0xFFE00113, // addi x2, x0, -2
        0x0020A023, // sw   x2, 0(x1)       mem[0x200..] = FE FF FF FF
        0x0000A183, // lw   x3, 0(x1)
        0x00008203, // lb   x4, 0(x1)
        0x0000C283, // lbu  x5, 0(x1)
        0x00009303, // lh   x6, 0(x1)
        0x0000D383, // lhu  x7, 0(x1)
        0x07F00413, // addi x8, x0, 0x7F
        0x008080A3, // sb   x8, 1(x1)       mem is now FE 7F FF FF
        0x0000A483, // lw   x9, 0(x1)
        0x00408593, // addi x11, x1, 4
        0xFFC5A503, // lw   x10, -4(x11)    negative offset
        0x0420A023, // sw   x2, 64(x1)      split S-type immediate
        0x0400A603, // lw   x12, 64(x1)
    };
    const int n2 = sizeof(prog2) / sizeof(prog2[0]);

    for (int i = 0; i < n2; i++) put32(&c, i * 4, prog2[i]);
    for (int i = 0; i < n2; i++) cpu_step(&c);

    CHECK(1, 0x200);
    CHECK(2, 0xFFFFFFFE);
    CHECK(3, 0xFFFFFFFE);
    CHECK(4, 0xFFFFFFFE);
    CHECK(5, 0x000000FE);
    CHECK(6, 0xFFFFFFFE);
    CHECK(7, 0x0000FFFE);
    CHECK(8, 0x7F);
    CHECK(9, 0xFFFF7FFE);
    CHECK(10, 0xFFFF7FFE);
    CHECK(12, 0xFFFFFFFE);

    if (c.mem[0x200] != 0xFE || c.mem[0x201] != 0x7F ||
        c.mem[0x202] != 0xFF || c.mem[0x203] != 0xFF) {
        printf("FAIL mem[0x200..0x203]: %02x %02x %02x %02x\n",
               c.mem[0x200], c.mem[0x201], c.mem[0x202], c.mem[0x203]);
        fails++;
    }

    if (c.pc != (uint32_t)(n2 * 4)) {
        printf("FAIL test2 pc: got %08x, expected %08x\n", c.pc, n2 * 4);
        fails++;
    }

    // ================= Test 3: beq/bne, loop, jal, jalr =================
    cpu_reset(&c);

    static const uint32_t prog3[] = {
        /*  0 */ 0x00500093, // addi x1, x0, 5
        /*  4 */ 0x00500113, // addi x2, x0, 5
        /*  8 */ 0x00208463, // beq  x1, x2, +8     taken, jumps to 16
        /* 12 */ 0x00100193, // addi x3, x0, 1      (skipped)
        /* 16 */ 0x00200213, // addi x4, x0, 2
        /* 20 */ 0x00209463, // bne  x1, x2, +8     not taken
        /* 24 */ 0x00700293, // addi x5, x0, 7
        /* 28 */ 0x00900313, // addi x6, x0, 9
        /* 32 */ 0x00300393, // addi x7, x0, 3
        /* 36 */ 0xFFF38393, // addi x7, x7, -1
        /* 40 */ 0xFE039EE3, // bne  x7, x0, -4     backward loop, 3 iterations
        /* 44 */ 0x0080046F, // jal  x8, +8         x8 = 48, jumps to 52
        /* 48 */ 0x00100493, // addi x9, x0, 1      (skipped)
        /* 52 */ 0x00200513, // addi x10, x0, 2
        /* 56 */ 0x04400613, // addi x12, x0, 68
        /* 60 */ 0x000605E7, // jalr x11, 0(x12)    x11 = 64, jumps to 68
        /* 64 */ 0x00100693, // addi x13, x0, 1     (skipped)
        /* 68 */ 0x00300713, // addi x14, x0, 3
    };
    const int n3 = sizeof(prog3) / sizeof(prog3[0]);

    for (int i = 0; i < n3; i++) put32(&c, i * 4, prog3[i]);
    run_until(&c, n3 * 4);   // 72

    CHECK(3, 0);
    CHECK(4, 2);
    CHECK(5, 7);
    CHECK(6, 9);
    CHECK(7, 0);
    CHECK(8, 48);
    CHECK(9, 0);
    CHECK(10, 2);
    CHECK(11, 64);
    CHECK(13, 0);
    CHECK(14, 3);

    // ================= Test 4: signed vs unsigned branches =================
    // Each branch jumps +8 over a marker "addi xN, x0, 1".
    // xN == 0 means the branch was taken, xN == 1 means it was not.
    cpu_reset(&c);

    const uint32_t BLT = 4, BGE = 5, BLTU = 6, BGEU = 7;
    const uint32_t prog4[] = {
        /*  0 */ ADDI(1, 0, -1),            // x1 = 0xFFFFFFFF (-1 signed)
        /*  4 */ ADDI(2, 0, 1),             // x2 = 1
        /*  8 */ enc_b(BLT,  1, 2, 8),      // -1 < 1 signed            -> taken
        /* 12 */ ADDI(3, 0, 1),
        /* 16 */ enc_b(BLTU, 1, 2, 8),      // 0xFFFFFFFF < 1 unsigned  -> NOT taken
        /* 20 */ ADDI(4, 0, 1),
        /* 24 */ enc_b(BGE,  1, 2, 8),      // -1 >= 1 signed           -> NOT taken
        /* 28 */ ADDI(5, 0, 1),
        /* 32 */ enc_b(BGEU, 1, 2, 8),      // 0xFFFFFFFF >= 1 unsigned -> taken
        /* 36 */ ADDI(6, 0, 1),
        /* 40 */ enc_b(BLTU, 2, 1, 8),      // 1 < 0xFFFFFFFF unsigned  -> taken
        /* 44 */ ADDI(7, 0, 1),
        /* 48 */ enc_b(BLTU, 1, 1, 8),      // equal, "<" is false      -> NOT taken
        /* 52 */ ADDI(8, 0, 1),
        /* 56 */ enc_b(BGEU, 1, 1, 8),      // equal, ">=" is true      -> taken
        /* 60 */ ADDI(9, 0, 1),
        /* 64 */ enc_b(BGE,  2, 2, 8),      // equal, ">=" is true      -> taken
        /* 68 */ ADDI(10, 0, 1),
    };
    const int n4 = sizeof(prog4) / sizeof(prog4[0]);

    for (int i = 0; i < n4; i++) put32(&c, i * 4, prog4[i]);
    run_until(&c, n4 * 4);   // 72

    CHECK(3, 0);    // blt  taken
    CHECK(4, 1);    // bltu not taken
    CHECK(5, 1);    // bge  not taken
    CHECK(6, 0);    // bgeu taken
    CHECK(7, 0);    // bltu taken
    CHECK(8, 1);    // bltu equal: not taken
    CHECK(9, 0);    // bgeu equal: taken
    CHECK(10, 0);   // bge  equal: taken

    // ================= Test 5: ecall write + exit =================
    cpu_reset(&c);

    static const uint32_t prog5[] = {
        /*  0 */ 0x04000893, // addi x17, x0, 64      a7 = write
        /*  4 */ 0x00100513, // addi x10, x0, 1       fd = stdout
        /*  8 */ 0x10000593, // addi x11, x0, 0x100   buf
        /* 12 */ 0x00300613, // addi x12, x0, 3       len
        /* 16 */ 0x00000073, // ecall                 prints "Hi\n"
        /* 20 */ 0x05D00893, // addi x17, x0, 93      a7 = exit
        /* 24 */ 0x00700513, // addi x10, x0, 7       exit code 7
        /* 28 */ 0x00000073, // ecall
    };
    const int n5 = sizeof(prog5) / sizeof(prog5[0]);

    for (int i = 0; i < n5; i++) put32(&c, i * 4, prog5[i]);
    put32(&c, 0x100, 0x000A6948);   // bytes 'H' 'i' '\n' 0
    cpu_run(&c);

    if (!c.halted)        { printf("FAIL: not halted\n"); fails++; }
    if (c.exit_code != 7) { printf("FAIL exit_code: %d\n", c.exit_code); fails++; }
    if (c.pc != 32)       { printf("FAIL test5 pc: %08x\n", c.pc); fails++; }

    /* ---------------- result ---------------- */
    fflush(stdout);
    if (fails == 0) printf("all tests passed\n");
    else            printf("%d check(s) failed\n", fails);
    return fails != 0;
}

/* ---------------- file mode ---------------- */

// Load a flat binary at address 0 and run it until it halts.
static int run_file(const char* path) {
    static CPU c;
    cpu_reset(&c);                       // reset first: it clears memory

    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }

    size_t n     = fread(c.mem, 1, MEM_SIZE, f);
    int    extra = fgetc(f);             // anything left means the file doesn't fit
    int    err   = ferror(f);
    fclose(f);

    if (err)          { fprintf(stderr, "%s: read error\n", path); return 1; }
    if (n == 0)       { fprintf(stderr, "%s: empty file\n", path); return 1; }
    if (extra != EOF) { fprintf(stderr, "%s: larger than memory (%d bytes)\n",
                                path, (int)MEM_SIZE); return 1; }

    c.regs[2] = MEM_SIZE;                // sp = top of memory (needed for C programs later)
    cpu_run(&c);
    fflush(stdout);
    return c.exit_code;
}

int main(int argc, char** argv) {
    if (argc > 2) {
        fprintf(stderr, "usage: %s [program.bin]\n", argv[0]);
        return 1;
    }
    if (argc == 2) return run_file(argv[1]);
    return run_self_tests();
}