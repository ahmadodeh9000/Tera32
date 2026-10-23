#ifndef cpu_h__
#define cpu_h__

#include <stdint.h>

#define MEM_SIZE    (1 << 16)

typedef struct CPU_t {
    uint32_t regs[32];
    uint32_t pc;

    uint8_t mem[MEM_SIZE];

    int halted;
    int exit_code;

} CPU;

void cpu_reset(CPU* cpu);
void cpu_dump(CPU* cpu);
uint32_t cpu_fetch(CPU* cpu);
void cpu_step(CPU* cpu);

void cpu_run(CPU* cpu);


#endif