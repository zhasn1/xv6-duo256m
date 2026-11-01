#define SPI_IOCTL_SET_SPEED 0
#define SPI_IOCTL_SET_MODE  1
#define SPI_IOCTL_TRANSFER  2

struct spi_msg {
  int len;
  uint64 tx_buf;
  uint64 rx_buf;
};
