/*****************************************************************************
 ******End Date:20231205
 * Filename:M
 * ---------
 *     w1c5590fronttxd_mipiraw_sensor.c
 * 
 * 
 * r
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!20231107
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
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "c5590_mipi_raw.h"

/****************************Modify Following Strings for Debug****************************/
#define PFX "C5590_SensroDriver"
#define C5590_MAIN_DEBUG 0
#if C5590_MAIN_DEBUG
#define LOG_INF(format, args...)    pr_info(PFX "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...)    pr_err(PFX "[%s] " format, __func__, ##args)
#define LOG_DBG(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)
#else
#define LOG_INF(format, args...)
#define LOG_ERR(format, args...)
#define LOG_DBG(format, args...)
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static kal_uint32 ratio = 64;
static kal_uint32 pre_frame_ratio = 64;

static struct imgsensor_info_struct imgsensor_info = {
    .sensor_id = C5590_SENSOR_ID,
    .checksum_value = 0xf7375923,        //checksum value for Camera Auto Test
    .pre = {
        .pclk = 168000000,            //record different mode's pclk//84
        .linelength = 2800,            //record different mode's linelength
        .framelength = 2000,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2592,        //record different mode's width of grabwindow
        .grabwindow_height = 1944,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 168000000,
    },
    .cap = {
        .pclk = 168000000,            //record different mode's pclk//84
        .linelength = 2800,            //record different mode's linelength
        .framelength = 2000,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2592,        //record different mode's width of grabwindow
        .grabwindow_height = 1944,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 168000000,
    },
    .cap1 = {
        .pclk = 168000000,            //record different mode's pclk//84
        .linelength = 2800,            //record different mode's linelength
        .framelength = 2000,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2592,        //record different mode's width of grabwindow
        .grabwindow_height = 1944,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 168000000,
    },
    .normal_video = {
        .pclk = 168000000,            //record different mode's pclk//84
        .linelength = 2800,            //record different mode's linelength
        .framelength = 2000,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2592,        //record different mode's width of grabwindow
        .grabwindow_height = 1944,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 168000000,
    },
    .hs_video = {
        .pclk = 168000000,            //record different mode's pclk//84
        .linelength = 2800,            //record different mode's linelength
        .framelength = 2000,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2592,        //record different mode's width of grabwindow
        .grabwindow_height = 1944,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 168000000,
    },
    .slim_video = {
        .pclk = 168000000,            //record different mode's pclk//84
        .linelength = 2800,            //record different mode's linelength
        .framelength = 2000,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2592,        //record different mode's width of grabwindow
        .grabwindow_height = 1944,        //record different mode's height of grabwindow
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,//unit , ns //85
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
        .mipi_pixel_rate = 168000000,
    },
    .margin = 4,            //sensor framelength & shutter margin
    .min_shutter = 4,        //min shutter
    .max_frame_length = 0x7fff,//max framelength by sensor register's limitation
    .ae_shut_delay_frame = 0,    //shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
    .ae_sensor_gain_delay_frame = 1,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
    .ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
    .ihdr_support = 0,      //1, support; 0,not support
    .ihdr_le_firstline = 0,  //1,le first ; 0, se first
    .sensor_mode_num = 5,      //support sensor mode num

    .cap_delay_frame = 2,
    .pre_delay_frame = 2,
    .video_delay_frame = 2,
    .hs_video_delay_frame = 2,
    .slim_video_delay_frame = 2,

    .min_gain = 64, /*1x gain*/
    .max_gain = 1008, /*15.75x gain*/
    .min_gain_iso = 100,
    .gain_step = 1,
    .gain_type = 0,
    .i2c_speed = 400,

    .isp_driving_current = ISP_DRIVING_8MA, //mclk driving current
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = 0,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,//sensor output first pixel color
    .mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
    .mipi_lane_num = SENSOR_MIPI_2_LANE,//mipi lane num
    .i2c_addr_table = {0x6c, 0xff},//record sensor support all write id addr, only supprt 4must end with 0xff
};

static struct imgsensor_struct imgsensor = {
    .mirror = IMAGE_H_MIRROR,                //mirrorflip information
    .sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
    .shutter = 0x3D0,                    //current shutter
    .gain = 0x100,                        //current gain
    .dummy_pixel = 0,                    //current dummypixel
    .dummy_line = 0,                    //current dummyline
    .current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
    .test_pattern = KAL_FALSE,        //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
    .ihdr_en = 0, //sensor need support LE, SE with HDR feature
    .i2c_write_id = 0x6c,//record current sensor's i2c write id
};

/* Sensor output window information */
static  struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5]=
{
	{ 2592, 1944,    0,   0, 2592, 1944, 2592, 1944,      0,   0, 2592, 1944,   0,    0, 2592, 1944},  // Preview
	{ 2592, 1944,	 0,	  0, 2592, 1944, 2592, 1944,      0,   0, 2592, 1944,   0,	  0, 2592, 1944},   // capture
	{ 2592, 1944,	 0,	  0, 2592, 1944, 2592, 1944,      0,   0, 2592, 1944,   0,	  0, 2592, 1944},   //video
	{ 2592, 1944,    0,   0, 2592, 1944, 2592, 1944,      0,   0, 2592, 1944,   0,    0, 2592, 1944},  //high speed video
	{ 2592, 1944,    0,   0, 2592, 1944, 2592, 1944,      0,   0, 2592, 1944,   0,    0, 2592, 1944}
}; //slim video 

#define TEST_CAM 0
static kal_uint8 reg0_bak = 0xff;
static kal_uint8 reg1_bak = 0xff;
static kal_uint8 reg2_bak = 0xff;
static kal_uint16 reg_shutter_bak = 0xff;
static kal_uint8 save_flag = 0xff;

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

static kal_uint32 return_sensor_id(void)
{
    kal_uint32 sensor_id = 0;

    sensor_id = ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
    return sensor_id;
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	LOG_INF("@CST1205:>>> set_max_framerate():framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;

	if (min_framelength_en)
	    imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);


	//update Frame Length
	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);

	LOG_INF("@CST1205:>>> set_max_framerate():framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);
}
/*************************************************************************
* FUNCTION
*    set_shutter
*
* DESCRIPTION
*    This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*    iShutter : exposured lines
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;

	LOG_INF("@CST1205:set_shutter()_input\n");
	if(shutter% 2 == 0)
    {
        ratio = 64;
    }else{
        ratio = shutter*64/(shutter-1);
    }
	shutter = shutter - (shutter% 2);
	LOG_INF("@CST1205:func= %s \n",__func__);
	

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	//printk("@CST_100:set_shutter(),input_shutter=0x%x\n",shutter);

	// if shutter bigger than frame_length, should extend frame length first
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
	    imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
	    imgsensor.frame_length = imgsensor.min_frame_length;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
	    imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	
	LOG_INF("@CST1205:set_shutter(): shutter =%x\n", shutter);


	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	// Update Shutter
	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	//printk("@CST_101:set_shutter(),writer_shutter=0x%x\n",shutter);
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);	
	write_cmos_sensor(0x0203, (shutter) & 0xFF);
	LOG_INF("@CST1205:set_shutter(): shutter =%x, framelength =%x,mini_framelegth=%x\n", shutter,imgsensor.frame_length,imgsensor.min_frame_length);
	#if 1
	reg_shutter_bak = shutter;
	#endif

	#if TEST_CAM	//for test
	{	
		kal_uint16 Rreg0202 = 0, Rreg0203 = 0;
		Rreg0202 = (shutter >> 8) & 0xff;
		Rreg0203 = shutter & 0xff;
		
		Rreg0202 = read_cmos_sensor(0x0202);
		Rreg0203 = read_cmos_sensor(0x0203);
		LOG_INF("@CST1205:set_shutter(): READ: reg0202=0x%x, reg0203=0x%x\n", Rreg0202,Rreg0203);
	}
	#endif	
}  

//+bug 720367 qinduilin.wt, add, 2022/03/03, modify for dualcam sync
#if 1
static void set_shutter_frame_length(kal_uint16 shutter, kal_uint16 frame_length)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;
	
	LOG_INF("@CST1205:set_shutter_frame_length()_input\n");
	if(shutter% 2 == 0)
    {
        ratio = 64;
    }else{
        ratio = shutter*64/(shutter-1);
    }
	shutter = shutter - (shutter% 2);

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	/* LOG_INF("shutter =%d, frame_time =%d\n", shutter, frame_time); */

	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	/*Change frame time */
	if (frame_length > 1)
		imgsensor.frame_length = frame_length;

	/*  */
	if (shutter > imgsensor.frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	}

	/* Update Shutter */
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x0203, (shutter) & 0xFF);
	LOG_INF("@CST1205:@CSTdebug  shutter =%d, framelength =%d/%d \n", shutter,
		imgsensor.frame_length, frame_length);
}
#endif

/*************************************************************************
* FUNCTION
*    set_gain
*
* DESCRIPTION
*    This function is to set global gain to sensor.
*
* PARAMETERS
*    iGain : sensor global gain(base: 0x40)
*
* RETURNS
*    the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
#define AGAIN_NUM    128
static	kal_uint16	again_table[AGAIN_NUM] = 
{
//     0   1    2    3    4    5    6    7    8    9   10  11    12   13   14   15   16   17   18  19   20    21   22   23   24   25   26
     64,  66,  68,  70,  72,  74,  76,  78,  80,  82,  84,  86,  88,  90,  92,  94,  96,  98, 100, 102, 104, 106, 108, 110, 112, 114, 116,
    118, 120, 122, 124, 126, 128, 132, 136, 140, 144, 148, 152, 156, 160, 164, 168, 172, 176, 180, 184, 188, 192, 196, 200, 204, 208, 212,
    216, 220, 224, 228, 232, 236, 240, 244, 248, 252, 256, 264, 272, 280, 288, 296, 304, 312, 320, 328, 336, 344, 352, 360, 368, 376, 384,
    392, 400, 408, 416, 424, 432, 440, 448, 456, 464, 472, 480, 488, 496, 504, 512, 528, 544, 560, 576, 592, 608, 624, 640, 656, 672, 688,
    704, 720, 736, 752, 768, 784, 800, 816, 832, 848, 864, 880, 896, 912, 928, 944, 960, 976, 992, 1008, //gain total
};    //gain
static	kal_uint16	again_register_table_1[AGAIN_NUM] = 
{ 
//    	0     1     2     3     4     5     6    7      8     9    10    11    12    13    14    15    16    17    18    19    20    21    22    23    24    25    26
	  0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	0x07, 0x07,	
	  0x07, 0x07, 0x07, 0x07, 0x07, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,	0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 
	  0x03,	0x03, 0x03,	0x03, 0x03,	0x03, 0x03,	0x03, 0x03,	0x03, 0x01,	0x01, 0x01,	0x01, 0x01,	0x01, 0x01,	0x01, 0x01,	0x01, 0x01,	0x01, 0x01,	0x01, 0x01,	0x01, 0x01,	
	  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	  0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 0x00,	0x00, 
}; //0x3293[2:0] 0xe00e
	
static	kal_uint16  again_register_table_2[AGAIN_NUM] = 
{ 
//		0	  1 	2	  3 	4	  5 	6	 7		8	  9    10	 11    12	 13    14	 15    16	 17    18	 19    20	 21    22	 23    24	 25    26
	  0x20,	0x21, 0x22,	0x23, 0x24,	0x25, 0x26,	0x27, 0x28,	0x29, 0x2A,	0x2B, 0x2C,	0x2D, 0x2E,	0x2F, 0x30,	0x31, 0x32,	0x33, 0x34,	0x35, 0x36,	0x37, 0x38,	0x39, 0x3A,	
	  0x3B,	0x3C, 0x3D,	0x3E, 0x3F,	0x20, 0x21,	0x22, 0x23,	0x24, 0x25,	0x26, 0x27,	0x28, 0x29,	0x2A, 0x2B,	0x2C, 0x2D,	0x2E, 0x2F,	0x30, 0x31,	0x32, 0x33,	0x34, 0x35,	
	  0x36,	0x37, 0x38,	0x39, 0x3A,	0x3B, 0x3C,	0x3D, 0x3E,	0x3F, 0x20,	0x21, 0x22,	0x23, 0x24,	0x25, 0x26,	0x27, 0x28,	0x29, 0x2A,	0x2B, 0x2C,	0x2D, 0x2E,	0x2F, 0x30,	
	  0x31,	0x32, 0x33,	0x34, 0x35,	0x36, 0x37,	0x38, 0x39,	0x3A, 0x3B,	0x3C, 0x3D,	0x3E, 0x3F,	0x20, 0x21,	0x22, 0x23,	0x24, 0x25,	0x26, 0x27,	0x28, 0x29,	0x2A, 0x2B,	
	  0x2C,	0x2D, 0x2E, 0x2F, 0x30,	0x31, 0x32,	0x33, 0x34,	0x35, 0x36,	0x37, 0x38,	0x39, 0x3A,	0x3B, 0x3C,	0x3D, 0x3E,	0x3F,	
}; //0x32a9[5:0] 0xe011
	
static	kal_uint16  again_register_table_3[AGAIN_NUM] = 
{ 
//		0	  1 	2	  3 	4	  5 	6	 7		8	  9    10	 11    12	 13    14	 15    16	 17    18	 19    20	 21    22	 23    24	 25    26
	  0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe4, 0xe4, 0xe4, 0xe4, 0xe4, 0xe4, 0xe4, 0xe4, 0xe4, 0xe4, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe7,
	  0xe7, 0xe7, 0xe7, 0xe7, 0xe7, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xe6, 0xec, 0xec, 0xec, 0xec, 0xec, 0xec, 0xec, 0xec, 0xec, 0xec, 0xf0, 0xf0, 0xf0, 0xf0,
	  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	  0xf2, 0xf2, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf4, 0xf2, 0xf2, 0xf2, 0xf2, 0xf5, 0xf5, 0xf5, 0xf6, 0xf6, 0xf7, 0xf7, 0xf7,
	  0xf8, 0xf8, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xf9, 0xfa, 0xfa, 0xfb, 0xfb, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0xff, 0xff, 

}; //0x32ac[7:0] 0xe014

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 tmp0 = 0, tmp1 = 0, tmp2 = 0, Again_base = 0;
	kal_uint16 iGain=gain*pre_frame_ratio/64;
	#if TEST_CAM
    kal_uint16 reg3293 = 0, reg32a9 = 0, reg32ac = 0;
	#endif
	kal_uint16 i = 0;
    LOG_INF("@CST1205:set_gain()_input,gain=%d,iGain=%d\n",gain,iGain);

    #if TEST_CAM
	reg3293 =read_cmos_sensor(0x3293);
	reg32a9 =read_cmos_sensor(0x32a9);
	reg32ac =read_cmos_sensor(0x32ac);
	LOG_INF("@CST1205:set_gain(),before_write,R,0x3293=0x%x,0x32a9=0x%x,0x32ac=0x%x\n", reg3293,reg32a9,reg32ac);  //max=fc   252
	#endif

    
    if(iGain >= again_table[AGAIN_NUM-1]){
        iGain = again_table[AGAIN_NUM-1];
        Again_base = AGAIN_NUM-1;
    }
    if(iGain <= 64){
        iGain = again_table[0];
    }

       for(i=1; i < AGAIN_NUM; i++){
        if(iGain < again_table[i]){
            Again_base = i - 1;
            break;
        }
    }
	tmp0 = again_register_table_1[Again_base];      //0x3293[2:0] 0xe00e
	tmp1 = again_register_table_2[Again_base];      //0x32a9[5:0] 0xe011
	tmp2 = again_register_table_3[Again_base];      //0x32ac[7:0] 0xe014

	#if 1	//bakup first Again value
	reg0_bak = tmp0;	//BAK 0x3293
	reg1_bak = tmp1;	//BAK 0x32a9
	reg2_bak = tmp2;	//BAK 0x32ac	
	LOG_INF("@CST1205:set_gain(), Save_Bak_Gain,0x3293=0x%x,0x32a9=0x%x,0x32ac=0x%x\n\n", tmp0,tmp1,tmp2);
	#endif
	
	write_cmos_sensor(0xe00c,0x32);
	write_cmos_sensor(0xe00d,0x93);					   
	write_cmos_sensor(0xe00e,tmp0);
	write_cmos_sensor(0xe00f,0x32);
	write_cmos_sensor(0xe010,0xa9);
	write_cmos_sensor(0xe011,tmp1);
	write_cmos_sensor(0xe012,0x32);
	write_cmos_sensor(0xe013,0xac);
	write_cmos_sensor(0xe014,tmp2);
	write_cmos_sensor(0x340f,0x11); //group delay 1frame write gain
	LOG_INF("@CST1205:set_gain(),W,0xe00e=0x%x,0xe011=0x%x,0xe014=0x%x\n", tmp0,tmp1,tmp2);  //max=fc   252
	pre_frame_ratio = ratio;
	#if TEST_CAM
	reg3293 =read_cmos_sensor(0x3293);
	reg32a9 =read_cmos_sensor(0x32a9);
	reg32ac =read_cmos_sensor(0x32ac);
	LOG_INF("@CST1205:set_gain(),R,0x3293=0x%x,0x32a9=0x%x,0x32ac=0x%x\n", reg3293,reg32a9,reg32ac);  //max=fc   252
	//LOG_INF("@CST1205:@CST_001:set_gain(),W,32a9=0x%x,32ac=0x%x,32ad=0x%x,3211=0x%x,3216=0x%x,3217=0x%x\n",tmp0,tmp1,tmp2,tmp3,tmp4,tmp5);
	#endif
	return gain;
}

/*************************************************************************
* FUNCTION
*    night_mode
*
* DESCRIPTION
*    This function night mode of sensor.
*
* PARAMETERS
*    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*    None
* 
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}    /*    night_mode    */

static void sensor_init(void)
{
	LOG_INF("@CST1203:@5590: sensor_init\n");
	//modify from spreadtrum initial param by cista yjp,20211110
	//--init start--
	
#if 1	//ori	
	write_cmos_sensor(0x0103,0x01);
	write_cmos_sensor(0x0101,0x00);
	write_cmos_sensor(0x3240,0xC0);
	write_cmos_sensor(0x32aa,0x06);
	write_cmos_sensor(0x32ab,0x08);
	write_cmos_sensor(0x3290,0x80);
	write_cmos_sensor(0x3287,0x14);
	write_cmos_sensor(0x3286,0x46);
	write_cmos_sensor(0x3298,0x44);
	write_cmos_sensor(0x3280,0x4c);
	write_cmos_sensor(0x323f,0xa3);
	write_cmos_sensor(0x3219,0x54);
	write_cmos_sensor(0x3295,0x8a);
	write_cmos_sensor(0x3299,0x28);
	write_cmos_sensor(0x329a,0x22);
	write_cmos_sensor(0x3288,0x38);
	write_cmos_sensor(0x0307,0x69);
	write_cmos_sensor(0x3211,0x12);
	write_cmos_sensor(0x3212,0x0f);
	write_cmos_sensor(0x3215,0x24);
	write_cmos_sensor(0x3216,0x55);
	write_cmos_sensor(0x3218,0x1c);
	write_cmos_sensor(0x3223,0x53);
	write_cmos_sensor(0x3224,0x40);
	write_cmos_sensor(0x322c,0x04);
	write_cmos_sensor(0x323c,0x09);
	write_cmos_sensor(0x323d,0x07);
	///<!-- if no mirror,0x0101=0x00/0x02,0x3180=0x09,0x3c14=0x00; if mirror 0x0101=0x01/0x03,0x3180=0x06,0x3c14=0x0c -->
	write_cmos_sensor(0x3180,0x09);
	write_cmos_sensor(0x3c14,0x00);
	write_cmos_sensor(0x0342,0x0a);
	write_cmos_sensor(0x0343,0xf0);
	write_cmos_sensor(0x3182,0x08);
	write_cmos_sensor(0x328c,0x01);	//0x01
	write_cmos_sensor(0x328d,0x08);	//0x08
	write_cmos_sensor(0x328f,0x32);
	write_cmos_sensor(0x3089,0x1c);
	write_cmos_sensor(0x3505,0x01);
	write_cmos_sensor(0x3500,0x10);
	write_cmos_sensor(0x3584,0x02);
	write_cmos_sensor(0x3400,0x80);
	write_cmos_sensor(0x3401,0x01);
	write_cmos_sensor(0x3405,0x03);
	write_cmos_sensor(0xe00c,0x32);
	write_cmos_sensor(0xe00d,0x93);
	write_cmos_sensor(0xe00e,0x07);
	write_cmos_sensor(0xe00f,0x32);
	write_cmos_sensor(0xe010,0xa9);
	write_cmos_sensor(0xe011,0x20);
	write_cmos_sensor(0xe012,0x32);
	write_cmos_sensor(0xe013,0xac);
	write_cmos_sensor(0xe014,0xfc);
	//<!-- for power noise 0x3584=0x22->0x20 -->
	write_cmos_sensor(0x3584,0x20);
	write_cmos_sensor(0x3500,0x00);
	write_cmos_sensor(0x3122,0x40);
	write_cmos_sensor(0x3113,0xd3);
	write_cmos_sensor(0x380e,0x31);
	write_cmos_sensor(0x32ac,0xf9);
	write_cmos_sensor(0x3293,0x01);
	write_cmos_sensor(0x32a9,0x20);
	write_cmos_sensor(0x0202,0x06);
	write_cmos_sensor(0x0203,0xea);
	//<!--init end-->
	write_cmos_sensor(0x0340,0x07);
	write_cmos_sensor(0x0341,0xd0);
	write_cmos_sensor(0x0342,0x0a);	//0x0a
	write_cmos_sensor(0x0343,0xf0);
	
	write_cmos_sensor(0x0309,0x0a);	
	write_cmos_sensor(0x0100,0x00);
#endif	
}

static void preview_setting(kal_uint16 currefps)
{
	LOG_INF("@CST1203:@5590: preview_setting\n");
    //fullsize    	
	//<!--2592x1944-->
	write_cmos_sensor(0x034c,0x0a);
	write_cmos_sensor(0x034d,0x20);
	write_cmos_sensor(0x034e,0x07);
	write_cmos_sensor(0x034f,0x98);
	write_cmos_sensor(0x3008,0x00);
	write_cmos_sensor(0x3009,0x04);
	write_cmos_sensor(0x300a,0x00);
	write_cmos_sensor(0x300b,0x04);
	//<!--2592x1944-->
	write_cmos_sensor(0x3021,0x1d);
	write_cmos_sensor(0x3022,0x01);
	write_cmos_sensor(0x3210,0x12);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x0304,0x00);	//0x00	
	write_cmos_sensor(0x3805,0x0a);
	write_cmos_sensor(0x3806,0x07);
	write_cmos_sensor(0x3807,0x07);
	write_cmos_sensor(0x3808,0x1e);
	write_cmos_sensor(0x3809,0x0c);
	write_cmos_sensor(0x380a,0x08);
	write_cmos_sensor(0x380b,0xec);
	write_cmos_sensor(0x380e,0x31);
	write_cmos_sensor(0x3812,0x07);
	write_cmos_sensor(0x3813,0x0e);
	write_cmos_sensor(0x0100,0x00);
}

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("@CST1203:@5590: capture_setting\n");
	#if 1 //add by cista_yjp,20231106(flow:oepn_init_streamON_captureseting)    	       
	//fullsize
	//<!--2592x1944-->
	write_cmos_sensor(0x034c,0x0a);
	write_cmos_sensor(0x034d,0x20);
	write_cmos_sensor(0x034e,0x07);
	write_cmos_sensor(0x034f,0x98);
	write_cmos_sensor(0x3008,0x00);
	write_cmos_sensor(0x3009,0x04);
	write_cmos_sensor(0x300a,0x00);
	write_cmos_sensor(0x300b,0x04);
	#if 1 //ori param
	//<!--2592x1944-->
	write_cmos_sensor(0x3021,0x1d);
	write_cmos_sensor(0x3022,0x01);
	write_cmos_sensor(0x3210,0x12);
	write_cmos_sensor(0x0387,0x01);
	write_cmos_sensor(0x0304,0x00);	//0x00
	write_cmos_sensor(0x3805,0x0a);
	write_cmos_sensor(0x3806,0x07);
	write_cmos_sensor(0x3807,0x07);
	write_cmos_sensor(0x3808,0x1e);
	write_cmos_sensor(0x3809,0x0c);
	write_cmos_sensor(0x380a,0x08);
	write_cmos_sensor(0x380b,0xec);
	write_cmos_sensor(0x380e,0x31);
	write_cmos_sensor(0x3812,0x07);
	write_cmos_sensor(0x3813,0x0e);
	write_cmos_sensor(0x0100,0x00);
	#endif
	#endif
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("@CST1205:@5590: normal_video_setting\n");
    capture_setting(currefps);
}

static void hs_video_setting(kal_uint16 currefps)
#if 1
{
	LOG_INF("@CST1205:@5590: hs_video_setting\n");
    
}
#endif
static void slim_video_setting(kal_uint16 currefps)
#if 1
{
	LOG_INF("@CST1205:@5590: slim_video_setting\n");
    
}
#endif
static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
     LOG_INF("@CST1205:func= %s \n",__func__);
     LOG_INF("@CST1205:enable: %d\n", enable);
    spin_lock(&imgsensor_drv_lock);
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

//-bug604664 qinduilin, add, 2021/11/12,c5590 eeprom bring up
/*************************************************************************
* FUNCTION
*    get_imgsensor_id
*
* DESCRIPTION
*    This function get the sensor ID
*
* PARAMETERS
*    *sensorID : return the sensor ID
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
//extern u32 pinSetIdx;
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
	LOG_INF("@CST1205:get_imgsensor_id()_inptutf\n");
	//printk("c5590 get_imgsensor_id pinSetIdx = %d\n ",pinSetIdx);
	//if(pinSetIdx)
		//return ERROR_SENSOR_CONNECT_FAIL;
	
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = return_sensor_id();
            if (*sensor_id == imgsensor_info.sensor_id) {
                printk("000 @CST1205:c5590_MAIN get_imgsensor_id OK,i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
                return ERROR_NONE;
            }
            printk("000 @CST1205:[@CST1018_MAIN]c5590_MAIN get_imgsensor_id fail, write id: 0x%x, id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        // if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}
/*************************************************************************
* FUNCTION
*    open
*
* DESCRIPTION
*    This function initialize the registers of CMOS sensor
*
* PARAMETERS
*    None
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint32 sensor_id = 0;
    LOG_INF("\n\n\n\n\n\n\n@@@@@@@@@@@@CST\n");
    LOG_INF("@CST1205:open()_input\n");
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();//((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
            if (sensor_id == imgsensor_info.sensor_id) {
                printk("111 @CST1205:C5590_MAIN open:Read sensor id OK,i2c write id: 0x%X, sensor id: 0x%X\n", imgsensor.i2c_write_id,sensor_id);
                break;
            }
            printk("111 @CST1205:C5590_MAIN Read sensor id fail, write id: 0x%X, id: 0x%X\n", imgsensor.i2c_write_id,sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }

    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;

    /* initail sequence write in  */
    sensor_init();

    spin_lock(&imgsensor_drv_lock);
    imgsensor.autoflicker_en= KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
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

    save_flag = 0xa5;

    LOG_INF("@CST1205:open()_end\n");

    return ERROR_NONE;
}    /*    open  */

/*************************************************************************
* FUNCTION
*    close
*
* DESCRIPTION
*
*
* PARAMETERS
*    None
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
    /*No Need to implement this function*/
    return ERROR_NONE;
}    /*    close  */

/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*    This function start the sensor preview.
*
* PARAMETERS
*    *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.autoflicker_en = KAL_FALSE;

    imgsensor.pclk= imgsensor_info.pre.pclk;
    imgsensor.line_length=imgsensor_info.pre.linelength;
    imgsensor.frame_length=imgsensor_info.pre.framelength;

    imgsensor.min_frame_length = imgsensor_info.pre.framelength;

    spin_unlock(&imgsensor_drv_lock);

    preview_setting(imgsensor.current_fps);
    return ERROR_NONE;
}    /*    preview   */

/*************************************************************************
* FUNCTION
*    capture
*
* DESCRIPTION
*    This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*    None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                          MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

    imgsensor.pclk= imgsensor_info.cap.pclk;
    imgsensor.line_length=imgsensor_info.cap.linelength;
    imgsensor.frame_length=imgsensor_info.cap.framelength;

    imgsensor.min_frame_length = imgsensor_info.cap.framelength;
    spin_unlock(&imgsensor_drv_lock);

    capture_setting(imgsensor.current_fps);
    return ERROR_NONE;
}    /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
    imgsensor.autoflicker_en = KAL_FALSE;

    imgsensor.pclk= imgsensor_info.normal_video.pclk;
    imgsensor.line_length=imgsensor_info.normal_video.linelength;
    imgsensor.frame_length=imgsensor_info.normal_video.framelength;

    imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    spin_unlock(&imgsensor_drv_lock);

    normal_video_setting(imgsensor.current_fps);
    return ERROR_NONE;
}    /*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.autoflicker_en = KAL_FALSE;
    imgsensor.pclk= imgsensor_info.hs_video.pclk;
    imgsensor.line_length=imgsensor_info.hs_video.linelength;
    imgsensor.frame_length=imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting(imgsensor.current_fps);

    return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.autoflicker_en = KAL_FALSE;

    imgsensor.pclk= imgsensor_info.slim_video.pclk;
    imgsensor.line_length=imgsensor_info.slim_video.linelength;
    imgsensor.frame_length=imgsensor_info.slim_video.framelength;

    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting(imgsensor.current_fps);

    return ERROR_NONE;
}    /*    slim_video     */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;

    sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;

    return ERROR_NONE;
}    /*    get_resolution    */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
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
    sensor_info->SensorHightSampling = 0;    // 0 is default 1x
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
}    /*    get_info  */

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
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
            preview(image_window, sensor_config_data);
            return ERROR_INVALID_SCENARIO_ID;
    }

    return ERROR_NONE;
}    /* control() */

static kal_uint32 set_video_mode(UINT16 framerate)
{
    // SetVideoMode Function should fix framerate
    if (framerate == 0)
        // Dynamic frame rate
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);

    set_max_framerate(imgsensor.current_fps,1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
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
    //kal_uint32 frame_length;
    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            imgsensor.pclk= imgsensor_info.pre.pclk;
            imgsensor.line_length=imgsensor_info.pre.linelength;
            imgsensor.frame_length=imgsensor_info.pre.framelength;
            imgsensor.current_fps=framerate;
            imgsensor.min_frame_length = imgsensor_info.pre.framelength;
            break;

        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            imgsensor.pclk= imgsensor_info.normal_video.pclk;
            imgsensor.line_length=imgsensor_info.normal_video.linelength;
            imgsensor.frame_length=imgsensor_info.normal_video.framelength;
            imgsensor.current_fps=framerate;
            imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
            break;

        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            imgsensor.pclk= imgsensor_info.cap.pclk;
            imgsensor.line_length=imgsensor_info.cap.linelength;
            imgsensor.frame_length=imgsensor_info.cap.framelength;
            imgsensor.current_fps=framerate;
            imgsensor.min_frame_length = imgsensor_info.cap.framelength;
            break;

        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            imgsensor.pclk= imgsensor_info.hs_video.pclk;
            imgsensor.line_length=imgsensor_info.hs_video.linelength;
            imgsensor.frame_length=imgsensor_info.hs_video.framelength;
            imgsensor.current_fps=framerate;
            imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
            break;

        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            imgsensor.pclk= imgsensor_info.slim_video.pclk;
            imgsensor.line_length=imgsensor_info.slim_video.linelength;
            imgsensor.frame_length=imgsensor_info.slim_video.framelength;
            imgsensor.current_fps=framerate;
            imgsensor.min_frame_length = imgsensor.frame_length;
            break;

        default:  //coding with  preview scenario by default
            imgsensor.pclk= imgsensor_info.pre.pclk;
            imgsensor.line_length=imgsensor_info.pre.linelength;
            imgsensor.frame_length=imgsensor_info.pre.framelength;
            imgsensor.current_fps=framerate;
            imgsensor.min_frame_length = imgsensor_info.pre.framelength;
            break;
    }

    set_max_framerate(imgsensor.current_fps,1);

    return ERROR_NONE;
}

static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
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

static kal_uint32 streaming_control(kal_bool enable)
{
	#if TEST_CAM	//for test
	kal_uint16 R_reg3293 = 0, R_reg32a9 = 0, R_reg32ac = 0, R_reg0202 = 0, R_reg0203 = 0;
	#endif
	LOG_INF("@CST1205:streaming_control(),on_off=%d\n",enable);
	///enable = 1;
	if (enable) 
	{
		write_cmos_sensor(0x0100, 0X01); // stream on
		LOG_INF("@CST1205:streaming_on\n");		
		mdelay(5);						//add by cista_yjp,20231205
		write_cmos_sensor(0x3299,0x2b);	//add by cista_yjp,20231205
		mdelay(1);						//add by cista_yjp,20231205
		write_cmos_sensor(0x3299,0x28);	//add by cista_yjp,20231205
		mdelay(5);						//add by cista_yjp,20231205
		if(save_flag == 0xa5)
		{
			save_flag = 0;
			#if TEST_CAM	//for test
			LOG_INF("@CST1205:tream_on(): BAK:reg3293=0x%x, reg32a9=0x%x,reg32ac=0x%x,reg_shutter_bak=0x%x\n", reg0_bak,reg1_bak,reg2_bak,reg_shutter_bak);
			#endif

			#if TEST_CAM	//for test
			R_reg32ac = read_cmos_sensor(0x32ac);
			R_reg3293 = read_cmos_sensor(0x3293);
			R_reg32a9 = read_cmos_sensor(0x32a9);
			R_reg0202 = read_cmos_sensor(0x0202);
			R_reg0203 = read_cmos_sensor(0x0203);
			LOG_INF("@CST1205:stream_on(): before_WT:reg3293=0x%x, reg32a9=0x%x,reg32ac=0x%x,reg0202=0x%x,reg0203=0x%x\n", R_reg3293,R_reg32a9,R_reg32ac,R_reg0202,R_reg0203);
			#endif
			write_cmos_sensor(0xe00c,0x32);
			write_cmos_sensor(0xe00d,0x93);
			write_cmos_sensor(0xe00e,reg0_bak);
			write_cmos_sensor(0xe00f,0x32);
			write_cmos_sensor(0xe010,0xa9);
			write_cmos_sensor(0xe011,reg1_bak);
			write_cmos_sensor(0xe012,0x32);
			write_cmos_sensor(0xe013,0xac);
			write_cmos_sensor(0xe014,reg2_bak);
			
			write_cmos_sensor(0x340f, 0x09);		//gain effective at once

			write_cmos_sensor(0x0202, (reg_shutter_bak >> 8) & 0xFF);	
			write_cmos_sensor(0x0203, (reg_shutter_bak) & 0xFF);	
			
            mdelay(110);
			
			#if TEST_CAM	//for test
			R_reg32ac = read_cmos_sensor(0x32ac);
			R_reg3293 = read_cmos_sensor(0x3293);
			R_reg32a9 = read_cmos_sensor(0x32a9);
			R_reg0202 = read_cmos_sensor(0x0202);
			R_reg0203 = read_cmos_sensor(0x0203);
			LOG_INF("@CST1205:stream_on(): after_WT:reg3293=0x%x, reg32a9=0x%x,reg32ac=0x%x,reg0202=0x%x,reg0203=0x%x\n", R_reg3293,R_reg32a9,R_reg32ac,R_reg0202,R_reg0203);
			#endif
		}
		///mdelay(110);	
	} 
	else 
	{
		write_cmos_sensor(0x0100, 0X00); // stream off
		LOG_INF("@CST1205:streaming_off\n");
		mdelay(10);
       }	
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16=(UINT16 *) feature_para;
    UINT16 *feature_data_16=(UINT16 *) feature_para;
    UINT32 *feature_return_para_32=(UINT32 *) feature_para;
    UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
    switch (feature_id) {
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
        //*(feature_data + 2) = imgsensor_info.exp_step;
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
    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
        set_shutter_frame_length(
            (UINT16) *feature_data, (UINT16) *(feature_data + 1));
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
    case SENSOR_FEATURE_SET_ESHUTTER:
        set_shutter(*feature_data);
    break;
    case SENSOR_FEATURE_SET_NIGHTMODE:
        night_mode((BOOL) * feature_data);
    break;
    case SENSOR_FEATURE_SET_GAIN:
        set_gain((UINT16) *feature_data);
    break;
    case SENSOR_FEATURE_SET_FLASHLIGHT:
    break;
    case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
    break;
    case SENSOR_FEATURE_SET_REGISTER:
        write_cmos_sensor(sensor_reg_data->RegAddr,
            sensor_reg_data->RegData);
    break;
    case SENSOR_FEATURE_GET_REGISTER:
        sensor_reg_data->RegData =
            read_cmos_sensor(sensor_reg_data->RegAddr);
    break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
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
        set_auto_flicker_mode((BOOL)*feature_data_16,
            *(feature_data_16+1));
    break;
    case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
        set_max_framerate_by_scenario(
            (enum MSDK_SCENARIO_ID_ENUM)*feature_data,
            *(feature_data+1));
    break;
    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
        get_default_framerate_by_scenario(
            (enum MSDK_SCENARIO_ID_ENUM)*(feature_data),
            (MUINT32 *)(uintptr_t)(*(feature_data+1)));
    break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
        set_test_pattern_mode((BOOL)*feature_data);
    break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
        *feature_return_para_32 = imgsensor_info.checksum_value;
        *feature_para_len = 4;
    break;
    case SENSOR_FEATURE_SET_FRAMERATE:
	    spin_lock(&imgsensor_drv_lock);
	    imgsensor.current_fps = (UINT16)*feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        LOG_INF("current fps :%d\n", imgsensor.current_fps);
    break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INF("GET_CROP_INFO scenarioId:%d\n",
            *feature_data_32);

        wininfo = (struct  SENSOR_WINSIZE_INFO_STRUCT *)
            (uintptr_t)(*(feature_data+1));
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
        LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",
            (UINT16)*feature_data, (UINT16)*(feature_data+1),
            (UINT16)*(feature_data+2));
        //   ihdr_write_shutter_gain((UINT16)*feature_data,
            //    (UINT16)*(feature_data+1),
            //    (UINT16)*(feature_data+2));
    break;
    case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
            switch (*feature_data) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.cap.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.normal_video.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.hs_video.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_SLIM_VIDEO:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.slim_video.mipi_pixel_rate;
                break;
            case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            default:
                *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
                    imgsensor_info.pre.mipi_pixel_rate;
                break;
            }
    break;
#ifdef FPT_PDAF_SUPPORT
/******************** PDAF START ********************/
    case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
            break;
        default:
            *(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PDAF_INFO:
        PDAFinfo = (struct SET_PD_BLOCK_INFO_T *)
            (uintptr_t)(*(feature_data+1));

        switch (*feature_data) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            memcpy((void *)PDAFinfo, (void *)&imgsensor_pd_info,
                sizeof(struct SET_PD_BLOCK_INFO_T));
            break;
        default:
            break;
        }
        break;
    case SENSOR_FEATURE_GET_VC_INFO:
        LOG_INF("@CST1205:SENSOR_FEATURE_GET_VC_INFO %d\n",
            (UINT16) *feature_data);
        pvcinfo =
        (struct SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

        switch (*feature_data_32) {
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            LOG_INF("@CST1205:Jesse+ CAPTURE_JPEG \n");
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
                   sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            LOG_INF("@CST1205:Jesse+ VIDEO_PREVIEW \n");
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
                   sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        default:
            LOG_INF("@CST1205:Jesse+ CAMERA_PREVIEW \n");
            memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
                   sizeof(struct SENSOR_VC_INFO_STRUCT));
            break;
        }
        break;
    case SENSOR_FEATURE_GET_PDAF_DATA:
        break;
    case SENSOR_FEATURE_SET_PDAF:
            imgsensor.pdaf_mode = *feature_data_16;
        break;
/******************** PDAF END ********************/
    //+bug 558061,zhanglinfeng.wt, modify, 2020/07/02, modify codes for factory mode of photo black screen
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
        /*
        * 1, if driver support new sw frame sync
        * set_shutter_frame_length() support third para auto_extend_en
        */
        *(feature_data + 1) = 1;
        /* margin info by scenario */
        *(feature_data + 2) = imgsensor_info.margin;
        break;
    //-bug 558061,zhanglinfeng.wt, modify, 2020/07/02, modify codes for factory mode of photo black screen
#endif
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
        streaming_control(KAL_FALSE);
        break;

    case SENSOR_FEATURE_SET_STREAMING_RESUME:
        if (*feature_data != 0)
            set_shutter(*feature_data);
        streaming_control(KAL_TRUE);
        break;
    //+bug 558061, zhanglinfeng.wt, modify, 2020/07/02, modify codes for factory mode of photo black screen
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
            //+bug 558061, shaozhuchao.wt, modify, 2020/07/23, modify codes for main camera hw remosaic
            *feature_return_para_32 = 1; /*BINNING_AVERAGED*/
            //-bug 558061, shaozhuchao.wt, modify, 2020/07/23, modify codes for main camera hw remosaic
            break;
        }
        LOG_INF("@CST1205:SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
            *feature_return_para_32);
            *feature_para_len = 4;
        break;
    //-bug 558061, zhanglinfeng.wt, modify, 2020/07/02, modify codes for factory mode of photo black screen
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

UINT32 C5590_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* To Do : Check Sensor status here */
    LOG_INF("@CST1205:ENTER!!!\n");
    if (pfFunc != NULL)
        *pfFunc = &sensor_func;
    return ERROR_NONE;

}
