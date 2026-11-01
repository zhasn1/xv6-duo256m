#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/spi.h"
#include "user/user.h"

#define SPEED 1000000
#define MODE  0
#define LEN   ((int)sizeof(tx))

static uchar tx[32] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x40, 0x00, 0x00, 0x00, 0x00,
  0x95, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0x0D,
};
static uchar rx[32];

static void
dump(char *label, uchar *buf, int n)
{
  static char hex[] = "0123456789ABCDEF";

  printf("%s |", label);
  for (int i = 0; i < n; i++)
    printf(" %c%c", hex[buf[i] >> 4], hex[buf[i] & 0xF]);
  printf("\n");
}

int
main(void)
{
  struct spi_msg m = {
    .len = LEN,
    .tx_buf = (uint64)tx,
    .rx_buf = (uint64)rx,
  };
  int fd;

  if ((fd = open("uspi", O_RDWR)) < 0) {
    fprintf(2, "spi: cannot open uspi\n");
    exit(1);
  }
  if (ioctl(fd, SPI_IOCTL_SET_SPEED, SPEED) < 0) {
    fprintf(2, "spi: set speed failed\n");
    exit(1);
  }
  if (ioctl(fd, SPI_IOCTL_SET_MODE, MODE) < 0) {
    fprintf(2, "spi: set mode failed\n");
    exit(1);
  }
  if (ioctl(fd, SPI_IOCTL_TRANSFER, (uint64)&m) != LEN) {
    fprintf(2, "spi: transfer failed\n");
    exit(1);
  }

  dump("TX", tx, LEN);
  dump("RX", rx, LEN);
  exit(0);
}
