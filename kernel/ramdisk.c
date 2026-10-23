#include "types.h"
#include "param.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

void ramdiskinit(void) { }

void ramdiskrw(struct buf *b, int write) { panic("ramdiskrw: no ramdisk yet"); }
