// SPDX-License-Identifier: GPL-2.0+
/*
 * this driver reuses code from u-boot/drivers/i2c/designware_i2c.c
 * here is the copyright
 *
 * (C) Copyright 2009
 * Vipin Kumar, ST Micoelectronics, vipin.kumar@st.com.
*/

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "defs.h"
#include "i2c.h"

#define IC_CON            0x00
#define IC_TAR            0x04
#define IC_DATA_CMD       0x10
#define IC_SS_SCL_HCNT    0x14
#define IC_SS_SCL_LCNT    0x18
#define IC_INTR_MASK      0x30
#define IC_RAW_INTR_STAT  0x34
#define IC_CLR_INTR       0x40
#define IC_CLR_TX_ABRT    0x54
#define IC_ENABLE         0x6C
#define IC_STATUS         0x70
#define IC_RXFLR          0x78
#define IC_SDA_HOLD       0x7C
#define IC_TX_ABRT_SOURCE 0x80
#define IC_ENABLE_STATUS  0x9C
#define IC_FS_SPKLEN      0xA0
#define IC_COMP_PARAM_1   0xF4
#define IC_COMP_TYPE      0xFC

#define IC_CON_SS_MASTER 0x63

#define IC_CMD_READ    (1 << 8)
#define IC_CMD_STOP    (1 << 9)
#define IC_CMD_RESTART (1 << 10)

#define IC_INTR_TX_ABRT  (1 << 6)
#define IC_INTR_STOP_DET (1 << 9)

#define IC_ENABLE_ABORT    (1 << 1)
#define IC_STATUS_ACTIVITY 1

#define ABRT_7B_ADDR_NOACK (1 << 0)
#define ABRT_TXDATA_NOACK  (1 << 3)

#define DW_COMP_TYPE 0x44570140

#define Reg(base, reg) ((volatile uint32 *)((base) + (reg)))

#define MAXFIFO     256
#define ENABLE_USEC 2500
#define XFER_USEC   10000
#define TIMER_MHZ   25


#define I2C_CLK_KHZ (I2C_CLK_HZ / 1000)
#define SS_HCNT     ((uint64)I2C_CLK_KHZ * 4300 / 1000000 - 3)
#define SS_LCNT     ((uint64)I2C_CLK_KHZ * 5000 / 1000000 - 1)
#define SDA_HOLD    ((uint64)I2C_CLK_KHZ * 300 / 1000000)
#define FS_SPKLEN   5

#define BASE I2C1

static struct {
  struct spinlock lock;
  struct sleeplock busy;
  int ok;
  int txdepth;
  int rxdepth;
  uchar wbuf[MAXFIFO];
  uchar rbuf[MAXFIFO];
} bus;

static int
enable(int on)
{
  *Reg(BASE, IC_ENABLE) = on;
  return mmio_wait(Reg(BASE, IC_ENABLE_STATUS), 1, on, ENABLE_USEC);
}

static int
abortstatus(uint32 src)
{
  if (src & ABRT_7B_ADDR_NOACK)
    return I2C_ADDR_NACK;
  if (src & ABRT_TXDATA_NOACK)
    return I2C_DATA_NACK;
  return I2C_ABORT;
}

static int
xfer(int addr, int wn, int rn)
{
  uint64 end;
  uint32 stat, c;
  int i, n = wn + rn, got = 0, r;

  if (mmio_wait(Reg(BASE, IC_STATUS), IC_STATUS_ACTIVITY, 0, XFER_USEC) < 0 ||
      enable(0) < 0)
    return I2C_TIMEOUT;
  *Reg(BASE, IC_TAR) = addr;
  (void)*Reg(BASE, IC_CLR_INTR);
  if (enable(1) < 0)
    return I2C_TIMEOUT;

  acquire(&bus.lock);
  for (i = 0; i < n; i++) {
    c = i < wn ? bus.wbuf[i] : IC_CMD_READ;
    if (i == wn && wn > 0)
      c |= IC_CMD_RESTART;
    if (i == n - 1)
      c |= IC_CMD_STOP;
    *Reg(BASE, IC_DATA_CMD) = c;
  }
  release(&bus.lock);

  end = r_time() + (uint64)XFER_USEC * TIMER_MHZ;
  for (;;) {
    stat = *Reg(BASE, IC_RAW_INTR_STAT);
    while (*Reg(BASE, IC_RXFLR) > 0) {
      c = *Reg(BASE, IC_DATA_CMD);
      if (got < rn)
        bus.rbuf[got++] = c;
    }
    if (stat & (IC_INTR_TX_ABRT | IC_INTR_STOP_DET))
      break;
    if (r_time() > end)
      break;
  }

  if (stat & IC_INTR_TX_ABRT) {
    r = abortstatus(*Reg(BASE, IC_TX_ABRT_SOURCE));
    (void)*Reg(BASE, IC_CLR_TX_ABRT);
  } else if (!(stat & IC_INTR_STOP_DET)) {
    *Reg(BASE, IC_ENABLE) = 1 | IC_ENABLE_ABORT;
    mmio_wait(Reg(BASE, IC_ENABLE), IC_ENABLE_ABORT, 0, ENABLE_USEC);
    r = I2C_TIMEOUT;
  } else if (got != rn) {
    r = I2C_ABORT;
  } else {
    r = I2C_OK;
  }

  (void)*Reg(BASE, IC_CLR_INTR);
  if (enable(0) < 0 && r == I2C_OK)
    r = I2C_TIMEOUT;
  return r;
}

static int
i2cioctl(int user_src, int req, uint64 arg)
{
  struct i2c_msg m;
  int wn, rn, r = 0;

  if ((req != I2C_IOCTL_READ && req != I2C_IOCTL_WRITE) || !bus.ok ||
      either_copyin(&m, user_src, arg, sizeof(m)) < 0)
    return -1;
  if (m.addr < 0 || m.addr > 0x7F || m.reg < -1 || m.reg > 0xFF ||
      m.len < 0 || m.len > bus.txdepth)
    return -1;
  wn = m.reg >= 0;
  rn = 0;
  if (req == I2C_IOCTL_READ) {
    if (m.len == 0 || m.len > bus.rxdepth)
      return -1;
    rn = m.len;
  } else {
    wn += m.len;
  }
  if (wn + rn == 0 || wn + rn > bus.txdepth)
    return -1;

  acquiresleep(&bus.busy);
  if (m.reg >= 0)
    bus.wbuf[0] = m.reg;
  if (req == I2C_IOCTL_WRITE && m.len > 0 &&
      either_copyin(bus.wbuf + (m.reg >= 0), user_src, m.buf, m.len) < 0)
    r = -1;
  if (r == 0) {
    r = xfer(m.addr, wn, rn);
    if (r == I2C_OK && rn > 0 &&
        either_copyout(user_src, m.buf, bus.rbuf, rn) < 0)
      r = -1;
  }
  releasesleep(&bus.busy);
  return r;
}

void
i2cinit(void)
{
  uint32 param;

  initlock(&bus.lock, "i2c1");
  initsleeplock(&bus.busy, "i2c1");
  devsw[I2C].ioctl = i2cioctl;
  if (*Reg(BASE, IC_COMP_TYPE) != DW_COMP_TYPE) {
    printk("i2c1: comp type %x, unusable\n", *Reg(BASE, IC_COMP_TYPE));
    return;
  }
  if (enable(0) < 0) {
    printk("i2c1: does not disable, unusable\n");
    return;
  }

  *Reg(BASE, IC_CON) = IC_CON_SS_MASTER;
  *Reg(BASE, IC_SS_SCL_HCNT) = SS_HCNT;
  *Reg(BASE, IC_SS_SCL_LCNT) = SS_LCNT;
  *Reg(BASE, IC_SDA_HOLD) = SDA_HOLD;
  *Reg(BASE, IC_FS_SPKLEN) = FS_SPKLEN;
  *Reg(BASE, IC_INTR_MASK) = 0; // polled

  param = *Reg(BASE, IC_COMP_PARAM_1);
  bus.txdepth = ((param >> 16) & 0xFF) + 1;
  bus.rxdepth = ((param >> 8) & 0xFF) + 1;
  printk("i2c1: fifo tx %d rx %d\n", bus.txdepth, bus.rxdepth);
  bus.ok = 1;
}
