    .section .text
    .globl _start
_start:
    li   a7, 64          # syscall write
    li   a0, 1           # fd = stdout
    la   a1, msg         # buffer
    li   a2, 4           # length
    ecall

	li 	 a7, 64
	li 	 a0, 1
	la	 a1, msg2
	li	 a2, 13
	ecall

    li   a0, 0           # exit code
    li   a7, 93          # syscall exit
    ecall

msg:
    .ascii "Hi!\n"

msg2:
	.ascii "3.1415926535\n"
