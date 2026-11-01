/*
 * this driver reuses code from u-boot/drivers/spi/designware_spi.c
 * here is the copyright
 *
 * Designware master SPI core controller driver
 *
 * Copyright (C) 2014 Stefan Roese <sr@denx.de>
 * Copyright (C) 2020 Sean Anderson <seanga2@gmail.com>
 *
 * Very loosely based on the Linux driver:
 * drivers/spi/spi-dw.c, which is:
 * Copyright (c) 2009, Intel Corporation.
*/

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "defs.h"
#include "spi.h"

#define CTRLR0 0x00
#define SSIENR 0x08
#define SER    0x10
#define BAUDR  0x14
#define TXFTLR 0x18
#define RXFLR  0x24
#define SR     0x28
#define IMR    0x2C
#define DR     0x60

#define CTRLR0_DFS8  7
#define CTRLR0_SCPH  (1 << 6)
#define CTRLR0_SCPOL (1 << 7)

#define SR_BUSY 1

#define Reg(reg) ((volatile uint32 *)(SPI2 + (reg)))

#define MAXFIFO 256
#define MAXDIV  0xFFFE
#define BOOT_HZ 1000000

static struct spinlock lock;
static struct sleeplock busy;
static int depth;
static uint32 ctrl;
static uint32 div;
static uint32 hz;
static uchar tx[MAXFIFO];
static uchar rx[MAXFIFO];

static uint32
divider(uint64 speed)
{
  uint64 d = (SPI_CLK_HZ + speed - 1) / speed;

  d = (d + 1) & ~1UL;
  if (d < 2)
    d = 2;
  if (d > MAXDIV)
    d = MAXDIV;
  return d;
}

static void
setup(void)
{
  *Reg(SSIENR) = 0;
  *Reg(CTRLR0) = ctrl;
  *Reg(BAUDR) = div;
  *Reg(SSIENR) = 1;
  hz = SPI_CLK_HZ / div;
}

static int
chunk(int n)
{
  int usec = (uint64)n * 8 * 2 * 1000000 / hz + 1000;
  int r = 0;

  acquire(&lock);
  for (int i = 0; i < n; i++)
    *Reg(DR) = tx[i];
  *Reg(SER) = 1;
  release(&lock);

  if (mmio_wait(Reg(RXFLR), 0x1FF, n, usec) < 0 ||
      mmio_wait(Reg(SR), SR_BUSY, 0, usec) < 0)
    r = -1;
  else
    for (int i = 0; i < n; i++)
      rx[i] = *Reg(DR);
  *Reg(SER) = 0;

  if (r < 0) {
    *Reg(SSIENR) = 0;
    *Reg(SSIENR) = 1;
  }
  return r;
}

static int
fifodepth(void)
{
  int d;

  *Reg(SSIENR) = 0;
  for (d = 1; d < MAXFIFO; d++) {
    *Reg(TXFTLR) = d;
    if (*Reg(TXFTLR) != d)
      break;
  }
  *Reg(TXFTLR) = 0;
  *Reg(SSIENR) = 1;
  return d == 1 ? 0 : d;
}

static int
setspeed(uint64 speed)
{
  if (speed == 0)
    return -1;
  div = divider(speed);
  setup();
  return 0;
}

static int
setmode(uint64 mode)
{
  if (mode > 3)
    return -1;
  ctrl = CTRLR0_DFS8;
  if (mode & 1)
    ctrl |= CTRLR0_SCPH;
  if (mode & 2)
    ctrl |= CTRLR0_SCPOL;
  setup();
  return 0;
}

static int
transfer(int user_src, struct spi_msg *m)
{
  int n;

  if (m->len <= 0)
    return -1;
  for (int done = 0; done < m->len; done += n) {
    n = m->len - done < depth ? m->len - done : depth;
    if (m->tx_buf == 0)
      memset(tx, 0, n);
    else if (either_copyin(tx, user_src, m->tx_buf + done, n) < 0)
      return -1;
    if (chunk(n) < 0)
      return -1;
    if (m->rx_buf != 0 && either_copyout(user_src, m->rx_buf + done, rx, n) < 0)
      return -1;
  }
  return m->len;
}

static int
spiioctl(int user_src, int req, uint64 arg)
{
  struct spi_msg m;
  int r;

  if (depth == 0)
    return -1;
  if (req == SPI_IOCTL_TRANSFER &&
      either_copyin(&m, user_src, arg, sizeof(m)) < 0)
    return -1;

  acquiresleep(&busy);
  switch (req) {
  case SPI_IOCTL_SET_SPEED:
    r = setspeed(arg);
    break;
  case SPI_IOCTL_SET_MODE:
    r = setmode(arg);
    break;
  case SPI_IOCTL_TRANSFER:
    r = transfer(user_src, &m);
    break;
  default:
    r = -1;
  }
  releasesleep(&busy);
  return r;
}

void
spiinit(void)
{
  initlock(&lock, "spi2");
  initsleeplock(&busy, "spi2");
  *Reg(SSIENR) = 0;
  *Reg(IMR) = 0;
  *Reg(SER) = 0;
  ctrl = CTRLR0_DFS8;
  div = divider(BOOT_HZ);
  setup();

  depth = fifodepth();
  printk("spi2: fifo depth %d\n", depth);
  if (depth == 0)
    printk("spi2: no fifo\n");
  devsw[SPI].ioctl = spiioctl;
}
