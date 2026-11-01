#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/i2c.h"
#include "user/user.h"

#define FIRST  0x08
#define LAST   0x77
#define MAXBUF 64

static char hex[] = "0123456789abcdef";
static char *errs[] = {
  "ok", "address nack", "data nack", "abort", "timeout",
};
static int fd;

static int
number(char *s, int max)
{
  int base = 10, v = 0, d;

  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    base = 16;
    s += 2;
  }
  if (*s == 0) {
    fprintf(2,
            "usage: i2c [scan]|[get addr reg [n]] | [set addr reg val...]\n");
    exit(1);
  }
  for (; *s; s++) {
    if (*s >= '0' && *s <= '9')
      d = *s - '0';
    else if (*s >= 'a' && *s <= 'f')
      d = *s - 'a' + 10;
    else if (*s >= 'A' && *s <= 'F')
      d = *s - 'A' + 10;
    else
      d = base;
    if (d >= base) {
      fprintf(2,
              "usage: i2c [scan]|[get addr reg [n]] | [set addr reg val...]\n");
      exit(1);
    }
    v = v * base + d;
    if (v > max) {
      fprintf(2,
              "usage: i2c [scan]|[get addr reg [n]] | [set addr reg val...]\n");
      exit(1);
    }
  }
  return v;
}

static int
io(int req, int addr, int reg, uchar *buf, int len)
{
  struct i2c_msg m;

  m.addr = addr;
  m.reg = reg;
  m.len = len;
  m.buf = (uint64)buf;
  return ioctl(fd, req, (uint64)&m);
}

static void
check(int r)
{
  if (r == I2C_OK)
    return;
  if (r < 0 || r > I2C_TIMEOUT)
    fprintf(2, "i2c: ioctl failed\n");
  else
    fprintf(2, "i2c: %s\n", errs[r]);
  exit(1);
}

static void
scan(void)
{
  uchar byte;
  int addr, r, errors = 0;

  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
  for (int row = 0; row < 0x80; row += 16) {
    printf("%c0:", hex[row >> 4]);
    for (int col = 0; col < 16; col++) {
      addr = row + col;
      if (addr < FIRST || addr > LAST) {
        printf("   ");
        continue;
      }
      r = io(I2C_IOCTL_READ, addr, -1, &byte, 1);
      if (r == I2C_OK) {
        printf(" %c%c", hex[addr >> 4], hex[addr & 0xF]);
      } else if (r == I2C_ADDR_NACK) {
        printf(" --");
      } else if (r == I2C_TIMEOUT) {
        printf("\ni2c: timeout at 0x%x: SCL held low? check wiring "
               "and pull-ups\n",
               addr);
        exit(1);
      } else if (r < 0) {
        printf("\n");
        check(r);
      } else {
        printf(" ER");
        errors++;
      }
    }
    printf("\n");
  }
  if (errors)
    printf("i2c: %d addresses gave errors other than NACK\n", errors);
}

int
main(int argc, char *argv[])
{
  uchar buf[MAXBUF];
  int addr, reg, n;

  if (argc < 2) {
    fprintf(2,
            "usage: i2c [scan]|[get addr reg [n]] | [set addr reg val...]\n");
    exit(1);
  }
  if ((fd = open("ui2c", O_RDWR)) < 0) {
    fprintf(2, "i2c: cannot open ui2c\n");
    exit(1);
  }

  if (strcmp(argv[1], "scan") == 0 && argc == 2) {
    scan();
  } else if (strcmp(argv[1], "get") == 0 && (argc == 4 || argc == 5)) {
    addr = number(argv[2], 0x7F);
    reg = number(argv[3], 0xFF);
    n = argc == 5 ? number(argv[4], MAXBUF) : 1;
    if (n == 0) {
      fprintf(2,
              "usage: i2c [scan]|[get addr reg [n]] | [set addr reg val...]\n");
      exit(1);
    }
    check(io(I2C_IOCTL_READ, addr, reg, buf, n));
    for (int i = 0; i < n; i++)
      printf("%s0x%c%c", i ? " " : "", hex[buf[i] >> 4], hex[buf[i] & 0xF]);
    printf("\n");
  } else if (strcmp(argv[1], "set") == 0 && argc >= 5 &&
             argc - 4 <= MAXBUF - 1) {
    addr = number(argv[2], 0x7F);
    reg = number(argv[3], 0xFF);
    n = argc - 4;
    for (int i = 0; i < n; i++)
      buf[i] = number(argv[4 + i], 0xFF);
    check(io(I2C_IOCTL_WRITE, addr, reg, buf, n));
  } else {
    fprintf(2,
            "usage: i2c [scan]|[get addr reg [n]] | [set addr reg val...]\n");
    exit(1);
  }
  exit(0);
}
