#define GPIO_IOCTL_CONFIG 0
#define GPIO_IOCTL_SET    1
#define GPIO_IOCTL_GET    2
#define GPIO_IOCTL_WAIT   3

#define GPIO_IN  1
#define GPIO_OUT 2

#define GPIO_PORT_SFT 8
#define GPIO_VAL_SFT  16

#define GPIO_PIN(port, bit) (((port) << GPIO_PORT_SFT) | (bit))
#define GPIO_VAL(v)         ((v) << GPIO_VAL_SFT)
#define GPIO_MODE(m)        ((m) << GPIO_VAL_SFT)
