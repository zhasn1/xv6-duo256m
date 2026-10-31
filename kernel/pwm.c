#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "defs.h"
#include "pwm.h"

#define HLPERIOD(n) (0x00 + 8 * (n)) // low cycles, normal polarity
#define PERIOD(n)   (0x04 + 8 * (n))
#define POLARITY    0x40 // bit n polarity, bit 8+n count mode
#define PWMSTART    0x44
#define PWMUPDATE   0x4C
#define PWM_OE      0xD0

#define Reg(base, reg) ((volatile uint32 *)((base) + (reg)))

#define NCHIP     4
#define MAXCYCLES 0x3FFFFFFF

struct pwmchip {
  uint64 base;
  struct spinlock lock;
};

static struct pwmchip chips[NCHIP] = {
  {.base = PWM0},
  {.base = PWM1},
  {.base = PWM2},
  {.base = PWM3},
};

static void
setbit(uint64 base, int reg, int bit, int on)
{
  volatile uint32 *r = Reg(base, reg);

  if (on)
    *r |= 1 << bit;
  else
    *r &= ~(1 << bit);
}

static void
stop(uint64 base, int n)
{
  setbit(base, PWMSTART, n, 0);
  setbit(base, PWM_OE, n, 0);
}

static int
config(struct pwm_config *c)
{
  struct pwmchip *p = &chips[c->chip];
  int n = c->chan;
  uint64 per, high;

  if (c->duty > c->period || c->period > 0xFFFFFFFFUL)
    return -1;
  per = (uint64)PWM_CLK_HZ * c->period / 1000000000;
  high = (uint64)PWM_CLK_HZ * c->duty / 1000000000;
  if (per < 2 || per > MAXCYCLES)
    return -1;
  if (high >= per)
    high = per - 1;

  acquire(&p->lock);
  if (high == 0) {
    stop(p->base, n);
    release(&p->lock);
    return 0;
  }
  *Reg(p->base, PERIOD(n)) = per;
  *Reg(p->base, HLPERIOD(n)) = per - high;
  if (*Reg(p->base, PWMSTART) & (1 << n)) {
    setbit(p->base, PWMUPDATE, n, 1);
    setbit(p->base, PWMUPDATE, n, 0);
  } else {
    setbit(p->base, POLARITY, n, 0);
    setbit(p->base, POLARITY, 8 + n, 0);
    setbit(p->base, PWM_OE, n, 1);
    setbit(p->base, PWMSTART, n, 0);
    setbit(p->base, PWMSTART, n, 1);
  }
  release(&p->lock);
  return 0;
}

static int
pwmioctl(int user_src, int req, uint64 arg)
{
  struct pwm_config c;

  if (req != PWM_IOCTL_CONFIG)
    return -1;
  if (either_copyin(&c, user_src, arg, sizeof(c)) < 0)
    return -1;
  if (c.chip < 0 || c.chip >= NCHIP || c.chan < 0 || c.chan > 3)
    return -1;

  if (!c.enable) {
    acquire(&chips[c.chip].lock);
    stop(chips[c.chip].base, c.chan);
    release(&chips[c.chip].lock);
    return 0;
  }
  return config(&c);
}

void
pwminit(void)
{
  initlock(&chips[0].lock, "pwm0");
  initlock(&chips[1].lock, "pwm1");
  initlock(&chips[2].lock, "pwm2");
  initlock(&chips[3].lock, "pwm3");
  devsw[PWM].ioctl = pwmioctl;
}
