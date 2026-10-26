#ifdef CONFIG_M_MODE
        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart to jump there.
        # kernel.ld causes the following code to
        # be placed at 0x80000000.
.section .text
.global _entry
_entry:
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + ((hartid + 1) * 4096)
        la sp, stack0
        li a0, 1024*4
        csrr a1, mhartid
        addi a1, a1, 1
        mul a0, a0, a1
        add sp, sp, a0
        # jump to start() in start.c
        call start
spin:
        j spin
#else
        # U-Boot jumps here in S-mode
.section .text
.global _entry
_entry:
        # no interrupts until the kernel asks for them.
        csrw sie, zero
        # clear sstatus SIE (bit 1), FS (bits 14:13), and VS (bits 24:23).
        # uboot leaves FS and VS dirty.
        li t0, (3 << 13) | (3 << 23) | 2
        csrc sstatus, t0
        # report any trap taken before main() sets up kernelvec
        la t0, earlytrapvec
        csrw stvec, t0
        # zero out the BSS segment. objcopy -O binary drops it
        # and loady doesn't clear RAM.
        la t0, _bss_start
        la t1, _bss_end
1:
        bgeu t0, t1, 2f
        sd zero, 0(t0)
        addi t0, t0, 8
        j 1b
2:
        # set up a stack for C.
        la sp, stack0
        li t0, 1024*4
        add sp, sp, t0
        call start
spin:
        j spin
        # early trap handler: print scause, sepc, and stval
        # through SBI, then spin.
        .balign 4
earlytrapvec:
        call earlytrap_c
        j spin
#endif
