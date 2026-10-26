OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  /*
   * ensure that entry.S / _entry is at KERNBASE:
   * qemu's -kernel jumps to 0x80000000,
   * and U-Boot loads the Duo kernel at 0x80200000.
   */
  . = KERNBASE;

  .text : {
    kernel/entry.o(_entry)
    *(.text .text.*)
    . = ALIGN(0x1000);
    _trampoline = .;
    *(trampsec)
    . = ALIGN(0x1000);
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
    PROVIDE(etext = .);
  }

  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)
    . = ALIGN(0x100);
    _ramdisk_start = .; /* fs.img on the Duo (ramdisk_data.S); empty on qemu */
    *(.ramdisk .ramdisk.*)
    _ramdisk_end = .;
  }

  .bss : {
    . = ALIGN(16);
    _bss_start = .;
    *(.sbss .sbss.*)
    . = ALIGN(16);
    *(.bss .bss.*)
    . = ALIGN(16);
    _bss_end = .;
  }

  PROVIDE(end = .);
}
