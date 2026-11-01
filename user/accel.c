#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/i2c.h"
#include "user/user.h"

#define ADDR 0x1F

#define XOUT_L    0x08
#define COTR      0x12
#define WHO_AM_I  0x13
#define CNTL1     0x1B
#define CNTL1_PC1 0x80
#define CNTL1_RES 0x40

#define WHO_AM_I_KX132 0x3D
#define COTR_OK        0x55
#define COUNTS_PER_G   16384

static int fd;

static char *errs[] = {
  "ok", "address nack", "data nack", "abort", "timeout",
};

static void
check(int r, char *what)
{
  if (r == I2C_OK)
    return;
  if (r < 0 || r > I2C_TIMEOUT)
    fprintf(2, "accel: %s: ioctl failed\n", what);
  else
    fprintf(2, "accel: %s: %s\n", what, errs[r]);
  exit(1);
}

static int
io(int req, int reg, uchar *buf, int len)
{
  struct i2c_msg m;

  m.addr = ADDR;
  m.reg = reg;
  m.len = len;
  m.buf = (uint64)buf;
  return ioctl(fd, req, (uint64)&m);
}

static void
readreg(int reg, uchar *buf, int n, char *what)
{
  check(io(I2C_IOCTL_READ, reg, buf, n), what);
}

static void
writereg(int reg, int val, char *what)
{
  uchar v = val;

  check(io(I2C_IOCTL_WRITE, reg, &v, 1), what);
}

static void
expect(int reg, int want, char *name)
{
  uchar v;

  readreg(reg, &v, 1, name);
  printf("accel: %s 0x%x\n", name, v);
  if (v != want) {
    printf("accel: expected 0x%x\n", want);
    exit(1);
  }
}

int
main(int argc, char *argv[])
{
  uchar d[6];
  int mg[3], count;

  count = argc > 1 ? atoi(argv[1]) : -1;
  if ((fd = open("ui2c", O_RDWR)) < 0) {
    fprintf(2, "accel: cannot open ui2c\n");
    exit(1);
  }

  expect(WHO_AM_I, WHO_AM_I_KX132, "WHO_AM_I");
  expect(COTR, COTR_OK, "COTR");

  writereg(CNTL1, 0, "standby");
  writereg(CNTL1, CNTL1_PC1 | CNTL1_RES, "operate");
  pause(5);

  for (int i = 0; count < 0 || i < count; i++) {
    readreg(XOUT_L, d, sizeof(d), "read xyz");
    for (int k = 0; k < 3; k++)
      mg[k] = (short)(d[2 * k] | d[2 * k + 1] << 8) * 1000 / COUNTS_PER_G;
    printf("x %d y %d z %d mg\n", mg[0], mg[1], mg[2]);
    pause(20);
  }
  exit(0);
}
