/*****************************************************************************
 *
 * Filename:
 * ---------
 *     OV5693mipi_Sensor.c
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
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx328mipiraw_Sensor.h"



/****************************Modify following Strings for debug****************************/
#define PFX "IMX328_camera_sensor"
#define LOG_1 LOG_INF("imx328,MIPI 4LANE\n")
#define LOG_2 LOG_INF("preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane; capture 5M@30fps,864Mbps/lane\n")
/****************************   Modify end    *******************************************/
#define LOG_INF(fmt, args...)   pr_err(PFX "[%s] " fmt, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define MIPI_SETTLEDELAY_AUTO     0
#define MIPI_SETTLEDELAY_MANNUAL  1

#define IMX328MIPI_4LANE   1


static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX328_SENSOR_ID,		//record sensor id defined in Kd_imgsensor.h
	
	.checksum_value = 0x9e08861c,		//checksum value for Camera Auto Test
	
	.pre = {
		.pclk = 225600000,				//record different mode's pclk
		.linelength = 4620,				//record different mode's linelength
		.framelength = 1626,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2104,		//record different mode's width of grabwindow
		.grabwindow_height = 1560,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
	.cap = {
		.pclk = 225600000,
		.linelength = 4620,
		.framelength = 3212,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4208,
		.grabwindow_height = 3120,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,

	},
	.cap1 = {							//capture for PIP 24fps relative information, capture1 mode must use same framelength, linelength with Capture mode for shutter calculate
		.pclk = 225600000,
		.linelength = 4620,
		.framelength = 3212,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4200,
		.grabwindow_height = 3116,
		.mipi_data_lp2hs_settle_dc = 24,//unit , ns
		.max_framerate = 150,	//less than 13M(include 13M),cap1 max framerate is 24fps,16M max framerate is 20fps, 20M max framerate is 15fps  
	},
	.normal_video = {
		.pclk = 225600000,
		.linelength = 4620,
		.framelength = 1606,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 24,//unit , ns
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 225600000,
		.linelength = 4620,
		.framelength = 1606,
		.startx = 8,
		.starty = 8,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 24,//unit , ns
		.max_framerate = 600,
	},
	.slim_video = {
		.pclk = 225600000,
		.linelength = 4620,
		.framelength = 1606,
		.startx = 8,
		.starty = 8,
		.grabwindow_width = 2104,
		.grabwindow_height = 1560,
		.mipi_data_lp2hs_settle_dc = 24,//unit , ns
		.max_framerate = 300,

	},
	.min_gain = 64,
	.max_gain = 1024,
	.min_gain_iso = 100,
	.gain_step = 1,
	.gain_type = 0,
	.margin = 10,			//sensor framelength & shutter margin
	.min_shutter = 1,		//min shutter
	.max_frame_length = 0xffff,//max framelength by sensor register's limitation
	.ae_shut_delay_frame = 0,	//shutter delay frame for AE cycle, 2 frame with ispGain_delay-shut_delay=2-0=2
	.ae_sensor_gain_delay_frame = 0,//sensor gain delay frame for AE cycle,2 frame with ispGain_delay-sensor_gain_delay=2-0=2
	.ae_ispGain_delay_frame = 2,//isp gain delay frame for AE cycle
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num
	
	.cap_delay_frame = 2,		//enter capture delay frame num
	.pre_delay_frame = 2, 		//enter preview delay frame num
	.video_delay_frame = 2,		//enter video delay frame num
	.hs_video_delay_frame = 2,	//enter high speed video  delay frame num
	.slim_video_delay_frame = 2,//enter slim video delay frame num
	
	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,    
	.i2c_addr_table = {0x20, 0x34, 0x40, 0x6c, 0xff},
	.i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
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
	.i2c_write_id = 0x20,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =	 
{{ 4208, 3120,	0,	0, 4208, 3120, 2104,  1560, 0, 0, 2104,  1560,	  0,	0, 2104, 1560}, // Preview
 { 4208, 3120,  0,  0, 4208, 3120, 4208,  3120, 0, 0, 4208,  3120,    0,    0, 4208, 3120}, // capture
 { 4208, 3120,	0,	0, 4208, 3120, 2104,  1560, 0, 0, 2104,  1560,	  0,	0, 2104, 1560},  // video 
 { 4208, 3120,	0,	0, 4208, 3120, 2104,  1560, 0, 0, 2104,  1560,	  0,	0, 2104, 1560}, //hight speed video 
 { 4208, 3120,	0,	0, 4208, 3120, 2104,  1560, 0, 0, 2104,  1560,	  0,	0, 2104, 1560}// slim video 
};

// Gain Index
#define IMX328MIPI_MaxGainIndex (105)
kal_uint16 IMX328MIPI_sensorGainMapping[IMX328MIPI_MaxGainIndex][2] ={
	{64 ,0  },   
	{68 ,12 },   
	{71 ,23 },   
	{74 ,33 },   
	{77 ,42 },   
	{81 ,52 },   
	{84 ,59 },   
	{87 ,66 },   
	{90 ,73 },   
	{93 ,79 },   
	{96 ,85 },   
	{100,91 },   
	{103,96 },   
	{106,101},   
	{109,105},   
	{113,110},   
	{116,114},   
	{120,118},   
	{122,121},   
	{125,125},   
	{128,128},   
	{132,131},   
	{135,134},   
	{138,137},
	{141,139},
	{144,142},   
	{148,145},   
	{151,147},   
	{153,149}, 
	{157,151},
	{160,153},      
	{164,156},   
	{168,158},   
	{169,159},   
	{173,161},   
	{176,163},   
	{180,165}, 
	{182,166},   
	{187,168},
	{189,169},
	{193,171},
	{196,172},
	{200,174},
	{203,175}, 
	{205,176},
	{208,177}, 
	{213,179}, 
	{216,180},  
	{219,181},   
	{222,182},
	{225,183},  
	{228,184},   
	{232,185},
	{235,186},
	{238,187},
	{241,188},
	{245,189},
	{249,190},
	{253,191},
	{256,192}, 
	{260,193},
	{265,194},
	{269,195},
	{274,196},   
	{278,197},
	{283,198},
	{288,199},
	{293,200},
	{298,201},   
	{304,202},   
	{310,203},
	{315,204},
	{322,205},   
	{328,206},   
	{335,207},   
	{342,208},   
	{349,209},   
	{357,210},   
	{365,211},   
	{373,212}, 
	{381,213},
	{400,215},      
	{420,217},   
	{432,218},   
	{443,219},      
	{468,221},   
	{482,222},   
	{497,223},   
	{512,224},
	{529,225}, 	 
	{546,226},   
	{566,227},   
	{585,228}, 	 
	{607,229},   
	{631,230},   
	{656,231},   
	{683,232},
	{712, 233},//
	{744, 234},//
	{780, 235},//
	{819, 236},//
	{862, 237},//
	{910, 238},//
	{963, 239},//
	{1024,240},//
};

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
	kal_uint16 get_byte=0;

	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
	char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
    //kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    char pu_send_cmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}   
static void set_dummy()
{
	printk("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/* you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel, or you can set dummy by imgsensor.frame_length and imgsensor.line_length */
    write_cmos_sensor(0x0104, 1); 
    write_cmos_sensor(0x0340, (imgsensor.frame_length >>8) & 0xFF);
    write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);	
    write_cmos_sensor(0x0342, (imgsensor.line_length >>8) & 0xFF);
    write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
    write_cmos_sensor(0x0104, 0);
  
}	/*	set_dummy  */


static kal_uint32 return_sensor_id()
{
    return ((read_cmos_sensor(0x0000)<<8)  | read_cmos_sensor(0x0001));
}

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	//kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;
	//printk("framerate = %d, min framelength should enable? \n", framerate,min_framelength_en);   
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)?frame_length:imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	//dummy_line = frame_length - imgsensor.min_frame_length;
	//if (dummy_line < 0)
	//	imgsensor.dummy_line = 0;
	//else
	//	imgsensor.dummy_line = dummy_line;
	//imgsensor.frame_length = frame_length + imgsensor.dummy_line;
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
static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
	//kal_uint32 frame_length = 0;
	   
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

    // Framelength should be an even number
    shutter = (shutter >> 1) << 1;
    imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;
	//write_cmos_sensor_8(0x0104, 0x01);
	if (imgsensor.autoflicker_en) { 
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
        {
            set_max_framerate(296,0);
            write_cmos_sensor_8(0x0104, 0x01);
            write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
            write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
            write_cmos_sensor(0x0342, (imgsensor.line_length >> 8) & 0xFF);
            write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
            write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
            write_cmos_sensor(0x0203, shutter  & 0xFF);
            write_cmos_sensor_8(0x0104, 0x00);
            printk("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
            //set_dummy();
        }
        else if(realtime_fps >= 147 && realtime_fps <= 150)
        {
            set_max_framerate(146,0);
            write_cmos_sensor_8(0x0104, 0x01);
            write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
            write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
            write_cmos_sensor(0x0342, (imgsensor.line_length >> 8) & 0xFF);
            write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
            write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
            write_cmos_sensor(0x0203, shutter  & 0xFF);
            write_cmos_sensor_8(0x0104, 0x00);
            printk("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
            //set_dummy();
        }
        else {
            // Extend frame length
            write_cmos_sensor_8(0x0104, 0x01);
            write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
            write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
            write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
            write_cmos_sensor(0x0203, shutter  & 0xFF);
            write_cmos_sensor_8(0x0104, 0x00);
		}
	} else {
		// Extend frame length
        write_cmos_sensor_8(0x0104, 0x01);
        write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	// Update Shutter
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
    write_cmos_sensor(0x0203, shutter  & 0xFF);
        write_cmos_sensor_8(0x0104, 0x00);
    }
// write_cmos_sensor_8(0x0104, 0x00);
	printk("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);

}    /*    set_shutter */



static void set_shutter(kal_uint16 shutter)
{
    unsigned long flags;
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    write_shutter(shutter);
}   /*  set_shutter */



static kal_uint16 gain2reg(const kal_uint16 gain)
{
    kal_uint8 iI;

    for (iI = 0; iI < (IMX328MIPI_MaxGainIndex-1); iI++) {
        if(gain <= IMX328MIPI_sensorGainMapping[iI][0]){
            break;
        }
    }
/*
    if(gain != sensorGainMapping[iI][0])
    {
         //SENSORDB("Gain mapping don't correctly:%d %d \n", gain, sensorGainMapping[iI][0]);
         return sensorGainMapping[iI][1];
    }
    else return (kal_uint16)gain;
*/
	return IMX328MIPI_sensorGainMapping[iI][1];
	//return NONE;

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
//UINT16 iPreGain = 0;
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;

    /* 0x350A[0:1], 0x350B[0:7] AGC real gain */
    /* [0:3] = N meams N /16 X  */
    /* [4:9] = M meams M X       */
    /* Total gain = M + N /16 X   */

    //
    if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
        printk("Error gain setting");

        if (gain < BASEGAIN)
            gain = BASEGAIN;
        else if (gain > 32 * BASEGAIN)
            gain = 32 * BASEGAIN;
    }

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    printk("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

    write_cmos_sensor_8(0x0104, 0x01);
    //  LE Gain
    write_cmos_sensor(0x0204, (reg_gain>>8)& 0xFF);
    write_cmos_sensor(0x0205, reg_gain & 0xFF);
    // SE  Gain
    //write_cmos_sensor(0x0233, reg_gain & 0xFF);
    write_cmos_sensor_8(0x0104, 0x00);

    return gain;
}   /*  set_gain  */

#if 0
static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
    kal_uint8 iRation;
    kal_uint8 iReg;

    printk("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
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

        // Framelength should be an even number
        le = (le >> 1) << 1;
        se = (se >> 1) << 1;
        imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

        // Extend frame length first
        write_cmos_sensor_8(0x0104, 0x01);
        write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8)& 0xFF);
        write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);

        write_cmos_sensor(0x0202, (le >> 8) & 0xFF);
 		write_cmos_sensor(0x0203, le  & 0xFF);

        write_cmos_sensor(0x0230, (se >> 8) & 0xFF);
        write_cmos_sensor(0x0231, se & 0xFF);

        iReg = gain2reg(gain);

        //  LE Gain
        write_cmos_sensor(0x0205, (kal_uint8)iReg);
        // SE  Gain
        write_cmos_sensor(0x0233, (kal_uint8)iReg);


        //SET LE/SE ration
        //iRation = (((LE + SE/2)/SE) >> 1 ) << 1 ;
        iRation = ((10 * le / se) + 5) / 10;
        if(iRation < 2)
            iRation = 0;
        else if(iRation < 4)
            iRation = 1;
        else if(iRation < 8)
            iRation = 2;
        else if(iRation < 16)
            iRation = 4;
        else if(iRation < 32)
            iRation = 8;
        else
            iRation = 0;
        write_cmos_sensor(0x0239,iRation);//   exposure ratio --> 2 : 1/4
        write_cmos_sensor_8(0x0104, 0x00);
        printk("[IMX135MIPI_IHDR_write_shutter_gain ] iRation:%d\n", iRation);

    }
   // write_cmos_sensor(0x0104, 0);

}

static void ihdr_write_shutter(kal_uint16 le, kal_uint16 se)
{
    kal_uint8 iRation;
    //kal_uint8 iReg;

    printk("le:0x%x, se:0x%x\n",le,se);
    //Test only
    return 0;
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


        // Extend frame length
        write_cmos_sensor_8(0x0104, 0x01);
        write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8)& 0xFF);
        write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
        //long exporsure
        write_cmos_sensor(0x0202, (le >> 8) & 0xFF);
        write_cmos_sensor(0x0203, le & 0xFF);
        //short exporsure
        write_cmos_sensor(0x0230, (se >> 8) & 0xFF);
        write_cmos_sensor(0x0231, se & 0xFF);

        //SET LE/SE ration
        //iRation = (((LE + SE/2)/SE) >> 1 ) << 1 ;
        iRation = ((10 * le / se) + 5) / 10;
        if(iRation < 2)
            iRation = 0;
        else if(iRation < 4)
            iRation = 1;
        else if(iRation < 8)
            iRation = 2;
        else if(iRation < 16)
            iRation = 4;
        else if(iRation < 32)
            iRation = 8;
        else
            iRation = 0;
        write_cmos_sensor(0x0239,iRation);//   exposure ratio --> 2 : 1/4
        write_cmos_sensor_8(0x0104, 0x00);
        printk("[IMX135MIPI_IHDR_write_shutter_gain ] iRation:%d\n", iRation);

    }
}
#endif

#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	/********************************************************
	   *
	   *   0x0101[2] ISP Vertical flip
	   *   0x0101[1] Sensor Vertical flip
	   *
	   *   0x0101[2] ISP Horizontal mirror
	   *   0x0101[1] Sensor Horizontal mirror
	   *
	   *   ISP and Sensor flip or mirror register bit should be the same!!
	   *
	   ********************************************************/
	kal_uint8  iTemp; 
	printk("set_mirror_flip function\n");
    iTemp = read_cmos_sensor(0x0101) & 0x03;	//Clear the mirror and flip bits.
    switch (image_mirror)
    {
        case IMAGE_NORMAL:
            write_cmos_sensor(0x0101, 0x03);	//Set normal
            break;
        case IMAGE_V_MIRROR:
            write_cmos_sensor(0x0101, iTemp | 0x01);	//Set flip
            break;
        case IMAGE_H_MIRROR:
            write_cmos_sensor(0x0101, iTemp | 0x02);	//Set mirror
            break;
        case IMAGE_HV_MIRROR:
            write_cmos_sensor(0x0101, 0x00);	//Set mirror and flip
            break;
    }
	printk("Error image_mirror setting\n");

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
	printk("E\n");
	//write_cmos_sensor(0x0100,0x01);  //wake up
	write_cmos_sensor(0x3032 ,0x40);//for OB level  
                                                 
	write_cmos_sensor(0x3087 ,0x53);                
	write_cmos_sensor(0x309D ,0x94);                
	write_cmos_sensor(0x30A1 ,0x08);                
	write_cmos_sensor(0x30C7 ,0x00);                
	write_cmos_sensor(0x3115 ,0x0E);                
	write_cmos_sensor(0x3118 ,0x42);                
	write_cmos_sensor(0x311D ,0x34);                
	write_cmos_sensor(0x3121 ,0x0D);                
	write_cmos_sensor(0x3212 ,0xF2);                
	write_cmos_sensor(0x3213 ,0x0F);                
	write_cmos_sensor(0x3215 ,0x0F);                
	write_cmos_sensor(0x3217 ,0x0B);                
	write_cmos_sensor(0x3219 ,0x0B);                
	write_cmos_sensor(0x321B ,0x0D);                
	write_cmos_sensor(0x321D ,0x0D);                
}	/*	sensor_init  */


static void preview_setting(void)
{
	//5.1.2 FQPreview 1640x1232 30fps 24M MCLK 4lane 256Mbps/lane
	printk("preview_setting E\n");
	write_cmos_sensor(0x0100,	0x00);	   						   
    write_cmos_sensor(0x0104,	0x01);
    write_cmos_sensor(0x0101 ,  0x03 );							 
    write_cmos_sensor(0x0305,	0x02);
    write_cmos_sensor(0x0307,	0x2F);
    write_cmos_sensor(0x30a4,	0x02);
    write_cmos_sensor(0x303C,	0x4B);

    write_cmos_sensor(0x0340,	0x06);			 
    write_cmos_sensor(0x0341,	0x5A);//5a	//4a		 
    write_cmos_sensor(0x0342,	0x12);			 
    write_cmos_sensor(0x0343,	0x0C);	
    write_cmos_sensor(0x0344,	0x00);			 
    write_cmos_sensor(0x0345,	0x08);			 
    write_cmos_sensor(0x0346,	0x00);			 
    write_cmos_sensor(0x0347,	0x30);			 
    write_cmos_sensor(0x0348,	0x10);			 
    write_cmos_sensor(0x0349,	0x77);			 
    write_cmos_sensor(0x034A,	0x0C);			 
    write_cmos_sensor(0x034B,	0x5F);	
    write_cmos_sensor(0x034C,	0x08);			 
    write_cmos_sensor(0x034D,	0x38);			 
    write_cmos_sensor(0x034E,	0x06);			 
    write_cmos_sensor(0x034F,	0x18);			 
    write_cmos_sensor(0x0381,	0x01);			 
    write_cmos_sensor(0x0383,	0x03);			 
    write_cmos_sensor(0x0385,	0x01);			 
    write_cmos_sensor(0x0387,	0x03);			 
    write_cmos_sensor(0x3033,	0x00);
    write_cmos_sensor(0x303D,	0x10);			 
    write_cmos_sensor(0x303E,	0xD0);
    write_cmos_sensor(0x3040,	0x08);
    write_cmos_sensor(0x3041,	0x97);

    write_cmos_sensor(0x3048,	0x01);			 
    write_cmos_sensor(0x304C,	0x7F);			 
    write_cmos_sensor(0x304D,	0x04);			 
    write_cmos_sensor(0x3064,	0x12);			 
    write_cmos_sensor(0x309B,	0x28);			 
    write_cmos_sensor(0x309E,	0x00);			 
    write_cmos_sensor(0x30A0,	0x14);			 
    write_cmos_sensor(0x30B2,	0x00);			 
    write_cmos_sensor(0x30D5,	0x09);			 
    write_cmos_sensor(0x30D6,	0x01);			 
    write_cmos_sensor(0x30D7,	0x01);			 
    write_cmos_sensor(0x30D8,	0x64);			 
    write_cmos_sensor(0x30D9,	0x89);			 
    write_cmos_sensor(0x30DE,	0x02);			 
    write_cmos_sensor(0x3102,	0x10);			 
    write_cmos_sensor(0x3103,	0x44);			 
    write_cmos_sensor(0x3104,	0x40);			 
    write_cmos_sensor(0x3105,	0x00);			 
    write_cmos_sensor(0x3106,	0x0D);			 
    write_cmos_sensor(0x3107,	0x01);			 
    write_cmos_sensor(0x310A,	0x0A);			 
    write_cmos_sensor(0x315C,	0x99);			 
    write_cmos_sensor(0x315D,	0x98);			 
    write_cmos_sensor(0x316E,	0x9A);			 
    write_cmos_sensor(0x316F,	0x99);			 
    write_cmos_sensor(0x3301,	0x03);			 
    write_cmos_sensor(0x3304,	0x05);			 
    write_cmos_sensor(0x3305,	0x04);			 
    write_cmos_sensor(0x3306,	0x12);			 
    write_cmos_sensor(0x3307,	0x03);			 
    write_cmos_sensor(0x3308,	0x0D);			 
    write_cmos_sensor(0x330A,	0x09);			 
    write_cmos_sensor(0x330C,	0x08);			 
    write_cmos_sensor(0x330D,	0x05);			 
    write_cmos_sensor(0x330E,	0x03);			 
    write_cmos_sensor(0x3318,	0x73);			 
    write_cmos_sensor(0x3322,	0x02);	 
    write_cmos_sensor(0x3342,	0x0F);
    write_cmos_sensor(0x0104,	0x00);		 									   
    write_cmos_sensor(0x0100,	0x01);	


// The register only need to enable 1 time.     
}	/*	preview_setting  */

static void capture_setting(kal_uint16 currefps)
{

	printk("capture_setting E! currefps:%d\n",currefps);
	
	if (currefps == 240) { //24fps for PIP
		write_cmos_sensor(0x0100,0x00);	   	   
		write_cmos_sensor(0x0104,0x01);	

        write_cmos_sensor(0x0101,	0x03);
	
		write_cmos_sensor(0x0307,0x17);////0x2f: 225.6  //0x17  12*23/2.5
		write_cmos_sensor(0x303C,0x4B);
		write_cmos_sensor(0x0340,((imgsensor_info.cap.framelength >> 8) & 0xFF));
		write_cmos_sensor(0x0341,(imgsensor_info.cap.framelength & 0xFF));	      
		write_cmos_sensor(0x0342,((imgsensor_info.cap.linelength >> 8) & 0xFF)); 
		write_cmos_sensor(0x0343,(imgsensor_info.cap.linelength & 0xFF));	      
		write_cmos_sensor(0x0344,0x00);
		write_cmos_sensor(0x0345,0x08);
		write_cmos_sensor(0x0346,0x00);
		write_cmos_sensor(0x0347,0x30);
		write_cmos_sensor(0x0348,0x10);
		write_cmos_sensor(0x0349,0x77);
		write_cmos_sensor(0x034A,0x0C);
		write_cmos_sensor(0x034B,0x5F);
		write_cmos_sensor(0x034C,0x10);
		write_cmos_sensor(0x034D,0x70);
		write_cmos_sensor(0x034E,0x0C);
		write_cmos_sensor(0x034F,0x30);
		write_cmos_sensor(0x0381,0x01);
		write_cmos_sensor(0x0383,0x01);
		write_cmos_sensor(0x0385,0x01);
		write_cmos_sensor(0x0387,0x01);
		write_cmos_sensor(0x3033,0x00);
		write_cmos_sensor(0x303D,0x10);
		write_cmos_sensor(0x303E,0xD0);
		write_cmos_sensor(0x3048,0x00);
		write_cmos_sensor(0x304C,0x7F);
		write_cmos_sensor(0x304D,0x04);
		write_cmos_sensor(0x3064,0x12);
		write_cmos_sensor(0x309B,0x20);
		write_cmos_sensor(0x309E,0x00);
		write_cmos_sensor(0x30A0,0x14);
		write_cmos_sensor(0x30B2,0x00);
		write_cmos_sensor(0x30D5,0x00);
		write_cmos_sensor(0x30D6,0x85);
		write_cmos_sensor(0x30D7,0x2A);
		write_cmos_sensor(0x30D8,0x64);
		write_cmos_sensor(0x30D9,0x89);
		write_cmos_sensor(0x30DE,0x00);
		write_cmos_sensor(0x3102,0x10);
		write_cmos_sensor(0x3103,0x44);
		write_cmos_sensor(0x3104,0x40);
		write_cmos_sensor(0x3105,0x00);
		write_cmos_sensor(0x3106,0x0D);
		write_cmos_sensor(0x3107,0x01);
		write_cmos_sensor(0x310A,0x0A);
		write_cmos_sensor(0x315C,0x99);
		write_cmos_sensor(0x315D,0x98);
		write_cmos_sensor(0x316E,0x9A);
		write_cmos_sensor(0x316F,0x99);
		write_cmos_sensor(0x3301,0x03);//////////fan
		write_cmos_sensor(0x3304,0x05);
		write_cmos_sensor(0x3305,0x04);
		write_cmos_sensor(0x3306,0x12);
		write_cmos_sensor(0x3307,0x03);
		write_cmos_sensor(0x3308,0x0D);
		write_cmos_sensor(0x330A,0x09);
		write_cmos_sensor(0x330C,0x08);
		write_cmos_sensor(0x330D,0x05);
		write_cmos_sensor(0x330E,0x03);
		write_cmos_sensor(0x3318,0x63);//0x60//0x64-->0x63
		write_cmos_sensor(0x3322,0x02);
		write_cmos_sensor(0x3342,0x0F);			 
		write_cmos_sensor(0x0104,0x00);		 								   
		write_cmos_sensor(0x0100,0x01);

	} else {   //30fps			//30fps for Normal capture & ZSD
		  
		write_cmos_sensor(0x0100,0x00);	   	   
		write_cmos_sensor(0x0104,0x01);	

        write_cmos_sensor(0x0101,	0x03);
	
		write_cmos_sensor(0x0307,0x17);////0x2f: 225.6
		write_cmos_sensor(0x303C,0x4B);
		write_cmos_sensor(0x0340,((imgsensor_info.cap.framelength >> 8) & 0xFF));
		write_cmos_sensor(0x0341,(imgsensor_info.cap.framelength & 0xFF));	      
		write_cmos_sensor(0x0342,((imgsensor_info.cap.linelength >> 8) & 0xFF)); 
		write_cmos_sensor(0x0343,(imgsensor_info.cap.linelength & 0xFF));	      
		write_cmos_sensor(0x0344,0x00);
		write_cmos_sensor(0x0345,0x08);
		write_cmos_sensor(0x0346,0x00);
		write_cmos_sensor(0x0347,0x30);
		write_cmos_sensor(0x0348,0x10);
		write_cmos_sensor(0x0349,0x77);
		write_cmos_sensor(0x034A,0x0C);
		write_cmos_sensor(0x034B,0x5F);
		write_cmos_sensor(0x034C,0x10);
		write_cmos_sensor(0x034D,0x70);
		write_cmos_sensor(0x034E,0x0C);
		write_cmos_sensor(0x034F,0x30);
		write_cmos_sensor(0x0381,0x01);
		write_cmos_sensor(0x0383,0x01);
		write_cmos_sensor(0x0385,0x01);
		write_cmos_sensor(0x0387,0x01);
		write_cmos_sensor(0x3033,0x00);
		write_cmos_sensor(0x303D,0x10);
		write_cmos_sensor(0x303E,0xD0);
		write_cmos_sensor(0x3048,0x00);
		write_cmos_sensor(0x304C,0x7F);
		write_cmos_sensor(0x304D,0x04);
		write_cmos_sensor(0x3064,0x12);
		write_cmos_sensor(0x309B,0x20);
		write_cmos_sensor(0x309E,0x00);
		write_cmos_sensor(0x30A0,0x14);
		write_cmos_sensor(0x30B2,0x00);
		write_cmos_sensor(0x30D5,0x00);
		write_cmos_sensor(0x30D6,0x85);
		write_cmos_sensor(0x30D7,0x2A);
		write_cmos_sensor(0x30D8,0x64);
		write_cmos_sensor(0x30D9,0x89);
		write_cmos_sensor(0x30DE,0x00);
		write_cmos_sensor(0x3102,0x10);
		write_cmos_sensor(0x3103,0x44);
		write_cmos_sensor(0x3104,0x40);
		write_cmos_sensor(0x3105,0x00);
		write_cmos_sensor(0x3106,0x0D);
		write_cmos_sensor(0x3107,0x01);
		write_cmos_sensor(0x310A,0x0A);
		write_cmos_sensor(0x315C,0x99);
		write_cmos_sensor(0x315D,0x98);
		write_cmos_sensor(0x316E,0x9A);
		write_cmos_sensor(0x316F,0x99);
		write_cmos_sensor(0x3301,0x03);///////////////fan
		write_cmos_sensor(0x3304,0x05);
		write_cmos_sensor(0x3305,0x04);
		write_cmos_sensor(0x3306,0x12);
		write_cmos_sensor(0x3307,0x03);
		write_cmos_sensor(0x3308,0x0D);
		write_cmos_sensor(0x330A,0x09);
		write_cmos_sensor(0x330C,0x08);
		write_cmos_sensor(0x330D,0x05);
		write_cmos_sensor(0x330E,0x03);
		write_cmos_sensor(0x3318,0x63);//0x60//0x64-->0x63
		write_cmos_sensor(0x3322,0x02);
		write_cmos_sensor(0x3342,0x0F);			 
		write_cmos_sensor(0x0104,0x00);		 								   
		write_cmos_sensor(0x0100,0x01);

		if (imgsensor.ihdr_en) {
		
	} else {
		
	}
		
	}
		
}

static void normal_video_setting(kal_uint16 currefps)
{
	printk("normal_video_setting E! currefps:%d\n",currefps);
	
	write_cmos_sensor(0x0100,	0x00);	   					   
	write_cmos_sensor(0x0104,	0x01);	

    write_cmos_sensor(0x0101,	0x03);
	
	write_cmos_sensor(0x0307,	0x2F);		   
	write_cmos_sensor(0x303C,	0x4B);	
	write_cmos_sensor(0x0340,	((imgsensor_info.normal_video.framelength >> 8) & 0xFF));    			 
	write_cmos_sensor(0x0341,	(imgsensor_info.normal_video.framelength & 0xFF));	       					 
	write_cmos_sensor(0x0342,	((imgsensor_info.normal_video.linelength >> 8) & 0xFF));   				 
	write_cmos_sensor(0x0343,	(imgsensor_info.normal_video.linelength & 0xFF));	          	
	write_cmos_sensor(0x0344,	0x00);
	write_cmos_sensor(0x0345,	0x08);
	write_cmos_sensor(0x0346,	0x00);
	write_cmos_sensor(0x0347,	0x30);
	write_cmos_sensor(0x0348,	0x10);
	write_cmos_sensor(0x0349,	0x77);
	write_cmos_sensor(0x034A,	0x0C);
	write_cmos_sensor(0x034B,	0x5F);
	write_cmos_sensor(0x034C,	0x08);
	write_cmos_sensor(0x034D,	0x38);
	write_cmos_sensor(0x034E,	0x06);
	write_cmos_sensor(0x034F,	0x18);
	write_cmos_sensor(0x0381,	0x01);
	write_cmos_sensor(0x0383,	0x03);
	write_cmos_sensor(0x0385,	0x01);
	write_cmos_sensor(0x0387,	0x03);
	write_cmos_sensor(0x3033,	0x00);
	write_cmos_sensor(0x303D,	0x10);
	write_cmos_sensor(0x303E,	0xD0);
	write_cmos_sensor(0x3048,	0x01);
	write_cmos_sensor(0x304C,	0x7F);
	write_cmos_sensor(0x304D,	0x04);
	write_cmos_sensor(0x3064,	0x12);
	write_cmos_sensor(0x309B,	0x28);
	write_cmos_sensor(0x309E,	0x00);
	write_cmos_sensor(0x30A0,	0x14);
	write_cmos_sensor(0x30B2,	0x00);
	write_cmos_sensor(0x30D5,	0x09);
	write_cmos_sensor(0x30D6,	0x01);
	write_cmos_sensor(0x30D7,	0x01);
	write_cmos_sensor(0x30D8,	0x64);
	write_cmos_sensor(0x30D9,	0x89);
	write_cmos_sensor(0x30DE,	0x02);
	write_cmos_sensor(0x3102,	0x10);
	write_cmos_sensor(0x3103,	0x44);
	write_cmos_sensor(0x3104,	0x40);
	write_cmos_sensor(0x3105,	0x00);
	write_cmos_sensor(0x3106,	0x0D);
	write_cmos_sensor(0x3107,	0x01);
	write_cmos_sensor(0x310A,	0x0A);
	write_cmos_sensor(0x315C,	0x99);
	write_cmos_sensor(0x315D,	0x98);
	write_cmos_sensor(0x316E,	0x9A);
	write_cmos_sensor(0x316F,	0x99);
	write_cmos_sensor(0x3301,	0x03);
	write_cmos_sensor(0x3304,	0x05);
	write_cmos_sensor(0x3305,	0x04);
	write_cmos_sensor(0x3306,	0x12);
	write_cmos_sensor(0x3307,	0x03);
	write_cmos_sensor(0x3308,	0x0D);
	write_cmos_sensor(0x330A,	0x09);
	write_cmos_sensor(0x330C,	0x08);
	write_cmos_sensor(0x330D,	0x05);
	write_cmos_sensor(0x330E,	0x03);
	write_cmos_sensor(0x3318,	0x73);
	write_cmos_sensor(0x3322,	0x02);
	write_cmos_sensor(0x3342,	0x0F);
	write_cmos_sensor(0x0104,	0x00);
	write_cmos_sensor(0x0100,	0x01);

	if (imgsensor.ihdr_en) {
	} else {
	}

}
static void hs_video_setting()
{
	printk("hs_video_setting E\n");
	write_cmos_sensor(0x0100,	0x00);	   					   
	write_cmos_sensor(0x0104,	0x01);	

    write_cmos_sensor(0x0101,	0x03);
	
	write_cmos_sensor(0x0307,	0x2F);		   
	write_cmos_sensor(0x303C,	0x4B);	
	write_cmos_sensor(0x0340,	((imgsensor_info.hs_video.framelength >> 8) & 0xFF));    			 
	write_cmos_sensor(0x0341,	(imgsensor_info.hs_video.framelength & 0xFF));	       					 
	write_cmos_sensor(0x0342,	((imgsensor_info.hs_video.linelength >> 8) & 0xFF));   				 
	write_cmos_sensor(0x0343,	(imgsensor_info.hs_video.linelength & 0xFF));	          	
	write_cmos_sensor(0x0344,	0x00);
	write_cmos_sensor(0x0345,	0x08);
	write_cmos_sensor(0x0346,	0x00);
	write_cmos_sensor(0x0347,	0x30);
	write_cmos_sensor(0x0348,	0x10);
	write_cmos_sensor(0x0349,	0x77);
	write_cmos_sensor(0x034A,	0x0C);
	write_cmos_sensor(0x034B,	0x5F);
	write_cmos_sensor(0x034C,	0x08);
	write_cmos_sensor(0x034D,	0x38);
	write_cmos_sensor(0x034E,	0x06);
	write_cmos_sensor(0x034F,	0x18);
	write_cmos_sensor(0x0381,	0x01);
	write_cmos_sensor(0x0383,	0x03);
	write_cmos_sensor(0x0385,	0x01);
	write_cmos_sensor(0x0387,	0x03);
	write_cmos_sensor(0x3033,	0x00);
	write_cmos_sensor(0x303D,	0x10);
	write_cmos_sensor(0x303E,	0xD0);
	write_cmos_sensor(0x3048,	0x01);
	write_cmos_sensor(0x304C,	0x7F);
	write_cmos_sensor(0x304D,	0x04);
	write_cmos_sensor(0x3064,	0x12);
	write_cmos_sensor(0x309B,	0x28);
	write_cmos_sensor(0x309E,	0x00);
	write_cmos_sensor(0x30A0,	0x14);
	write_cmos_sensor(0x30B2,	0x00);
	write_cmos_sensor(0x30D5,	0x09);
	write_cmos_sensor(0x30D6,	0x01);
	write_cmos_sensor(0x30D7,	0x01);
	write_cmos_sensor(0x30D8,	0x64);
	write_cmos_sensor(0x30D9,	0x89);
	write_cmos_sensor(0x30DE,	0x02);
	write_cmos_sensor(0x3102,	0x10);
	write_cmos_sensor(0x3103,	0x44);
	write_cmos_sensor(0x3104,	0x40);
	write_cmos_sensor(0x3105,	0x00);
	write_cmos_sensor(0x3106,	0x0D);
	write_cmos_sensor(0x3107,	0x01);
	write_cmos_sensor(0x310A,	0x0A);
	write_cmos_sensor(0x315C,	0x99);
	write_cmos_sensor(0x315D,	0x98);
	write_cmos_sensor(0x316E,	0x9A);
	write_cmos_sensor(0x316F,	0x99);
	write_cmos_sensor(0x3301,	0x03);
	write_cmos_sensor(0x3304,	0x05);
	write_cmos_sensor(0x3305,	0x04);
	write_cmos_sensor(0x3306,	0x12);
	write_cmos_sensor(0x3307,	0x03);
	write_cmos_sensor(0x3308,	0x0D);
	write_cmos_sensor(0x330A,	0x09);
	write_cmos_sensor(0x330C,	0x08);
	write_cmos_sensor(0x330D,	0x05);
	write_cmos_sensor(0x330E,	0x03);
	write_cmos_sensor(0x3318,	0x73);
	write_cmos_sensor(0x3322,	0x02);
	write_cmos_sensor(0x3342,	0x0F);
	write_cmos_sensor(0x0104,	0x00);
	write_cmos_sensor(0x0100,	0x01);
	if (imgsensor.ihdr_en) {
	} else {
	}

}

static void slim_video_setting()
{
	printk("slim_video_setting E\n");
	write_cmos_sensor(0x0100,	0x00);	   					   
	write_cmos_sensor(0x0104,	0x01);	

    write_cmos_sensor(0x0101,	0x03);
	
	write_cmos_sensor(0x0307,	0x2F);		   
	write_cmos_sensor(0x303C,	0x4B);	
	write_cmos_sensor(0x0340,	((imgsensor_info.slim_video.framelength >> 8) & 0xFF));    			 
	write_cmos_sensor(0x0341,	(imgsensor_info.slim_video.framelength & 0xFF));	       					 
	write_cmos_sensor(0x0342,	((imgsensor_info.slim_video.linelength >> 8) & 0xFF));   				 
	write_cmos_sensor(0x0343,	(imgsensor_info.slim_video.linelength & 0xFF));	          	
	write_cmos_sensor(0x0344,	0x00);
	write_cmos_sensor(0x0345,	0x08);
	write_cmos_sensor(0x0346,	0x00);
	write_cmos_sensor(0x0347,	0x30);
	write_cmos_sensor(0x0348,	0x10);
	write_cmos_sensor(0x0349,	0x77);
	write_cmos_sensor(0x034A,	0x0C);
	write_cmos_sensor(0x034B,	0x5F);
	write_cmos_sensor(0x034C,	0x08);
	write_cmos_sensor(0x034D,	0x38);
	write_cmos_sensor(0x034E,	0x06);
	write_cmos_sensor(0x034F,	0x18);
	write_cmos_sensor(0x0381,	0x01);
	write_cmos_sensor(0x0383,	0x03);
	write_cmos_sensor(0x0385,	0x01);
	write_cmos_sensor(0x0387,	0x03);
	write_cmos_sensor(0x3033,	0x00);
	write_cmos_sensor(0x303D,	0x10);
	write_cmos_sensor(0x303E,	0xD0);
	write_cmos_sensor(0x3048,	0x01);
	write_cmos_sensor(0x304C,	0x7F);
	write_cmos_sensor(0x304D,	0x04);
	write_cmos_sensor(0x3064,	0x12);
	write_cmos_sensor(0x309B,	0x28);
	write_cmos_sensor(0x309E,	0x00);
	write_cmos_sensor(0x30A0,	0x14);
	write_cmos_sensor(0x30B2,	0x00);
	write_cmos_sensor(0x30D5,	0x09);
	write_cmos_sensor(0x30D6,	0x01);
	write_cmos_sensor(0x30D7,	0x01);
	write_cmos_sensor(0x30D8,	0x64);
	write_cmos_sensor(0x30D9,	0x89);
	write_cmos_sensor(0x30DE,	0x02);
	write_cmos_sensor(0x3102,	0x10);
	write_cmos_sensor(0x3103,	0x44);
	write_cmos_sensor(0x3104,	0x40);
	write_cmos_sensor(0x3105,	0x00);
	write_cmos_sensor(0x3106,	0x0D);
	write_cmos_sensor(0x3107,	0x01);
	write_cmos_sensor(0x310A,	0x0A);
	write_cmos_sensor(0x315C,	0x99);
	write_cmos_sensor(0x315D,	0x98);
	write_cmos_sensor(0x316E,	0x9A);
	write_cmos_sensor(0x316F,	0x99);
	write_cmos_sensor(0x3301,	0x03);
	write_cmos_sensor(0x3304,	0x05);
	write_cmos_sensor(0x3305,	0x04);
	write_cmos_sensor(0x3306,	0x12);
	write_cmos_sensor(0x3307,	0x03);
	write_cmos_sensor(0x3308,	0x0D);
	write_cmos_sensor(0x330A,	0x09);
	write_cmos_sensor(0x330C,	0x08);
	write_cmos_sensor(0x330D,	0x05);
	write_cmos_sensor(0x330E,	0x03);
	write_cmos_sensor(0x3318,	0x73);
	write_cmos_sensor(0x3322,	0x02);
	write_cmos_sensor(0x3342,	0x0F);
	write_cmos_sensor(0x0104,	0x00);
	write_cmos_sensor(0x0100,	0x01);

	//@@video_720p_30fps_800Mbps
	
	if (imgsensor.ihdr_en) {
	} else {
	}
}
//
static kal_uint8  test_pattern_flag=0;

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	printk("enable: %d\n", enable);
	if(imgsensor.current_scenario_id != MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG)
	   {
		   if(enable) 
		   {   
			   //1640 x 1232
			   // enable color bar
			   test_pattern_flag=TRUE;
			   write_cmos_sensor(0x0600, 0x00); 
			   write_cmos_sensor(0x0601, 0x02); 	 
			   write_cmos_sensor(0x0624, 0x06); //W:3280---h
			   write_cmos_sensor(0x0625, 0x68); //		  l
			   write_cmos_sensor(0x0626, 0x04); //H:2464   h
			   write_cmos_sensor(0x0627, 0xd0); //		  l
			   write_cmos_sensor(0x6128, 0x00); 
			   write_cmos_sensor(0x6129, 0x02); 		 
			   write_cmos_sensor(0x613C, 0x06); //W 		h
			   write_cmos_sensor(0x613D, 0x68); //		   l
			   write_cmos_sensor(0x613E, 0x04); //H 		h
			   write_cmos_sensor(0x613F, 0xd0); //			   l
			   write_cmos_sensor(0x6506, 0x00); 
			   write_cmos_sensor(0x6507, 0x00);
	
		   } 
		   else 
		   {   
			   //1640 x 1232
			   test_pattern_flag=FALSE;
			   write_cmos_sensor(0x0600, 0x00); 
			   write_cmos_sensor(0x0601, 0x00); 	 
			   write_cmos_sensor(0x0624, 0x06); //W:3280---h
			   write_cmos_sensor(0x0625, 0x68); //		  l
			   write_cmos_sensor(0x0626, 0x04); //H:2464   h
			   write_cmos_sensor(0x0627, 0xd0); //		  l
			   write_cmos_sensor(0x6128, 0x00); 
			   write_cmos_sensor(0x6129, 0x02); 		 
			   write_cmos_sensor(0x613C, 0x06); //W 		h
			   write_cmos_sensor(0x613D, 0x68); //		   l
			   write_cmos_sensor(0x613E, 0x04); //H 		h
			   write_cmos_sensor(0x613F, 0xd0); //			   l
			   write_cmos_sensor(0x6506, 0x00); 
			   write_cmos_sensor(0x6507, 0x00);
	
		   }
	   }
	   else
	   {
		   if(enable) 
		   {   
			   //3280 x 2464
			   // enable color bar
			   test_pattern_flag=TRUE;
			   write_cmos_sensor(0x0600, 0x00); 
			   write_cmos_sensor(0x0601, 0x02); 	 
			   write_cmos_sensor(0x0624, 0x0C); //W:3280---h
			   write_cmos_sensor(0x0625, 0xD0); //		  l
			   write_cmos_sensor(0x0626, 0x09); //H:2464   h
			   write_cmos_sensor(0x0627, 0xA0); //		  l
			   write_cmos_sensor(0x6128, 0x00); 
			   write_cmos_sensor(0x6129, 0x02); 		 
			   write_cmos_sensor(0x613C, 0x0C); //W 		h
			   write_cmos_sensor(0x613D, 0xD0); //		   l
			   write_cmos_sensor(0x613E, 0x09); //H 		h
			   write_cmos_sensor(0x613F, 0xA0); //			   l
			   write_cmos_sensor(0x6506, 0x00); 
			   write_cmos_sensor(0x6507, 0x00);
	
		   } 
		   else 
		   {   
			   test_pattern_flag=FALSE;
			   write_cmos_sensor(0x0600, 0x00); 
			   write_cmos_sensor(0x0601, 0x02); 	 
			   write_cmos_sensor(0x0624, 0x0C); //W:3280---h
			   write_cmos_sensor(0x0625, 0xD0); //		  l
			   write_cmos_sensor(0x0626, 0x09); //H:2464   h
			   write_cmos_sensor(0x0627, 0xA0); //		  l
			   write_cmos_sensor(0x6128, 0x00); 
			   write_cmos_sensor(0x6129, 0x02); 		 
			   write_cmos_sensor(0x613C, 0x0C); //W 		h
			   write_cmos_sensor(0x613D, 0xD0); //		   l
			   write_cmos_sensor(0x613E, 0x09); //H 		h
			   write_cmos_sensor(0x613F, 0xA0); //			   l
			   write_cmos_sensor(0x6506, 0x00); 
			   write_cmos_sensor(0x6507, 0x00);
	
	
		   }
	   }
		   
	   return ERROR_NONE;

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
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
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
            
            *sensor_id = return_sensor_id();

			if (*sensor_id == imgsensor_info.sensor_id) {				
				printk("imx328 i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);	  
				return ERROR_NONE;
			}	
			printk("imx328 Read sensor id fail, id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, *sensor_id);
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
	kal_uint32 sensor_id = 0; 
	LOG_1;	
	LOG_2;
	printk("by zx:imx328 open \n");
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
            sensor_id = return_sensor_id();
            
			if (sensor_id == imgsensor_info.sensor_id) {				
				printk("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);	  
				break;
			}	
			printk("Read sensor id fail, id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
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
	/*
	iTemp = read_cmos_sensor(0x0101);
	iTemp&= ~0x03; //Clear the mirror and flip bits.
	write_cmos_sensor_8(0x0101, iTemp | 0x03); //Set mirror and flip
*/
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
	imgsensor.ihdr_en = KAL_FALSE;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);
	printk("by zx:imx328 open exit\n");
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
	printk("E\n");

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
	printk("E\n");

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
	//set_mirror_flip(imgsensor.mirror);
	printk("by zx:imx328preview\n");
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
    printk("E\n");
	
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
    if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {//PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
        imgsensor.pclk = imgsensor_info.cap1.pclk;
        imgsensor.line_length = imgsensor_info.cap1.linelength;
        imgsensor.frame_length = imgsensor_info.cap1.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    } else {
        //if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
            //printk("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",imgsensor.current_fps,imgsensor_info.cap.max_framerate/10);
        imgsensor.pclk = imgsensor_info.cap.pclk;
        imgsensor.line_length = imgsensor_info.cap.linelength;
        imgsensor.frame_length = imgsensor_info.cap.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);
    capture_setting(imgsensor.current_fps);
	if(imgsensor.test_pattern == KAL_TRUE)
	{
		set_test_pattern_mode(TRUE);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.test_pattern = KAL_FALSE;
		spin_unlock(&imgsensor_drv_lock);
	}

	//set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
}    /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("E\n");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;  
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	//set_mirror_flip(imgsensor.mirror);
	
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("E\n");
	
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
	//set_mirror_flip(imgsensor.mirror);
	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	printk("E\n");
	
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
    return ERROR_NONE;
}   /*  slim_video   */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	printk("E\n");
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
	printk("scenario_id = %d\n", scenario_id);

	
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
	printk("scenario_id = %d\n", scenario_id);
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
			printk("Error ScenarioId setting");
			preview(image_window, sensor_config_data);
			return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}	/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	printk("framerate = %d\n ", framerate);
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
	printk("enable = %d, framerate = %d \n", enable, framerate);
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
	printk("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength):0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();			
			break;			
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			if(framerate == 0)
				return ERROR_NONE;
			frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength):0;			
			imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();			
			break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        	  if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            } else {
        		    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                    printk("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",framerate,imgsensor_info.cap.max_framerate/10);
                frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            }
			set_dummy();			
			break;	
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();			
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
			imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();	
			break;		
		default:  //coding with  preview scenario by default
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();	
			printk("error scenario_id = %d, we use preview scenario \n", scenario_id);
			break;
	}	
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
		enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
	printk("scenario_id = %d\n", scenario_id);

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
 
    printk("feature_id = %d\n", feature_id);
	switch (feature_id) {
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
			*feature_para_len=4;
			break;
		case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:	 
            LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n", imgsensor.pclk,imgsensor.current_fps);
			*feature_return_para_32 = imgsensor.pclk;
			*feature_para_len=4;
			break;		   
		case SENSOR_FEATURE_SET_ESHUTTER:
            set_shutter(*feature_data);
			break;
		case SENSOR_FEATURE_SET_NIGHTMODE:
            night_mode((BOOL) *feature_data);
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
			set_auto_flicker_mode(
			(BOOL) (*feature_data_16), *(feature_data_16 + 1));
			break;
		case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
			set_max_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *feature_data,
						      *(feature_data + 1));
			break;
		case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
			get_default_framerate_by_scenario(
			(enum MSDK_SCENARIO_ID_ENUM) *(feature_data),
			(MUINT32 *) (uintptr_t) (*(feature_data + 1)));
			break;
		case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((BOOL)*feature_data);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing			 
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len=4;							 
			break;				
		case SENSOR_FEATURE_SET_FRAMERATE:
            LOG_INF("current fps :%llu\n", *feature_data_32);
			spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data_32;
			spin_unlock(&imgsensor_drv_lock);		
			break;
		case SENSOR_FEATURE_SET_HDR:
            LOG_INF("ihdr enable :%d\n", *feature_data_32);
			spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_en = (UINT8)*feature_data_32;
			spin_unlock(&imgsensor_drv_lock);		
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
            LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%llu\n", *feature_data);
			wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));
		
			switch (*feature_data_32) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy(
					(void *)wininfo,
					(void *)&imgsensor_winsize_info[1],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				memcpy(
					(void *)wininfo,
					(void *)&imgsensor_winsize_info[2],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				memcpy(
					(void *)wininfo,
					(void *)&imgsensor_winsize_info[3],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				memcpy(
					(void *)wininfo,
					(void *)&imgsensor_winsize_info[4],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
				break;
        	        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        	        default:
				memcpy(
					(void *)wininfo,
					(void *)&imgsensor_winsize_info[0],
					sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
        	            break;
        	    }
			break;
		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
			//printk("SENSOR_SET_SENSOR_IHDR is no support");
			//printk("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data_32,(UINT16)*(feature_data_32+1),(UINT16)*(feature_data_32+2)); 
			//ihdr_write_shutter_gain((UINT16)*feature_data_32,(UINT16)*(feature_data_32+1),(UINT16)*(feature_data_32+2));	
			break;
		case SENSOR_FEATURE_GET_PIXEL_RATE:
		{
			kal_uint32 rate;

			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				rate = (imgsensor_info.cap.pclk /
				       (imgsensor_info.cap.linelength - 80))*
				       imgsensor_info.cap.grabwindow_width;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
				rate = (imgsensor_info.normal_video.pclk /
				       (imgsensor_info.normal_video.linelength - 80))*
				       imgsensor_info.normal_video.grabwindow_width;
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				rate = (imgsensor_info.hs_video.pclk /
				       (imgsensor_info.hs_video.linelength - 80))*
				       imgsensor_info.hs_video.grabwindow_width;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				rate = (imgsensor_info.slim_video.pclk /
				       (imgsensor_info.slim_video.linelength - 80))*
				       imgsensor_info.slim_video.grabwindow_width;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
			default:
				rate = (imgsensor_info.pre.pclk /
				       (imgsensor_info.pre.linelength - 80))*
				       imgsensor_info.pre.grabwindow_width;
				break;
			}
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
		}
		break;
		default:
		break;
	}
	return ERROR_NONE;
}	/*	feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX328_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	OV5693_MIPI_RAW_SensorInit	*/
