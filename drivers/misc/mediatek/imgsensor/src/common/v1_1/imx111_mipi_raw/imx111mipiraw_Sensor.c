/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX111mipiraw_sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 * sensor setting : AM11
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
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx111mipiraw_Sensor.h"

#define PFX "IMX111"



//#define LOG_WRN(format, args...) xlog_printk(ANDROID_LOG_WARN ,PFX, "[%S] " format, __FUNCTION__, ##args)
//#defineLOG_INF(format, args...) xlog_printk(ANDROID_LOG_INFO ,PFX, "[%s] " format, __FUNCTION__, ##args)
//#define LOG_DBG(format, args...) xlog_printk(ANDROID_LOG_DEBUG ,PFX, "[%S] " format, __FUNCTION__, ##args)
#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);



static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX111_SENSOR_ID,		//record sensor id defined in Kd_imgsensor.h

	.checksum_value = 0xd6d43c1f,		//checksum value for Camera Auto Test

	.pre = {
		.pclk = 199200000,
		.linelength = 3536,
		.framelength = 2516,
		.startx = 8,
		.starty = 8,
		.grabwindow_width  = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 220,
		//.mipi_pixel_rate = 676800000,
	},
	.cap = {
		.pclk = 199200000,
		.linelength = 3536,
		.framelength = 2516,
		.startx = 8,
		.starty = 8,
		.grabwindow_width  = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 220,
		//.mipi_pixel_rate = 676800000,
	},
	.cap1 = {
		.pclk = 199200000,
		.linelength = 3536,
		.framelength = 2516,
		.startx = 8,
		.starty = 8,
		.grabwindow_width  = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 220,
		//.mipi_pixel_rate = 676800000,
	},
	.normal_video = {
		.pclk = 199200000,
		.linelength = 3536,
		.framelength = 2516,
		.startx = 8,
		.starty = 8,
		.grabwindow_width  = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 220,
		//.mipi_pixel_rate = 676800000,
	},
	.hs_video = {
		.pclk = 134400000,
		.linelength = 3536,
		.framelength = 1254,
		.startx = 4,
		.starty = 4,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
		//.mipi_pixel_rate = 676800000,
	},
	.slim_video = {
		.pclk = 134400000,
		.linelength = 3536,
		.framelength = 1254,
		.startx = 4,
		.starty = 4,
		.grabwindow_width = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 23,
		.max_framerate = 300,
		//.mipi_pixel_rate = 676800000,
	},
	.margin = 6,			//sensor framelength & shutter margin
	.min_shutter = 4,		//min shutter
	.min_gain = 64,
	.max_gain = 1024,
	.min_gain_iso = 100,
	.exp_step = 2,
	.gain_step = 1,
	.gain_type = 2,
	.max_frame_length = 0xffff,//max framelength by sensor register's limitation
	.ae_shut_delay_frame = 0,	//shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num ,don't support Slow motion

	.cap_delay_frame = 2,//3,		//enter capture delay frame num
	.pre_delay_frame = 2,//3, 		//enter preview delay frame num
	.video_delay_frame = 2,//3,		//enter video delay frame num
	.hs_video_delay_frame = 2,//3,	//enter high speed video  delay frame num
	.slim_video_delay_frame = 2,//3,//enter slim video delay frame num
	.frame_time_delay_frame = 2,

	.isp_driving_current = ISP_DRIVING_8MA, //mclk driving current
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = 0, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	#ifdef CONFIG_IMX111_MIRROR_HV
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	#else
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,//B//sensor output first pixel color
	#endif
	.mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_2_LANE,//mipi lane num
	.i2c_addr_table = {0x20, 0x40, 0xff},//record sensor support all write id addr, only supprt 4must end with 0xff  //2015-1-14 sp_miao
    .i2c_speed = 200, // i2c read/write speed
};


static struct imgsensor_struct imgsensor = {
	#ifdef CONFIG_IMX111_MIRROR_HV
	.mirror = IMAGE_HV_MIRROR,
	#else
    .mirror = IMAGE_NORMAL,             //mirrorflip information
    #endif
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0,					//current shutter
	.gain = 0x100,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,//record current sensor's i2c write id

};


/* Sensor output window information*/
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =
{{ 3280, 2464, 0, 0, 3280, 2464, 3280, 2464, 0, 0, 3280, 2464,  8, 8, 3264, 2448}, // Preview
 { 3280, 2464, 0, 0, 3280, 2464, 3280, 2464, 0, 0, 3280, 2464,  8, 8, 3264, 2448}, // capture
 { 3280, 2464, 0, 0, 3280, 2464, 3280, 2464, 0, 0, 3280, 2464,  8, 8, 3264, 2448}, // video
 { 3280, 2464, 0, 0, 3280, 2464, 1640, 1232, 0, 0, 1640, 1232,  4, 4, 1632, 1224}, //hight speed video
 { 3280, 2464, 0, 0, 3280, 2464, 1640, 1232, 0, 0, 1640, 1232,  4, 4, 1632, 1224}};// slim video

#define MaxGainIndex 85
// Gain Indexs
kal_uint16 sensorGainMapping[MaxGainIndex][2] = {
	{64 ,1	},
	{67 ,13 },
	{70 ,24 },
	{74 ,34 },
	{77 ,43 },
	{80 ,52 },
	{83 ,59 },
	{87 ,67 },
	{90 ,73 },
	{93 ,80 },
	{96 ,85 },
	{99 ,91 },
	{102,96 },
	{106,101},
	{109,105},
	{112,110},
	{115,114},
	{119,118},
	{122,121},
	{125,125},
	{128,128},
	{131,131},
	{134,134},
	{138,137},
	{141,140},
	{143,141},
	{144,142},
	{148,145},
	{150,147},
	{153,149},
	{157,152},
	{159,153},
	{164,156},
	{167,158},
	{171,160},
	{174,162},
	{178,164},
	{182,166},
	{186,168},
	{191,170},
	{195,172},
	{200,174},
	{205,176},
	{210,178}, 
	{216,180}, 
	{221,182}, 
	{228,184}, 
	{234,186}, 
	{241,188}, 
	{248,190}, 
	{256,192}, 
	{264,194},
	{273,196},
	{282,198},
	{292,200},
	{303,202},
	{328,206},
	{341,208},
	{356,210},
	{372,212},
	{381,213},
	{390,214},
	{399,215},
	{410,216},
	{420,217},
	{431,218},
	{443,219},
	{455,220},
	{468,221},
	{482,222},
	{497,223},
	{512,224},
	{512,224},
	{546,226},
	{585,228},
	{630,230},
	{683,232},
	{712,233},
	{745,234},
	{780,235},
	{819,236},
	{862,237},
	{910,238},
	{964,239},
	{1024,240}
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

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pu_send_cmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	//kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}   
#if 0
static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel, or you can set dummy by imgsensor.frame_length and imgsensor.line_length */

}	/*	set_dummy  */
#endif
static kal_uint32 return_sensor_id()
{   
    //imx 111 get sensor id
	return (((read_cmos_sensor(0x0000) & 0x0f)<<8) | read_cmos_sensor(0x0001));
	/*
	int sensorid;
	sensorid =  ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
	printk("IMX111 read sensor id:0x%x", sensorid);
	return sensorid;
	*/
}

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	//kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	LOG_INF("framerate = %d, min framelength should enable %d\n", framerate,min_framelength_en);

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
    //set_dummy();
}	/*	set_max_framerate  */

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
    //unsigned long flags;
    kal_uint16 realtime_fps = 0;
    //kal_uint32 frame_length = 0;
	spin_lock(&imgsensor_drv_lock);
       imgsensor.shutter = shutter;
	spin_unlock(&imgsensor_drv_lock);

    //write_shutter(shutter);
    /* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
    /* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

    // OV Recommend Solution
    // if shutter bigger than frame_length, should extend frame length first
    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296,0);
        else if(realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146,0);
        else {
            // Extend frame length
            write_cmos_sensor_8(0x0104, 0x01); 
            write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
            write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
            write_cmos_sensor_8(0x0104, 0x00); 

        }
    } else {
        // Extend frame length
        write_cmos_sensor_8(0x0104, 0x01); 
        write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
        write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
        write_cmos_sensor_8(0x0104, 0x00); 

    }

    // Update Shutter
    write_cmos_sensor_8(0x0104, 0x01); 
    write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
    write_cmos_sensor(0x0203, shutter  & 0xFF);	
    write_cmos_sensor_8(0x0104, 0x00); 
    LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);

}    /*    set_shutter */



static kal_uint16 gain2reg(const kal_uint16 gain)
{
    kal_uint8 iI;

    for (iI = 0; iI < (MaxGainIndex-1); iI++) {
        if(gain <= sensorGainMapping[iI][0]){    
            break;
        }
    }
    if(gain != sensorGainMapping[iI][0])
    {
         printk("[IMX111MIPIGain2Reg] Gain mapping don't correctly:%d %d \n", gain, sensorGainMapping[iI][0]);
    }
    return sensorGainMapping[iI][1];
/*
    if(gain != sensorGainMapping[iI][0])
    {
         //SENSORDB("Gain mapping don't correctly:%d %d \n", gain, sensorGainMapping[iI][0]);		 
		 return sensorGainMapping[iI][1];
    }
    else return (kal_uint16)gain;

	return sensorGainMapping[iI][1];*/

}

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
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;

    /* 0x350A[0:1], 0x350B[0:7] AGC real gain */
    /* [0:3] = N meams N /16 X	*/
    /* [4:9] = M meams M X		 */
    /* Total gain = M + N /16 X   */
    if (gain < BASEGAIN || gain > 32 * BASEGAIN) 
    {
        LOG_INF("Error gain setting");
        if (gain < BASEGAIN)
            gain = BASEGAIN;
        else if (gain > 32 * BASEGAIN)
            gain = 32 * BASEGAIN;		 
    }
    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain; 
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
    write_cmos_sensor_8(0x0104, 0x01); 
    //write_cmos_sensor(0x0204, (reg_gain>>8)& 0xFF);
    write_cmos_sensor(0x0205, reg_gain & 0xFF);
    write_cmos_sensor_8(0x0104, 0x00); 	
    return gain;
}	/*	set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
	if (imgsensor.ihdr_en) {

		spin_lock(&imgsensor_drv_lock);
			if (le > imgsensor.min_frame_length - imgsensor_info.margin)
				imgsensor.frame_length = le + imgsensor_info.margin;
			else
				imgsensor.frame_length = imgsensor.min_frame_length;
			if (imgsensor.frame_length > imgsensor_info.max_frame_length)
				imgsensor.frame_length = imgsensor_info.max_frame_length;
			spin_unlock(&imgsensor_drv_lock);
			if (le < imgsensor_info.min_shutter) le = imgsensor_info.min_shutter;
			if (se < imgsensor_info.min_shutter) se = imgsensor_info.min_shutter;


		// Extend frame length first
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor(0x0341,  imgsensor.frame_length & 0xFF);


		write_cmos_sensor(0x0202, (le >> 8) & 0xFF);
		write_cmos_sensor(0x0203, le  & 0xFF);

		write_cmos_sensor(0x0230, ((se>>8) & 0xFF));
		write_cmos_sensor(0x0231, (se& 0xFF));
		write_cmos_sensor(0x0104, 0x00);

		set_gain(gain);
	}

}


#if 1
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("set_mirror_flip\n");
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
	switch (image_mirror) {
		case IMAGE_NORMAL:
			write_cmos_sensor(0x0101, 0x00);	//Set normal
		break;

		case IMAGE_H_MIRROR:
			write_cmos_sensor(0x0101,  0x01);	//Set mirror
		break;

		case IMAGE_V_MIRROR:
			write_cmos_sensor(0x0101,  0x02);	//Set flip
		break;

		case IMAGE_HV_MIRROR:
			write_cmos_sensor(0x0101, 0x03);	//Set mirror and flip
		break;

		default:
			LOG_INF("Error image_mirror setting\n");
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
//static void night_mode(kal_bool enable)
//{
/*No Need to implement this function*/
//}   /*  night_mode  */

static void sensor_init(void)
{
	LOG_INF("sensor_init\n");
		write_cmos_sensor(0x0101, 0x00);//
		write_cmos_sensor(0x3080, 0x50);//
		write_cmos_sensor(0x3087, 0x53);//
		write_cmos_sensor(0x309D, 0x94);//
		write_cmos_sensor(0x30B1, 0x03);//
		write_cmos_sensor(0x30C6, 0x00);//
		write_cmos_sensor(0x30C7, 0x00);//
		write_cmos_sensor(0x3115, 0x0B);//
		write_cmos_sensor(0x3118, 0x30);//
		write_cmos_sensor(0x311D, 0x25);//
		write_cmos_sensor(0x3121, 0x0A);//
		write_cmos_sensor(0x3212, 0xF2);//
		write_cmos_sensor(0x3213, 0x0F);//
		write_cmos_sensor(0x3215, 0x0F);//
		write_cmos_sensor(0x3217, 0x0B);//
		write_cmos_sensor(0x3219, 0x0B);//
		write_cmos_sensor(0x321B, 0x0D);//
		write_cmos_sensor(0x321D, 0x0D);//
		write_cmos_sensor(0x32AA, 0x11);//
		write_cmos_sensor(0x3301,0x04);
		write_cmos_sensor(0x3032, 0x40);//black level setting  
		LOG_INF("IMX111MIPI_globle_setting  end \n");
}   /*  IMX111MIPI_Sensor_Init  */

static void preview_setting(void)
{
	#if 0
        LOG_INF("preview_setting\n");
		//PLL Setting  mipiClock 672Mhz/Lane  
		write_cmos_sensor(0x0104,1);   
		write_cmos_sensor(0x0100, 0x00);
		mdelay(10);
		write_cmos_sensor(0x0305, 0x02);
		write_cmos_sensor(0x0307, 0x38);
		write_cmos_sensor(0x30A4, 0x02);
		write_cmos_sensor(0x303C, 0x4B);
		//size setting  s
		write_cmos_sensor(0x0340, 0x04);
		write_cmos_sensor(0x0341, 0xE6);
		write_cmos_sensor(0x0342, 0x0D);
		write_cmos_sensor(0x0343, 0xD0);
		write_cmos_sensor(0x0344, 0x00);
		write_cmos_sensor(0x0345, 0x08);
		write_cmos_sensor(0x0346, 0x00);
		write_cmos_sensor(0x0347, 0x30);
		write_cmos_sensor(0x0348, 0x0C);
		write_cmos_sensor(0x0349, 0xD7);
		write_cmos_sensor(0x034A, 0x09);
		write_cmos_sensor(0x034B, 0xCF);
		write_cmos_sensor(0x034C, 0x06);
		write_cmos_sensor(0x034D, 0x68);
		write_cmos_sensor(0x034E, 0x04);
		write_cmos_sensor(0x034F, 0xD0);
		write_cmos_sensor(0x0381, 0x01);
		write_cmos_sensor(0x0383, 0x03);
		write_cmos_sensor(0x0385, 0x01);
		write_cmos_sensor(0x0387, 0x03);
		write_cmos_sensor(0x3033, 0x00);
		write_cmos_sensor(0x303D, 0x10);
		write_cmos_sensor(0x303E, 0x40);
		write_cmos_sensor(0x3040, 0x08);
		write_cmos_sensor(0x3041, 0x97);
		write_cmos_sensor(0x3048, 0x01);
		write_cmos_sensor(0x304C, 0x6F);
		write_cmos_sensor(0x304D, 0x03);
		write_cmos_sensor(0x3064, 0x12);
		write_cmos_sensor(0x3073, 0x00);
		write_cmos_sensor(0x3074, 0x11);
		write_cmos_sensor(0x3075, 0x11);
		write_cmos_sensor(0x3076, 0x11);
		write_cmos_sensor(0x3077, 0x11);
		write_cmos_sensor(0x3079, 0x00);
		write_cmos_sensor(0x307A, 0x00);
		write_cmos_sensor(0x309B, 0x28);
		write_cmos_sensor(0x309C, 0x13);//add
		write_cmos_sensor(0x309E, 0x00);
		write_cmos_sensor(0x30A0, 0x14);
		write_cmos_sensor(0x30A1, 0x09);
		write_cmos_sensor(0x30AA, 0x03);
		write_cmos_sensor(0x30B2, 0x05);
		write_cmos_sensor(0x30D5, 0x09);
		write_cmos_sensor(0x30D6, 0x01);
		write_cmos_sensor(0x30D7, 0x01);
		write_cmos_sensor(0x30D8, 0x64);
		write_cmos_sensor(0x30D9, 0x89);
		write_cmos_sensor(0x30DE, 0x02);
		write_cmos_sensor(0x30DF, 0x20);
		write_cmos_sensor(0x3102, 0x08);
		write_cmos_sensor(0x3103, 0x22);
		write_cmos_sensor(0x3104, 0x20);
		write_cmos_sensor(0x3105, 0x00);
		write_cmos_sensor(0x3106, 0x87);
		write_cmos_sensor(0x3107, 0x00);
		write_cmos_sensor(0x3108, 0x03);
		write_cmos_sensor(0x3109, 0x02);
		write_cmos_sensor(0x310A, 0x03);
		write_cmos_sensor(0x315C, 0x9C);
		write_cmos_sensor(0x315D, 0x9B);
		write_cmos_sensor(0x316E, 0x9D);
		write_cmos_sensor(0x316F, 0x9C);
		write_cmos_sensor(0x3318, 0x72);
		write_cmos_sensor(0x3348, 0xE0);
		write_cmos_sensor(0x0104,0);   
		write_cmos_sensor(0x0100,0x01);//STREAM START
		#else
				write_cmos_sensor(0x0100,0x00);// STREAM STop
		mdelay(10);
		write_cmos_sensor(0x0305, 0x02);
		write_cmos_sensor(0x0307, 0x53);
		write_cmos_sensor(0x30A4, 0x02);
		write_cmos_sensor(0x303C, 0x4B);
		write_cmos_sensor(0x0202, 0x04);
		write_cmos_sensor(0x0203, 0xEC);
		write_cmos_sensor(0x0204, 0x00);
		write_cmos_sensor(0x0205, 0xAC);
		write_cmos_sensor(0x0340, 0x09);
		write_cmos_sensor(0x0341, 0xD4);
		write_cmos_sensor(0x0342, 0x0D);
		write_cmos_sensor(0x0343, 0xD0);
		write_cmos_sensor(0x0344, 0x00);
		write_cmos_sensor(0x0345, 0x08);
		write_cmos_sensor(0x0346, 0x00);
		write_cmos_sensor(0x0347, 0x30);
		write_cmos_sensor(0x0348, 0x0C);
		write_cmos_sensor(0x0349, 0xD7);
		write_cmos_sensor(0x034A, 0x09);
		write_cmos_sensor(0x034B, 0xCF);
		write_cmos_sensor(0x034C, 0x0C);
		write_cmos_sensor(0x034D, 0xD0);
		write_cmos_sensor(0x034E, 0x09);
		write_cmos_sensor(0x034F, 0xA0);
		write_cmos_sensor(0x0381, 0x01);
		write_cmos_sensor(0x0383, 0x01);
		write_cmos_sensor(0x0385, 0x01);
		write_cmos_sensor(0x0387, 0x01);
		write_cmos_sensor(0x3033, 0x00);
		write_cmos_sensor(0x303D, 0x00);
		write_cmos_sensor(0x303E, 0x41);
		write_cmos_sensor(0x3040, 0x08);
		write_cmos_sensor(0x3041, 0x97);
		write_cmos_sensor(0x3048, 0x00);
		write_cmos_sensor(0x304C, 0x6F);
		write_cmos_sensor(0x304D, 0x03);
		write_cmos_sensor(0x3064, 0x12);
		write_cmos_sensor(0x3073, 0x00);
		write_cmos_sensor(0x3074, 0x11);
		write_cmos_sensor(0x3075, 0x11);
		write_cmos_sensor(0x3076, 0x11);
		write_cmos_sensor(0x3077, 0x11);
		write_cmos_sensor(0x3079, 0x00);
		write_cmos_sensor(0x307A, 0x00);
		write_cmos_sensor(0x309B, 0x20);
		write_cmos_sensor(0x309E, 0x00);
		write_cmos_sensor(0x30A0, 0x14);
		write_cmos_sensor(0x30A1, 0x08);
		write_cmos_sensor(0x30AA, 0x03);
		write_cmos_sensor(0x30B2, 0x07);
		write_cmos_sensor(0x30D5, 0x00);
		write_cmos_sensor(0x30D6, 0x85);
		write_cmos_sensor(0x30D7, 0x2A);
		write_cmos_sensor(0x30D8, 0x64);
		write_cmos_sensor(0x30D9, 0x89);
		write_cmos_sensor(0x30DE, 0x00);
		write_cmos_sensor(0x30DF, 0x20);
		write_cmos_sensor(0x3102, 0x10);
		write_cmos_sensor(0x3103, 0x44);
		write_cmos_sensor(0x3104, 0x40);
		write_cmos_sensor(0x3105, 0x00);
		write_cmos_sensor(0x3106, 0x0D);
		write_cmos_sensor(0x3107, 0x01);
		write_cmos_sensor(0x3108, 0x09);
		write_cmos_sensor(0x3109, 0x08);
		write_cmos_sensor(0x310A, 0x0F);
		write_cmos_sensor(0x315C, 0x5D);
		write_cmos_sensor(0x315D, 0x5C);
		write_cmos_sensor(0x316E, 0x5E);
		write_cmos_sensor(0x316F, 0x5D);
		write_cmos_sensor(0x3318, 0x60);
		write_cmos_sensor(0x3348, 0xE0);
		write_cmos_sensor(0x0100,0x01);//STREAM START	
		
		#endif

	}    /*    preview_setting  */

	static void capture_setting(kal_uint16 currefps)
	{
		LOG_INF("capture_setting\n");
		write_cmos_sensor(0x0100,0x00);// STREAM STop
		mdelay(10);
		write_cmos_sensor(0x0305, 0x02);
		write_cmos_sensor(0x0307, 0x53);
		write_cmos_sensor(0x30A4, 0x02);
		write_cmos_sensor(0x303C, 0x4B);
		write_cmos_sensor(0x0202, 0x04);
		write_cmos_sensor(0x0203, 0xEC);
		write_cmos_sensor(0x0204, 0x00);
		write_cmos_sensor(0x0205, 0xAC);
		write_cmos_sensor(0x0340, 0x09);
		write_cmos_sensor(0x0341, 0xD4);
		write_cmos_sensor(0x0342, 0x0D);
		write_cmos_sensor(0x0343, 0xD0);
		write_cmos_sensor(0x0344, 0x00);
		write_cmos_sensor(0x0345, 0x08);
		write_cmos_sensor(0x0346, 0x00);
		write_cmos_sensor(0x0347, 0x30);
		write_cmos_sensor(0x0348, 0x0C);
		write_cmos_sensor(0x0349, 0xD7);
		write_cmos_sensor(0x034A, 0x09);
		write_cmos_sensor(0x034B, 0xCF);
		write_cmos_sensor(0x034C, 0x0C);
		write_cmos_sensor(0x034D, 0xD0);
		write_cmos_sensor(0x034E, 0x09);
		write_cmos_sensor(0x034F, 0xA0);
		write_cmos_sensor(0x0381, 0x01);
		write_cmos_sensor(0x0383, 0x01);
		write_cmos_sensor(0x0385, 0x01);
		write_cmos_sensor(0x0387, 0x01);
		write_cmos_sensor(0x3033, 0x00);
		write_cmos_sensor(0x303D, 0x00);
		write_cmos_sensor(0x303E, 0x41);
		write_cmos_sensor(0x3040, 0x08);
		write_cmos_sensor(0x3041, 0x97);
		write_cmos_sensor(0x3048, 0x00);
		write_cmos_sensor(0x304C, 0x6F);
		write_cmos_sensor(0x304D, 0x03);
		write_cmos_sensor(0x3064, 0x12);
		write_cmos_sensor(0x3073, 0x00);
		write_cmos_sensor(0x3074, 0x11);
		write_cmos_sensor(0x3075, 0x11);
		write_cmos_sensor(0x3076, 0x11);
		write_cmos_sensor(0x3077, 0x11);
		write_cmos_sensor(0x3079, 0x00);
		write_cmos_sensor(0x307A, 0x00);
		write_cmos_sensor(0x309B, 0x20);
		write_cmos_sensor(0x309E, 0x00);
		write_cmos_sensor(0x30A0, 0x14);
		write_cmos_sensor(0x30A1, 0x08);
		write_cmos_sensor(0x30AA, 0x03);
		write_cmos_sensor(0x30B2, 0x07);
		write_cmos_sensor(0x30D5, 0x00);
		write_cmos_sensor(0x30D6, 0x85);
		write_cmos_sensor(0x30D7, 0x2A);
		write_cmos_sensor(0x30D8, 0x64);
		write_cmos_sensor(0x30D9, 0x89);
		write_cmos_sensor(0x30DE, 0x00);
		write_cmos_sensor(0x30DF, 0x20);
		write_cmos_sensor(0x3102, 0x10);
		write_cmos_sensor(0x3103, 0x44);
		write_cmos_sensor(0x3104, 0x40);
		write_cmos_sensor(0x3105, 0x00);
		write_cmos_sensor(0x3106, 0x0D);
		write_cmos_sensor(0x3107, 0x01);
		write_cmos_sensor(0x3108, 0x09);
		write_cmos_sensor(0x3109, 0x08);
		write_cmos_sensor(0x310A, 0x0F);
		write_cmos_sensor(0x315C, 0x5D);
		write_cmos_sensor(0x315D, 0x5C);
		write_cmos_sensor(0x316E, 0x5E);
		write_cmos_sensor(0x316F, 0x5D);
		write_cmos_sensor(0x3318, 0x60);
		write_cmos_sensor(0x3348, 0xE0);
		write_cmos_sensor(0x0100,0x01);//STREAM START

        LOG_INF("E! currefps:%d\n",currefps);

}

static void normal_video_setting(void)
{
  preview_setting();

}
static void hs_video_setting(void)
{
	LOG_INF("hs_video_setting\n");
  preview_setting();
}

static void slim_video_setting(void)
{
	LOG_INF("slim_video_setting\n");
	preview_setting();
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
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	LOG_INF("get_imgsensor_id\n");
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
            *sensor_id = return_sensor_id();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail,i2c write id: 0x%x,sensor id :0x%x\n", imgsensor.i2c_write_id,*sensor_id);
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
	kal_uint16 sensor_id = 0;
	LOG_INF("PLATFORM:MT6595,MIPI 2 LANE\n");
	//LOG_INF("preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane; capture 5M@30fps,864Mbps/lane\n");

	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
            sensor_id = return_sensor_id();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("open : i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, id: 0x%x\n", sensor_id);
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
	mdelay(10);
	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x2D00;
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
}	/*	open  */



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
	LOG_INF("close\n");

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
	LOG_INF("preview\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	//imgsensor.video_mode = KAL_FALSE;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	mdelay(10);
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
	LOG_INF("capture\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {//15fps
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
            LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	mdelay(10);
    set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("normal_video\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting();
	mdelay(10);

    set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("hs_video\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	mdelay(10);
    set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("slim_video\n");

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
	mdelay(10);
	set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	slim_video	 */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("get_resolution\n");
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
	LOG_INF("get_info\n");
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
	sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame;

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
	LOG_INF("control\n");
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
	LOG_INF("set_video_mode\n");
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
	set_max_framerate(imgsensor.current_fps,1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("set_auto_flicker_mode\n");
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
	kal_uint32 frame_length;
	LOG_INF("set_max_framerate_by_scenario\n");
	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
            //set_dummy();			
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(framerate == 0)
				return ERROR_NONE;
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
            //set_dummy();			
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		//case MSDK_SCENARIO_ID_CAMERA_ZSD:
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
            //set_dummy();			
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
            //set_dummy();			
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
            //set_dummy();			
			break;
		default:  //coding with  preview scenario by default
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
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
	LOG_INF("get_default_framerate_by_scenario\n");
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

static kal_uint32 set_test_pattern_mode(kal_uint32 modes, struct SET_SENSOR_PATTERN_SOLID_COLOR *pdata)
{
	LOG_INF("set_test_pattern_mode\n");
	LOG_INF("enable: %d\n", modes);

	if (modes) {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x0601, 0x02);  //2015-1-14 sp_miao
	} else {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x0601, 0x00);   //2015-1-14 sp_miao
	}
	spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = modes;
	spin_unlock(&imgsensor_drv_lock);
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

	LOG_INF("feature_control\n");
	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
		case SENSOR_FEATURE_GET_PERIOD:
			*feature_return_para_16++ = imgsensor.line_length;
			*feature_return_para_16 = imgsensor.frame_length;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_SET_ESHUTTER:
            set_shutter(*feature_data);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            //night_mode((BOOL) *feature_data);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            set_gain((UINT16) *feature_data);
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
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            set_video_mode(*feature_data);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            get_imgsensor_id(feature_return_para_32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
            break;
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
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
		case SENSOR_FEATURE_GET_BINNING_TYPE:
			switch (*(feature_data + 1)) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*feature_return_para_32 = 1; /* NON */
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
			*feature_return_para_32 = 2; /*BINNING_AVERAGED*/
			break;
			}
			pr_debug("SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n", *feature_return_para_32);
			*feature_para_len = 4;
			break;
		case SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO:
			LOG_INF("SENSOR_FEATURE_GET_PERIOD_BY_SCENARIO \n");
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
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO:
			LOG_INF("SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ_BY_SCENARIO \n");
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
        case SENSOR_FEATURE_SET_TEST_PATTERN:
			set_test_pattern_mode((UINT32)*feature_data,
			(struct SET_SENSOR_PATTERN_SOLID_COLOR *)(uintptr_t)(*(feature_data + 1)));
            break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
            *feature_return_para_32 = imgsensor_info.checksum_value;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_FRAMERATE:
            LOG_INF("current fps :%d\n", (UINT32)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.current_fps = *feature_data;
            spin_unlock(&imgsensor_drv_lock);
            break;
        case SENSOR_FEATURE_SET_HDR:
            LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.ihdr_en = (BOOL)*feature_data;
            spin_unlock(&imgsensor_drv_lock);
            break;
		case SENSOR_FEATURE_GET_CROP_INFO:
            LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);

            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

            switch (*feature_data_32) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
            }
			break;
	   case SENSOR_FEATURE_GET_PIXEL_RATE:

		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;
	  /*case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
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
		break;*/
        case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            break;
        case SENSOR_FEATURE_SET_HDR_SHUTTER:
            LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1));
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


UINT32 IMX111_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	OV5693_MIPI_RAW_SensorInit	*/

