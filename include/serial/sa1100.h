#ifndef __LINUX_SERIAL_SA1100_H
#define __LINUX_SERIAL_SA1100_H

/*
 * definitions for the SA1100 SoC family serial devices
 */

// completely insane hack
// arch/arm/mach-sa1100/include/mach/SA-1100.h

#define SERIAL_SA1100_UTCR0(Nb)      __REG(0x80010000 + ((Nb) - 1)*0x00020000)  /* UART Control Reg. 0 [1..3] */
#define SERIAL_SA1100_UTCR1(Nb)      __REG(0x80010004 + ((Nb) - 1)*0x00020000)  /* UART Control Reg. 1 [1..3] */
#define SERIAL_SA1100_UTCR2(Nb)      __REG(0x80010008 + ((Nb) - 1)*0x00020000)  /* UART Control Reg. 2 [1..3] */
#define SERIAL_SA1100_UTCR3(Nb)      __REG(0x8001000C + ((Nb) - 1)*0x00020000)  /* UART Control Reg. 3 [1..3] */
#define SERIAL_SA1100_UTCR4(Nb)      __REG(0x80010010 + ((Nb) - 1)*0x00020000)  /* UART Control Reg. 4 [2] */
#define SERIAL_SA1100_UTDR(Nb)       __REG(0x80010014 + ((Nb) - 1)*0x00020000)  /* UART Data Reg. [1..3] */
#define SERIAL_SA1100_UTSR0(Nb)      __REG(0x8001001C + ((Nb) - 1)*0x00020000)  /* UART Status Reg. 0 [1..3] */
#define SERIAL_SA1100_UTSR1(Nb)      __REG(0x80010020 + ((Nb) - 1)*0x00020000)  /* UART Status Reg. 1 [1..3] */

#endif /* __LINUX_SERIAL_SA1100_H */
