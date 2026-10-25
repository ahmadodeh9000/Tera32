#ifndef cpu_h__
#define cpu_h__

#include <stdint.h>
#include "common.h"

#define MEM_SIZE    (1 << 16)

typedef struct CPU_t {
    u32 regs[32];
    u32 pc;

    u8 mem[MEM_SIZE];

    int halted;
    int exit_code;

} CPU;

void cpu_reset(CPU* cpu);
void cpu_dump(CPU* cpu);
u32 cpu_fetch(CPU* cpu);
void cpu_step(CPU* cpu);

void cpu_run(CPU* cpu);


#endif