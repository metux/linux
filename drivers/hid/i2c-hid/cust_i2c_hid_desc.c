#include <linux/string.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include "i2c-hid.h"
#include "cust_i2c_hid_desc.h"

#define Logical_X_Max		1500
#define Logical_Y_Max		900
#define Physical_X_Max		1030
#define Physical_Y_Max		585


struct i2c_hid_desc_override {
	union {
		struct i2c_hid_desc *i2c_hid_desc;
		uint8_t             *i2c_hid_desc_buffer;
	};
	uint8_t              *hid_report_desc;
	unsigned int          hid_report_desc_size;
	uint8_t              *i2c_name;
};

struct cust_i2c_hid_desc_id {
	char *name;
	void *override;
};

static const struct i2c_hid_desc_override elandev_desc = {
	.i2c_hid_desc_buffer = (uint8_t [])
	{0x1E,0x00,                  /* Length of descriptor                 */
	 0x00,0x01,                  /* Version of descriptor                */
	 0x89,0x02,                  /* Length of report descriptor          */
	 0x21,0x00,                  /* Location of report descriptor        */
	 0x24,0x00,                  /* Location of input report             */
	 0x20,0x00,                  /* Max input report length              */
	 0x25,0x00,                  /* Location of output report            */
	 0x11,0x00,                  /* Max output report length             */
	 0x22,0x00,                  /* Location of command register         */
	 0x23,0x00,                  /* Location of data register            */
	 0x11,0x09,                  /* Vendor ID                            */
	 0x88,0x52,                  /* Product ID                           */
	 0x06,0x00,                  /* Version ID                           */
	 0x00,0x00, 0x00, 0x00       /* Reserved                             */
	},

	.hid_report_desc = (uint8_t [])
	{0x05,0x01,					// USAGE PAGE(GENERIC DESKTOP)
	 0x09,0x06,					// USAGE(KEYBOARD)
	 0xA1,0x01,					// COLLECTION(APPICATION)
	 0x85,0x08,					//Report ID = 0x08
	 0x05,0x07,					// USAGE PAGES(KEY CODES)
	 0x19,0xE0,					// USAGE MINIMUM(224)
	 0x29,0xE7,					// USAGE MAXIMUM(231)
	 0x15,0x00,					// LOGICAL MINIMUM(0)
	 0x25,0x01,					// LOGICAL MAXIMUM(1)
	 0x75,0x01,					// REPORT SIZE(1)
	 0x95,0x08,					// REPORT COUNT(8)
	 0x81,0x02,					// INPUT(DATA,VARIABLE,ABSOLUTE)
	 0x95,0x01,					// REPORT COUNT(1)
	 0x75,0x08,					// REPORT SIZE(8)
	 0x81,0x01,					// INPUT(CONSTANT)
	 
	 0x95,0x03,					// REPORT COUNT(3)
	 0x75,0x01,					// REPORT SIZE (1)
	 0x05,0x08,					// USAGE PAGES(PAGE# FOR LEDS)
	 0x19,0x01,					// USAGE MINIMUM(1)
	 0x29,0x03,					// USAGE MAXIMUM(3)
	 0x91,0x02,					// OUTPUT(DATA,VARIABLE,ABSOLUTE)
	 0x95,0x05,					// REPORT COUNT (5)
	 0x75,0x01,					// REPORT SIZE (1)
	 0x91,0x01,					// OUTPUT (CONSTANT)
	 
	 0x95,0x06,					// REPORT COUNT (6)
	 0x75,0x08,					// REPORT SIZE (8)
	 0x15,0x00,					// LOGICAL MINIMUM(0)
	 0x26,0xFF,0x00,				// LOGICAL MAXIMUM(255)
	 0x05,0x07,					// USAGE PAGE(KEY CODES)
	 0x19,0x00,					// USAGE MINIMUM(0)
	 0x2A,0xFF,0x00,				// USAGE MAXIMUM(255)
	 0x81,0x00,					// INPUT(DATA,ARRAY)
	 0xC0,						// END COLLECTION
	 
	 0x06,0x01,0x00,			//Usage Page (Generic Desktop Control)
	 0x09,0x80,			    //Usage (SYSTEM CONTROL)
	 0xA1,0x01,	            //Collection (Application)	
	 0x85,0x09,		    	//Report ID (ACPI) ID = 0x09
	 0x25,0x01,			    //	Logical Maximum (1)
	 0x15,0x00,			    //	Logical Minimum (0)
	 0x75,0x01,			    //	Report Size
	 0x0A,0x81,0x00,	    	//	USAGE SYSTEM POWER DOWN		98
	 0x0A,0x82,0x00,	    	//	USAGE SYSTEM SLEEP			99
	 0x0A,0x83,0x00,		    //	USAGE SYSTEM WAKE UP		9a
	 0x95,0x03,			    //	REPORT_COUNT (03H)
	 0x81,0x02,			    //	INPUT (DATA, VAR)                                
	 0x95,0x05,			    //	Report Count (05)
	 0x81,0x01,			    //	Input (CONSTANT)
	 0xC0,				    //	END COLLECTION
	 
	 0x06,0x0C,0x00, 		//	USAGE PAGE (CONSUMER PAGE) 	
	 0x09,0x01,			    //	USAGE (CONSUMER CONTROL)
	 0xA1,0x01,			    //	  COLLECTION (APPLICATION)
	 0x85,0x0a,				//	REPORT_ID (CONSUMER) ID = 0x0a
	 0x19,0x00,		// USAGE MINIMUM(1)
	 0x2a,0x3c,0x02,		// USAGE MAXIMUM(5)
	 0x15,0x00,		//Logical Minimum (0)
	 0x26,0x3c,0x02,		 //Logical Maximum (1)
	 0x95,0x01,			    //	Report Count (8)
	 0x75,0x10,			    //	REPORT_SIZE(1)
	 0x81,0x00,			    //	Input (Data, Variable)
	 0xC0,				    //	  END COLLECTION
	 
	 0x05,0x0D,	//Usage page(Digitizers)----->开始定义PTP
	 0x09,0x05,	//Usage(Touch Pad)
	 0xA1,0x01,	//Collection(Application)
	 0x85,0x04,	//REPORT_ID(4)
	 0x05,0x0D,	//Usage Page(Digitizers)
	 0x09,0x22,	//Usage(Finger)-----------定义第1个手指
	 0xA1,0x02,	//Collection(Logical)
	 0x15,0x00,	//Logical Minimum(0)
	 0x25,0x01,	//Logical Maximum(1)
	 0x09,0x47,	//Usage(Confidence)-手指大小
	 0x09,0x42,	//Usage(Tip switch)-手指是否在TP上
	 0x95,0x02,	//Report Count(2)
	 0x75,0x01,	//Report Size(1)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x95,0x01,	//Report Count(1)
	 0x75,0x03,	//Report Size(3)
	 0x25,0x05,	//Logical Maximum(5)
	 0x09,0x51,	//Usage(Contact Identifier)--指示第几个手指
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x75,0x01,	//Report Size(1)
	 0x95,0x03,	//Report Count(3)
	 0x81,0x03,	//Input(Cnst,Var,Abs)
	 0x05,0x01,	//Usage Page(Generic Desktop)
	 0x15,0x00,	//Logical Minimum(0)
	 0x26,Logical_X_Max%256, Logical_X_Max/256,	//Logical Maximum()报告能上传X的坐标最大值=Trace*Pos
	 0x75,0x10,	//Report Size(16bit)
	 0x55,0x0E,	//Unit Expnet(-2)-单位幂指数
	 0x65,0x13,	//Unit(Inch,EngLinear)-单位英寸
	 0x09,0x30,	//Usage(X)
	 0x35,0x00,	//Physical Minimum(0)-最小物理值
	 0x46,Physical_X_Max%256, Physical_X_Max/256,	//Physical Maximum()-物理最大值，X实际长度，注意转换为英寸
	 0x95,0x01,	//Report Count(1)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x46,Physical_Y_Max%256, Physical_Y_Max/256,	//Physical Maximum()-物理最大值，Y实际长度，注意转换为英寸
	 0x26,Logical_Y_Max%256, Logical_Y_Max/256,	//Logical Maximum()报告能上传Y的坐标最大值=Trace*Pos
	 0x09,0x31,	//Usage(Y)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0xC0,	//End Collection
	 
	 0x05,0x0D,	//Usage page(Digitizers)
	 0x09,0x22,	//Usage(Finger)-----------定义第2个手指
	 0xA1,0x02,	//Collection(Logical)
	 0x15,0x00,	//Logical Minimum(0)
	 0x25,0x01,	//Logical Maximum(1)
	 0x09,0x47,	//Usage(Confidence)-手指大小
	 0x09,0x42,	//Usage(Tip switch)-手指是否在TP上
	 0x95,0x02,	//Report Count(2)
	 0x75,0x01,	//Report Size(1)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x95,0x01,	//Report Count(1)
	 0x75,0x03,	//Report Size(3)
	 0x25,0x05,	//Logical Maximum(5)
	 0x09,0x51,	//Usage(Contact Identifier)--指示第几个手指
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x75,0x01,	//Report Size(1)
	 0x95,0x03,	//Report Count(3)
	 0x81,0x03,	//Input(Cnst,Var,Abs)
	 0x05,0x01,	//Usage Page(Generic Desktop)
	 0x15,0x00,	//Logical Minimum(0)
	 0x26,Logical_X_Max%256, Logical_X_Max/256,	//Logical Maximum()报告能上传X的坐标最大值=Trace*Pos
	 0x75,0x10,	//Report Size(16bit)
	 0x55,0x0E,	//Unit Expnet(-2)-单位幂指数
	 0x65,0x13,	//Unit(Inch,EngLinear)-单位英寸
	 0x09,0x30,	//Usage(X)
	 0x35,0x00,	//Physical Minimum(0)-最小物理值
	 0x46,Physical_X_Max%256, Physical_X_Max/256,	//Physical Maximum()-物理最大值，X实际长度，注意转换为英寸
	 0x95,0x01,	//Report Count(1)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x46,Physical_Y_Max%256, Physical_Y_Max/256,	//Physical Maximum()-物理最大值，Y实际长度，注意转换为英寸
	 0x26,Logical_Y_Max%256, Logical_Y_Max/256,	//Logical Maximum()报告能上传Y的坐标最大值=Trace*Pos
	 0x09,0x31,	//Usage(Y)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0xC0,	//End Collection
	 
	 0x05,0x0D,	//Usage page(Digitizers)
	 0x09,0x22,	//Usage(Finger)-----------定义第3个手指
	 0xA1,0x02,	//Collection(Logical)
	 0x15,0x00,	//Logical Minimum(0)
	 0x25,0x01,	//Logical Maximum(1)
	 0x09,0x47,	//Usage(Confidence)-手指大小
	 0x09,0x42,	//Usage(Tip switch)-手指是否在TP上
	 0x95,0x02,	//Report Count(2)
	 0x75,0x01,	//Report Size(1)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x95,0x01,	//Report Count(1)
	 0x75,0x03,	//Report Size(3)
	 0x25,0x05,	//Logical Maximum(5)
	 0x09,0x51,	//Usage(Contact Identifier)--指示第几个手指
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x75,0x01,	//Report Size(1)
	 0x95,0x03,	//Report Count(3)
	 0x81,0x03,	//Input(Cnst,Var,Abs)
	 0x05,0x01,	//Usage Page(Generic Desktop)
	 0x15,0x00,	//Logical Minimum(0)
	 0x26,Logical_X_Max%256, Logical_X_Max/256,	//Logical Maximum()报告能上传X的坐标最大值=Trace*Pos
	 0x75,0x10,	//Report Size(16bit)
	 0x55,0x0E,	//Unit Expnet(-2)-单位幂指数
	 0x65,0x13,	//Unit(Inch,EngLinear)-单位英寸
	 0x09,0x30,	//Usage(X)
	 0x35,0x00,	//Physical Minimum(0)-最小物理值
	 0x46,Physical_X_Max%256, Physical_X_Max/256,	//Physical Maximum()-物理最大值，X实际长度，注意转换为英寸
	 0x95,0x01,	//Report Count(1)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x46,Physical_Y_Max%256, Physical_Y_Max/256,	//Physical Maximum()-物理最大值，Y实际长度，注意转换为英寸
	 0x26,Logical_Y_Max%256, Logical_Y_Max/256,	//Logical Maximum()报告能上传Y的坐标最大值=Trace*Pos
	 0x09,0x31,	//Usage(Y)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0xC0,	//End Collection
	 
	 0x05,0x0D,	//Usage page(Digitizers)
	 0x09,0x22,	//Usage(Finger)-----------定义第4个手指
	 0xA1,0x02,	//Collection(Logical)
	 0x15,0x00,	//Logical Minimum(0)
	 0x25,0x01,	//Logical Maximum(1)
	 0x09,0x47,	//Usage(Confidence)-手指大小
	 0x09,0x42,	//Usage(Tip switch)-手指是否在TP上
	 0x95,0x02,	//Report Count(2)
	 0x75,0x01,	//Report Size(1)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x95,0x01,	//Report Count(1)
	 0x75,0x03,	//Report Size(3)
	 0x25,0x05,	//Logical Maximum(5)
	 0x09,0x51,	//Usage(Contact Identifier)--指示第几个手指
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x75,0x01,	//Report Size(1)
	 0x95,0x03,	//Report Count(3)
	 0x81,0x03,	//Input(Cnst,Var,Abs)
	 0x05,0x01,	//Usage Page(Generic Desktop)
	 0x15,0x00,	//Logical Minimum(0)
	 0x26,Logical_X_Max%256, Logical_X_Max/256,	//Logical Maximum()报告能上传X的坐标最大值=Trace*Pos
	 0x75,0x10,	//Report Size(16bit)
	 0x55,0x0E,	//Unit Expnet(-2)-单位幂指数
	 0x65,0x13,	//Unit(Inch,EngLinear)-单位英寸
	 0x09,0x30,	//Usage(X)
	 0x35,0x00,	//Physical Minimum(0)-最小物理值
	 0x46,Physical_X_Max%256, Physical_X_Max/256,	//Physical Maximum()-物理最大值，X实际长度，注意转换为英寸
	 0x95,0x01,	//Report Count(1)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x46,Physical_Y_Max%256, Physical_Y_Max/256,	//Physical Maximum()-物理最大值，Y实际长度，注意转换为英寸
	 0x26,Logical_Y_Max%256, Logical_Y_Max/256,	//Logical Maximum()报告能上传Y的坐标最大值=Trace*Pos
	 0x09,0x31,	//Usage(Y)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0xC0,	//End Collection
	 
	 0x05,0x0D,	//Usage page(Digitizers)
	 0x09,0x22,	//Usage(Finger)-----------定义第5个手指
	 0xA1,0x02,	//Collection(Logical)
	 0x15,0x00,	//Logical Minimum(0)
	 0x25,0x01,	//Logical Maximum(1)
	 0x09,0x47,	//Usage(Confidence)-手指大小
	 0x09,0x42,	//Usage(Tip switch)-手指是否在TP上
	 0x95,0x02,	//Report Count(2)
	 0x75,0x01,	//Report Size(1)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x95,0x01,	//Report Count(1)
	 0x75,0x03,	//Report Size(3)
	 0x25,0x05,	//Logical Maximum(5)
	 0x09,0x51,	//Usage(Contact Identifier)--指示第几个手指
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x75,0x01,	//Report Size(1)
	 0x95,0x03,	//Report Count(3)
	 0x81,0x03,	//Input(Cnst,Var,Abs)
	 0x05,0x01,	//Usage Page(Generic Desktop)
	 0x15,0x00,	//Logical Minimum(0)
	 0x26,Logical_X_Max%256, Logical_X_Max/256,	//Logical Maximum()报告能上传X的坐标最大值=Trace*Pos
	 0x75,0x10,	//Report Size(16bit)
	 0x55,0x0E,	//Unit Expnet(-2)-单位幂指数
	 0x65,0x13,	//Unit(Inch,EngLinear)-单位英寸
	 0x09,0x30,	//Usage(X)
	 0x35,0x00,	//Physical Minimum(0)-最小物理值
	 0x46,Physical_X_Max%256, Physical_X_Max/256,	//Physical Maximum()-物理最大值，X实际长度，注意转换为英寸
	 0x95,0x01,	//Report Count(1)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x46,Physical_Y_Max%256, Physical_Y_Max/256,	//Physical Maximum()-物理最大值，Y实际长度，注意转换为英寸
	 0x26,Logical_Y_Max%256, Logical_Y_Max/256,	//Logical Maximum()报告能上传Y的坐标最大值=Trace*Pos
	 0x09,0x31,	//Usage(Y)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0xC0,	//End Collection
	 
	 0x05,0x0D,	//Usage page(Digitizers)
	 0x55,0x0C,	//Unit Expnet(-4)-单位幂指数
	 0x66,0x01,0x10,	//Unit(Seconds)单位秒,实际单位=1*10-4=100us
	 0x47,0xFF,0xFF,0x00,0x00,	//Physical Maximum(65535)
	 0x27,0xFF,0xFF,0x00,0x00,	//Logical Maximum(65535)
	 0x75,0x10,	//Report Size(16bit)
	 0x95,0x01,	//Report Count(1)
	 0x09,0x56,	//Usage(Scan Time)-----每个数据包的时间戳,单位是100us,会影响到滚轮的快慢
	 0x81,0x02,	//Input(Data,Var,Abs)
	 
	 0x09,0x54,	//Usage(Contact count)-----手指个数
	 0x25,0x7F,	//Logical Maximum(127)
	 0x95,0x01,	//Report Count(1)
	 0x75,0x08,	//Report Size(8bit)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 
	 0x05,0x09,	//Usage Page(Button)-----TP上的按键
	 0x09,0x01,	//Usage(Button1)
	 0x09,0x02,	//Usage(Button2)
	 0x09,0x03,	//Usage(Button3)
	 0x25,0x01,	//Logical Maximum(1)
	 0x75,0x01,	//Report Size(1bit)
	 0x95,0x03,	//Report Count(3)
	 0x81,0x02,	//Input(Data,Var,Abs)
	 0x95,0x05,	//Report Count(5)
	 0x81,0x03,	//Input(Cnst,Var,Abs)
	 
	 0x05,0x0D,	//Usage page(Digitizers)
	 0x85,0x02,	//Report ID 0x02-------Feature,与主机通讯获得手指个数等资讯
	 0x09,0x55,	//Usage(Contact Count Maximum)
	 0x09,0x59,	//Usage(Pad Type)
	 0x75,0x04,	//Report Size(4bit)
	 0x95,0x02,	//Report Count(2)
	 0x25,0x0F,	//Logical Maximum(15)
	 0xB1,0x02,	//Feature(Data,Var,Abs)
	 0x05,0x0D,	//Usage page(Digitizers)
	 0x85,0x07,	//Report ID 0x07
	 0x09,0x60,	//Usage()??????????????????????????????????????????????
	 0x75,0x01,	//Report Size(1bit)
	 0x95,0x01,	//Report Count(1)
	 0x15,0x00,	//Logical Minimum(0)
	 0x25,0x01,	//Logical Maximum(1)
	 0xB1,0x02,	//Feature(Data,Var,Abs)
	 0x95,0x07,	//Report Count(7)
	 0xB1,0x03,	//Feature(Cnst,Var,Abs)
	 0x85,0x06,	//Report ID 0x06------------CERTIF
	 0x06,0x00,0xFF,	//Usage Page(Vendor Defined)
	 0x09,0xC5,	//Usage(Vendor Usage 0xC5)
	 0x15,0x00,	//Logical Minimum(0)
	 0x26,0xFF,0x00,	//Logical Maximum(255)
	 0x75,0x08,	//Report Size(8bit)
	 0x96,0x00,0x01,	//Report Count(256)
	 0xB1,0x02,	//Feature(Data,Var,Abs)
	 0xC0,	//End Collection
	 
	 0x05,0x0D,	//Usage page(Digitizers)
	 0x09,0x0E,	//Usage(Configuration)
	 0xA1,0x01,	//Collection(Application)
	 0x85,0x03,	//Report ID 0x03
	 0x09,0x22,	//Usage(Finger)
	 0xA1,0x02,	//Collection(physical) 
	 0x09,0x52,	//Usage(Input Mode)
	 0x15,0x00,	//Logical Minimum(0)
	 0x25,0x0A,	//Logical Maximum(10)
	 0x75,0x08,	//Report Size(8bit)
	 0x95,0x01,	//Report Count(1)
	 0xB1,0x02,	//Feature(Data,Var,Abs)
	 0xC0,	//End Collection
	 
	 0x09,0x22,	//Usage(Finger)
	 0xA1,0x00,	//Collection(Physical)
	 0x85,0x05,	//Report ID 0x05
	 0x09,0x57,	//Usage(Surface switch)
	 0x09,0x58,	//Usage(Button switch)
	 0x75,0x01,	//Report Size(1bit)
	 0x95,0x02,	//Report Count(2)
	 0x25,0x01,	//Logical Maximum(1)
	 0xB1,0x02,	//Feature(Data,Var,Abs)
	 0x95,0x06,	//Report Count(6)
	 0xB1,0x03,	//Feature(Cnst,Var,Abs)
	 0xC0,	//End Collection
	 0xC0,	//End Collection
	 
	 0x05, 0x0D,        // Usage Page (Digitizer)
	 0x09, 0x05,        // Usage (Touch Pad)
	 0xA1, 0x01,        // Collection (Application)
	 0x85, 0x04,        //   Report ID (4)
	 
	 0x05, 0x0D,        //   Usage Page (Digitizer)
	 0x09, 0x22,        //   Usage (Finger)
	 0xA1, 0x02,        //   Collection (Logical)
	 0x15, 0x00,        //     Logical Minimum (0)
	 0x25, 0x01,        //     Logical Maximum (1)
	 0x09, 0x47,        //     Usage (0x47)
	 0x09, 0x42,        //     Usage (Tip Switch)
	 0x95, 0x02,        //     Report Count (2)
	 0x75, 0x01,        //     Report Size (1)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x95, 0x01,        //     Report Count (1)
	 0x75, 0x03,        //     Report Size (3)
	 0x25, 0x05,        //     Logical Maximum (5)
	 0x09, 0x51,        //     Usage (0x51)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x75, 0x01,        //     Report Size (1)
	 0x95, 0x03,        //     Report Count (3)
	 0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	 0x15, 0x00,        //     Logical Minimum (0)
	 0x26, 0xCA, 0xCA,  //     Logical Maximum (-13622)
	 0x75, 0x10,        //     Report Size (16)
	 0x55, 0x0E,        //     Unit Exponent (-2)
	 0x65, 0x13,        //     Unit (System: English Linear, Length: Centimeter)
	 0x09, 0x30,        //     Usage (X)
	 0x35, 0x00,        //     Physical Minimum (0)
	 0x46, 0xCA, 0xCA,  //     Physical Maximum (-13622)
	 0x95, 0x01,        //     Report Count (1)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x46, 0xCA, 0xCA,  //     Physical Maximum (-13622)
	 0x26, 0xCA, 0xCA,  //     Logical Maximum (-13622)
	 0x09, 0x31,        //     Usage (Y)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0xC0,              //   End Collection
	 
	 0x05, 0x0D,        //   Usage Page (Digitizer)
	 0x09, 0x22,        //   Usage (Finger)
	 0xA1, 0x02,        //   Collection (Logical)
	 0x15, 0x00,        //     Logical Minimum (0)
	 0x25, 0x01,        //     Logical Maximum (1)
	 0x09, 0x47,        //     Usage (0x47)
	 0x09, 0x42,        //     Usage (Tip Switch)
	 0x95, 0x02,        //     Report Count (2)
	 0x75, 0x01,        //     Report Size (1)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x95, 0x01,        //     Report Count (1)
	 0x75, 0x03,        //     Report Size (3)
	 0x25, 0x05,        //     Logical Maximum (5)
	 0x09, 0x51,        //     Usage (0x51)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x75, 0x01,        //     Report Size (1)
	 0x95, 0x03,        //     Report Count (3)
	 0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	 0x15, 0x00,        //     Logical Minimum (0)
	 0x26, 0xCA, 0xCA,  //     Logical Maximum (-13622)
	 0x75, 0x10,        //     Report Size (16)
	 0x55, 0x0E,        //     Unit Exponent (-2)
	 0x65, 0x13,        //     Unit (System: English Linear, Length: Centimeter)
	 0x09, 0x30,        //     Usage (X)
	 0x35, 0x00,        //     Physical Minimum (0)
	 0x46, 0xCA, 0xCA,  //     Physical Maximum (-13622)
	 0x95, 0x01,        //     Report Count (1)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x46, 0xCA, 0xCA,  //     Physical Maximum (-13622)
	 0x26, 0xCA, 0xCA,  //     Logical Maximum (-13622)
	 0x09, 0x31,        //     Usage (Y)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0xC0,              //   End Collection
	 
	 0x05, 0x0D,        //   Usage Page (Digitizer)
	 0x09, 0x22,        //   Usage (Finger)
	 0xA1, 0x02,        //   Collection (Logical)
	 0x15, 0x00,        //     Logical Minimum (0)
	 0x25, 0x01,        //     Logical Maximum (1)
	 0x09, 0x47,        //     Usage (0x47)
	 0x09, 0x42,        //     Usage (Tip Switch)
	 0x95, 0x02,        //     Report Count (2)
	 0x75, 0x01,        //     Report Size (1)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x95, 0x01,        //     Report Count (1)
	 0x75, 0x03,        //     Report Size (3)
	 0x25, 0x05,        //     Logical Maximum (5)
	 0x09, 0x51,        //     Usage (0x51)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x75, 0x01,        //     Report Size (1)
	 0x95, 0x03,        //     Report Count (3)
	 0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	 0x15, 0x00,        //     Logical Minimum (0)
	 0x26, 0xCA, 0xCA,  //     Logical Maximum (-13622)
	 0x75, 0x10,        //     Report Size (16)
	 0x55, 0x0E,        //     Unit Exponent (-2)
	 0x65, 0x13,        //     Unit (System: English Linear, Length: Centimeter)
	 0x09, 0x30,        //     Usage (X)
	 0x35, 0x00,        //     Physical Minimum (0)
	 0x46, 0xCA, 0xCA,  //     Physical Maximum (-13622)
	 0x95, 0x01,        //     Report Count (1)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x46, 0xCA, 0xCA,  //     Physical Maximum (-13622)
	 0x26, 0xCA, 0xCA,  //     Logical Maximum (-13622)
	 0x09, 0x31,        //     Usage (Y)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0xC0,              //   End Collection
	 
	 0x05, 0x0D,        //   Usage Page (Digitizer)
	 0x09, 0x22,        //   Usage (Finger)
	 0xA1, 0x02,        //   Collection (Logical)
	 0x15, 0x00,        //     Logical Minimum (0)
	 0x25, 0x01,        //     Logical Maximum (1)
	 0x09, 0x47,        //     Usage (0x47)
	 0x09, 0x42,        //     Usage (Tip Switch)
	 0x95, 0x02,        //     Report Count (2)
	 0x75, 0x01,        //     Report Size (1)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x95, 0x01,        //     Report Count (1)
	 0x75, 0x03,        //     Report Size (3)
	 0x25, 0x05,        //     Logical Maximum (5)
	 0x09, 0x51,        //     Usage (0x51)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x75, 0x01,        //     Report Size (1)
	 0x95, 0x03,        //     Report Count (3)
	 0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	 0x15, 0x00,        //     Logical Minimum (0)
	 0x26, 0xCA, 0xCA,  //     Logical Maximum (-13622)
	 0x75, 0x10,        //     Report Size (16)
	 0x55, 0x0E,        //     Unit Exponent (-2)
	 0x65, 0x13,        //     Unit (System: English Linear, Length: Centimeter)
	 0x09, 0x30,        //     Usage (X)
	 0x35, 0x00,        //     Physical Minimum (0)
	 0x46, 0xCA, 0xCA,  //     Physical Maximum (-13622)
	 0x95, 0x01,        //     Report Count (1)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x46, 0xCA, 0xCA,  //     Physical Maximum (-13622)
	 0x26, 0xCA, 0xCA,  //     Logical Maximum (-13622)
	 0x09, 0x31,        //     Usage (Y)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0xC0,              //   End Collection
	 
	 0x05, 0x0D,        //   Usage Page (Digitizer)
	 0x09, 0x22,        //   Usage (Finger)
	 0xA1, 0x02,        //   Collection (Logical)
	 0x15, 0x00,        //     Logical Minimum (0)
	 0x25, 0x01,        //     Logical Maximum (1)
	 0x09, 0x47,        //     Usage (0x47)
	 0x09, 0x42,        //     Usage (Tip Switch)
	 0x95, 0x02,        //     Report Count (2)
	 0x75, 0x01,        //     Report Size (1)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x95, 0x01,        //     Report Count (1)
	 0x75, 0x03,        //     Report Size (3)
	 0x25, 0x05,        //     Logical Maximum (5)
	 0x09, 0x51,        //     Usage (0x51)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x75, 0x01,        //     Report Size (1)
	 0x95, 0x03,        //     Report Count (3)
	 0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	 0x15, 0x00,        //     Logical Minimum (0)
	 0x26, 0xCA, 0xCA,  //     Logical Maximum (-13622)
	 0x75, 0x10,        //     Report Size (16)
	 0x55, 0x0E,        //     Unit Exponent (-2)
	 0x65, 0x13,        //     Unit (System: English Linear, Length: Centimeter)
	 0x09, 0x30,        //     Usage (X)
	 0x35, 0x00,        //     Physical Minimum (0)
	 0x46, 0xCA, 0xCA,  //     Physical Maximum (-13622)
	 0x95, 0x01,        //     Report Count (1)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x46, 0xCA, 0xCA,  //     Physical Maximum (-13622)
	 0x26, 0xCA, 0xCA,  //     Logical Maximum (-13622)
	 0x09, 0x31,        //     Usage (Y)
	 0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0xC0,              //   End Collection
	 
	 0x05, 0x0D,        //   Usage Page (Digitizer)
	 0x55, 0x0C,        //   Unit Exponent (-4)
	 0x66, 0x01, 0x10,  //   Unit (System: SI Linear, Time: Seconds)
	 0x47, 0xFF, 0xFF, 0x00, 0x00,  //   Physical Maximum (65534)
	 0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65534)
	 0x75, 0x10,        //   Report Size (16)
	 0x95, 0x01,        //   Report Count (1)
	 0x09, 0x56,        //   Usage (0x56)
	 0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x09, 0x54,        //   Usage (0x54)
	 0x25, 0x7F,        //   Logical Maximum (127)
	 0x95, 0x01,        //   Report Count (1)
	 0x75, 0x08,        //   Report Size (8)
	 0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x05, 0x09,        //   Usage Page (Button)
	 0x09, 0x01,        //   Usage (0x01)
	 0x09, 0x02,        //   Usage (0x02)
	 0x09, 0x03,        //   Usage (0x03)
	 0x25, 0x01,        //   Logical Maximum (1)
	 0x75, 0x01,        //   Report Size (1)
	 0x95, 0x03,        //   Report Count (3)
	 0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x95, 0x05,        //   Report Count (5)
	 0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	 0x05, 0x0D,        //   Usage Page (Digitizer)
	 0x85, 0x02,        //   Report ID (2)
	 0x09, 0x55,        //   Usage (0x55)
	 0x09, 0x59,        //   Usage (0x59)
	 0x75, 0x04,        //   Report Size (4)
	 0x95, 0x02,        //   Report Count (2)
	 0x25, 0x0F,        //   Logical Maximum (15)
	 0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	 0x05, 0x0D,        //   Usage Page (Digitizer)
	 0x85, 0x07,        //   Report ID (7)
	 0x09, 0x60,        //   Usage (0x60)
	 0x75, 0x01,        //   Report Size (1)
	 0x95, 0x01,        //   Report Count (1)
	 0x15, 0x00,        //   Logical Minimum (0)
	 0x25, 0x01,        //   Logical Maximum (1)
	 0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	 0x95, 0x07,        //   Report Count (7)
	 0xB1, 0x03,        //   Feature (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	 0x85, 0x06,        //   Report ID (6)
	 0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
	 0x09, 0xC5,        //   Usage (0xC5)
	 0x15, 0x00,        //   Logical Minimum (0)
	 0x26, 0xFF, 0x00,  //   Logical Maximum (255)
	 0x75, 0x08,        //   Report Size (8)
	 0x96, 0x00, 0x01,  //   Report Count (256)
	 0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	 0xC0,              // End Collection
	},
	.hid_report_desc_size = 649,
	.i2c_name = "elan"
};

static const struct cust_i2c_hid_desc_id cust_override_table[] = {
	{
		.name = "elan",
		.override = (void *)&elandev_desc,
	},
	{//End
		.name = "None",
		.override = (void *)&elandev_desc,
	}
};

struct i2c_hid_desc *i2c_hid_get_cust_i2c_hid_desc_override(const char *i2c_name)
{
	int i;
	struct i2c_hid_desc_override *override;

	for (i = 0;; i++) {
		override = cust_override_table[i].override;

		if (strcmp(cust_override_table[i].name, "None") == 0) {
			pr_info("i2c_hid:%s no found i2c_hid_desc, use default !\n",__func__);
			break;
		}

		if (strcmp(override->i2c_name, i2c_name) == 0) {
			pr_info("i2c_hid:%s found i2c_hid_desc:%s \n",__func__,override->i2c_name);
			break;
		}
	}

	return override->i2c_hid_desc;
}

char *i2c_hid_get_cust_hid_report_desc_override(const char *i2c_name,
					       unsigned int *size)
{
	int i;
	struct i2c_hid_desc_override *override;

	for (i = 0;; i++) {
		override = cust_override_table[i].override;

		if (strcmp(cust_override_table[i].name, "None") == 0) {
			pr_info("i2c_hid:%s no found hid_report_desc, use default !\n",__func__);
			break;
		}

		if (strcmp(override->i2c_name, i2c_name) == 0) {
			pr_info("i2c_hid:%s found hid_report_desc:%s \n",__func__,override->i2c_name);
			break;
		}
	}

	*size = override->hid_report_desc_size;
	return override->hid_report_desc;
}