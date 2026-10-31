#define PWM_IOCTL_CONFIG 0

struct pwm_config {
  uint64 period;
  uint64 duty;
  int chip;
  int chan;
  int enable;
};
