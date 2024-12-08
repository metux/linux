#ifndef __LCM_LT8911EXB_I2C_H_
#define __LCM_LT8911EXB_I2C_H_


extern void LT8911EXB_IIC_Write_byte(u8 cmd, u8 data);
extern u8 LT8911EXB_IIC_Read_byte(u8 cmd);
extern int _LT8911EXB_driver_init(void);
extern void _LT8911EXB_driver_exit(void);
#endif