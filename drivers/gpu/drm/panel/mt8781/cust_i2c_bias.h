#ifndef __CUST_I2C_BIAS_H_
#define __CUST_I2C_BIAS_H_


extern int cust_bias_init(void);
extern void cust_bias_exit(void);
extern int cust_bias_i2c_write_bytes(unsigned char addr, unsigned char value);
#endif