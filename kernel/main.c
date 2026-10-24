#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

#ifdef DUO
// boot markers
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
    kinit(); // physical page allocator
    MARK("kinit");
    kvminit(); // create kernel page table
    MARK("kvminit");
    kvminithart(); // turn on paging
    MARK("kvminithart");
    procinit(); // process table
    MARK("procinit");
    trapinit(); // trap vectors
    MARK("trapinit");
    trapinithart(); // install kernel trap vector
    MARK("trapinithart");
    plicinit(); // set up interrupt controller
    MARK("plicinit");
    plicinithart(); // ask PLIC for device interrupts
    MARK("plicinithart");
    binit(); // buffer cache
    MARK("binit");
    iinit(); // inode table
    MARK("iinit");
    fileinit(); // file table
    MARK("fileinit");
#ifdef DUO
    ramdiskinit(); // fs.img in kernel memory
    MARK("ramdiskinit");
#else
    virtio_disk_init(); // emulated hard disk
#endif
    userinit(); // first user process
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
