.section .data
buffer: .space 16
outbuf: .space 12

.section .text
.globl _start

_start:
    li a7, 63
    li a0, 0
    la a1, buffer
    li a2, 16
    ecall                      # a0 = bytes read (0 = EOF, -1 = error)
    blez a0, fail

    # ---- atoi: t2 = parsed value ----
    la t0, buffer
    add t1, t0, a0             # end pointer
    li t2, 0
    li t4, 10
parse:
    bgeu t0, t1, parsed
    lbu t3, 0(t0)
    addi t3, t3, -48           # c - '0'
    bgeu t3, t4, parsed        # unsigned compare rejects <'0' and >'9' (e.g. '\n')
    slli t5, t2, 3
    slli t6, t2, 1
    add t2, t5, t6             # t2 * 10
    add t2, t2, t3
    addi t0, t0, 1
    j parse
parsed:

    # ---- itoa: print t2 + newline ----
    mv a3, t2
    la t0, outbuf+11
    li t3, 10
    sb t3, 0(t0)               # '\n' at the end
conv:
    li t4, 0                   # quotient
div10:
    bltu a3, t3, divdone
    addi a3, a3, -10
    addi t4, t4, 1
    j div10
divdone:
    addi a3, a3, 48            # remainder -> ASCII
    addi t0, t0, -1
    sb a3, 0(t0)
    mv a3, t4
    bnez a3, conv              # do-while, so 0 prints "0"

    la t1, outbuf+12
    sub a2, t1, t0             # length
    mv a1, t0
    li a0, 1
    li a7, 64
    ecall

    li a0, 0
    li a7, 93
    ecall

fail:
    li a0, 1
    li a7, 93
    ecall