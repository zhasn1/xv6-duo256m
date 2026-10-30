#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

#ifdef DUO
#define MARK(name) printk("main: " name " ok\n")
#else
#define MARK(name)
#endif

// start() jumps here in supervisor mode on all CPUs.
void
main()
{
  if (cpuid() == 0) {
    consoleinit();
    printkinit();
    printk("\n");
    printk("xv6 kernel is booting\n");
    printk("\n");
    kinit();
    MARK("kinit");
    kvminit();
    MARK("kvminit");
    kvminithart();
    MARK("kvminithart");
    procinit();
    MARK("procinit");
    trapinit();
    MARK("trapinit");
    trapinithart();
    MARK("trapinithart");
    plicinit();
    MARK("plicinit");
    plicinithart();
    MARK("plicinithart");
#ifdef DUO
    pinmuxinit();
    MARK("pinmux");
    gpioinit();
    MARK("gpio");
    pwminit();
    MARK("pwm");
    spiinit();
    MARK("spi");
    i2cinit();
    MARK("i2c");
#endif
    binit();
    MARK("binit");
    iinit();
    MARK("iinit");
    fileinit();
    MARK("fileinit");
#ifdef DUO
    ramdiskinit();
    MARK("ramdiskinit");
#else
    virtio_disk_init(); // emulated hard disk
#endif
    userinit();
    MARK("userinit");

    __atomic_store_n(&started, 1, __ATOMIC_RELEASE);
  } else {
    while (__atomic_load_n(&started, __ATOMIC_ACQUIRE) == 0)
      ;

    printk("hart %d starting\n", cpuid());
    kvminithart();  // turn on paging
    trapinithart(); // install kernel trap vector
    plicinithart(); // ask PLIC for device interrupts
  }

  scheduler();
}
