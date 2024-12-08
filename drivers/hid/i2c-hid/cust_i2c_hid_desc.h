#ifndef __CUST_I2C_HID_DESC_H_
#define __CUST_I2C_HID_DESC_H_

extern struct i2c_hid_desc *i2c_hid_get_cust_i2c_hid_desc_override(const char *i2c_name);
extern char *i2c_hid_get_cust_hid_report_desc_override(const char *i2c_name, unsigned int *size);
#endif