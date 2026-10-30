#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/gpio.h"
#include "user/user.h"

#define LED GPIO_PIN(4, 2)

int
main(int argc, char *argv[])
{
  int fd, n = 5;

  if (argc > 1)
    n = atoi(argv[1]);
  if ((fd = open("gpio", O_RDWR)) < 0) {
    fprintf(2, "blink: cannot open gpio\n");
    exit(1);
  }
  if (ioctl(fd, GPIO_IOCTL_CONFIG, LED | GPIO_MODE(GPIO_OUT)) < 0) {
    fprintf(2, "blink: gpio config failed\n");
    exit(1);
  }

  for (int i = 0; i < n; i++) {
    ioctl(fd, GPIO_IOCTL_SET, LED | GPIO_VAL(1));
    pause(50);
    ioctl(fd, GPIO_IOCTL_SET, LED | GPIO_VAL(0));
    pause(50);
  }
  exit(0);
}
