#include "cpu.h"

#include <stdio.h>  // printf
#include <string.h> // memset

#define MASK(n) ((1u << (n)) - 1)

static void execute(CPU* cpu, uint32_t instr);

static uint32_t mem_read(CPU* cpu, uint32_t addr, int size) {
    if (addr > MEM_SIZE - size) {
        fprintf(stderr, "read out of range: %08x\n", addr);
        return 0;
    }

    uint32_t value = 0;
    for (int i = 0; i < size; i++) {
        value |= (uint32_t)cpu->mem[addr + i] << (8 * i);
    }
    return value;
}

static void mem_write(CPU* cpu, uint32_t addr, int size, uint32_t val) {
    if (addr > MEM_SIZE - size) {
        fprintf(stderr, "write out of range: %08x\n", addr);
        return;
    }

    for (int i = 0; i < size; i++) {
        cpu->mem[addr + i] = (val >> (8 * i)) & 0xFF;
    }
}


void cpu_reset(CPU* cpu) {
    if (NULL == cpu) return;

    cpu->pc         = 0;
    cpu->halted     = 0;
    cpu->exit_code  = 0;

    memset(cpu->mem,0, MEM_SIZE);
    memset(cpu->regs,0,sizeof(cpu->regs));

}
void cpu_dump(CPU* cpu) {
    if (NULL == cpu) return;
    printf("pc : %0.08x\n\n",cpu->pc);
    
    for (uint8_t i = 0; i < 32; i++) {
        printf("x%d\t: %0.08x\n",i,cpu->regs[i]);
    }

}

uint32_t cpu_fetch(CPU* cpu) {
    return  (uint32_t)cpu->mem[cpu->pc]
     | ((uint32_t)cpu->mem[cpu->pc + 1] << 8)
     | ((uint32_t)cpu->mem[cpu->pc + 2] << 16)
     | ((uint32_t)cpu->mem[cpu->pc + 3] << 24);
}

static void execute(CPU* cpu, uint32_t instr) {
    uint32_t opcode = (instr)       & MASK(7);  // MASK(WIDTH)
    uint32_t rd     = (instr >> 7)  & MASK(5);
    // I-Type
    uint32_t funct3 = (instr >> 12) & MASK(3);
    uint32_t rs1    = (instr >> 15) & MASK(5);

    // R-Type
    uint32_t rs2    = (instr >> 20) & MASK(5);
    uint32_t funct7 = (instr >> 25) & MASK(7);

    switch (opcode) {
        case 0x13: {
            int32_t imm = (int32_t)instr >> 20;      // CASTING IS SO FUCKING IMPORTANT AHMAD

            switch (funct3) {
                case 0x0: { // addi
                    cpu->regs[rd] = cpu->regs[rs1] + (uint32_t) imm;
                    break;
                }

                case 0x4: { // xori
                    cpu->regs[rd] = cpu->regs[rs1] ^ (uint32_t) imm;
                    break;
                }

                case 0x6: { // ori
                    cpu->regs[rd] = cpu->regs[rs1] | (uint32_t) imm;
                    break;
                }

                case 0x7: { // andi
                    cpu->regs[rd] = cpu->regs[rs1] & (uint32_t) imm;
                    break;
                }

                case 0x1: { //slli
                    uint32_t shmat = imm & MASK(5);
                    cpu->regs[rd] = cpu->regs[rs1] << shmat;
                    break;
                }

                case 0x2: { // slti
                    cpu->regs[rd] = (int32_t) cpu->regs[rs1] < imm;
                    break;
                }

                case 0x3: { // sltiu
                    cpu->regs[rd] = cpu->regs[rs1] < (uint32_t)imm;
                    break;
                }

                case 0x5: {
                    uint32_t shamt = imm & MASK(5);
                    if ((instr >> 30) & 1)   // srai: arithmetic
                        cpu->regs[rd] = (uint32_t)((int32_t)cpu->regs[rs1] >> shamt);
                    else                     // srli: logical
                        cpu->regs[rd] = cpu->regs[rs1] >> shamt;
                    break;
                }

                default: {
                    fprintf(stderr,"unimplemented funct3\n");
                    break;
                }
            }

            break;
        }

        case 0x33: {
            switch (funct3) {
                case 0x0: {
                    if (0x0 == funct7) { // add
                        cpu->regs[rd] = cpu->regs[rs1] + cpu->regs[rs2];
                        break;
                    }

                    if (0x20 == funct7) { // sub
                        cpu->regs[rd] = cpu->regs[rs1] - cpu->regs[rs2];
                        break;
                    }

                    break;
                }

                case 0x1: { // sll
                    if (0x0 == funct7) {
                        cpu->regs[rd] = cpu->regs[rs1] << (cpu->regs[rs2] & 0x1F);
                        break;
                    }
                    break;
                }
                case 0x2: { // slt
                    if (0x0 == funct7) {
                        cpu->regs[rd] = (int32_t)cpu->regs[rs1] < (int32_t)cpu->regs[rs2];
                        break;
                    }
                    break;
                }
                case 0x3: { // sltu
                    if (0x0 == funct7) {
                        cpu->regs[rd] = cpu->regs[rs1] < cpu->regs[rs2];
                        break;
                    }
                    break;
                }
                case 0x4: { // xor
                    if (0x0 == funct7) {
                        cpu->regs[rd] = cpu->regs[rs1] ^ cpu->regs[rs2];
                        break;
                    }
                    break;
                }

                case 0x5: { 
                    if (0x0 == funct7) { // srl (logical)
                        cpu->regs[rd] = cpu->regs[rs1] >> (cpu->regs[rs2] & 0x1f);
                        break;
                    }
                    if (0x20 == funct7) { // sra (arithmetic)
                        cpu->regs[rd] = (int32_t) cpu->regs[rs1] >> (cpu->regs[rs2] & 0x1f);
                        break;
                    }

                    break;
                }

                case 0x6: { // or
                    if (0x0 == funct7) {
                        cpu->regs[rd] = cpu->regs[rs1] | cpu->regs[rs2];
                        break;
                    }
                    break;
                }

                case 0x7: { // and
                    if (0x0 == funct7) {
                        cpu->regs[rd] = cpu->regs[rs1] & cpu->regs[rs2];
                        break;
                    }
                    break;
                }
                
            }
            break;
        }
        
        case 0x37: {
            cpu->regs[rd] = instr & 0xFFFFF000;
            break;
        }

        case 0x17: {
            cpu->regs[rd] = (cpu->pc - 4) + (instr & 0xFFFFF000);
            break;
        }

        case 0x03: { // loads (I-type)
            int32_t  imm  = (int32_t)instr >> 20;
            uint32_t addr = cpu->regs[rs1] + (uint32_t)imm;

            switch (funct3) {
                case 0x0: // lb
                    cpu->regs[rd] = (uint32_t)(int32_t)(int8_t)mem_read(cpu, addr, 1);
                    break;
                case 0x1: // lh
                    cpu->regs[rd] = (uint32_t)(int32_t)(int16_t)mem_read(cpu, addr, 2);
                    break;
                    case 0x2: // lw
                    cpu->regs[rd] = mem_read(cpu, addr, 4);
                    break;
                case 0x4: // lbu
                    cpu->regs[rd] = mem_read(cpu, addr, 1);
                    break;
                case 0x5: // lhu
                    cpu->regs[rd] = mem_read(cpu, addr, 2);
                    break;
                default:
                    fprintf(stderr, "unimplemented load funct3 %x\n", funct3);
                    break;
            }
            break;
        }


        case 0x23: { // stores (S-type)
            // imm[11:5] = instr[31:25], imm[4:0] = instr[11:7]
            uint32_t imm_s = (uint32_t)((int32_t)(instr & 0xFE000000) >> 20)
                           | ((instr >> 7) & MASK(5));
            uint32_t addr  = cpu->regs[rs1] + imm_s;
                
            switch (funct3) {
                case 0x0: // sb
                    mem_write(cpu, addr, 1, cpu->regs[rs2]);
                    break;
                case 0x1: // sh
                    mem_write(cpu, addr, 2, cpu->regs[rs2]);
                    break;
                case 0x2: // sw
                    mem_write(cpu, addr, 4, cpu->regs[rs2]);
                    break;
                default:
                    fprintf(stderr, "unimplemented store funct3 %x\n", funct3);
                    break;
            }
            break;
        }
        
        case 0x63: { // B-type
            int32_t imm_b = ((int32_t)(instr & 0x80000000) >> 19)  // bit 31  -> imm[12], sign-extended
              | ((instr & 0x80) << 4)                  // bit 7   -> imm[11]
              | ((instr >> 20) & 0x7E0)                // 30:25   -> imm[10:5]
              | ((instr >> 7)  & 0x1E);                // 11:8    -> imm[4:1]

            switch (funct3) {
                case 0x0: { // beq
                    if (cpu->regs[rs1] == cpu->regs[rs2]) {
                        cpu->pc = (cpu->pc - 4) + (uint32_t )imm_b;
                    }
                    break;
                }
                case 0x1: { // bne
                    if (cpu->regs[rs1] != cpu->regs[rs2]) {
                        cpu->pc = (cpu->pc - 4) + (uint32_t )imm_b;
                    }
                    break;
                }
                case 0x4: { // blt
                    if ((int32_t) cpu->regs[rs1]  < (int32_t) cpu->regs[rs2]) {
                        cpu->pc = (cpu->pc - 4) + (uint32_t )imm_b;
                    }
                    break;
                }
                case 0x5: { // bge
                    if ((int32_t) cpu->regs[rs1] >= (int32_t) cpu->regs[rs2]) {
                        cpu->pc = (cpu->pc - 4) + (uint32_t )imm_b;
                    }
                    break;
                }
                case 0x6: { // bltu
                    if (cpu->regs[rs1] < cpu->regs[rs2]) {
                        cpu->pc = (cpu->pc - 4) + (uint32_t )imm_b;
                    }
                    break;
                }
                case 0x7: { // bgeu
                    if (cpu->regs[rs1] >= cpu->regs[rs2]) {
                        cpu->pc = (cpu->pc - 4) + (uint32_t )imm_b;
                    }
                    break;
                }
                default: {
                    fprintf(stderr, "unimplemented branch funct3 %x\n", funct3);
                    break;
                }
            }
            break;
        }

        case 0x6f: { // J-type
            int32_t imm_j = ((int32_t)(instr & 0x80000000) >> 11)  // bit 31   -> imm[20], sign-extended
              | (instr & 0xFF000)                      // 19:12    -> imm[19:12] (already in place)
              | ((instr >> 9)  & 0x800)                // bit 20   -> imm[11]
              | ((instr >> 20) & 0x7FE);               // 30:21    -> imm[10:1]

            cpu->regs[rd] = cpu->pc;
            cpu->pc = (cpu->pc - 4) + (uint32_t) imm_j;
            break;
            
        }

        case 0x67: {
            int32_t imm = (int32_t)instr >> 20;

            uint32_t target = (cpu->regs[rs1] + (uint32_t)imm) & ~1u;
            cpu->regs[rd]   = cpu->pc;
            cpu->pc         = target;
            break;
        }
        
        case 0x0F: // fence
            break;

        case 0x73: {
            uint32_t imm = instr >> 20;           // unsigned: 0 = ecall, 1 = ebreak

            if (funct3 != 0) { cpu->halted = 1; break; }

            

            if (0 == imm) {
                switch (cpu->regs[17]) {
                    case 93:                      // exit
                        cpu->exit_code = (int)cpu->regs[10];
                        cpu->halted = 1;
                        break;

                    case 64: {  // write(fd, buf, len)
                        uint32_t fd  = cpu->regs[10];
                        uint32_t buf = cpu->regs[11];
                        uint32_t len = cpu->regs[12];

                        if (len > MEM_SIZE || buf > MEM_SIZE - len) {   // overflow-safe range check
                            fprintf(stderr, "write: buffer out of range\n");
                            cpu->regs[10] = (uint32_t)-1;               // error return
                            break;
                        }
                    
                        if (fd == 1) {
                            fwrite(&cpu->mem[buf], 1, len, stdout);
                        } else if (fd == 2) {
                            fwrite(&cpu->mem[buf], 1, len, stderr);
                        } else {
                            cpu->regs[10] = (uint32_t)-1;               // unsupported fd
                            break;
                        }
                    
                        cpu->regs[10] = len;                            // return value = bytes written
                        break;
                    }
                    
                    
                    default:
                        fprintf(stderr, "unknown ecall %u\n", cpu->regs[17]);
                        cpu->halted = 1;
                        break;
                }
            } else if (imm == 1) {                // ebreak
                cpu->halted = 1;
            }
            break;
        }
        
        default: {
            fprintf(stderr,"unimplemented opcode\n");
            break;
        }
    }

}

void cpu_step(CPU* cpu) {
    uint32_t instr = cpu_fetch(cpu);
    cpu->pc += 4;
    execute(cpu,instr);
    cpu->regs[0] = 0x0;
}

void cpu_run(CPU* cpu) {
    while (!cpu->halted) 
        cpu_step(cpu);
    
}