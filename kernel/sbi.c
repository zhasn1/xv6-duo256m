#include "types.h"
#include "riscv.h"
#include "defs.h"

#define SBI_EXT_TIME 0x54494D45
#define SBI_EXT_TIME_SET_TIMER 0
#define SBI_EXT_0_1_CONSOLE_PUTCHAR 0x01

static uint64
sbi_call(uint64 eid, uint64 fid, uint64 arg0)
{
  register uint64 a0 asm("a0") = arg0;
  register uint64 a1 asm("a1") = 0;
  register uint64 a6 asm("a6") = fid;
  register uint64 a7 asm("a7") = eid;
  asm volatile("ecall" : "+r"(a0), "+r"(a1) : "r"(a6), "r"(a7) : "memory");
  return a0;
}

void
sbi_putchar(int c)
{
  sbi_call(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, c);
}
void
sbi_set_timer(uint64 t)
{
  sbi_call(SBI_EXT_TIME, SBI_EXT_TIME_SET_TIMER, t);
}

void
sbi_puts(const char *s)
{
  for (; *s; s++) {
    if (*s == '\n')
      sbi_putchar('\r');
    sbi_putchar(*s);
  }
}

void
sbi_puthex(uint64 x)
{
  sbi_puts("0x");
  for (int i = 60; i >= 0; i -= 4)
    sbi_putchar("0123456789abcdef"[(x >> i) & 0xf]);
}
