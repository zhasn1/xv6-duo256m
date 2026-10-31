#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/pwm.h"
#include "user/user.h"

#define CHIP 1
#define CHAN 2

int
main(int argc, char *argv[])
{
  struct pwm_config c;
  int fd;

  if (argc != 3) {
    fprintf(2, "usage: pwm period_ns duty_ns\n");
    exit(1);
  }
  if ((fd = open("upwm", O_RDWR)) < 0) {
    fprintf(2, "pwm: cannot open upwm\n");
    exit(1);
  }
  c.period = atoi(argv[1]);
  c.duty = atoi(argv[2]);
  c.chip = CHIP;
  c.chan = CHAN;
  c.enable = 1;
  if (ioctl(fd, PWM_IOCTL_CONFIG, (uint64)&c) < 0) {
    fprintf(2, "pwm: config failed\n");
    exit(1);
  }
  exit(0);
}
