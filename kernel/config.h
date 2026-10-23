
// qemu
#define CONFIG_M_MODE 1

#define UART0       0x10000000L
#define UART0_IRQ   10
#define VIRTIO0     0x10001000
#define VIRTIO0_IRQ 1
#define PLIC        0x0c000000L
#define PLIC_SIZE   0x4000000L
#define KERNBASE    0x80000000L
#define PHYSTOP     (KERNBASE + 128 * 1024 * 1024)
#define INTERVAL    1000000
