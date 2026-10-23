#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

//
// the riscv Platform Level Interrupt Controller (PLIC).
//

void
plicinit(void)
{
  // set desired IRQ priorities non-zero (otherwise disabled).
  *(volatile uint32 *)(PLIC + UART0_IRQ * 4) = 1;
#ifndef DUO
  *(volatile uint32 *)(PLIC + VIRTIO0_IRQ * 4) = 1;
#endif
}

void
plicinithart(void)
{
  int hart = cpuid();

#ifdef DUO
  // IRQs 32-63 are in the second enable word (0x70002084 for hart 0).
  // This assignment replaces the whole word: when adding IRQ 36 (SD0) or
  // 50-58, set all bits in one write or read-modify-write.
  *(volatile uint32 *)(PLIC_SENABLE(hart) + 4) = 1 << (UART0_IRQ - 32);
#else
  // set enable bits for this hart's S-mode
  // for the uart and virtio disk.
  *(volatile uint32 *)PLIC_SENABLE(hart) =
    (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);
#endif

  // set this hart's S-mode priority threshold to 0.
  *(volatile uint32 *)PLIC_SPRIORITY(hart) = 0;
}

// ask the PLIC what interrupt we should serve.
int
plic_claim(void)
{
  int hart = cpuid();
  int irq = *(volatile uint32 *)PLIC_SCLAIM(hart);
  return irq;
}

// tell the PLIC we've served this IRQ.
void
plic_complete(int irq)
{
  int hart = cpuid();
  *(volatile uint32 *)PLIC_SCLAIM(hart) = irq;
}
