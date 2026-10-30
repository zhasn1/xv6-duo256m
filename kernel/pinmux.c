#include "types.h"
#include "riscv.h"
#include "defs.h"

static struct {
  uint32 off;
  uint32 val;
} pins[] = {
  {0x068, 2}, {0x0d4, 1}, {0x0d8, 1}, {0x0e4, 1},
  {0x0e0, 1}, {0x0dc, 1}, {0x0d0, 1}, {0x03c, 3},
};

void
pinmuxinit(void)
{
  for (int i = 0; i < NELEM(pins); i++)
    *(volatile uint32 *)(PINMUX + pins[i].off) = pins[i].val;
}

int
mmio_wait(volatile uint32 *reg, uint32 mask, uint32 want, int usec)
{
  uint64 end = r_time() + (uint64)usec * 25;
  while ((*reg & mask) != want)
    if (r_time() > end)
      return -1;
  return 0;
}
