#define I2C_IOCTL_READ  0
#define I2C_IOCTL_WRITE 1
#define I2C_OK         0
#define I2C_ADDR_NACK  1
#define I2C_DATA_NACK  2
#define I2C_ABORT      3
#define I2C_TIMEOUT    4

struct i2c_msg {
  int addr;
  int reg;
  int len;
  uint64 buf;
};
