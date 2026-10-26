#include "types.h"
#include "param.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

extern char _ramdisk_start[], _ramdisk_end[];
static uint64 nblocks;

void
ramdiskinit(void)
{
  nblocks = (_ramdisk_end - _ramdisk_start) / BSIZE;
  struct superblock *sb = (struct superblock *)(_ramdisk_start + BSIZE);
  if (sb->magic != FSMAGIC)
    panic("ramdiskinit: bad superblock magic");
  printk("ramdisk: fs.img at %p, %d blocks\n", _ramdisk_start, (int)nblocks);
}

void
ramdiskrw(struct buf *b, int write)
{
  if (!holdingsleep(&b->lock))
    panic("ramdiskrw: buf not locked");
  if (b->blockno >= nblocks)
    panic("ramdiskrw: blockno too big");
  char *p = _ramdisk_start + (uint64)b->blockno * BSIZE;
  if (write)
    memmove(p, b->data, BSIZE);
  else
    memmove(b->data, p, BSIZE);
}
