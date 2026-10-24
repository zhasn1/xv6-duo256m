#ifdef CONFIG_M_MODE
        # qemu -kernel loads the kernel at 0x80000000
        # and causes each hart (i.e. CPU) to jump there.
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
        # U-Boot (loady/go or bootm) jumps here in S-mode
        # at KERNBASE, MMU off. a0/a1 are ignored.
        .section .text
        .global _entry
_entry:
        csrw sie, zero
        li t0, (3 << 13) | (3 << 23) | 2   # sstatus FS, VS, SIE
        csrc sstatus, t0
        la t0, earlytrap
        csrw stvec, t0
        la t0, _bss_start
        la t1, _bss_end
1:      bgeu t0, t1, 2f
        sd zero, 0(t0)
        addi t0, t0, 8
        j 1b
2:      la sp, stack0
        li t0, 4096
        add sp, sp, t0
        call start
3:      j 3b

        .balign 4
earlytrap:
        call earlytrap_c
4:      j 4b
#endif
