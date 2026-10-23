# Tera32 stress test: self-checking RV32I program (no M, no C extension).
#
# Prints one line per test ("name ok" or "name FAIL (got X, want Y)") and
# exits with the number of failed checks, so `echo $?` should print 0.
#
# Memory map (64 KiB): code+data at 0x0000, scratch RAM at 0x8000+, stack at top.
#
# Build:
#   riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -nostdlib -static \
#       -Wl,-Ttext=0x0 -Wl,--no-relax stress.s -o stress.elf
#   riscv64-unknown-elf-objcopy -O binary stress.elf stress.bin

    .equ ARR,   0x8000          # 16-word array for the sort test
    .equ SIEVE, 0x9000          # 101-byte sieve flags
    .equ BYTES, 0x9100          # byte/halfword scratch
    .equ STR,   0xA000          # string scratch

    .section .text
    .globl _start

# CHECK got_reg, want_imm, name_label   (clobbers a0-a2, ra)
    .macro CHECK got, want, name
    mv   a0, \got
    li   a1, \want
    la   a2, \name
    jal  ra, check
    .endm

_start:
    lui  sp, 0x10               # sp = 0x10000 (top of 64 KiB)
    li   s11, 0                 # s11 = global failure counter

    la   a0, s_banner
    jal  ra, print_str

# ---------------------------------------------------------------- fib table
    la   a0, s_fibhdr
    jal  ra, print_str
    li   s0, 0
fib_tbl:
    mv   a0, s0
    jal  ra, fib
    jal  ra, print_dec
    la   a0, s_space
    jal  ra, print_str
    addi s0, s0, 1
    li   t0, 15
    blt  s0, t0, fib_tbl
    la   a0, s_nl
    jal  ra, print_str

# ------------------------------------------------- T1: recursion + stack
    li   a0, 15
    jal  ra, fib
    mv   s0, a0
    CHECK s0, 610, n_fib

# ------------------------------------------- T2: software multiply
    li   a0, 1234
    li   a1, 567
    jal  ra, mulu
    mv   s0, a0
    CHECK s0, 699678, n_mul1
    li   a0, 0xFFFF
    li   a1, 0xFFFF
    jal  ra, mulu
    mv   s0, a0
    CHECK s0, 0xFFFE0001, n_mul2

# --------------------------------------------- T3: software divide
    li   a0, 1000000007
    li   a1, 97
    jal  ra, udivmod
    mv   s0, a0
    mv   s1, a1
    CHECK s0, 10309278, n_divq
    CHECK s1, 41, n_divr

# ------------------------------------------------------- T4: gcd
    li   a0, 1071
    li   a1, 462
    jal  ra, gcd
    mv   s0, a0
    CHECK s0, 21, n_gcd1
    li   a0, 48
    li   a1, 18
    jal  ra, gcd
    mv   s0, a0
    CHECK s0, 6, n_gcd2

# ------------------------------------- T5: bubble sort (signed compares)
    la   t0, arr_src
    li   t1, ARR
    li   t2, 16
cp_arr:
    lw   t3, 0(t0)
    sw   t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    addi t2, t2, -1
    bnez t2, cp_arr

    li   a0, ARR
    li   a1, 16
    jal  ra, bubble_sort

    li   s0, ARR
    lw   s1, 0(s0)              # smallest
    lw   s2, 60(s0)             # largest
    CHECK s1, 0x80000000, n_sort_min
    CHECK s2, 0x7FFFFFFF, n_sort_max

    li   s3, 0                  # ordering violations
    li   s4, 0                  # wrapping sum
    li   t0, 0
    li   t1, 15
chk_sorted:
    slli t2, t0, 2
    add  t2, t2, s0
    lw   t3, 0(t2)
    lw   t4, 4(t2)
    add  s4, s4, t3
    ble  t3, t4, sorted_ok      # pseudo: bge t4, t3
    addi s3, s3, 1
sorted_ok:
    addi t0, t0, 1
    blt  t0, t1, chk_sorted
    lw   t3, 60(s0)
    add  s4, s4, t3
    CHECK s3, 0, n_sort_ord
    CHECK s4, 123798, n_sort_sum

# --------------------------------------------- T6: sieve of Eratosthenes
    li   s0, SIEVE
    li   t0, 0
    li   t1, 101
sv_clear:
    add  t2, s0, t0
    sb   zero, 0(t2)
    addi t0, t0, 1
    blt  t0, t1, sv_clear

    li   t0, 2                  # i
sv_outer:
    add  t2, s0, t0
    lbu  t3, 0(t2)
    bnez t3, sv_next            # already composite
    add  t4, t0, t0             # j = 2i
sv_mark:
    li   t5, 100
    bgt  t4, t5, sv_next
    add  t2, s0, t4
    li   t6, 1
    sb   t6, 0(t2)
    add  t4, t4, t0
    j    sv_mark
sv_next:
    addi t0, t0, 1
    li   t5, 101
    blt  t0, t5, sv_outer

    li   s1, 0                  # prime count
    li   s2, 0                  # largest prime
    li   t0, 2
sv_count:
    add  t2, s0, t0
    lbu  t3, 0(t2)
    bnez t3, sv_skip
    addi s1, s1, 1
    mv   s2, t0
sv_skip:
    addi t0, t0, 1
    li   t5, 101
    blt  t0, t5, sv_count
    CHECK s1, 25, n_primes
    CHECK s2, 97, n_lastprime

# ------------------------------- T7: byte / halfword loads and stores
    li   s0, BYTES
    li   t0, 0x80
    sb   t0, 0(s0)
    lb   s1, 0(s0)
    lbu  s2, 0(s0)
    CHECK s1, 0xFFFFFF80, n_lb
    CHECK s2, 0x80, n_lbu
    li   t0, 0x8001
    sh   t0, 4(s0)
    lh   s1, 4(s0)
    lhu  s2, 4(s0)
    CHECK s1, 0xFFFF8001, n_lh
    CHECK s2, 0x8001, n_lhu
    li   t0, 0xDEADBEEF
    sw   t0, 8(s0)
    lb   s1, 8(s0)              # 0xEF
    lbu  s2, 11(s0)             # 0xDE
    lh   s3, 10(s0)             # 0xDEAD
    lhu  s4, 10(s0)
    lw   s5, 8(s0)
    CHECK s1, 0xFFFFFFEF, n_lb2
    CHECK s2, 0xDE, n_lbu2
    CHECK s3, 0xFFFFDEAD, n_lh2
    CHECK s4, 0xDEAD, n_lhu2
    CHECK s5, 0xDEADBEEF, n_lw
    li   t0, 0x55
    sb   t0, 9(s0)              # patch one byte, rest must survive
    lw   s5, 8(s0)
    CHECK s5, 0xDEAD55EF, n_sb_patch

# --------------------------------------- T8: strings (copy, reverse, cmp)
    la   a0, hello
    jal  ra, strlen
    mv   s0, a0
    CHECK s0, 14, n_strlen
    li   a0, STR
    la   a1, hello
    jal  ra, strcpy
    li   a0, STR
    jal  ra, strrev
    li   a0, STR
    la   a1, hello_rev
    jal  ra, strcmp
    mv   s0, a0
    CHECK s0, 0, n_strrev
    li   a0, STR
    la   a1, hello
    jal  ra, strcmp             # reversed != original, must be nonzero
    snez s0, a0
    CHECK s0, 1, n_strne

# ----------------------------------------------------- T9: CRC-32
    la   a0, crc_in
    li   a1, 9
    jal  ra, crc32
    mv   s0, a0
    CHECK s0, 0xCBF43926, n_crc

# ------------------------------------------------- T10: bit tricks
    li   a0, 0xDEADBEEF
    jal  ra, popcount
    mv   s0, a0
    CHECK s0, 24, n_popcnt
    li   t0, 0x12345678
    slli t1, t0, 13
    srli t2, t0, 19
    or   s0, t1, t2
    CHECK s0, 0x8ACF0246, n_rotl
    li   t0, -1024
    srai s0, t0, 3
    CHECK s0, 0xFFFFFF80, n_srai
    li   t0, 0x80000000
    srli s0, t0, 31
    CHECK s0, 1, n_srli
    li   t0, -1
    li   t1, 1
    slt  t2, t0, t1             # signed:   -1 < 1  -> 1
    sltu t3, t0, t1             # unsigned: max < 1 -> 0
    slli t2, t2, 1
    or   s0, t2, t3
    CHECK s0, 2, n_slt
    li   t0, 0x0F0F0F0F
    li   t1, 0x00FF00FF
    and  t2, t0, t1
    or   t3, t0, t1
    xor  t4, t0, t1
    sub  t5, t3, t2
    CHECK t2, 0x000F000F, n_and
    CHECK t3, 0x0FFF0FFF, n_or
    CHECK t4, 0x0FF00FF0, n_xor
    CHECK t5, 0x0FF00FF0, n_sub

# --------------------------- T11: indirect calls through a jump table
    la   s0, jtab
    li   s1, 0
    lw   t0, 0(s0)
    jalr ra, 0(t0)
    add  s1, s1, a0
    lw   t0, 4(s0)
    jalr ra, 0(t0)
    add  s1, s1, a0
    lw   t0, 8(s0)
    jalr ra, 0(t0)
    add  s1, s1, a0
    CHECK s1, 66, n_jtab

# ---------------------------- T12: every branch, taken and not taken
    li   t0, -1                 # 0xFFFFFFFF signed -1
    li   t1, 1
    li   s0, 0
    blt  t0, t1, br1            # -1 < 1 signed: taken
    j    br1e
br1:
    ori  s0, s0, 0x01
br1e:
    bltu t0, t1, br2e           # max < 1 unsigned: NOT taken
    ori  s0, s0, 0x02
br2e:
    bge  t1, t0, br3            # 1 >= -1 signed: taken
    j    br3e
br3:
    ori  s0, s0, 0x04
br3e:
    bgeu t1, t0, br4e           # 1 >= max unsigned: NOT taken
    ori  s0, s0, 0x08
br4e:
    beq  t0, t0, br5
    j    br5e
br5:
    ori  s0, s0, 0x10
br5e:
    bne  t0, t1, br6
    j    br6e
br6:
    ori  s0, s0, 0x20
br6e:
    CHECK s0, 0x3F, n_branches

# -------------------------------------------- T13: x0 and lui/auipc
    li   t0, 123
    addi zero, t0, 5            # write to x0 must vanish
    mv   s0, zero
    CHECK s0, 0, n_x0
    lui  s0, 0x12345
    addi s0, s0, 0x678
    CHECK s0, 0x12345678, n_lui
auipc_pt:
    auipc s0, 0                 # s0 = address of this very instruction
    la   t0, auipc_pt           # same address, computed by the assembler
    sub  s0, s0, t0
    CHECK s0, 0, n_auipc

# ------------------------------------------------------------ summary
    la   a0, s_summary
    jal  ra, print_str
    mv   a0, s11
    jal  ra, print_dec
    la   a0, s_nl
    jal  ra, print_str

    mv   a0, s11
    li   a7, 93                 # exit(failures)
    ecall
    ebreak                      # not reached

# ===================================================================
#                              functions
# ===================================================================

# fib(a0) recursive
fib:
    li   t0, 2
    blt  a0, t0, fib_ret
    addi sp, sp, -16
    sw   ra, 12(sp)
    sw   s0, 8(sp)
    sw   s1, 4(sp)
    mv   s0, a0
    addi a0, s0, -1
    jal  ra, fib
    mv   s1, a0
    addi a0, s0, -2
    jal  ra, fib
    add  a0, a0, s1
    lw   ra, 12(sp)
    lw   s0, 8(sp)
    lw   s1, 4(sp)
    addi sp, sp, 16
fib_ret:
    ret

# mulu(a0, a1) -> a0 = a0 * a1 (low 32 bits), shift-and-add
mulu:
    li   t0, 0
mulu_loop:
    beqz a1, mulu_done
    andi t1, a1, 1
    beqz t1, mulu_skip
    add  t0, t0, a0
mulu_skip:
    slli a0, a0, 1
    srli a1, a1, 1
    j    mulu_loop
mulu_done:
    mv   a0, t0
    ret

# udivmod(a0 = n, a1 = d) -> a0 = quotient, a1 = remainder (restoring division)
udivmod:
    li   t0, 0                  # quotient
    li   t1, 0                  # remainder
    li   t2, 32
udiv_loop:
    srli t3, a0, 31
    slli a0, a0, 1
    slli t1, t1, 1
    or   t1, t1, t3
    slli t0, t0, 1
    bltu t1, a1, udiv_next
    sub  t1, t1, a1
    ori  t0, t0, 1
udiv_next:
    addi t2, t2, -1
    bnez t2, udiv_loop
    mv   a0, t0
    mv   a1, t1
    ret

# gcd(a0, a1) by repeated subtraction
gcd:
gcd_loop:
    beq  a0, a1, gcd_done
    bltu a0, a1, gcd_swap
    sub  a0, a0, a1
    j    gcd_loop
gcd_swap:
    sub  a1, a1, a0
    j    gcd_loop
gcd_done:
    ret

# bubble_sort(a0 = base, a1 = n words), signed ascending
bubble_sort:
    addi t0, a1, -1             # passes
bs_outer:
    blez t0, bs_done
    li   t1, 0                  # index
    mv   t2, a0
bs_inner:
    bge  t1, t0, bs_next
    lw   t3, 0(t2)
    lw   t4, 4(t2)
    ble  t3, t4, bs_noswap
    sw   t4, 0(t2)
    sw   t3, 4(t2)
bs_noswap:
    addi t2, t2, 4
    addi t1, t1, 1
    j    bs_inner
bs_next:
    addi t0, t0, -1
    j    bs_outer
bs_done:
    ret

# strlen(a0) -> a0
strlen:
    mv   t0, a0
sl_loop:
    lbu  t1, 0(t0)
    beqz t1, sl_done
    addi t0, t0, 1
    j    sl_loop
sl_done:
    sub  a0, t0, a0
    ret

# strcpy(a0 = dst, a1 = src)
strcpy:
sc_loop:
    lbu  t0, 0(a1)
    sb   t0, 0(a0)
    addi a0, a0, 1
    addi a1, a1, 1
    bnez t0, sc_loop
    ret

# strrev(a0) in place
strrev:
    addi sp, sp, -16
    sw   ra, 12(sp)
    sw   s0, 8(sp)
    mv   s0, a0
    jal  ra, strlen             # a0 = length
    add  t1, s0, a0
    addi t1, t1, -1             # last char
    mv   t0, s0                 # first char
sr_loop:
    bge  t0, t1, sr_done
    lbu  t2, 0(t0)
    lbu  t3, 0(t1)
    sb   t3, 0(t0)
    sb   t2, 0(t1)
    addi t0, t0, 1
    addi t1, t1, -1
    j    sr_loop
sr_done:
    lw   ra, 12(sp)
    lw   s0, 8(sp)
    addi sp, sp, 16
    ret

# strcmp(a0, a1) -> a0 = byte difference (0 when equal)
strcmp:
cmp_loop:
    lbu  t0, 0(a0)
    lbu  t1, 0(a1)
    bne  t0, t1, cmp_diff
    beqz t0, cmp_eq
    addi a0, a0, 1
    addi a1, a1, 1
    j    cmp_loop
cmp_diff:
    sub  a0, t0, t1
    ret
cmp_eq:
    li   a0, 0
    ret

# crc32(a0 = ptr, a1 = len) -> a0 (bitwise, reflected, poly 0xEDB88320)
crc32:
    li   t0, -1
    li   t3, 0xEDB88320
crc_byte:
    beqz a1, crc_done
    lbu  t1, 0(a0)
    xor  t0, t0, t1
    li   t2, 8
crc_bit:
    andi t4, t0, 1
    srli t0, t0, 1
    beqz t4, crc_nox
    xor  t0, t0, t3
crc_nox:
    addi t2, t2, -1
    bnez t2, crc_bit
    addi a0, a0, 1
    addi a1, a1, -1
    j    crc_byte
crc_done:
    not  a0, t0
    ret

# popcount(a0) -> a0 (Kernighan)
popcount:
    li   t0, 0
pc_loop:
    beqz a0, pc_done
    addi t1, a0, -1
    and  a0, a0, t1
    addi t0, t0, 1
    j    pc_loop
pc_done:
    mv   a0, t0
    ret

# jump table targets
jf1:
    li   a0, 11
    ret
jf2:
    li   a0, 22
    ret
jf3:
    li   a0, 33
    ret

# write_buf(a0 = ptr, a1 = len): write(1, ptr, len)
write_buf:
    mv   a2, a1
    mv   a1, a0
    li   a0, 1
    li   a7, 64
    ecall
    ret

# print_str(a0 = NUL-terminated string)
print_str:
    mv   t0, a0
ps_loop:
    lbu  t1, 0(t0)
    beqz t1, ps_done
    addi t0, t0, 1
    j    ps_loop
ps_done:
    sub  a1, t0, a0
    j    write_buf              # tail call

# print_dec(a0 = unsigned value)
print_dec:
    addi sp, sp, -48
    sw   ra, 44(sp)
    sw   s0, 40(sp)
    sw   s1, 36(sp)
    addi s0, sp, 32             # digits are written backwards from here
    mv   s1, a0
    bnez s1, pd_loop
    addi s0, s0, -1
    li   t0, 48
    sb   t0, 0(s0)
    j    pd_out
pd_loop:
    mv   a0, s1
    li   a1, 10
    jal  ra, udivmod
    addi a1, a1, 48
    addi s0, s0, -1
    sb   a1, 0(s0)
    mv   s1, a0
    bnez s1, pd_loop
pd_out:
    addi a1, sp, 32
    sub  a1, a1, s0
    mv   a0, s0
    jal  ra, write_buf
    lw   ra, 44(sp)
    lw   s0, 40(sp)
    lw   s1, 36(sp)
    addi sp, sp, 48
    ret

# check(a0 = got, a1 = want, a2 = name)
check:
    addi sp, sp, -32
    sw   ra, 28(sp)
    sw   s0, 24(sp)
    sw   s1, 20(sp)
    sw   s2, 16(sp)
    mv   s0, a0
    mv   s1, a1
    mv   s2, a2
    mv   a0, s2
    jal  ra, print_str
    beq  s0, s1, ck_ok
    addi s11, s11, 1
    la   a0, s_fail
    jal  ra, print_str
    mv   a0, s0
    jal  ra, print_dec
    la   a0, s_want
    jal  ra, print_str
    mv   a0, s1
    jal  ra, print_dec
    la   a0, s_close
    jal  ra, print_str
    j    ck_ret
ck_ok:
    la   a0, s_ok
    jal  ra, print_str
ck_ret:
    lw   ra, 28(sp)
    lw   s0, 24(sp)
    lw   s1, 20(sp)
    lw   s2, 16(sp)
    addi sp, sp, 32
    ret

# ===================================================================
#                               data
# ===================================================================
    .balign 4
jtab:
    .word jf1, jf2, jf3
arr_src:
    .word -5, 300, 42, -1000, 7, 0, 65535, -65536
    .word 123456, 99, -99, 2147483647, -2147483648, 1, -1, 1000

s_banner:  .asciz "Tera32 stress test\n"
s_fibhdr:  .asciz "fib(0..14): "
s_space:   .asciz " "
s_nl:      .asciz "\n"
s_ok:      .asciz "  ok\n"
s_fail:    .asciz "  FAIL (got "
s_want:    .asciz ", want "
s_close:   .asciz ")\n"
s_summary: .asciz "failed checks: "

hello:     .asciz "Hello, Tera32!"
hello_rev: .asciz "!23areT ,olleH"
crc_in:    .ascii "123456789"
           .byte 0

n_fib:        .asciz "recursive fib(15)    "
n_mul1:       .asciz "soft mul 1234*567    "
n_mul2:       .asciz "soft mul 65535^2     "
n_divq:       .asciz "soft div quotient    "
n_divr:       .asciz "soft div remainder   "
n_gcd1:       .asciz "gcd(1071,462)        "
n_gcd2:       .asciz "gcd(48,18)           "
n_sort_min:   .asciz "sort: minimum        "
n_sort_max:   .asciz "sort: maximum        "
n_sort_ord:   .asciz "sort: order violations"
n_sort_sum:   .asciz "sort: wrapping sum   "
n_primes:     .asciz "sieve: prime count   "
n_lastprime:  .asciz "sieve: largest prime "
n_lb:         .asciz "lb  sign-extend      "
n_lbu:        .asciz "lbu zero-extend      "
n_lh:         .asciz "lh  sign-extend      "
n_lhu:        .asciz "lhu zero-extend      "
n_lb2:        .asciz "lb  from word        "
n_lbu2:       .asciz "lbu from word        "
n_lh2:        .asciz "lh  from word        "
n_lhu2:       .asciz "lhu from word        "
n_lw:         .asciz "lw  round trip       "
n_sb_patch:   .asciz "sb  patches one byte "
n_strlen:     .asciz "strlen               "
n_strrev:     .asciz "strcpy + strrev      "
n_strne:      .asciz "strcmp unequal       "
n_crc:        .asciz "crc32(123456789)     "
n_popcnt:     .asciz "popcount(0xDEADBEEF) "
n_rotl:       .asciz "rotate left 13       "
n_srai:       .asciz "srai (arithmetic)    "
n_srli:       .asciz "srli (logical)       "
n_slt:        .asciz "slt vs sltu          "
n_and:        .asciz "and                  "
n_or:         .asciz "or                   "
n_xor:        .asciz "xor                  "
n_sub:        .asciz "sub                  "
n_jtab:       .asciz "jalr jump table      "
n_branches:   .asciz "all six branch types "
n_x0:         .asciz "x0 stays zero        "
n_lui:        .asciz "lui + addi constant  "
n_auipc:      .asciz "auipc = own address  "
