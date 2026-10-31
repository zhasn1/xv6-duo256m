#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/gpio.h"
#include "user/user.h"

#define BTN GPIO_PIN(0, 15)
#define LED GPIO_PIN(4, 2)

int
main(int argc, char *argv[])
{
  int fd, n;

  if ((fd = open("ugpio", O_RDWR)) < 0) {
    fprintf(2, "button: cannot open ugpio\n");
    exit(1);
  }
  if (ioctl(fd, GPIO_IOCTL_CONFIG, BTN | GPIO_MODE(GPIO_IN)) < 0 ||
      ioctl(fd, GPIO_IOCTL_CONFIG, LED | GPIO_MODE(GPIO_OUT)) < 0) {
    fprintf(2, "button: gpio config failed\n");
    exit(1);
  }
  printf("button: level %d\n", ioctl(fd, GPIO_IOCTL_GET, BTN));

  for (n = 1;; n++) {
    if (ioctl(fd, GPIO_IOCTL_WAIT, BTN | GPIO_VAL(0)) < 0) {
      fprintf(2, "button: wait failed\n");
      exit(1);
    }
    ioctl(fd, GPIO_IOCTL_SET, LED | GPIO_VAL(n % 2));
    printf("press %d\n", n);
  }
}
