/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 S5K4H5YCmipiraw_sensor.c
 *
 * Project:
 * --------
 *	 ALPS MT6763
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"
#include "s5k4h5ycmipiraw_Sensor.h"

/*===FEATURE SWITH===*/
 // #define FPTPDAFSUPPORT   //for pdaf switch
 // #define FANPENGTAO   //for debug log

 //#define NONCONTINUEMODE
/*===FEATURE SWITH===*/

/****************************Modify Following Strings for Debug****************************/
#define PFX "S5K4H5YC4LAN"
#define LOG_INF_NEW(format, args...)    printk(PFX "[%s] " format, __FUNCTION__, ##args)
#define LOG_INF LOG_INF_NEW
#define LOG_1 LOG_INF("S5K4H5YC4LAN,MIPI 4LANE\n")
#define SENSORDB LOG_INF
/****************************   Modify end    *******************************************/

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static imgsensor_info_struct imgsensor_info = {
	.sensor_id = S5K4H5YC_SENSOR_ID,		//Sensor ID Value: 0x30C8//record sensor id defined in Kd_imgsensor.h

	.checksum_value = 0xe1dcd710,
	
	.pre = {
		.pclk = 280000000,
		.linelength = 3688,
		.framelength = 2512,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1640,		//record different mode's width of grabwindow
		.grabwindow_height = 1232,		//record different mode's height of grabwindow
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 280000000,
		.linelength = 3688,
		.framelength = 2512,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
	},
	.normal_video = {
		.pclk = 280000000,
		.linelength = 3688,
		.framelength = 2512,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 280000000,
		.linelength = 3688,
		.framelength = 2512,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
	},
	.slim_video = {
		.pclk = 280000000,
		.linelength = 3688,
		.framelength = 2512,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3280,
		.grabwindow_height = 2464,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
	},
	.margin = 16,
	.min_shutter = 5,
	.min_gain = 73,
	.max_gain = 4096,
	.min_gain_iso = 100,
	.exp_step = 2,
    .gain_step = 1,
	.gain_type = 0,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 1,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num
	
	.cap_delay_frame = 3, 
	.pre_delay_frame = 3, 
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = 1,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
#ifdef CONFIG_S5K4H5YC_MIRROR_HV
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,//Gr
#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,//Gr
#endif
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	//.i2c_addr_table = {0x6e, 0x20, 0x5a, 0x6c, 0xff},
#ifdef CONFIG_S5K4H5YC_MULTI_ADDR
	.i2c_addr_table = {0x6e, 0x20, 0x30, 0x6c,0xff},
#else
	.i2c_addr_table = {0x20, 0xff},
#endif
  .i2c_speed = 300, // i2c read/write speed
};


static struct imgsensor_struct imgsensor = {
#ifdef CONFIG_S5K4H5YC_MIRROR_HV
	.mirror = IMAGE_HV_MIRROR,//IMAGE_NORMAL,				//mirrorflip information
#else
	.mirror = IMAGE_NORMAL,//IMAGE_NORMAL,				//mirrorflip information
#endif
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0,					//current shutter
	.gain = 0x100,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
    .current_fps = 30,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_TRUE for in test pattern mode, KAL_FALSE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,
};


/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[] = {
 { 3280, 2464,	  0,	  0,   3280, 2464, 1640, 1232, 0, 0, 1640, 1232,	0,	0, 1640, 1232}, // Preview 
 { 3280, 2464,	  0,	  0,   3280, 2464, 3280, 2464, 0, 0, 3280, 2464,	0,	0, 3280, 2464}, // capture 
 { 3280, 2464,	  0,	  0,   3280, 2464, 3280, 2464, 0, 0, 3280, 2464,	0,	0, 3280, 2464}, // video 
 { 3280, 2464,	  0,	  0,   3280, 2464, 3280, 2464, 0, 0, 3280, 2464,	0,	0, 3280, 2464}, //hight speed video 
 { 3280, 2464,	  0,	  0,   3280, 2464, 3280, 2464, 0, 0, 3280, 2464,	0,	0, 3280, 2464},
};


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte=0;
    char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);
    return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static kal_uint16 read_sensor_otp_mid(void)
{
	static kal_uint16 chip_mid = 0;
	 
	write_cmos_sensor(0x3A02, 0x00);
	write_cmos_sensor(0x3A00, 0x01);
	mdelay(5);
	chip_mid = read_cmos_sensor(0x3A33);
	write_cmos_sensor(0x3A00, 0x00);
	//chip_mid = 2;
	//chip_mid = read_cmos_sensor(0x0002);
	if(chip_mid == 0x8B){
		chip_mid = 2;
	}else{
		chip_mid = 3;
	}
	printk("s5k4h5 read_sensor_otp_mid=%d\n",chip_mid);
    
    return chip_mid;
}

static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);

	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);	  
	write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
}	/*	set_dummy  */
static kal_uint32 return_sensor_id(void)
{

    return ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));

}
static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	LOG_INF("framerate = %d, min framelength should enable(%d) \n", framerate,min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
	{
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}	/*	set_max_framerate  */


static void write_shutter(kal_uint16 shutter)
	{
		kal_uint16 realtime_fps = 0;
	//	kal_uint32 frame_length = 0;
		// shutter=2512;//add for debug capture framerate  
		/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
		/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */
		
		// OV Recommend Solution
		// if shutter bigger than frame_length, should extend frame length first
	
		if(imgsensor.sensor_mode == IMGSENSOR_MODE_HIGH_SPEED_VIDEO)
		{
			if(shutter > imgsensor.min_frame_length - imgsensor_info.margin)
				shutter = imgsensor.min_frame_length - imgsensor_info.margin;
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF); 
 			write_cmos_sensor(0x0203, shutter  & 0xFF);
			LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
			return;
		}
		spin_lock(&imgsensor_drv_lock);
		if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
			{
			imgsensor.frame_length = shutter + imgsensor_info.margin;
			}
		else
			{
			imgsensor.frame_length = imgsensor.min_frame_length;
			}
		if (imgsensor.frame_length > imgsensor_info.max_frame_length)
			{
			imgsensor.frame_length = imgsensor_info.max_frame_length;
			}
		spin_unlock(&imgsensor_drv_lock);
		if (shutter < imgsensor_info.min_shutter) 
			shutter = imgsensor_info.min_shutter;
		
		if (imgsensor.autoflicker_en == KAL_TRUE) { 
			realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
			if(realtime_fps >= 297 && realtime_fps <= 305)
			{
				set_max_framerate(296,0);
				//set_dummy();
			}
			else if(realtime_fps >= 147 && realtime_fps <= 150)
			{
				set_max_framerate(146,0);	
				//set_dummy();
			}
			else{
			//write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF); 
 			write_cmos_sensor(0x0203, shutter  & 0xFF);
			//write_cmos_sensor(0x0104, 0x00);
			return;
			}
		} else {
			// Extend frame length
			//write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF); 
			write_cmos_sensor(0x0203, shutter  & 0xFF);
			//write_cmos_sensor(0x0104, 0x00);
			return;
		}
	
		// Update Shutter
		//write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF); 
 		write_cmos_sensor(0x0203, shutter  & 0xFF);
 		//write_cmos_sensor(0x0104, 0x00);
 		LOG_INF("realtime_fps =%d\n", realtime_fps);
		LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
	
		//LOG_INF("frame_length = %d ", frame_length);
		
	}




/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	write_shutter(shutter);
}	/*	set_shutter */



/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_gain(kal_uint16 gain)
{
	gain = gain / 2;
	
	//write_cmos_sensor(0x0104, 0x01);	
	write_cmos_sensor(0x0204,(gain>>8));
	write_cmos_sensor(0x0205,(gain&0xff));
	//write_cmos_sensor(0x0104, 0x00);
	return ;
}   /*  S5K4H5YCYCMIPI_SetGain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
	return ;
}


#if 1
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	/********************************************************
	   *
	   *   0x3820[2] ISP Vertical flip
	   *   0x3820[1] Sensor Vertical flip
	   *
	   *   0x3821[2] ISP Horizontal mirror
	   *   0x3821[1] Sensor Horizontal mirror
	   *
	   *   ISP and Sensor flip or mirror register bit should be the same!!
	   *
	   ********************************************************/
	
	switch (image_mirror)
    {
        case IMAGE_NORMAL: //B
            write_cmos_sensor(0x0101, 0x00);	//Set normal
            break;
        case IMAGE_V_MIRROR: //Gr X
            write_cmos_sensor(0x0101, 0x01);	//Set flip
            break;
        case IMAGE_H_MIRROR: //Gb
            write_cmos_sensor(0x0101, 0x02);	//Set mirror
            break;
        case IMAGE_HV_MIRROR: //R
            write_cmos_sensor(0x0101, 0x03);	//Set mirror and flip
            break;
    }

}
#endif
/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}	/*	night_mode	*/
static void sensor_init(void)
{
}	/*	sensor_init  */


static void preview_setting(void)
	{

	 kal_uint16 chip_id = 0;
	
	/*Delay 1 frame*/
	mDELAY(33);	
  chip_id = read_cmos_sensor(0x0002);
   write_cmos_sensor(0x0100,0x00);

  if (chip_id == 0x01){

  		write_cmos_sensor(0x0101, 0x00);
		write_cmos_sensor(0x0204, 0x00);
		write_cmos_sensor(0x0205, 0x20);
		write_cmos_sensor(0x0200, 0x0D);
		write_cmos_sensor(0x0201, 0x78);
		write_cmos_sensor(0x0202, 0x04);
		write_cmos_sensor(0x0203, 0xE2);
		write_cmos_sensor(0x0340, 0x09);
		write_cmos_sensor(0x0341, 0xD0);
		write_cmos_sensor(0x0342, 0x0E);
		write_cmos_sensor(0x0343, 0x68);
		write_cmos_sensor(0x0344, 0x00);
		write_cmos_sensor(0x0345, 0x08);
		write_cmos_sensor(0x0346, 0x00);
		write_cmos_sensor(0x0347, 0x08);
		write_cmos_sensor(0x0348, 0x0C);
		write_cmos_sensor(0x0349, 0xC9);
		write_cmos_sensor(0x034A, 0x09);
		write_cmos_sensor(0x034B, 0x97);
		write_cmos_sensor(0x034C, 0x06);
		write_cmos_sensor(0x034D, 0x60);
		write_cmos_sensor(0x034E, 0x04);
		write_cmos_sensor(0x034F, 0xC8);
		write_cmos_sensor(0x0390, 0x01);
		write_cmos_sensor(0x0391, 0x22);
		write_cmos_sensor(0x0381, 0x01);
		write_cmos_sensor(0x0383, 0x03);
		write_cmos_sensor(0x0385, 0x01);
		write_cmos_sensor(0x0387, 0x03);
		write_cmos_sensor(0x0301, 0x02);
		write_cmos_sensor(0x0303, 0x01);
		write_cmos_sensor(0x0305, 0x06);
		write_cmos_sensor(0x0306, 0x00);
		write_cmos_sensor(0x0307, 0x8C);
		write_cmos_sensor(0x0309, 0x02);
		write_cmos_sensor(0x030B, 0x01);
		write_cmos_sensor(0x3C59, 0x00);
		write_cmos_sensor(0x030D, 0x06);
		write_cmos_sensor(0x030E, 0x00);
		write_cmos_sensor(0x030F, 0xAF);
		write_cmos_sensor(0x3C5A, 0x00);
		write_cmos_sensor(0x0310, 0x01);
		write_cmos_sensor(0x3C50, 0x53);
		write_cmos_sensor(0x3C62, 0x02);
		write_cmos_sensor(0x3C63, 0xBC);
		write_cmos_sensor(0x3C64, 0x00);
		write_cmos_sensor(0x3C65, 0x00);
		write_cmos_sensor(0x3C1E, 0x00);
		write_cmos_sensor(0x302A, 0x0A);
		write_cmos_sensor(0x304B, 0x2A);
		write_cmos_sensor(0x3205, 0x84);
		write_cmos_sensor(0x3207, 0x85);
		write_cmos_sensor(0x3214, 0x94);
		write_cmos_sensor(0x3216, 0x95);
		write_cmos_sensor(0x303A, 0x9F);
		write_cmos_sensor(0x3201, 0x07);
		write_cmos_sensor(0x3051, 0xFF);
		write_cmos_sensor(0x3052, 0xFF);
		write_cmos_sensor(0x3054, 0xF0);
		write_cmos_sensor(0x302D, 0x7F);
		write_cmos_sensor(0x3002, 0x0D);
		write_cmos_sensor(0x300A, 0x0D);
		write_cmos_sensor(0x3037, 0x12);
		write_cmos_sensor(0x3045, 0x04);
		write_cmos_sensor(0x300C, 0x78);
		write_cmos_sensor(0x300D, 0x80);
		write_cmos_sensor(0x305C, 0x82);
		write_cmos_sensor(0x3010, 0x0A);
		write_cmos_sensor(0x305E, 0x11);
		write_cmos_sensor(0x305F, 0x11);
		write_cmos_sensor(0x3060, 0x10);
		write_cmos_sensor(0x3091, 0x04);
		write_cmos_sensor(0x3092, 0x07);
		write_cmos_sensor(0x303D, 0x05);
		write_cmos_sensor(0x3038, 0x99);
		write_cmos_sensor(0x3B29, 0x01);
		write_cmos_sensor(0x0100, 0x01);


	}
   else if (chip_id == 0x03) {
		write_cmos_sensor(0x0101, 0x00);
		write_cmos_sensor(0x0204,0x00);
		write_cmos_sensor(0x0205,0x20);
		write_cmos_sensor(0x0200,0x0D);//Caval 140613
		write_cmos_sensor(0x0201,0x78);//Caval 140613
		write_cmos_sensor(0x0202,0x04);
		write_cmos_sensor(0x0203,0xE2);
		write_cmos_sensor(0x0340,0x09);
		write_cmos_sensor(0x0341,0xD0);
		write_cmos_sensor(0x0342,0x0E);
		write_cmos_sensor(0x0343,0x68);
		write_cmos_sensor(0x0344,0x00);
		write_cmos_sensor(0x0345,0x00);
		write_cmos_sensor(0x0346,0x00);
		write_cmos_sensor(0x0347,0x00);
		write_cmos_sensor(0x0348,0x0C);
		write_cmos_sensor(0x0349,0xD1);//Caval 140613
		write_cmos_sensor(0x034A,0x09);
		write_cmos_sensor(0x034B,0x9F);
		write_cmos_sensor(0x034C,0x06);
		write_cmos_sensor(0x034D,0x68);
		write_cmos_sensor(0x034E,0x04);
		write_cmos_sensor(0x034F,0xD0);
		write_cmos_sensor(0x0390,0x01);
		write_cmos_sensor(0x0391,0x22);
		write_cmos_sensor(0x0940,0x00);
		write_cmos_sensor(0x0381,0x01);
		write_cmos_sensor(0x0383,0x03);
		write_cmos_sensor(0x0385,0x01);
		write_cmos_sensor(0x0387,0x03);
		write_cmos_sensor(0x0301,0x02);
		write_cmos_sensor(0x0303,0x01);
		write_cmos_sensor(0x0305,0x06);
		write_cmos_sensor(0x0306,0x00);
		write_cmos_sensor(0x0307,0x8C);
		write_cmos_sensor(0x0309,0x02);
		write_cmos_sensor(0x030B,0x01);
		write_cmos_sensor(0x3C59,0x00);
		write_cmos_sensor(0x030D,0x06);
		write_cmos_sensor(0x030E,0x00);
		write_cmos_sensor(0x030F,0xAF);
		write_cmos_sensor(0x3C5A,0x00);
		write_cmos_sensor(0x0310,0x01);
		write_cmos_sensor(0x3C50,0x53);
		write_cmos_sensor(0x3C62,0x02);
		write_cmos_sensor(0x3C63,0xBC);
		write_cmos_sensor(0x3C64,0x00);
		write_cmos_sensor(0x3C65,0x00);
		write_cmos_sensor(0x0114,0x03);
		write_cmos_sensor(0x3C1E,0x0F);//Caval 140613
		write_cmos_sensor(0x3500,0x0C);
		write_cmos_sensor(0x3C1A,0xA8);
		write_cmos_sensor(0x3B29,0x01);//Caval 140613
		write_cmos_sensor(0x3300,0x01);//Caval 140613			//20150309
		write_cmos_sensor(0x3000,0x07);
		write_cmos_sensor(0x3001,0x05);
		write_cmos_sensor(0x3002,0x03);
		write_cmos_sensor(0x0200,0x0C);
		write_cmos_sensor(0x0201,0xB4);
		write_cmos_sensor(0x300A,0x03);
		write_cmos_sensor(0x300C,0x65);
		write_cmos_sensor(0x300D,0x54);
		write_cmos_sensor(0x3010,0x00);
		write_cmos_sensor(0x3012,0x14);
		write_cmos_sensor(0x3014,0x19);
		write_cmos_sensor(0x3017,0x0F);
		write_cmos_sensor(0x3018,0x1A);
		write_cmos_sensor(0x3019,0x6C);
		write_cmos_sensor(0x301A,0x78);
		write_cmos_sensor(0x306F,0x00);
		write_cmos_sensor(0x3070,0x00);
		write_cmos_sensor(0x3071,0x00);
		write_cmos_sensor(0x3072,0x00);
		write_cmos_sensor(0x3073,0x00);
		write_cmos_sensor(0x3074,0x00);
		write_cmos_sensor(0x3075,0x00);
		write_cmos_sensor(0x3076,0x0A);
		write_cmos_sensor(0x3077,0x03);
		write_cmos_sensor(0x3078,0x84);
		write_cmos_sensor(0x3079,0x00);
		write_cmos_sensor(0x307A,0x00);
		write_cmos_sensor(0x307B,0x00);
		write_cmos_sensor(0x307C,0x00);
		write_cmos_sensor(0x3085,0x00);
		write_cmos_sensor(0x3086,0x72);
		write_cmos_sensor(0x30A6,0x01);
		write_cmos_sensor(0x30A7,0x0E);
		write_cmos_sensor(0x3032,0x01);
		write_cmos_sensor(0x3037,0x02);
		write_cmos_sensor(0x304A,0x01);
		write_cmos_sensor(0x3054,0xF0);
		write_cmos_sensor(0x3044,0x20);
		write_cmos_sensor(0x3045,0x20);
		write_cmos_sensor(0x3047,0x04);
		write_cmos_sensor(0x3048,0x11);
		write_cmos_sensor(0x303D,0x08);
		write_cmos_sensor(0x304B,0x31);
		write_cmos_sensor(0x3063,0x00);
		write_cmos_sensor(0x303A,0x0B);
		write_cmos_sensor(0x302D,0x7F);
		write_cmos_sensor(0x3039,0x45);
		write_cmos_sensor(0x3038,0x10);
		write_cmos_sensor(0x3097,0x11);
		write_cmos_sensor(0x3096,0x01);
		write_cmos_sensor(0x3042,0x01);
		write_cmos_sensor(0x3053,0x01);
		write_cmos_sensor(0x320B,0x40);
		write_cmos_sensor(0x320C,0x06);
		write_cmos_sensor(0x320D,0xC0);
		write_cmos_sensor(0x3202,0x00);
		write_cmos_sensor(0x3203,0x3D);
		write_cmos_sensor(0x3204,0x00);
		write_cmos_sensor(0x3205,0x3D);
		write_cmos_sensor(0x3206,0x00);
		write_cmos_sensor(0x3207,0x3D);
		write_cmos_sensor(0x3208,0x00);
		write_cmos_sensor(0x3209,0x3D);
		write_cmos_sensor(0x3211,0x02);
		write_cmos_sensor(0x3212,0x21);
		write_cmos_sensor(0x3213,0x02);
		write_cmos_sensor(0x3214,0x21);
		write_cmos_sensor(0x3215,0x02);
		write_cmos_sensor(0x3216,0x21);
		write_cmos_sensor(0x3217,0x02);
		write_cmos_sensor(0x3218,0x21);
		write_cmos_sensor(0x0100,0x01);
	 
  	}	/*	preview_setting  */

       write_cmos_sensor(0x0100,0x01);
	}


static void capture_setting(void)
	{
		
	
	  kal_uint16 chip_id = 0;
	  
			/*Delay 1 frame*/
			mDELAY(33); 
	  chip_id = read_cmos_sensor(0x0002);
	  write_cmos_sensor(0x0100, 0x00);
	
	  if (chip_id == 0x01) {
	   #ifdef XUNHU_LPS_TEKHW_SUPPORT
			if (0xff != tekhw_s5k4h5yc4lane_mirror)
			{
				set_mirror_flip(tekhw_s5k4h5yc4lane_mirror);
			}
#else
		  write_cmos_sensor(0x0101, 0x00);
#endif
	write_cmos_sensor(0x0204, 0x00);
	write_cmos_sensor(0x0205, 0x20);
	write_cmos_sensor(0x0200, 0x0D);
	write_cmos_sensor(0x0201, 0x78);
	write_cmos_sensor(0x0340, 0x09);
	write_cmos_sensor(0x0341, 0xD0);
	write_cmos_sensor(0x0342, 0x0E);
	write_cmos_sensor(0x0343, 0x68);
	write_cmos_sensor(0x0344, 0x00);
	write_cmos_sensor(0x0345, 0x00);
	write_cmos_sensor(0x0346, 0x00);
	write_cmos_sensor(0x0347, 0x00);
	write_cmos_sensor(0x0348, 0x0C);
	write_cmos_sensor(0x0349, 0xCF);
	write_cmos_sensor(0x034A, 0x09);
	write_cmos_sensor(0x034B, 0x9F);
	write_cmos_sensor(0x034C, 0x0C);
	write_cmos_sensor(0x034D, 0xD0);//3280
	write_cmos_sensor(0x034E, 0x09);
	write_cmos_sensor(0x034F, 0xA0);//2464
	write_cmos_sensor(0x0390, 0x00);
	write_cmos_sensor(0x0391, 0x00);
	write_cmos_sensor(0x0381, 0x01);
	write_cmos_sensor(0x0383, 0x01);
	write_cmos_sensor(0x0385, 0x01);
	write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0301, 0x02);
	write_cmos_sensor(0x0303, 0x01);
	write_cmos_sensor(0x0305, 0x06);
	write_cmos_sensor(0x0306, 0x00);
	write_cmos_sensor(0x0307, 0x8C);
	write_cmos_sensor(0x0309, 0x02);
	write_cmos_sensor(0x030B, 0x01);
	write_cmos_sensor(0x3C59, 0x00);
	write_cmos_sensor(0x030D, 0x06);
	write_cmos_sensor(0x030E, 0x00);
	write_cmos_sensor(0x030F, 0xAF);
	write_cmos_sensor(0x3C5A, 0x00);
	write_cmos_sensor(0x0310, 0x01);
	write_cmos_sensor(0x3C50, 0x53);
	write_cmos_sensor(0x3C62, 0x02);
	write_cmos_sensor(0x3C63, 0xBC);
	write_cmos_sensor(0x3C64, 0x00);
	write_cmos_sensor(0x3C65, 0x00);
	write_cmos_sensor(0x3C1E, 0x00);
	write_cmos_sensor(0x302A, 0x0A);
	write_cmos_sensor(0x304B, 0x2A);
	write_cmos_sensor(0x3205, 0x84);
	write_cmos_sensor(0x3207, 0x85);
	write_cmos_sensor(0x3214, 0x94);
	write_cmos_sensor(0x3216, 0x95);
	write_cmos_sensor(0x303A, 0x9F);
	write_cmos_sensor(0x3201, 0x07);
	write_cmos_sensor(0x3051, 0xFF);
	write_cmos_sensor(0x3052, 0xFF);
	write_cmos_sensor(0x3054, 0xF0);
	write_cmos_sensor(0x302D, 0x7F);
	write_cmos_sensor(0x3002, 0x0D);
	write_cmos_sensor(0x300A, 0x0D);
	write_cmos_sensor(0x3037, 0x12);
	write_cmos_sensor(0x3045, 0x04);
	write_cmos_sensor(0x300C, 0x78);
	write_cmos_sensor(0x300D, 0x80);
	write_cmos_sensor(0x305C, 0x82);
	write_cmos_sensor(0x3010, 0x0A);
	write_cmos_sensor(0x305E, 0x11);
	write_cmos_sensor(0x305F, 0x11);
	write_cmos_sensor(0x3060, 0x10);
	write_cmos_sensor(0x3091, 0x04);
	write_cmos_sensor(0x3092, 0x07);
	write_cmos_sensor(0x303D, 0x05);
	write_cmos_sensor(0x3038, 0x99);
	write_cmos_sensor(0x3B29, 0x01);
	write_cmos_sensor(0x0100, 0x01);
		}
	
	   else if (chip_id == 0x03) {
	 #ifdef XUNHU_LPS_TEKHW_SUPPORT
			if (0xff != tekhw_s5k4h5yc4lane_mirror)
			{
				set_mirror_flip(tekhw_s5k4h5yc4lane_mirror);
			}
#else
		  write_cmos_sensor(0x0101, 0x00);
#endif


	write_cmos_sensor(0x0204,0x00);
	write_cmos_sensor(0x0205,0x20);
	write_cmos_sensor(0x0202,0x04);
	write_cmos_sensor(0x0203,0xE2);
	write_cmos_sensor(0x0340,0x09);
	write_cmos_sensor(0x0341,0xD0);
	write_cmos_sensor(0x0342,0x0E);
	write_cmos_sensor(0x0343,0x68);
	write_cmos_sensor(0x0344,0x00);
	write_cmos_sensor(0x0345,0x00);
	write_cmos_sensor(0x0346,0x00);
	write_cmos_sensor(0x0347,0x00);
	write_cmos_sensor(0x0348,0x0C);
	write_cmos_sensor(0x0349,0xCF);
	write_cmos_sensor(0x034A,0x09);
	write_cmos_sensor(0x034B,0x9F);
	write_cmos_sensor(0x034C,0x0C);
	write_cmos_sensor(0x034D,0xD0);
	write_cmos_sensor(0x034E,0x09);
	write_cmos_sensor(0x034F,0xA0);
	write_cmos_sensor(0x0390,0x00);
	write_cmos_sensor(0x0391,0x00);
	write_cmos_sensor(0x0940,0x00);
	write_cmos_sensor(0x0381,0x01);
	write_cmos_sensor(0x0383,0x01);
	write_cmos_sensor(0x0385,0x01);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x0301,0x02);
	write_cmos_sensor(0x0303,0x01);
	write_cmos_sensor(0x0305,0x06);
	write_cmos_sensor(0x0306,0x00);
	write_cmos_sensor(0x0307,0x8C);
	write_cmos_sensor(0x0309,0x02);
	write_cmos_sensor(0x030B,0x01);
	write_cmos_sensor(0x3C59,0x00);
	write_cmos_sensor(0x030D,0x06);
	write_cmos_sensor(0x030E,0x00);
	write_cmos_sensor(0x030F,0xAF);
	write_cmos_sensor(0x3C5A,0x00);
	write_cmos_sensor(0x0310,0x01);
	write_cmos_sensor(0x3C50,0x53);
	write_cmos_sensor(0x3C62,0x02);
	write_cmos_sensor(0x3C63,0xBC);
	write_cmos_sensor(0x3C64,0x00);
	write_cmos_sensor(0x3C65,0x00);


	
	write_cmos_sensor(0x0114,0x03);

	
	write_cmos_sensor(0x3C1E,0x0F); 	//	[3] reg_isp_fe_TN_SMIA_sync_sel
	


	// BPC on/off --------------------------------------
	write_cmos_sensor(0x3500,0x0C); 	//	[3] bpcms_rate_auto
	write_cmos_sensor(0x3C1A,0xA8);
	write_cmos_sensor(0x3B29,0x01);//Caval 140613
	write_cmos_sensor(0x3300,0x01);//Caval 140613		//20150309	//0x00 is open, 0x01 is close

	// Analog Tuning for 3280x2464 30fps R10 131115
	write_cmos_sensor(0x3000,0x07);    // ct_ld_start
	write_cmos_sensor(0x3001,0x05);    // ct_sl_start
	write_cmos_sensor(0x3002,0x03);    // ct_rx_start
	write_cmos_sensor(0x0200,0x0C);
	write_cmos_sensor(0x0201,0xB4);    // (fine_integ_time) (LSB)
	write_cmos_sensor(0x300A,0x03);    // ct_cds_start
	write_cmos_sensor(0x300C,0x65);    // ct_s3_width
	write_cmos_sensor(0x300D,0x54);    // ct_s4_width
	write_cmos_sensor(0x3010,0x00);    // ct_pbr_width
	write_cmos_sensor(0x3012,0x14);    // ct_pbs_width
	write_cmos_sensor(0x3014,0x19);    // ct_pbr_ob_width
	write_cmos_sensor(0x3017,0x0F);    // ct_cds_lim_start
	write_cmos_sensor(0x3018,0x1A);    // ct_rmp_off_start
	write_cmos_sensor(0x3019,0x6C);    // ct_rmp_rst_start
	write_cmos_sensor(0x301A,0x78);    // ct_rmp_sig_start
	write_cmos_sensor(0x306F,0x00);    // ct_opt_l0_width1 (MSB)
	write_cmos_sensor(0x3070,0x00);    // ct_opt_l0_width1 (LSB)
	write_cmos_sensor(0x3071,0x00);    // ct_opt_l0_start2 (MSB)
	write_cmos_sensor(0x3072,0x00);    // ct_opt_l0_start2 (LSB)
	write_cmos_sensor(0x3073,0x00);    // ct_opt_l0_width2 (MSB)
	write_cmos_sensor(0x3074,0x00);    // ct_opt_l0_width2 (LSB)
	write_cmos_sensor(0x3075,0x00);    // ct_opt_l1_start1 (MSB)
	write_cmos_sensor(0x3076,0x0A);    // ct_opt_l1_start1 (LSB)
	write_cmos_sensor(0x3077,0x03);    // ct_opt_l1_width1 (MSB)
	write_cmos_sensor(0x3078,0x84);    // ct_opt_l1_width1 (LSB)
	write_cmos_sensor(0x3079,0x00);    // ct_opt_l1_start2 (MSB)
	write_cmos_sensor(0x307A,0x00);    // ct_opt_l1_start2 (LSB)
	write_cmos_sensor(0x307B,0x00);    // ct_opt_l1_width2 (MSB)
	write_cmos_sensor(0x307C,0x00);    // ct_opt_l1_width2 (LSB)
	write_cmos_sensor(0x3085,0x00);    // ct_opt_h0_start1 (MSB)
	write_cmos_sensor(0x3086,0x72);    // ct_opt_h0_start1 (LSB)
	write_cmos_sensor(0x30A6,0x01);    // cds_option 2
	write_cmos_sensor(0x30A7,0x0E);    // cds_option 1
	write_cmos_sensor(0x3032,0x01);    // rmp_option
	write_cmos_sensor(0x3037,0x02);    // dbs_option
	write_cmos_sensor(0x304A,0x01);    // dbr_option
	write_cmos_sensor(0x3054,0xF0);    // rdv_option
	write_cmos_sensor(0x3044,0x20);    // dbr_tune_tg
	write_cmos_sensor(0x3045,0x20);    // dbr_tune_rgsl
	write_cmos_sensor(0x3047,0x04);    // dbr_tune_ntg
	write_cmos_sensor(0x3048,0x11);    // dbr_tune_rd
	write_cmos_sensor(0x303D,0x08);    // off_rst
	write_cmos_sensor(0x304B,0x31);    // adc_sat (530mV)
	write_cmos_sensor(0x3063,0x00);    // ldb_ctrl
	write_cmos_sensor(0x303A,0x0B);    // clp_lvl
	write_cmos_sensor(0x302D,0x7F);    // (aig_main2)
	write_cmos_sensor(0x3039,0x45);    // pix/pxbst_bias
	write_cmos_sensor(0x3038,0x10);    // comp1/2_bias comp1_bias @AG<=x16
	write_cmos_sensor(0x3097,0x11);    // comp1_bias @AG<x8, @AG<x4, respectively
	write_cmos_sensor(0x3096,0x01);    // comp1_bias @AG<x2
	write_cmos_sensor(0x3042,0x01);    // # of L-OB
	write_cmos_sensor(0x3053,0x01);    // lp_vblk_en								  
	write_cmos_sensor(0x320B,0x40);    // adc_default (LSB) 						   
	write_cmos_sensor(0x320C,0x06);    // adc_max (MSB) 							   
	write_cmos_sensor(0x320D,0xC0);    // adc_max (LSB) 							   
	write_cmos_sensor(0x3202,0x00);    // adc_offset_even0 (MSB)					   
	write_cmos_sensor(0x3203,0x3D);    // adc_offset_even0 (LSB)					   
	write_cmos_sensor(0x3204,0x00);    // adc_offset_odd0 (MSB) 					   
	write_cmos_sensor(0x3205,0x3D);    // adc_offset_odd0 (LSB) 					   
	write_cmos_sensor(0x3206,0x00);    // adc_offset_even1 (MSB)					   
	write_cmos_sensor(0x3207,0x3D);    // adc_offset_even1 (LSB)					   
	write_cmos_sensor(0x3208,0x00);    // adc_offset_odd1 (MSB) 					   
	write_cmos_sensor(0x3209,0x3D);    // adc_offset_odd1 (LSB) 					   
	write_cmos_sensor(0x3211,0x02);    // adc_offset_ms_even0 (MSB) 				   
	write_cmos_sensor(0x3212,0x21);    // adc_offset_ms_even0 (LSB) 				   
	write_cmos_sensor(0x3213,0x02);    // adc_offset_ms_odd0 (MSB)					   
	write_cmos_sensor(0x3214,0x21);    // adc_offset_ms_odd0 (LSB)					   
	write_cmos_sensor(0x3215,0x02);    // adc_offset_ms_even1 (MSB) 				   
	write_cmos_sensor(0x3216,0x21);    // adc_offset_ms_even1 (LSB) 				   
	write_cmos_sensor(0x3217,0x02);    // adc_offset_ms_odd1 (MSB)					   
	write_cmos_sensor(0x3218,0x21);    // adc_offset_ms_odd1 (LSB)
	write_cmos_sensor(0x0100,0x01);
		}

	write_cmos_sensor(0x0100, 0x01);
	  
	}



static void normal_video_setting(void)
{     
  capture_setting();
}


static void hs_video_setting(void) 
{
  capture_setting();
}


static void slim_video_setting(void)
{
  capture_setting();
}

/*************************************************************************
* FUNCTION
*	get_imgsensor_id
*
* DESCRIPTION
*	This function get the sensor ID
*
* PARAMETERS
*	*sensorID : return the sensor ID
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
extern int imgsensor_idx_now;
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
	kal_uint8 retry = 2;
    //sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	#if defined(S5K4H5YCF_MIPI_RAW)
	if(imgsensor_idx_now == 1){
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	#endif
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = return_sensor_id();
            if ((*sensor_id == imgsensor_info.sensor_id) && (read_sensor_otp_mid() != 2)) {
                printk("s5k4h5yc i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
				return ERROR_NONE;
            }
            printk("s5k4h5yc Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        	retry = 2;
    }
    if ((*sensor_id != imgsensor_info.sensor_id) || (read_sensor_otp_mid() == 2)) {
        // if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}

/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
    //const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
    kal_uint8 i = 0;
	kal_uint8 retry = 2;
    kal_uint32 sensor_id = 0;
	LOG_1;
    //sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();
            if ((sensor_id == imgsensor_info.sensor_id) && (read_sensor_otp_mid() != 2)) {
                printk("s5k4h5yc open i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
                break;
            }
            printk("s5k4h5yc open  Read sensor id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        if ((sensor_id == imgsensor_info.sensor_id) && (read_sensor_otp_mid() != 2))
            break;
        retry = 2;
    }
    if ((imgsensor_info.sensor_id != sensor_id) || (read_sensor_otp_mid() == 2))
        return ERROR_SENSOR_CONNECT_FAIL;

    /* initail sequence write in  */
    sensor_init();

    spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_en = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);

    return ERROR_NONE;
}   /*  open  */

/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function*/

	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.current_fps = imgsensor.current_fps;
	//imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;  
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	spin_unlock(&imgsensor_drv_lock);
	capture_setting();
	set_mirror_flip(imgsensor.mirror);	
	return ERROR_NONE;
}	/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	//imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength; 
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}   /*  S5K4H5YCYCMIPIPreview   */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;		

	
	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;
	
  	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;
	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	//sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
	//sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
	//imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame; 
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame; 
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;	
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	
	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num; 
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3; /* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2; /* not use */
	sensor_info->SensorPixelClockCount = 3; /* not use */
	sensor_info->SensorDataLatchCount = 2; /* not use */
	
	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0; 
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x 
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;		
			
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			sensor_info->SensorGrabStartX = imgsensor_info.cap.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
				  
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc; 

			break;	 
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			
			sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
	   
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc; 

			break;	  
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:			
			sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
				  
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc; 

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
				  
			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc; 

			break;
		default:			
			sensor_info->SensorGrabStartX = imgsensor_info.pre.startx; 
			sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;		

			sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
			break;
	}

	return ERROR_NONE;
}	/*	get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			preview(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			capture(image_window, sensor_config_data);
			break;	
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			normal_video(image_window, sensor_config_data);
			break;	  
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			hs_video(image_window, sensor_config_data);
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			slim_video(image_window, sensor_config_data);
			break;	  
		default:
			LOG_INF("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("framerate = %d\n ", framerate);
	// SetVideoMode Function should fix framerate
	if (framerate == 0)
		// Dynamic frame rate
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	//set_max_framerate(imgsensor.current_fps,1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) //enable auto flicker
		imgsensor.autoflicker_en = KAL_TRUE;
	else //Cancel Auto flick
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
//	kal_int16 dummyLine;
	kal_uint32 frameHeight;
  
	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);
				if(framerate == 0)
				return ERROR_NONE;
	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frameHeight = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.pre.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.frame_length =imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(framerate == 0)
				return ERROR_NONE;
			frameHeight = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.normal_video.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;			
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);

			//set_dummy();			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		//case MSDK_SCENARIO_ID_CAMERA_ZSD:			
			frameHeight = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.cap.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.frame_length =imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
			break;	
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frameHeight = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.hs_video.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frameHeight = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.slim_video.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;			
			imgsensor.frame_length =imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();			
		default:  //coding with  preview scenario by default
			frameHeight = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = frameHeight - imgsensor_info.pre.framelength;
			if (imgsensor.dummy_line < 0)
				imgsensor.dummy_line = 0;
			imgsensor.frame_length =imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			//set_dummy();
			LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
			break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			*framerate = imgsensor_info.pre.max_framerate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*framerate = imgsensor_info.normal_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*framerate = imgsensor_info.cap.max_framerate;
			break;		
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*framerate = imgsensor_info.hs_video.max_framerate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO: 
			*framerate = imgsensor_info.slim_video.max_framerate;
			break;
		default:
			break;
	}

	return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if(enable) 	 
		write_cmos_sensor(0x0601, 0x02);
	else		  
		write_cmos_sensor(0x0601, 0x00);  

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT16 *feature_data_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	UINT32 *feature_data_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *) feature_para;
	/* unsigned long long *feature_return_para=(unsigned long long *) feature_para; */

	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *)feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {

	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
	break;


	 case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CUSTOM3:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 1;
            break;
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
        *(feature_data + 1) = imgsensor_info.min_gain;
        *(feature_data + 2) = imgsensor_info.max_gain;
        break;
    case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
        *(feature_data + 0) = imgsensor_info.min_gain_iso;
        *(feature_data + 1) = imgsensor_info.gain_step;
        *(feature_data + 2) = imgsensor_info.gain_type;
        break;
    case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
        *(feature_data + 1) = imgsensor_info.min_shutter;
        *(feature_data + 2) = imgsensor_info.exp_step;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.cap.pclk;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.normal_video.pclk;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.hs_video.pclk;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.slim_video.pclk;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = imgsensor_info.pre.pclk;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = (imgsensor_info.cap.framelength << 16)
                + imgsensor_info.cap.linelength;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = (imgsensor_info.normal_video.framelength << 16)
                + imgsensor_info.normal_video.linelength;
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = (imgsensor_info.hs_video.framelength << 16)
                + imgsensor_info.hs_video.linelength;
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = (imgsensor_info.slim_video.framelength << 16)
                + imgsensor_info.slim_video.linelength;
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data + 1))
                = (imgsensor_info.pre.framelength << 16)
                + imgsensor_info.pre.linelength;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        switch (*(feature_data + 1)) {
        case MSDK_SCENARIO_ID_CUSTOM3:
            *feature_return_para_32 = 1; /*BINNING_NONE*/
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        case MSDK_SCENARIO_ID_CUSTOM4:
        default:
            *feature_return_para_32 = 1; /*BINNING_AVERAGED*/
            break;
        }
        pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",*feature_return_para_32);
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
        *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 3000000;
        break;

		
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	/*
	 case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	 	{
	 		kal_uint32 rate;
	 
	 		switch (*feature_data) {
	 		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	 			rate = imgsensor_info.cap.mipi_pixel_rate;
	 			break;
	 		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	 			rate = imgsensor_info.normal_video.mipi_pixel_rate;
	 			break;
	 		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	 			rate = imgsensor_info.hs_video.mipi_pixel_rate;
	 			break;
	 		case MSDK_SCENARIO_ID_SLIM_VIDEO:
	 			rate = imgsensor_info.slim_video.mipi_pixel_rate;
	 			break;
	 		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	 		default:
	 			rate = imgsensor_info.pre.mipi_pixel_rate;
	 			break;
	 		}
	 		*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
	 	}
	 	break;
	*/
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter(*feature_data);
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		LOG_INF("adb_i2c_read 0x%x = 0x%x\n", sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL)*feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL)*feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: /*for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32)*feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
//		case SENSOR_FEATURE_SET_HDR:
	//		LOG_INF("ihdr enable :%d\n", *feature_data_16);
	//		spin_lock(&imgsensor_drv_lock);
//			imgsensor.ihdr_en = *feature_data_16;
	//		spin_unlock(&imgsensor_drv_lock);		
//			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			//LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", *feature_data_32);
			//wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(*(feature_data_32+1));
            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));
		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					memcpy((void *)wininfo,
						(void *)&imgsensor_winsize_info[1],
						sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					memcpy((void *)wininfo,
						(void *)&imgsensor_winsize_info[2],
						sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
					memcpy((void *)wininfo,
						(void *)&imgsensor_winsize_info[3],
						sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
					memcpy((void *)wininfo,
						(void *)&imgsensor_winsize_info[4],
						sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
					memcpy((void *)wininfo,
						(void *)&imgsensor_winsize_info[0],
						sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
			break;
		case SENSOR_FEATURE_GET_PIXEL_RATE:
			switch (*feature_data) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.cap.pclk /
					(imgsensor_info.cap.linelength - 80)) *
					imgsensor_info.cap.grabwindow_width;

				break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.normal_video.pclk /
					(imgsensor_info.normal_video.linelength - 80)) *
					imgsensor_info.normal_video.grabwindow_width;

				break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.hs_video.pclk /
					(imgsensor_info.hs_video.linelength - 80)) *
					imgsensor_info.hs_video.grabwindow_width;

				break;
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.slim_video.pclk /
					(imgsensor_info.slim_video.linelength - 80)) *
					imgsensor_info.slim_video.grabwindow_width;

				break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				default:
					*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
					(imgsensor_info.pre.pclk /
					(imgsensor_info.pre.linelength - 80)) *
					imgsensor_info.pre.grabwindow_width;
				break;
			}
			 break;
	default:
		break;
	}

	return ERROR_NONE;
}    /*    feature_control()  */


static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};


UINT32 S5K4H5YC_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	S5K4H5YC_MIPI_RAW_SensorInit	*/



