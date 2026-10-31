#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/pwm.h"
#include "user/user.h"

#define CHIP 1
#define CHAN 2

#define PERIOD 1000000
#define NSTEP  ((int)(sizeof(curve) / sizeof(curve[0])))

static int curve[32] = {
  0,   1,   2,   6,   11,  18,  27,  38,  51,  66,   83,
  102, 124, 148, 174, 202, 233, 267, 302, 341, 381,  425,
  470, 519, 569, 623, 679, 738, 799, 864, 930, 1000,
};

static int fd;

static void
set(int permille)
{
  struct pwm_config c;

  c.period = PERIOD;
  c.duty = permille * (PERIOD / 1000);
  c.chip = CHIP;
  c.chan = CHAN;
  c.enable = 1;
  if (ioctl(fd, PWM_IOCTL_CONFIG, (uint64)&c) < 0) {
    fprintf(2, "breathe: config failed\n");
    exit(1);
  }
}

int
main(int argc, char *argv[])
{
  int n = 5;

  if (argc > 1)
    n = atoi(argv[1]);
  if ((fd = open("upwm", O_RDWR)) < 0) {
    fprintf(2, "breathe: cannot open upwm\n");
    exit(1);
  }

  for (int b = 0; b < n; b++) {
    for (int i = 0; i < NSTEP; i++) {
      set(curve[i]);
      pause(4);
    }
    for (int i = NSTEP - 2; i > 0; i--) {
      set(curve[i]);
      pause(4);
    }
  }
  set(0);
  exit(0);
}
