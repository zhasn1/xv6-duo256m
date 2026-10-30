#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "proc.h"
#include "defs.h"
#include "gpio.h"

#define PORT_DR       0x00
#define PORT_DDR      0x04
#define INTEN         0x30
#define INTMASK       0x34
#define INTTYPE_LEVEL 0x38
#define INT_POLARITY  0x3C
#define INTSTATUS     0x40
#define PORTA_EOI     0x4C
#define EXT_PORTA     0x50

#define Reg(base, reg) ((volatile uint32 *)((base) + (reg)))

#define DEBOUNCE 500000

#define NPORT 5

struct gpioport {
  uint64 base;
  int irq;
  struct spinlock lock;
  uint32 armed;
  uint32 pending;
  uint32 state;
  uint64 last[32];
  uint falls[32];
  uint rises[32];
};

static struct gpioport ports[NPORT] = {
  {.base = GPIO0, .irq = GPIO0_IRQ}, {.base = GPIO1, .irq = GPIO1_IRQ},
  {.base = GPIO2, .irq = GPIO2_IRQ}, {.base = GPIO3, .irq = GPIO3_IRQ},
  {.base = GPIO4, .irq = GPIO4_IRQ},
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
arm(struct gpioport *p, int bit)
{
  uint32 m = 1 << bit;
  int level = (*Reg(p->base, EXT_PORTA) & m) != 0;

  setbit(p->base, INTEN, bit, 0);
  setbit(p->base, INTTYPE_LEVEL, bit, 1);
  setbit(p->base, INT_POLARITY, bit, !level);
  setbit(p->base, INTMASK, bit, 0);
  *Reg(p->base, PORTA_EOI) = m;
  if (level)
    p->state |= m;
  else
    p->state &= ~m;
  p->pending &= ~m;
  p->armed |= m;
  setbit(p->base, INTEN, bit, 1);
}

static void
disarm(struct gpioport *p, int bit)
{
  setbit(p->base, INTEN, bit, 0);
  p->armed &= ~(1 << bit);
  p->pending &= ~(1 << bit);
}

static int
gpioioctl(int user_src, int req, uint64 arg)
{
  int port = (arg >> GPIO_PORT_SFT) & 0xFF;
  int bit = arg & 0xFF;
  int val = (arg >> GPIO_VAL_SFT) & 0xFF;
  struct gpioport *p;
  uint *count;
  uint start;

  if (port >= NPORT || bit > 31)
    return -1;
  p = &ports[port];

  switch (req) {
  case GPIO_IOCTL_CONFIG:
    if (val != GPIO_IN && val != GPIO_OUT)
      return -1;
    acquire(&p->lock);
    if (val == GPIO_OUT)
      disarm(p, bit);
    setbit(p->base, PORT_DDR, bit, val == GPIO_OUT);
    release(&p->lock);
    return 0;

  case GPIO_IOCTL_SET:
    acquire(&p->lock);
    setbit(p->base, PORT_DR, bit, val != 0);
    release(&p->lock);
    return 0;

  case GPIO_IOCTL_GET:
    return (*Reg(p->base, EXT_PORTA) >> bit) & 1;

  case GPIO_IOCTL_WAIT:
    acquire(&p->lock);
    if (*Reg(p->base, PORT_DDR) & (1 << bit)) {
      release(&p->lock);
      return -1;
    }
    if (!(p->armed & (1 << bit)))
      arm(p, bit);
    count = val ? &p->rises[bit] : &p->falls[bit];
    start = *count;
    while (*count == start) {
      if (killed(myproc())) {
        release(&p->lock);
        return -1;
      }
      sleep_prepare(count);
      release(&p->lock);
      sleep();
      acquire(&p->lock);
    }
    release(&p->lock);
    return 0;
  }
  return -1;
}

void
gpioinit(void)
{
  initlock(&ports[0].lock, "gpio0");
  initlock(&ports[1].lock, "gpio1");
  initlock(&ports[2].lock, "gpio2");
  initlock(&ports[3].lock, "gpio3");
  initlock(&ports[4].lock, "gpio4");
  devsw[GPIO].ioctl = gpioioctl;
}

void
gpiointr(int irq)
{
  struct gpioport *p;
  uint64 now = r_time();
  uint32 status, stray, in;

  for (p = ports; p < &ports[NPORT]; p++)
    if (p->irq == irq)
      break;
  if (p == &ports[NPORT])
    return;
  acquire(&p->lock);

  status = *Reg(p->base, INTSTATUS);
  *Reg(p->base, PORTA_EOI) = status;

  stray = status & ~p->armed;
  if (stray)
    *Reg(p->base, INTEN) &= ~stray;

  in = *Reg(p->base, EXT_PORTA);
  for (int bit = 0; bit < 32; bit++) {
    if (!(status & p->armed & (1 << bit)))
      continue;
    p->last[bit] = now;
    p->pending |= 1 << bit;
    setbit(p->base, INT_POLARITY, bit, !(in & (1 << bit)));
  }
  release(&p->lock);
}

void
gpiotick(void)
{
  struct gpioport *p;
  uint64 now = r_time();
  uint32 in, m;
  int level;

  for (p = ports; p < &ports[NPORT]; p++) {
    acquire(&p->lock);
    if (p->pending) {
      in = *Reg(p->base, EXT_PORTA);
      for (int bit = 0; bit < 32; bit++) {
        m = 1 << bit;
        if (!(p->pending & m) || now - p->last[bit] < DEBOUNCE)
          continue;
        p->pending &= ~m;
        level = (in & m) != 0;
        setbit(p->base, INT_POLARITY, bit, !level);
        if (level == ((p->state & m) != 0))
          continue;
        p->state ^= m;
        if (level) {
          p->rises[bit]++;
          wakeup(&p->rises[bit]);
        } else {
          p->falls[bit]++;
          wakeup(&p->falls[bit]);
        }
      }
    }
    release(&p->lock);
  }
}
