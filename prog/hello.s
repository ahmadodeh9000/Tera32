    .section .text
    .globl _start
_start:
    # write(1, msg, 15)
    li   a7, 64
    li   a0, 1
    la   a1, msg            # expands to auipc + addi, so this tests auipc too
    li   a2, 13
    ecall

    # sum = 1 + 2 + ... + 10
    li   t0, 0              # sum
    li   t1, 1              # i
    li   t2, 11
loop:
    add  t0, t0, t1
    addi t1, t1, 1
    blt  t1, t2, loop       # backward branch

    # exit(sum)
    mv   a0, t0
    li   a7, 93
    ecall

msg:
    .ascii "Risc-V rules\n"
