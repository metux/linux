/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "ov08d10_mipi_raw_Sensor.h"

#define PFX "OV08D10_camera_sensor"
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)

/* Camera Hardwareinfo */
//extern struct global_otp_struct hw_info_main2_otp;

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static  imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV08D10_SENSOR_ID,

    .checksum_value = 0xb1893b4f, /*checksum value for Camera Auto Test*/

    .pre = {
		.pclk = 36000000,    /*record different mode's pclk*/
		.linelength  =  478,    /*record different mode's linelength*/
		.framelength = 2504,    /*record different mode's framelength*/
		.startx = 0, /*record different mode's startx of grabwindow*/
		.starty = 0,    /*record different mode's starty of grabwindow*/
		.grabwindow_width  = 1632,
		.grabwindow_height = 1224,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 144000000,
    },
    .cap = {
		.pclk = 36000000,
		.linelength  =  460,
		.framelength = 2608,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
    },
    .normal_video = { 
		.pclk = 36000000,
		.linelength  =  460,
		.framelength = 2608,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 3264,
		.grabwindow_height = 2448,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 288000000,
    },
    .hs_video = {
		.pclk = 36000000,
		.linelength  = 478,
		.framelength = 628,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 640,
		.grabwindow_height =  480,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 1200,
		.mipi_pixel_rate = 144000000,
    },
    .slim_video = {
		.pclk = 36000000,
		.linelength  =  478,
		.framelength = 2504,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
		.mipi_pixel_rate = 144000000,
     },
	.margin = 20,            //sensor framelength & shutter margin
	.min_shutter = 4,        //min shutter
    .min_gain =  64,   /*1x gain*/
    .max_gain = 992,   /*15.5x gain*/
    .min_gain_iso = 100,
    .gain_step = 1,
    .gain_type = 1,    /*to be modify,no gain table for sony*/
    .max_frame_length = 0x7FFFEA,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,   /*isp gain delay frame for AE cycle*/
    .frame_time_delay_frame = 2,
    .ihdr_support = 0,             /*1, support; 0,not support*/
    .ihdr_le_firstline = 0,        /*1,le first ; 0, se first*/
    .temperature_support = 0,/* 1, support; 0,not support */
    .sensor_mode_num = 5,
    .cap_delay_frame = 3,          /*enter capture delay frame num*/
    .pre_delay_frame = 3,          /*enter preview delay frame num*/
    .video_delay_frame = 3,        /*enter video delay frame num*/
    .hs_video_delay_frame = 3, /*enter high speed video  delay frame num*/
    .slim_video_delay_frame = 3,/*enter slim video delay frame num*/
    .isp_driving_current = ISP_DRIVING_8MA, /*mclk driving current*/
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,//sensor_interface_type
	.mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = 0, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,//mclk value, suggest 24 or 26 for 24Mhz or 26Mhz
	.mipi_lane_num = SENSOR_MIPI_2_LANE,//mipi lane num
	.i2c_addr_table = {0x6c,0x20,0xff},
	.i2c_speed = 400,
};


static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,                //mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
    .shutter = 0x0410,            /*current shutter*/
    .gain = 0x40,                 /*current gain*/
	.dummy_pixel = 0,                    //current dummypixel
	.dummy_line = 0,                    //current dummyline
	.current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,        //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
    .ihdr_mode = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x6c,//record current sensor's i2c write id
    .vblank_convert = 2504, /* vts to vblank*/
    .current_ae_effective_frame = 2,
    .max_shutter = 1523810,
};


/*add rawinfo start*/
//static struct  SENSOR_RAWINFO_STRUCT imgsensor_raw_info = {
//	3264,//raw_weight
//	2448,//raw_height
//	2,//unpack raw byte,raw10 packed by 16bit(2byte)
//	BAYER_RGGB,//raw_colorFilterValue
//	64,//raw_blackLevel
//	77.3,//raw_viewAngle
//	10,//raw_bitWidth
//	16//raw_maxSensorGain 64x
//};
/*add rawinfo end*/


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
    {3264, 2448,   0,   0, 3264, 2448, 1632, 1224, 0, 0, 1632, 1224, 0, 0, 1632, 1224}, // Preview
    {3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, // capture   
    {3264, 2448,   0,   0, 3264, 2448, 3264, 2448, 0, 0, 3264, 2448, 0, 0, 3264, 2448}, // video
    {3264, 2448, 992, 744, 1280,  960,  640,  480, 0, 0,  640,  480, 0, 0,  640,  480}, // hs_video  
    {3264, 2448, 352, 504, 2560, 1440, 1280,  720, 0, 0, 1280,  720, 0, 0, 1280,  720}, // slim_video
};

#ifdef Enable_table_write
#define I2C_BUFFER_LEN 255 /* trans# max is 255, each 3 bytes */

static kal_uint16 table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
  char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, index;
  kal_uint16 addr = 0, addr_last = 0, data;

  tosend = 0;
	index = 0;
	while (len > index) {
		addr = para[index];
		puSendCmd[tosend++] = (char)(addr & 0xFF);
		data = para[index + 1];
		puSendCmd[tosend++] = (char)(data & 0xFF);
		index += 2;
		addr_last = addr;
		/* Write when remain buffer size is less than 3 bytes
		 * or reach end of data
		 */
		if ((I2C_BUFFER_LEN - tosend) < 3
			|| index == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd,
				tosend,
				imgsensor.i2c_write_id,
				2,
				imgsensor_info.i2c_speed);
			tosend = 0;
		}
	}
	return 0;
}
#endif

static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kal_uint16 get_byte = 0;
    char pu_send_cmd[1] = { (char)(addr & 0xFF) };
    iReadRegI2C(pu_send_cmd, 1, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
    return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[2] = {(char)(addr & 0xFF), (char)(para & 0xFF)};
    iWriteRegI2C(pu_send_cmd, 2, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
    if (imgsensor.frame_length%2 != 0) {
        imgsensor.frame_length = imgsensor.frame_length - imgsensor.frame_length % 2;
    }
    LOG_INF("ov08d10-->imgsensor.frame_length = %d\n", imgsensor.frame_length);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x05, (((imgsensor.frame_length - imgsensor.vblank_convert) * 2) & 0x7F00) >> 8);
    write_cmos_sensor(0x06, ((imgsensor.frame_length - imgsensor.vblank_convert )* 2) & 0xFF);
    write_cmos_sensor(0x01, 0x01);
} 

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	LOG_INF("ov08d10--> framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    if (frame_length >= imgsensor.min_frame_length) {
	imgsensor.frame_length = frame_length;
    } 
	else {
	imgsensor.frame_length = imgsensor.min_frame_length;
    }
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;

    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
	imgsensor.frame_length = imgsensor_info.max_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if (min_framelength_en) {
	imgsensor.min_frame_length = imgsensor.frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);

	set_dummy();
}    /*    set_max_framerate  */

static void set_shutter_frame_length(
		kal_uint16 shutter, kal_uint16 frame_length)
{
    unsigned long flags;
    kal_uint16 realtime_fps = 0;
    kal_int32 dummy_line = 0;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
    spin_lock(&imgsensor_drv_lock);
    /*Change frame time*/
    if (frame_length > 1) {
	dummy_line = frame_length - imgsensor.frame_length;
    }
    imgsensor.frame_length = imgsensor.frame_length + dummy_line;

    if (shutter > imgsensor.frame_length - imgsensor_info.margin) {
	imgsensor.frame_length = shutter + imgsensor_info.margin;
    }
    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
	imgsensor.frame_length = imgsensor_info.max_frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);

	shutter = (shutter < imgsensor_info.min_shutter) ?	imgsensor_info.min_shutter : shutter;

    if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) {
	shutter = (imgsensor_info.max_frame_length - imgsensor_info.margin);
    }

    //imgsensor.frame_length = imgsensor.frame_length - imgsensor.frame_length % 2;

    if (imgsensor.autoflicker_en) {
	realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

	if (realtime_fps >= 297 && realtime_fps <= 305) {
	    set_max_framerate(296, 0);
	} else if (realtime_fps >= 147 && realtime_fps <= 150) {
	    set_max_framerate(146, 0);
	} else {
	    /* Extend frame length*/
	    write_cmos_sensor(0xfd, 0x01);
	    write_cmos_sensor(0x05, (((imgsensor.frame_length - imgsensor.vblank_convert) * 2) & 0x7F00) >> 8);
	    write_cmos_sensor(0x06, (((imgsensor.frame_length - imgsensor.vblank_convert) * 2)) & 0xFF);
	    write_cmos_sensor(0x01, 0x01);
	}
    } else {
	/* Extend frame length*/
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x05, (((imgsensor.frame_length - imgsensor.vblank_convert) * 2) & 0x7F00) >> 8);
	write_cmos_sensor(0x06, (((imgsensor.frame_length - imgsensor.vblank_convert) * 2)) & 0xFF);
	write_cmos_sensor(0x01, 0x01);
    }

    /* Update Shutter*/
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x02, (shutter*2 >> 16) & 0xFF);
    write_cmos_sensor(0x03, (shutter*2 >> 8) & 0xFF);
    write_cmos_sensor(0x04,  shutter*2  & 0xFF);
    write_cmos_sensor(0x01, 0x01);
    LOG_INF("ov08d10-->Add for N3D! shutterlzl =%d, framelength =%d\n", shutter, imgsensor.frame_length);

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

static int long_exposure_status = 0;

static void write_shutter(kal_uint32 shutter)
{
    kal_uint16 realtime_fps = 0;

    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin) {
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    } else {
        imgsensor.frame_length = imgsensor.min_frame_length;
    }
    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    }
    spin_unlock(&imgsensor_drv_lock);
    if (shutter < imgsensor_info.min_shutter) {
        shutter = imgsensor_info.min_shutter;
    }
    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305){
            set_max_framerate(296, 0);
        } else if(realtime_fps >= 147 && realtime_fps <= 150){
            set_max_framerate(146, 0);
        } else {
            set_max_framerate(realtime_fps, 0);
		}
    }

    if (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) {
        if (shutter > imgsensor.max_shutter) {
            shutter = imgsensor.max_shutter;
        }
        //Frame exposure mode customization for LE
        imgsensor.ae_frm_mode.frame_mode_1 = IMGSENSOR_AE_MODE_SE;
        imgsensor.ae_frm_mode.frame_mode_2 = IMGSENSOR_AE_MODE_SE;
        write_cmos_sensor(0xfd, 0x01);
        write_cmos_sensor(0x24, 0x10);
        write_cmos_sensor(0x02, 0x02);
        write_cmos_sensor(0x03, 0x63);
        write_cmos_sensor(0x04, 0x69);
        write_cmos_sensor(0x01, 0x01);
        long_exposure_status = 1;
    } else if(long_exposure_status == 1){
        pr_debug("le shutter is %d exit\n",shutter);
        write_cmos_sensor(0xfd, 0x00);
        write_cmos_sensor(0x24, 0x10);
        write_cmos_sensor(0x02, 0x00);
        write_cmos_sensor(0x03, 0x06);
        write_cmos_sensor(0x04, 0x1D);
        write_cmos_sensor(0x01, 0x01);
        long_exposure_status = 0;
    }

    imgsensor.current_ae_effective_frame = 2;

    if(long_exposure_status == 0){
        imgsensor.frame_length = (imgsensor.frame_length  >> 1) << 1;
	    write_cmos_sensor(0xfd, 0x01);
	    write_cmos_sensor(0x05, (((imgsensor.frame_length - imgsensor.vblank_convert) * 2) & 0x7F00) >> 8);
	    write_cmos_sensor(0x06, (((imgsensor.frame_length - imgsensor.vblank_convert) * 2)) & 0xFF);
	    write_cmos_sensor(0x01, 0x01);
    }
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x02, (shutter*2 >> 16) & 0xFF);
    write_cmos_sensor(0x03, (shutter*2 >> 8) & 0xFF);
    write_cmos_sensor(0x04,  shutter*2  & 0xFF);
    write_cmos_sensor(0x01, 0x01);

     LOG_INF("ov08d10--> shutter =%d, framelength =%d, realtime_fps =%d\n",
	 	shutter, imgsensor.frame_length, realtime_fps);
}				/* set_shutter_frame_length */

static void set_shutter(kal_uint32 shutter)
{
    unsigned long flags;
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
    write_shutter(shutter);
} /* set_shutter */
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
static kal_uint16 gain2reg(kal_uint16 gain)
{
    kal_uint16 reg_gain = 0x0;
    
    if (gain < 64) {
	gain = 64;
    } 
	else if (gain > (15.5 * 64)) {
	gain = 15.5 * 64;
    }
    reg_gain = gain / 4;
    return (kal_uint16) reg_gain;
}

static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("ov08d10-->gain = %d, reg_gain = 0x%x\n ", gain, reg_gain);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x24, (reg_gain & 0xFF));
    write_cmos_sensor(0x01, 0x01);

    return gain;
} /* set_gain */

static void ihdr_write_shutter_gain(
	    kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
    LOG_INF("ov08d10-->le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
    if (imgsensor.ihdr_mode) {

	spin_lock(&imgsensor_drv_lock);
	if (le > imgsensor.min_frame_length - imgsensor_info.margin) {
	    imgsensor.frame_length = le + imgsensor_info.margin;
	} else {
	    imgsensor.frame_length = imgsensor.min_frame_length;
	}
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
	    imgsensor.frame_length = imgsensor_info.max_frame_length;
	}
	spin_unlock(&imgsensor_drv_lock);

	if (le < imgsensor_info.min_shutter) {
	    le = imgsensor_info.min_shutter;
	}
	if (se < imgsensor_info.min_shutter) {
	    se = imgsensor_info.min_shutter;
	}

	/* Extend frame length first*/
	set_gain(gain);
    }
}

static void set_mirror_flip(kal_uint8 image_mirror)
{
	printk("ov08d10-->image_mirror = %d\n", image_mirror);

	/********************************************************
	 *

	 *   ISP and Sensor flip or mirror register bit should be the same!!
	 *
	 ********************************************************/

	switch (image_mirror) {
	case IMAGE_NORMAL:

		break;
	case IMAGE_H_MIRROR:

		break;
	case IMAGE_V_MIRROR:

		break;
	case IMAGE_HV_MIRROR:

		break;
	default:
		printk("ov08d10-->Error image_mirror setting\n");
	}

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

static void load_init_setting(void)
{
    // initial_setting
    LOG_INF("ov08d10--> sensor_init Start\n");
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0e);
    write_cmos_sensor(0x20, 0x0b);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x11, 0x2a);
    write_cmos_sensor(0x14, 0x43);
    write_cmos_sensor(0x1e, 0x23);
    write_cmos_sensor(0x16, 0x82);
    write_cmos_sensor(0x21, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x12, 0x00);
    write_cmos_sensor(0x02, 0x00);
    write_cmos_sensor(0x03, 0x12);
    write_cmos_sensor(0x04, 0x50);
    write_cmos_sensor(0x05, 0x00);
    write_cmos_sensor(0x06, 0xd0);
    write_cmos_sensor(0x07, 0x05);
    write_cmos_sensor(0x21, 0x02);
    write_cmos_sensor(0x24, 0x30);
    write_cmos_sensor(0x33, 0x03);
    write_cmos_sensor(0x01, 0x03);
    write_cmos_sensor(0x19, 0x10);
    write_cmos_sensor(0x42, 0x55);
    write_cmos_sensor(0x43, 0x00);
    write_cmos_sensor(0x47, 0x07);
    write_cmos_sensor(0x48, 0x08);
    write_cmos_sensor(0x4c, 0x38);
    write_cmos_sensor(0xb2, 0x7e);
    write_cmos_sensor(0xb3, 0x7b);
    write_cmos_sensor(0xbd, 0x08);
    write_cmos_sensor(0xd2, 0x47);
    write_cmos_sensor(0xd3, 0x10);
    write_cmos_sensor(0xd4, 0x0d);
    write_cmos_sensor(0xd5, 0x08);
    write_cmos_sensor(0xd6, 0x07);
    write_cmos_sensor(0xb1, 0x00);
    write_cmos_sensor(0xb4, 0x00);
    write_cmos_sensor(0xb7, 0x0a);
    write_cmos_sensor(0xbc, 0x44);
    write_cmos_sensor(0xbf, 0x42);
    write_cmos_sensor(0xc1, 0x10);
    write_cmos_sensor(0xc3, 0x24);
    write_cmos_sensor(0xc8, 0x03);
    write_cmos_sensor(0xc9, 0xf8);
    write_cmos_sensor(0xe1, 0x33);
    write_cmos_sensor(0xe2, 0xbb);
    write_cmos_sensor(0x51, 0x0c);
    write_cmos_sensor(0x52, 0x0a);
    write_cmos_sensor(0x57, 0x8c);
    write_cmos_sensor(0x59, 0x09);
    write_cmos_sensor(0x5a, 0x08);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x60, 0x02);
    write_cmos_sensor(0x6d, 0x5c);
    write_cmos_sensor(0x76, 0x16);
    write_cmos_sensor(0x7c, 0x11);
    write_cmos_sensor(0x90, 0x28);
    write_cmos_sensor(0x91, 0x16);
    write_cmos_sensor(0x92, 0x1c);
    write_cmos_sensor(0x93, 0x24);
    write_cmos_sensor(0x95, 0x48);
    write_cmos_sensor(0x9c, 0x06);
    write_cmos_sensor(0xca, 0x0c);
    write_cmos_sensor(0xce, 0x0d);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc0, 0x00);
    write_cmos_sensor(0xdd, 0x18);
    write_cmos_sensor(0xde, 0x19);
    write_cmos_sensor(0xdf, 0x32);
    write_cmos_sensor(0xe0, 0x70);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc2, 0x05);
    write_cmos_sensor(0xd7, 0x88);
    write_cmos_sensor(0xd8, 0x77);
    write_cmos_sensor(0xd9, 0x66);
    write_cmos_sensor(0xfd, 0x07);
    write_cmos_sensor(0x00, 0xf8);
    write_cmos_sensor(0x01, 0x2b);
    write_cmos_sensor(0x05, 0x40);
    write_cmos_sensor(0x08, 0x06);
    write_cmos_sensor(0x09, 0x11);
    write_cmos_sensor(0x28, 0x6f);
    write_cmos_sensor(0x2a, 0x20);
    write_cmos_sensor(0x2b, 0x05);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x52, 0x00);
    write_cmos_sensor(0x53, 0x80);
    write_cmos_sensor(0x54, 0x00);
    write_cmos_sensor(0x55, 0x80);
    write_cmos_sensor(0x56, 0x00);
    write_cmos_sensor(0x57, 0x80);
    write_cmos_sensor(0x58, 0x00);
    write_cmos_sensor(0x59, 0x80);
    write_cmos_sensor(0x5c, 0x3f);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0x9a, 0x30);
    write_cmos_sensor(0xa8, 0x02);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0xa0, 0x00);
    write_cmos_sensor(0xa1, 0x08);
    write_cmos_sensor(0xa2, 0x09);
    write_cmos_sensor(0xa3, 0x90);
    write_cmos_sensor(0xa4, 0x00);
    write_cmos_sensor(0xa5, 0x08);
    write_cmos_sensor(0xa6, 0x0c);
    write_cmos_sensor(0xa7, 0xc0);
    write_cmos_sensor(0xfd, 0x05);
    write_cmos_sensor(0x04, 0x40);
    write_cmos_sensor(0x07, 0x00);
    write_cmos_sensor(0x0D, 0x01);
    write_cmos_sensor(0x0F, 0x01);
    write_cmos_sensor(0x10, 0x00);
    write_cmos_sensor(0x11, 0x00);
    write_cmos_sensor(0x12, 0x0C);
    write_cmos_sensor(0x13, 0xCF);
    write_cmos_sensor(0x14, 0x00);
    write_cmos_sensor(0x15, 0x00);
    write_cmos_sensor(0x18, 0x00);
    write_cmos_sensor(0x19, 0x00);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x24, 0x01);
    write_cmos_sensor(0xc0, 0x16);
    write_cmos_sensor(0xc1, 0x08);
    write_cmos_sensor(0xc2, 0x30);
    write_cmos_sensor(0x8e, 0x0c);
    write_cmos_sensor(0x8f, 0xc0);
    write_cmos_sensor(0x90, 0x09);
    write_cmos_sensor(0x91, 0x90);
    write_cmos_sensor(0xb7, 0x02);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0f);
    write_cmos_sensor(0xe7, 0x03);
    write_cmos_sensor(0xe7, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0xa0, 0x00);
    LOG_INF("ov08d10--> sensor_init End\n");
}

/*************************************************************************
 * FUNCTION
 *    preview_setting
 *
 * DESCRIPTION
 *    Sensor preview
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/

static void load_preview_setting(void)
{
    // 1632x1224_2lane_144M_30fps
    LOG_INF("ov08d10--> preview_setting E!\n");
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0e);
    write_cmos_sensor(0x20, 0x0b);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x1d, 0x00);
    write_cmos_sensor(0x18, 0x3c);
    write_cmos_sensor(0x1c, 0x19);
    write_cmos_sensor(0x11, 0x2a);
    write_cmos_sensor(0x14, 0x43);
    write_cmos_sensor(0x1e, 0x23);
    write_cmos_sensor(0x16, 0x82);
    write_cmos_sensor(0x21, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x1a, 0x0a);
    write_cmos_sensor(0x1b, 0x08);
    write_cmos_sensor(0x2a, 0x01);
    write_cmos_sensor(0x2b, 0x9a);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x12, 0x00);
    write_cmos_sensor(0x02, 0x00);
    write_cmos_sensor(0x03, 0x09);
    write_cmos_sensor(0x04, 0x6a);
    write_cmos_sensor(0x05, 0x09);
    write_cmos_sensor(0x06, 0xc8);
    write_cmos_sensor(0x07, 0x05);
    write_cmos_sensor(0x21, 0x02);
    write_cmos_sensor(0x24, 0xf8);
    write_cmos_sensor(0x31, 0x06);
    write_cmos_sensor(0x33, 0x03);
    write_cmos_sensor(0x01, 0x03);
    write_cmos_sensor(0x19, 0x10);
    write_cmos_sensor(0x42, 0x55);
    write_cmos_sensor(0x43, 0x00);
    write_cmos_sensor(0x47, 0x07);
    write_cmos_sensor(0x48, 0x08);
    write_cmos_sensor(0x4c, 0x38);
    write_cmos_sensor(0xb2, 0x7e);
    write_cmos_sensor(0xb3, 0x7b);
    write_cmos_sensor(0xbd, 0x08);
    write_cmos_sensor(0xd2, 0x47);
    write_cmos_sensor(0xd3, 0x10);
    write_cmos_sensor(0xd4, 0x0d);
    write_cmos_sensor(0xd5, 0x08);
    write_cmos_sensor(0xd6, 0x07);
    write_cmos_sensor(0xb1, 0x00);
    write_cmos_sensor(0xb4, 0x00);
    write_cmos_sensor(0xb7, 0x0a);
    write_cmos_sensor(0xbc, 0x44);
    write_cmos_sensor(0xbf, 0x42);
    write_cmos_sensor(0xc1, 0x10);
    write_cmos_sensor(0xc3, 0x24);
    write_cmos_sensor(0xc8, 0x03);
    write_cmos_sensor(0xc9, 0xf8);
    write_cmos_sensor(0xe1, 0x33);
    write_cmos_sensor(0xe2, 0xbb);
    write_cmos_sensor(0x51, 0x0c);
    write_cmos_sensor(0x52, 0x0a);
    write_cmos_sensor(0x57, 0x8c);
    write_cmos_sensor(0x59, 0x09);
    write_cmos_sensor(0x5a, 0x08);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x60, 0x02);
    write_cmos_sensor(0x6d, 0x5c);
    write_cmos_sensor(0x76, 0x16);
    write_cmos_sensor(0x7c, 0x1a);
    write_cmos_sensor(0x90, 0x28);
    write_cmos_sensor(0x91, 0x16);
    write_cmos_sensor(0x92, 0x1c);
    write_cmos_sensor(0x93, 0x24);
    write_cmos_sensor(0x95, 0x48);
    write_cmos_sensor(0x9c, 0x06);
    write_cmos_sensor(0xca, 0x0c);
    write_cmos_sensor(0xce, 0x0d);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc0, 0x00);
    write_cmos_sensor(0xdd, 0x18);
    write_cmos_sensor(0xde, 0x19);
    write_cmos_sensor(0xdf, 0x32);
    write_cmos_sensor(0xe0, 0x70);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc2, 0x05);
    write_cmos_sensor(0xd7, 0x88);
    write_cmos_sensor(0xd8, 0x77);
    write_cmos_sensor(0xd9, 0x66);
    write_cmos_sensor(0xfd, 0x07);
    write_cmos_sensor(0x00, 0xf8);
    write_cmos_sensor(0x01, 0x2b);
    write_cmos_sensor(0x05, 0x40);
    write_cmos_sensor(0x08, 0x03);
    write_cmos_sensor(0x09, 0x08);
    write_cmos_sensor(0x28, 0x6f);
    write_cmos_sensor(0x2a, 0x20);
    write_cmos_sensor(0x2b, 0x05);
    write_cmos_sensor(0x2c, 0x01);
    write_cmos_sensor(0x50, 0x02);
    write_cmos_sensor(0x51, 0x03);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x52, 0x00);
    write_cmos_sensor(0x53, 0x80);
    write_cmos_sensor(0x54, 0x00);
    write_cmos_sensor(0x55, 0x80);
    write_cmos_sensor(0x56, 0x00);
    write_cmos_sensor(0x57, 0x80);
    write_cmos_sensor(0x58, 0x00);
    write_cmos_sensor(0x59, 0x80);
    write_cmos_sensor(0x5c, 0x3f);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0x9a, 0x30);
    write_cmos_sensor(0xa8, 0x02);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0xa9, 0x04);
    write_cmos_sensor(0xaa, 0xd0);
    write_cmos_sensor(0xab, 0x06);
    write_cmos_sensor(0xac, 0x68);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0xa0, 0x00);
    write_cmos_sensor(0xa1, 0x04);
    write_cmos_sensor(0xa2, 0x04);
    write_cmos_sensor(0xa3, 0xc8);
    write_cmos_sensor(0xa4, 0x00);
    write_cmos_sensor(0xa5, 0x04);
    write_cmos_sensor(0xa6, 0x06);
    write_cmos_sensor(0xa7, 0x60);
    write_cmos_sensor(0xfd, 0x05);
    write_cmos_sensor(0xfd, 0x05);
    write_cmos_sensor(0x04, 0x60);
    write_cmos_sensor(0x06, 0x80);
    write_cmos_sensor(0x07, 0x99);
    write_cmos_sensor(0x0D, 0x03);
    write_cmos_sensor(0x0F, 0x03);
    write_cmos_sensor(0x10, 0x00);
    write_cmos_sensor(0x11, 0x00);
    write_cmos_sensor(0x12, 0x0C);
    write_cmos_sensor(0x13, 0xCF);
    write_cmos_sensor(0x14, 0x00);
    write_cmos_sensor(0x15, 0x00);
    write_cmos_sensor(0x18, 0x06);
    write_cmos_sensor(0x19, 0x68);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x24, 0x01);
    write_cmos_sensor(0xc0, 0x16);
    write_cmos_sensor(0xc1, 0x08);
    write_cmos_sensor(0xc2, 0x30);
    write_cmos_sensor(0x8e, 0x06);
    write_cmos_sensor(0x8f, 0x60);
    write_cmos_sensor(0x90, 0x04);
    write_cmos_sensor(0x91, 0xc8);
    write_cmos_sensor(0x93, 0x0e);
    write_cmos_sensor(0x94, 0x77);
    write_cmos_sensor(0x95, 0x77);
    write_cmos_sensor(0x96, 0x10);
    write_cmos_sensor(0x98, 0x88);
    write_cmos_sensor(0x9c, 0x1a);
    write_cmos_sensor(0xb7, 0x02);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0f);
    write_cmos_sensor(0xe7, 0x03);
    write_cmos_sensor(0xe7, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    LOG_INF("ov08d10--> preview_setting END!\n");

}    /*    preview_setting  */


static void load_capture_setting(void)
{
    // 3264x2448_2lane_288M_30fps
    LOG_INF("ov08d10--> capture_setting E!\n");
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0e);
    write_cmos_sensor(0x20, 0x0b);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x11, 0x2a);
    write_cmos_sensor(0x14, 0x43);
    write_cmos_sensor(0x1e, 0x23);
    write_cmos_sensor(0x16, 0x82);
    write_cmos_sensor(0x21, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x12, 0x00);
    write_cmos_sensor(0x02, 0x00);
    write_cmos_sensor(0x03, 0x12);
    write_cmos_sensor(0x04, 0x50);
    write_cmos_sensor(0x05, 0x00);
    write_cmos_sensor(0x06, 0xd0);
    write_cmos_sensor(0x07, 0x05);
    write_cmos_sensor(0x21, 0x02);
    write_cmos_sensor(0x24, 0x30);
    write_cmos_sensor(0x33, 0x03);
    write_cmos_sensor(0x01, 0x03);
    write_cmos_sensor(0x19, 0x10);
    write_cmos_sensor(0x42, 0x55);
    write_cmos_sensor(0x43, 0x00);
    write_cmos_sensor(0x47, 0x07);
    write_cmos_sensor(0x48, 0x08);
    write_cmos_sensor(0x4c, 0x38);
    write_cmos_sensor(0xb2, 0x7e);
    write_cmos_sensor(0xb3, 0x7b);
    write_cmos_sensor(0xbd, 0x08);
    write_cmos_sensor(0xd2, 0x47);
    write_cmos_sensor(0xd3, 0x10);
    write_cmos_sensor(0xd4, 0x0d);
    write_cmos_sensor(0xd5, 0x08);
    write_cmos_sensor(0xd6, 0x07);
    write_cmos_sensor(0xb1, 0x00);
    write_cmos_sensor(0xb4, 0x00);
    write_cmos_sensor(0xb7, 0x0a);
    write_cmos_sensor(0xbc, 0x44);
    write_cmos_sensor(0xbf, 0x42);
    write_cmos_sensor(0xc1, 0x10);
    write_cmos_sensor(0xc3, 0x24);
    write_cmos_sensor(0xc8, 0x03);
    write_cmos_sensor(0xc9, 0xf8);
    write_cmos_sensor(0xe1, 0x33);
    write_cmos_sensor(0xe2, 0xbb);
    write_cmos_sensor(0x51, 0x0c);
    write_cmos_sensor(0x52, 0x0a);
    write_cmos_sensor(0x57, 0x8c);
    write_cmos_sensor(0x59, 0x09);
    write_cmos_sensor(0x5a, 0x08);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x60, 0x02);
    write_cmos_sensor(0x6d, 0x5c);
    write_cmos_sensor(0x76, 0x16);
    write_cmos_sensor(0x7c, 0x11);
    write_cmos_sensor(0x90, 0x28);
    write_cmos_sensor(0x91, 0x16);
    write_cmos_sensor(0x92, 0x1c);
    write_cmos_sensor(0x93, 0x24);
    write_cmos_sensor(0x95, 0x48);
    write_cmos_sensor(0x9c, 0x06);
    write_cmos_sensor(0xca, 0x0c);
    write_cmos_sensor(0xce, 0x0d);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc0, 0x00);
    write_cmos_sensor(0xdd, 0x18);
    write_cmos_sensor(0xde, 0x19);
    write_cmos_sensor(0xdf, 0x32);
    write_cmos_sensor(0xe0, 0x70);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc2, 0x05);
    write_cmos_sensor(0xd7, 0x88);
    write_cmos_sensor(0xd8, 0x77);
    write_cmos_sensor(0xd9, 0x66);
    write_cmos_sensor(0xfd, 0x07);
    write_cmos_sensor(0x00, 0xf8);
    write_cmos_sensor(0x01, 0x2b);
    write_cmos_sensor(0x05, 0x40);
    write_cmos_sensor(0x08, 0x06);
    write_cmos_sensor(0x09, 0x11);
    write_cmos_sensor(0x28, 0x6f);
    write_cmos_sensor(0x2a, 0x20);
    write_cmos_sensor(0x2b, 0x05);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x52, 0x00);
    write_cmos_sensor(0x53, 0x80);
    write_cmos_sensor(0x54, 0x00);
    write_cmos_sensor(0x55, 0x80);
    write_cmos_sensor(0x56, 0x00);
    write_cmos_sensor(0x57, 0x80);
    write_cmos_sensor(0x58, 0x00);
    write_cmos_sensor(0x59, 0x80);
    write_cmos_sensor(0x5c, 0x3f);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0x9a, 0x30);
    write_cmos_sensor(0xa8, 0x02);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0xa0, 0x00);
    write_cmos_sensor(0xa1, 0x08);
    write_cmos_sensor(0xa2, 0x09);
    write_cmos_sensor(0xa3, 0x90);
    write_cmos_sensor(0xa4, 0x00);
    write_cmos_sensor(0xa5, 0x08);
    write_cmos_sensor(0xa6, 0x0c);
    write_cmos_sensor(0xa7, 0xc0);
    write_cmos_sensor(0xfd, 0x05);
    write_cmos_sensor(0x04, 0x40);
    write_cmos_sensor(0x07, 0x00);
    write_cmos_sensor(0x0D, 0x01);
    write_cmos_sensor(0x0F, 0x01);
    write_cmos_sensor(0x10, 0x00);
    write_cmos_sensor(0x11, 0x00);
    write_cmos_sensor(0x12, 0x0C);
    write_cmos_sensor(0x13, 0xCF);
    write_cmos_sensor(0x14, 0x00);
    write_cmos_sensor(0x15, 0x00);
    write_cmos_sensor(0x18, 0x00);
    write_cmos_sensor(0x19, 0x00);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x24, 0x01);
    write_cmos_sensor(0xc0, 0x16);
    write_cmos_sensor(0xc1, 0x08);
    write_cmos_sensor(0xc2, 0x30);
    write_cmos_sensor(0x8e, 0x0c);
    write_cmos_sensor(0x8f, 0xc0);
    write_cmos_sensor(0x90, 0x09);
    write_cmos_sensor(0x91, 0x90);
    write_cmos_sensor(0xb7, 0x02);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0f);
    write_cmos_sensor(0xe7, 0x03);
    write_cmos_sensor(0xe7, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    LOG_INF("ov08d10--> capture_setting END!\n");
}

static void load_normal_video_setting(void)
{
    LOG_INF("normal_video_setting E!\n");
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0e);
    write_cmos_sensor(0x20, 0x0b);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x11, 0x2a);
    write_cmos_sensor(0x14, 0x43);
    write_cmos_sensor(0x1e, 0x23);
    write_cmos_sensor(0x16, 0x82);
    write_cmos_sensor(0x21, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x12, 0x00);
    write_cmos_sensor(0x02, 0x00);
    write_cmos_sensor(0x03, 0x12);
    write_cmos_sensor(0x04, 0x50);
    write_cmos_sensor(0x05, 0x00);
    write_cmos_sensor(0x06, 0xd0);
    write_cmos_sensor(0x07, 0x05);
    write_cmos_sensor(0x21, 0x02);
    write_cmos_sensor(0x24, 0x30);
    write_cmos_sensor(0x33, 0x03);
    write_cmos_sensor(0x01, 0x03);
    write_cmos_sensor(0x19, 0x10);
    write_cmos_sensor(0x42, 0x55);
    write_cmos_sensor(0x43, 0x00);
    write_cmos_sensor(0x47, 0x07);
    write_cmos_sensor(0x48, 0x08);
    write_cmos_sensor(0x4c, 0x38);
    write_cmos_sensor(0xb2, 0x7e);
    write_cmos_sensor(0xb3, 0x7b);
    write_cmos_sensor(0xbd, 0x08);
    write_cmos_sensor(0xd2, 0x47);
    write_cmos_sensor(0xd3, 0x10);
    write_cmos_sensor(0xd4, 0x0d);
    write_cmos_sensor(0xd5, 0x08);
    write_cmos_sensor(0xd6, 0x07);
    write_cmos_sensor(0xb1, 0x00);
    write_cmos_sensor(0xb4, 0x00);
    write_cmos_sensor(0xb7, 0x0a);
    write_cmos_sensor(0xbc, 0x44);
    write_cmos_sensor(0xbf, 0x42);
    write_cmos_sensor(0xc1, 0x10);
    write_cmos_sensor(0xc3, 0x24);
    write_cmos_sensor(0xc8, 0x03);
    write_cmos_sensor(0xc9, 0xf8);
    write_cmos_sensor(0xe1, 0x33);
    write_cmos_sensor(0xe2, 0xbb);
    write_cmos_sensor(0x51, 0x0c);
    write_cmos_sensor(0x52, 0x0a);
    write_cmos_sensor(0x57, 0x8c);
    write_cmos_sensor(0x59, 0x09);
    write_cmos_sensor(0x5a, 0x08);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x60, 0x02);
    write_cmos_sensor(0x6d, 0x5c);
    write_cmos_sensor(0x76, 0x16);
    write_cmos_sensor(0x7c, 0x11);
    write_cmos_sensor(0x90, 0x28);
    write_cmos_sensor(0x91, 0x16);
    write_cmos_sensor(0x92, 0x1c);
    write_cmos_sensor(0x93, 0x24);
    write_cmos_sensor(0x95, 0x48);
    write_cmos_sensor(0x9c, 0x06);
    write_cmos_sensor(0xca, 0x0c);
    write_cmos_sensor(0xce, 0x0d);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc0, 0x00);
    write_cmos_sensor(0xdd, 0x18);
    write_cmos_sensor(0xde, 0x19);
    write_cmos_sensor(0xdf, 0x32);
    write_cmos_sensor(0xe0, 0x70);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc2, 0x05);
    write_cmos_sensor(0xd7, 0x88);
    write_cmos_sensor(0xd8, 0x77);
    write_cmos_sensor(0xd9, 0x66);
    write_cmos_sensor(0xfd, 0x07);
    write_cmos_sensor(0x00, 0xf8);
    write_cmos_sensor(0x01, 0x2b);
    write_cmos_sensor(0x05, 0x40);
    write_cmos_sensor(0x08, 0x06);
    write_cmos_sensor(0x09, 0x11);
    write_cmos_sensor(0x28, 0x6f);
    write_cmos_sensor(0x2a, 0x20);
    write_cmos_sensor(0x2b, 0x05);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x52, 0x00);
    write_cmos_sensor(0x53, 0x80);
    write_cmos_sensor(0x54, 0x00);
    write_cmos_sensor(0x55, 0x80);
    write_cmos_sensor(0x56, 0x00);
    write_cmos_sensor(0x57, 0x80);
    write_cmos_sensor(0x58, 0x00);
    write_cmos_sensor(0x59, 0x80);
    write_cmos_sensor(0x5c, 0x3f);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0x9a, 0x30);
    write_cmos_sensor(0xa8, 0x02);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0xa0, 0x00);
    write_cmos_sensor(0xa1, 0x08);
    write_cmos_sensor(0xa2, 0x09);
    write_cmos_sensor(0xa3, 0x90);
    write_cmos_sensor(0xa4, 0x00);
    write_cmos_sensor(0xa5, 0x08);
    write_cmos_sensor(0xa6, 0x0c);
    write_cmos_sensor(0xa7, 0xc0);
    write_cmos_sensor(0xfd, 0x05);
    write_cmos_sensor(0x04, 0x40);
    write_cmos_sensor(0x07, 0x00);
    write_cmos_sensor(0x0D, 0x01);
    write_cmos_sensor(0x0F, 0x01);
    write_cmos_sensor(0x10, 0x00);
    write_cmos_sensor(0x11, 0x00);
    write_cmos_sensor(0x12, 0x0C);
    write_cmos_sensor(0x13, 0xCF);
    write_cmos_sensor(0x14, 0x00);
    write_cmos_sensor(0x15, 0x00);
    write_cmos_sensor(0x18, 0x00);
    write_cmos_sensor(0x19, 0x00);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x24, 0x01);
    write_cmos_sensor(0xc0, 0x16);
    write_cmos_sensor(0xc1, 0x08);
    write_cmos_sensor(0xc2, 0x30);
    write_cmos_sensor(0x8e, 0x0c);
    write_cmos_sensor(0x8f, 0xc0);
    write_cmos_sensor(0x90, 0x09);
    write_cmos_sensor(0x91, 0x90);
    write_cmos_sensor(0xb7, 0x02);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0f);
    write_cmos_sensor(0xe7, 0x03);
    write_cmos_sensor(0xe7, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    LOG_INF("ov08d10-->normal_video_setting END!\n");
}

static void load_hs_video_setting(void)
{
    /*
    // 1280x720_2lane_144M_90fps
    LOG_INF("hs_video_setting E!\n");
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0e);
    write_cmos_sensor(0x20, 0x0b);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x1d, 0x00);
    write_cmos_sensor(0x18, 0x3c);
    write_cmos_sensor(0x1c, 0x19);
    write_cmos_sensor(0x11, 0x2a);
    write_cmos_sensor(0x14, 0x43);
    write_cmos_sensor(0x1e, 0x23);
    write_cmos_sensor(0x16, 0x82);
    write_cmos_sensor(0x21, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x12, 0x00);
    write_cmos_sensor(0x02, 0x00);
    write_cmos_sensor(0x03, 0x05);
    write_cmos_sensor(0x04, 0xe2);
    write_cmos_sensor(0x05, 0x00);
    write_cmos_sensor(0x06, 0x88);//90
    write_cmos_sensor(0x07, 0x05);
    write_cmos_sensor(0x21, 0x02);
    write_cmos_sensor(0x24, 0xf8);
    write_cmos_sensor(0x1a, 0x0a);
    write_cmos_sensor(0x1b, 0x08);
    write_cmos_sensor(0x31, 0x07);
    write_cmos_sensor(0x27, 0x14);
    write_cmos_sensor(0x28, 0x00);
    write_cmos_sensor(0x29, 0x2c);
    write_cmos_sensor(0x2a, 0x01);
    write_cmos_sensor(0x2b, 0x42);
    write_cmos_sensor(0x2c, 0x00);
    write_cmos_sensor(0x2d, 0xf2);
    write_cmos_sensor(0x2e, 0x05);
    write_cmos_sensor(0x2f, 0xb0);
    write_cmos_sensor(0x33, 0x03);
    write_cmos_sensor(0x01, 0x03);
    write_cmos_sensor(0x19, 0x10);
    write_cmos_sensor(0x42, 0x55);
    write_cmos_sensor(0x43, 0x00);
    write_cmos_sensor(0x47, 0x07);
    write_cmos_sensor(0x48, 0x08);
    write_cmos_sensor(0x4c, 0x38);
    write_cmos_sensor(0xb2, 0x7e);
    write_cmos_sensor(0xb3, 0x7b);
    write_cmos_sensor(0xbd, 0x08);
    write_cmos_sensor(0xd2, 0x47);
    write_cmos_sensor(0xd3, 0x10);
    write_cmos_sensor(0xd4, 0x0d);
    write_cmos_sensor(0xd5, 0x08);
    write_cmos_sensor(0xd6, 0x07);
    write_cmos_sensor(0xb1, 0x00);
    write_cmos_sensor(0xb4, 0x00);
    write_cmos_sensor(0xb7, 0x0a);
    write_cmos_sensor(0xbc, 0x44);
    write_cmos_sensor(0xbf, 0x42);
    write_cmos_sensor(0xc1, 0x10);
    write_cmos_sensor(0xc3, 0x24);
    write_cmos_sensor(0xc8, 0x03);
    write_cmos_sensor(0xc9, 0xf8);
    write_cmos_sensor(0xe1, 0x33);
    write_cmos_sensor(0xe2, 0xbb);
    write_cmos_sensor(0x51, 0x0c);
    write_cmos_sensor(0x52, 0x0a);
    write_cmos_sensor(0x57, 0x8c);
    write_cmos_sensor(0x59, 0x09);
    write_cmos_sensor(0x5a, 0x08);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x60, 0x02);
    write_cmos_sensor(0x6d, 0x5c);
    write_cmos_sensor(0x76, 0x16);
    write_cmos_sensor(0x7c, 0x1a);
    write_cmos_sensor(0x90, 0x28);
    write_cmos_sensor(0x91, 0x16);
    write_cmos_sensor(0x92, 0x1c);
    write_cmos_sensor(0x93, 0x24);
    write_cmos_sensor(0x95, 0x48);
    write_cmos_sensor(0x9c, 0x06);
    write_cmos_sensor(0xca, 0x0c);
    write_cmos_sensor(0xce, 0x0d);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc0, 0x00);
    write_cmos_sensor(0xdd, 0x18);
    write_cmos_sensor(0xde, 0x19);
    write_cmos_sensor(0xdf, 0x32);
    write_cmos_sensor(0xe0, 0x70);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc2, 0x05);
    write_cmos_sensor(0xd7, 0x88);
    write_cmos_sensor(0xd8, 0x77);
    write_cmos_sensor(0xd9, 0x66);
    write_cmos_sensor(0xfd, 0x07);
    write_cmos_sensor(0x00, 0xf8);
    write_cmos_sensor(0x01, 0x2b);
    write_cmos_sensor(0x05, 0x40);
    write_cmos_sensor(0x08, 0x03);
    write_cmos_sensor(0x09, 0x08);
    write_cmos_sensor(0x28, 0x6f);
    write_cmos_sensor(0x2a, 0x20);
    write_cmos_sensor(0x2b, 0x05);
    write_cmos_sensor(0x2c, 0x01);
    write_cmos_sensor(0x50, 0x02);
    write_cmos_sensor(0x51, 0x03);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x52, 0x00);
    write_cmos_sensor(0x53, 0x80);
    write_cmos_sensor(0x54, 0x00);
    write_cmos_sensor(0x55, 0x80);
    write_cmos_sensor(0x56, 0x00);
    write_cmos_sensor(0x57, 0x80);
    write_cmos_sensor(0x58, 0x00);
    write_cmos_sensor(0x59, 0x80);
    write_cmos_sensor(0x5c, 0x3f);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0x9a, 0x30);
    write_cmos_sensor(0xa8, 0x02);
    write_cmos_sensor(0xa9, 0x02);
    write_cmos_sensor(0xaa, 0xd8);
    write_cmos_sensor(0xab, 0x05);
    write_cmos_sensor(0xac, 0x08);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0xa0, 0x00);
    write_cmos_sensor(0xa1, 0x04);
    write_cmos_sensor(0xa2, 0x02);
    write_cmos_sensor(0xa3, 0xd0);
    write_cmos_sensor(0xa4, 0x00);
    write_cmos_sensor(0xa5, 0x04);
    write_cmos_sensor(0xa6, 0x05);
    write_cmos_sensor(0xa7, 0x00);
    write_cmos_sensor(0xfd, 0x05);
    write_cmos_sensor(0x04, 0x60);
    write_cmos_sensor(0x06, 0x80);
    write_cmos_sensor(0x07, 0x99);
    write_cmos_sensor(0x0d, 0x03);
    write_cmos_sensor(0x0f, 0x03);
    write_cmos_sensor(0x10, 0x01);
    write_cmos_sensor(0x11, 0x60);
    write_cmos_sensor(0x12, 0x0b);
    write_cmos_sensor(0x13, 0x6f);
    write_cmos_sensor(0x14, 0x01);
    write_cmos_sensor(0x15, 0xf8);
    write_cmos_sensor(0x18, 0x05);
    write_cmos_sensor(0x19, 0x08);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x24, 0x01);
    write_cmos_sensor(0xc0, 0x16);
    write_cmos_sensor(0xc1, 0x08);
    write_cmos_sensor(0xc2, 0x30);
    write_cmos_sensor(0x8e, 0x05);
    write_cmos_sensor(0x8f, 0x00);
    write_cmos_sensor(0x90, 0x02);
    write_cmos_sensor(0x91, 0xd0);
    write_cmos_sensor(0x93, 0x0e);
    write_cmos_sensor(0x94, 0x77);
    write_cmos_sensor(0x95, 0x77);
    write_cmos_sensor(0x96, 0x10);
    write_cmos_sensor(0x98, 0x88);
    write_cmos_sensor(0x9c, 0x1a);
    write_cmos_sensor(0xb7, 0x02);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0f);
    write_cmos_sensor(0xe7, 0x03);
    write_cmos_sensor(0xe7, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    LOG_INF("hs_video_setting END!\n");
    */
    // 640x480_2lane_144M_120fps
    LOG_INF("ov08d10-->hs_video_setting E!\n");
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0e);
	write_cmos_sensor(0x20, 0x0b);
	write_cmos_sensor(0x10, 0x05);
	write_cmos_sensor(0x11, 0x2a);
	write_cmos_sensor(0x12, 0x03);
	write_cmos_sensor(0x13, 0x05);
	write_cmos_sensor(0x14, 0x43);
	write_cmos_sensor(0x15, 0x02);
	write_cmos_sensor(0x16, 0x82);
	write_cmos_sensor(0x17, 0x05);
	write_cmos_sensor(0x18, 0x3c);
	write_cmos_sensor(0x19, 0x04);
	write_cmos_sensor(0x1a, 0x05);
	write_cmos_sensor(0x1b, 0xf0);
	write_cmos_sensor(0x1c, 0x19);
	write_cmos_sensor(0x1d, 0x00);
	write_cmos_sensor(0x1e, 0x23);
	write_cmos_sensor(0x1f, 0x0f);
	write_cmos_sensor(0x20, 0x0f);
	write_cmos_sensor(0x21, 0x00);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0x12, 0x00);
	write_cmos_sensor(0x03, 0x04);
	write_cmos_sensor(0x04, 0xb6);
	write_cmos_sensor(0x05, 0x00);
	write_cmos_sensor(0x06, 0xc8);
	write_cmos_sensor(0x07, 0x05);
	write_cmos_sensor(0x21, 0x02);
	write_cmos_sensor(0x24, 0xff);
	write_cmos_sensor(0x1a, 0x0a);
	write_cmos_sensor(0x1b, 0x08);
	write_cmos_sensor(0x31, 0x07);
	write_cmos_sensor(0x27, 0x14);
	write_cmos_sensor(0x28, 0x00);
	write_cmos_sensor(0x29, 0x7c);
	write_cmos_sensor(0x2a, 0x00);
	write_cmos_sensor(0x2b, 0xA2);
	write_cmos_sensor(0x2c, 0x02);
	write_cmos_sensor(0x2d, 0xD4);
	write_cmos_sensor(0x2e, 0x03);
	write_cmos_sensor(0x2f, 0xD0);
	write_cmos_sensor(0x33, 0x03);
	write_cmos_sensor(0x01, 0x03);
	write_cmos_sensor(0x19, 0x10);
	write_cmos_sensor(0x42, 0x55);
	write_cmos_sensor(0x43, 0x00);
	write_cmos_sensor(0x47, 0x07);
	write_cmos_sensor(0x48, 0x08);
	write_cmos_sensor(0x4c, 0x38);
	write_cmos_sensor(0xb2, 0x7e);
	write_cmos_sensor(0xb3, 0x7b);
	write_cmos_sensor(0xbd, 0x08);
	write_cmos_sensor(0xd2, 0x47);
	write_cmos_sensor(0xd3, 0x10);
	write_cmos_sensor(0xd4, 0x0d);
	write_cmos_sensor(0xd5, 0x08);
	write_cmos_sensor(0xd6, 0x07);
	write_cmos_sensor(0xb1, 0x00);
	write_cmos_sensor(0xb4, 0x00);
	write_cmos_sensor(0xb7, 0x0a);
	write_cmos_sensor(0xbc, 0x44);
	write_cmos_sensor(0xbf, 0x42);
	write_cmos_sensor(0xc1, 0x10);
	write_cmos_sensor(0xc3, 0x24);
	write_cmos_sensor(0xc8, 0x03);
	write_cmos_sensor(0xc9, 0xf8);
	write_cmos_sensor(0xe1, 0x33);
	write_cmos_sensor(0xe2, 0xbb);
	write_cmos_sensor(0x51, 0x0c);
	write_cmos_sensor(0x52, 0x0a);
	write_cmos_sensor(0x57, 0x8c);
	write_cmos_sensor(0x59, 0x09);
	write_cmos_sensor(0x5a, 0x08);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x60, 0x02);
	write_cmos_sensor(0x6d, 0x5c);
	write_cmos_sensor(0x76, 0x16);
	write_cmos_sensor(0x7c, 0x1a);
	write_cmos_sensor(0x90, 0x28);
	write_cmos_sensor(0x91, 0x16);
	write_cmos_sensor(0x92, 0x1c);
	write_cmos_sensor(0x93, 0x24);
	write_cmos_sensor(0x95, 0x48);
	write_cmos_sensor(0x9c, 0x06);
	write_cmos_sensor(0xca, 0x0c);
	write_cmos_sensor(0xce, 0x0d);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc0, 0x00);
	write_cmos_sensor(0xdd, 0x18);
	write_cmos_sensor(0xde, 0x19);
	write_cmos_sensor(0xdf, 0x32);
	write_cmos_sensor(0xe0, 0x70);
	write_cmos_sensor(0xfd, 0x01);
	write_cmos_sensor(0xc2, 0x05);
	write_cmos_sensor(0xd7, 0x88);
	write_cmos_sensor(0xd8, 0x77);
	write_cmos_sensor(0xd9, 0x66);
	write_cmos_sensor(0xfd, 0x07);
	write_cmos_sensor(0x00, 0xf8);
	write_cmos_sensor(0x01, 0x2b);
	write_cmos_sensor(0x05, 0x40);
	write_cmos_sensor(0x08, 0x03);
	write_cmos_sensor(0x09, 0x08);
	write_cmos_sensor(0x28, 0x6f);
	write_cmos_sensor(0x2a, 0x20);
	write_cmos_sensor(0x2b, 0x05);
	write_cmos_sensor(0x2c, 0x01);
	write_cmos_sensor(0x50, 0x02);
	write_cmos_sensor(0x51, 0x03);
	write_cmos_sensor(0x5e, 0x10);
	write_cmos_sensor(0x52, 0x00);
	write_cmos_sensor(0x53, 0x80);
	write_cmos_sensor(0x54, 0x00);
	write_cmos_sensor(0x55, 0x80);
	write_cmos_sensor(0x56, 0x00);
	write_cmos_sensor(0x57, 0x80);
	write_cmos_sensor(0x58, 0x00);
	write_cmos_sensor(0x59, 0x80);
	write_cmos_sensor(0x5c, 0x3f);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0x9a, 0x30);
	write_cmos_sensor(0xa8, 0x02);
	write_cmos_sensor(0xa9, 0x01);
	write_cmos_sensor(0xaa, 0xe8);
	write_cmos_sensor(0xab, 0x02);
	write_cmos_sensor(0xac, 0x88);
	write_cmos_sensor(0xfd, 0x02);
	write_cmos_sensor(0xa0, 0x00);
	write_cmos_sensor(0xa1, 0x04);
	write_cmos_sensor(0xa2, 0x01);
	write_cmos_sensor(0xa3, 0xe0);
	write_cmos_sensor(0xa4, 0x00);
	write_cmos_sensor(0xa5, 0x04);
	write_cmos_sensor(0xa6, 0x02);
	write_cmos_sensor(0xa7, 0x80);
	write_cmos_sensor(0xfd, 0x05);
	write_cmos_sensor(0x04, 0x60);
	write_cmos_sensor(0x06, 0x80);
	write_cmos_sensor(0x07, 0x99);
	write_cmos_sensor(0x0d, 0x03);
	write_cmos_sensor(0x0f, 0x03);
	write_cmos_sensor(0x10, 0x01);
	write_cmos_sensor(0x11, 0x60);
	write_cmos_sensor(0x12, 0x0b);
	write_cmos_sensor(0x13, 0x6f);
	write_cmos_sensor(0x14, 0x01);
	write_cmos_sensor(0x15, 0xf8);
	write_cmos_sensor(0x18, 0x05);
	write_cmos_sensor(0x19, 0x08);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x24, 0x01);
	write_cmos_sensor(0xc0, 0x16);
	write_cmos_sensor(0xc1, 0x08);
	write_cmos_sensor(0xc2, 0x30);
	write_cmos_sensor(0x8e, 0x02);
	write_cmos_sensor(0x8f, 0x80);
	write_cmos_sensor(0x90, 0x01);
	write_cmos_sensor(0x91, 0xe0);
	write_cmos_sensor(0x93, 0x0e);
	write_cmos_sensor(0x94, 0x77);
	write_cmos_sensor(0x95, 0x77);
	write_cmos_sensor(0x96, 0x10);
	write_cmos_sensor(0x98, 0x88);
	write_cmos_sensor(0x9c, 0x1a);
	write_cmos_sensor(0xb7, 0x02);
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0x20, 0x0f);
	write_cmos_sensor(0xe7, 0x03);
	write_cmos_sensor(0xe7, 0x00);
	LOG_INF("ov08d10--> hs_video_setting END!\n");
}

static void load_slim_video_setting(void)
{
    // 1280x720_2lane_144M_30fps
    LOG_INF("ov08d10--> slim_video_setting E!\n");
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0e);
    write_cmos_sensor(0x20, 0x0b);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x1d, 0x00);
    write_cmos_sensor(0x18, 0x3c);
    write_cmos_sensor(0x1c, 0x19);
    write_cmos_sensor(0x11, 0x2a);
    write_cmos_sensor(0x14, 0x43);
    write_cmos_sensor(0x1e, 0x23);
    write_cmos_sensor(0x16, 0x82);
    write_cmos_sensor(0x21, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0x12, 0x00);
    write_cmos_sensor(0x02, 0x00);
    write_cmos_sensor(0x03, 0x05);
    write_cmos_sensor(0x04, 0xe2);
    write_cmos_sensor(0x05, 0x0d);
    write_cmos_sensor(0x06, 0x90);
    write_cmos_sensor(0x07, 0x05);
    write_cmos_sensor(0x21, 0x02);
    write_cmos_sensor(0x24, 0xf8);
    write_cmos_sensor(0x1a, 0x0a);
    write_cmos_sensor(0x1b, 0x08);
    write_cmos_sensor(0x31, 0x07);
    write_cmos_sensor(0x27, 0x14);
    write_cmos_sensor(0x28, 0x00);
    write_cmos_sensor(0x29, 0x2c);
    write_cmos_sensor(0x2a, 0x01);
    write_cmos_sensor(0x2b, 0x42);
    write_cmos_sensor(0x2c, 0x00);
    write_cmos_sensor(0x2d, 0xf2);
    write_cmos_sensor(0x2e, 0x05);
    write_cmos_sensor(0x2f, 0xb0);
    write_cmos_sensor(0x33, 0x03);
    write_cmos_sensor(0x01, 0x03);
    write_cmos_sensor(0x19, 0x10);
    write_cmos_sensor(0x42, 0x55);
    write_cmos_sensor(0x43, 0x00);
    write_cmos_sensor(0x47, 0x07);
    write_cmos_sensor(0x48, 0x08);
    write_cmos_sensor(0x4c, 0x38);
    write_cmos_sensor(0xb2, 0x7e);
    write_cmos_sensor(0xb3, 0x7b);
    write_cmos_sensor(0xbd, 0x08);
    write_cmos_sensor(0xd2, 0x47);
    write_cmos_sensor(0xd3, 0x10);
    write_cmos_sensor(0xd4, 0x0d);
    write_cmos_sensor(0xd5, 0x08);
    write_cmos_sensor(0xd6, 0x07);
    write_cmos_sensor(0xb1, 0x00);
    write_cmos_sensor(0xb4, 0x00);
    write_cmos_sensor(0xb7, 0x0a);
    write_cmos_sensor(0xbc, 0x44);
    write_cmos_sensor(0xbf, 0x42);
    write_cmos_sensor(0xc1, 0x10);
    write_cmos_sensor(0xc3, 0x24);
    write_cmos_sensor(0xc8, 0x03);
    write_cmos_sensor(0xc9, 0xf8);
    write_cmos_sensor(0xe1, 0x33);
    write_cmos_sensor(0xe2, 0xbb);
    write_cmos_sensor(0x51, 0x0c);
    write_cmos_sensor(0x52, 0x0a);
    write_cmos_sensor(0x57, 0x8c);
    write_cmos_sensor(0x59, 0x09);
    write_cmos_sensor(0x5a, 0x08);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x60, 0x02);
    write_cmos_sensor(0x6d, 0x5c);
    write_cmos_sensor(0x76, 0x16);
    write_cmos_sensor(0x7c, 0x1a);
    write_cmos_sensor(0x90, 0x28);
    write_cmos_sensor(0x91, 0x16);
    write_cmos_sensor(0x92, 0x1c);
    write_cmos_sensor(0x93, 0x24);
    write_cmos_sensor(0x95, 0x48);
    write_cmos_sensor(0x9c, 0x06);
    write_cmos_sensor(0xca, 0x0c);
    write_cmos_sensor(0xce, 0x0d);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc0, 0x00);
    write_cmos_sensor(0xdd, 0x18);
    write_cmos_sensor(0xde, 0x19);
    write_cmos_sensor(0xdf, 0x32);
    write_cmos_sensor(0xe0, 0x70);
    write_cmos_sensor(0xfd, 0x01);
    write_cmos_sensor(0xc2, 0x05);
    write_cmos_sensor(0xd7, 0x88);
    write_cmos_sensor(0xd8, 0x77);
    write_cmos_sensor(0xd9, 0x66);
    write_cmos_sensor(0xfd, 0x07);
    write_cmos_sensor(0x00, 0xf8);
    write_cmos_sensor(0x01, 0x2b);
    write_cmos_sensor(0x05, 0x40);
    write_cmos_sensor(0x08, 0x03);
    write_cmos_sensor(0x09, 0x08);
    write_cmos_sensor(0x28, 0x6f);
    write_cmos_sensor(0x2a, 0x20);
    write_cmos_sensor(0x2b, 0x05);
    write_cmos_sensor(0x2c, 0x01);
    write_cmos_sensor(0x50, 0x02);
    write_cmos_sensor(0x51, 0x03);
    write_cmos_sensor(0x5e, 0x10);
    write_cmos_sensor(0x52, 0x00);
    write_cmos_sensor(0x53, 0x80);
    write_cmos_sensor(0x54, 0x00);
    write_cmos_sensor(0x55, 0x80);
    write_cmos_sensor(0x56, 0x00);
    write_cmos_sensor(0x57, 0x80);
    write_cmos_sensor(0x58, 0x00);
    write_cmos_sensor(0x59, 0x80);
    write_cmos_sensor(0x5c, 0x3f);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0x9a, 0x30);
    write_cmos_sensor(0xa8, 0x02);
    write_cmos_sensor(0xa9, 0x02);
    write_cmos_sensor(0xaa, 0xd8);
    write_cmos_sensor(0xab, 0x05);
    write_cmos_sensor(0xac, 0x08);
    write_cmos_sensor(0xfd, 0x02);
    write_cmos_sensor(0xa0, 0x00);
    write_cmos_sensor(0xa1, 0x04);
    write_cmos_sensor(0xa2, 0x02);
    write_cmos_sensor(0xa3, 0xd0);
    write_cmos_sensor(0xa4, 0x00);
    write_cmos_sensor(0xa5, 0x04);
    write_cmos_sensor(0xa6, 0x05);
    write_cmos_sensor(0xa7, 0x00);
    write_cmos_sensor(0xfd, 0x05);
    write_cmos_sensor(0x04, 0x60);
    write_cmos_sensor(0x06, 0x80);
    write_cmos_sensor(0x07, 0x99);
    write_cmos_sensor(0x0d, 0x03);
    write_cmos_sensor(0x0f, 0x03);
    write_cmos_sensor(0x10, 0x01);
    write_cmos_sensor(0x11, 0x60);
    write_cmos_sensor(0x12, 0x0b);
    write_cmos_sensor(0x13, 0x6f);
    write_cmos_sensor(0x14, 0x01);
    write_cmos_sensor(0x15, 0xf8);
    write_cmos_sensor(0x18, 0x05);
    write_cmos_sensor(0x19, 0x08);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x24, 0x01);
    write_cmos_sensor(0xc0, 0x16);
    write_cmos_sensor(0xc1, 0x08);
    write_cmos_sensor(0xc2, 0x30);
    write_cmos_sensor(0x8e, 0x05);
    write_cmos_sensor(0x8f, 0x00);
    write_cmos_sensor(0x90, 0x02);
    write_cmos_sensor(0x91, 0xd0);
    write_cmos_sensor(0x93, 0x0e);
    write_cmos_sensor(0x94, 0x77);
    write_cmos_sensor(0x95, 0x77);
    write_cmos_sensor(0x96, 0x10);
    write_cmos_sensor(0x98, 0x88);
    write_cmos_sensor(0x9c, 0x1a);
    write_cmos_sensor(0xb7, 0x02);
    write_cmos_sensor(0xfd, 0x00);
    write_cmos_sensor(0x20, 0x0f);
    write_cmos_sensor(0xe7, 0x03);
    write_cmos_sensor(0xe7, 0x00);
    write_cmos_sensor(0xfd, 0x01);
    LOG_INF("ov08d10--> slim_video_setting END!\n");
}

//extern int check_i2c_timeout(u16 addr, u16 i2cid);

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
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;

    //printk("ov08d10-->Start to obtain sensor ID£¡");
    /*sensor have two i2c address 0x6c 0x6d & 0x21 0x20,
     * we should detect the module used i2c address
     */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			write_cmos_sensor(0xfd, 0x00);
	   	   *sensor_id = (read_cmos_sensor(0x00) << 24) |(read_cmos_sensor(0x01) << 16) |(read_cmos_sensor(0x02) << 8) | read_cmos_sensor(0x03);
			printk("ov08d10  sensor_id: 0x%x,imgsensor_info.sensor_id :0x%x\n",*sensor_id,imgsensor_info.sensor_id);
			if (*sensor_id == imgsensor_info.sensor_id) {


			printk("ov08d10 i2c write id : 0x%x, sensor id: 0x%x\n",
			imgsensor.i2c_write_id, *sensor_id);
		    
				return ERROR_NONE;
			}
			printk("ov08d10 Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != imgsensor_info.sensor_id) {
		printk("ov08d10 imgsensor id: 0x%x fail\n", *sensor_id);
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 streaming_control(kal_bool enable)
{
    LOG_INF("streaming_enable(0=Sw Standby,1=streaming): %d\n", enable);
    if (enable) {
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0xa0, 0x01);
    } 
	else {
	write_cmos_sensor(0xfd, 0x00);
	write_cmos_sensor(0xa0, 0x00);
	mdelay(10);
    }
    return ERROR_NONE;
}

static void sensor_init(void)
{
    pr_debug("OV08D10MIPIRAW sensor_init E\n");
    load_init_setting();
    set_mirror_flip(imgsensor.mirror);
    pr_debug("OV08D10MIPIRAW sensor_init X\n");
}	/* sensor_init */

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
    mdelay(9);


	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
		    write_cmos_sensor(0xfd, 0x00);
	  		sensor_id = (read_cmos_sensor(0x00) << 24) |(read_cmos_sensor(0x01) << 16) |(read_cmos_sensor(0x02) << 8) | read_cmos_sensor(0x03);
			printk("ov08d10  sensor_id: 0x%x\n",sensor_id);
			if (sensor_id == imgsensor_info.sensor_id) {
				printk("ov08d10 i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id, sensor_id);
				break;
			}
			printk("ov08d10 Read sensor id fail: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id,
				sensor_id);
			retry--;
		} while(retry > 0);
		i++;
	if (sensor_id == imgsensor_info.sensor_id) {
			break;
	}
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id) {
		printk("ov08d10 Open sensor id: 0x%x fail\n", sensor_id);
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* initail sequence write in  */
	sensor_init();
    mdelay(10);
	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en= KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.shutter = 0x0400;
    imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
    imgsensor.ihdr_mode = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

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
    LOG_INF("ov08d10-->close E\n");
    streaming_control(KAL_FALSE);
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
	LOG_INF("ov08d10--> preview E");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.vblank_convert = 1252; //for 1632x1224 30fps 
    imgsensor.pclk = imgsensor_info.pre.pclk;
    /*imgsensor.video_mode = KAL_FALSE;*/
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    load_preview_setting();
    set_mirror_flip(imgsensor.mirror);
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
    LOG_INF("ov08d10-->capture E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
    imgsensor.vblank_convert = 2504; //for 3264x2448
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
    imgsensor.current_fps = imgsensor_info.cap.max_framerate;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    load_capture_setting();
    set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}    /* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("ov08d10-->normal_video E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
    imgsensor.vblank_convert = 2504; //for 3264x2448
    imgsensor.pclk = imgsensor_info.normal_video.pclk;
    imgsensor.line_length = imgsensor_info.normal_video.linelength;
    imgsensor.frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.current_fps = imgsensor_info.normal_video.max_framerate;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    /*imgsensor.current_fps = 300;*/
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    load_normal_video_setting();
    set_mirror_flip(imgsensor.mirror);
    
    return ERROR_NONE;
}    /*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("ov08d10-->hs_video E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.vblank_convert = 776;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    /*imgsensor.video_mode = KAL_TRUE;*/
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.current_fps = imgsensor_info.hs_video.max_framerate;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    /*imgsensor.current_fps = 300;*/
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    load_hs_video_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
		 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("ov08d10-->slim_video E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.vblank_convert = 528;//776;//1280x720_90fps
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    /*imgsensor.video_mode = KAL_TRUE;*/
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.current_fps = imgsensor_info.slim_video.max_framerate;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    /*imgsensor.current_fps = 300;*/
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    load_slim_video_setting();
    set_mirror_flip(imgsensor.mirror);

	return ERROR_NONE;
}    /*    slim_video     */

static kal_uint32 get_resolution(
	    MSDK_SENSOR_RESOLUTION_INFO_STRUCT(*sensor_resolution))
{
    LOG_INF("ov08d10-->get_resolution E\n");
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
    LOG_INF("ov08d10-->scenario_id = %d\n", scenario_id);

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
/* The frame of setting shutter default 0 for TG int */
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
/* The frame of setting sensor gain */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->FrameTimeDelayFrame = imgsensor_info.frame_time_delay_frame; /* The delay frame of setting frame length  */
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


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("ov08d10--> control scenario_id = %d\n", scenario_id);
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
		LOG_INF("ov08d10--> [odin]default mode\n");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}    /* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
	LOG_INF("ov08d10--> set_video_mode framerate = %d ", framerate);
    /* SetVideoMode Function should fix framerate*/
    if (framerate == 0) {
	/* Dynamic frame rate*/
	return ERROR_NONE;
    }
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE)) {
	imgsensor.current_fps = 296;
    } else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE)) {
	imgsensor.current_fps = 146;
    } else {
	imgsensor.current_fps = framerate;
    }
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("ov08d10-->  enable = %d, framerate = %d ", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable) {/*enable auto flicker      */
	imgsensor.autoflicker_en = KAL_TRUE;
    } else {/*Cancel Auto flick*/
	imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(
	enum MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("ov08d10-->  set_max_framerate_by_scenario :scenario_id = %d, framerate = %d\n",
				scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		//Test
		//framerate = 1200;

	frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;

	spin_lock(&imgsensor_drv_lock);
	if (frame_length > imgsensor_info.pre.framelength) {
	    imgsensor.dummy_line = (frame_length - imgsensor_info.pre.framelength);
	} else {
	    imgsensor.dummy_line = 0;
	}
	imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;

	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	break;

    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	if (framerate == 0) {
	return ERROR_NONE;
	}

	frame_length = imgsensor_info.normal_video.pclk / framerate * 10;

	frame_length /= imgsensor_info.normal_video.linelength;
	spin_lock(&imgsensor_drv_lock);

	if (frame_length > imgsensor_info.normal_video.framelength) {
	    imgsensor.dummy_line = frame_length - imgsensor_info.normal_video.framelength;
	} else {
	    imgsensor.dummy_line = 0;
	}

	imgsensor.frame_length =
	imgsensor_info.normal_video.framelength + imgsensor.dummy_line;

	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	break;

    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;

	spin_lock(&imgsensor_drv_lock);
	if (frame_length > imgsensor_info.cap.framelength) {
	    imgsensor.dummy_line = (frame_length - imgsensor_info.cap.framelength);
	} else {
	    imgsensor.dummy_line = 0;
	}
	imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;

	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	break;

    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	frame_length = imgsensor_info.hs_video.pclk / framerate * 10;

	frame_length /= imgsensor_info.hs_video.linelength;

	spin_lock(&imgsensor_drv_lock);
	if (frame_length > imgsensor_info.hs_video.framelength) {
	    imgsensor.dummy_line = (frame_length - imgsensor_info.hs_video.framelength);
	} else {
	    imgsensor.dummy_line = 0;
	}
	imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;

	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	break;

    case MSDK_SCENARIO_ID_SLIM_VIDEO:
	frame_length = imgsensor_info.slim_video.pclk / framerate * 10;

	frame_length /= imgsensor_info.slim_video.linelength;

	spin_lock(&imgsensor_drv_lock);

	if (frame_length > imgsensor_info.slim_video.framelength) {
	    imgsensor.dummy_line = (frame_length - imgsensor_info.slim_video.framelength);
	} else {
	    imgsensor.dummy_line = 0;
	}
	imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;

	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);

	break;

    default:  /*coding with  preview scenario by default*/
	frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;

	spin_lock(&imgsensor_drv_lock);
	if (frame_length > imgsensor_info.pre.framelength) {
	    imgsensor.dummy_line = (frame_length - imgsensor_info.pre.framelength);
	} else {
	    imgsensor.dummy_line = 0;
	}
	imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;

	imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);


	    LOG_INF("ov08d10--> error scenario_id = %d, we use preview scenario\n",
				scenario_id);
	break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(
				enum MSDK_SCENARIO_ID_ENUM scenario_id,
				MUINT32 *framerate)
{
	LOG_INF("ov08d10--> get_default_framerate_by_scenario: scenario_id = %d\n", scenario_id);

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
	LOG_INF("ov08d10--> set_test_pattern_mode: enable: %d\n", enable);

	if (enable) {
		write_cmos_sensor(0xfd, 0x03);
		write_cmos_sensor(0x81, 0x01);
	} else {
		write_cmos_sensor(0xfd, 0x03);
		write_cmos_sensor(0x81, 0x00);
    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

static kal_uint32 ana_gain_table_16x[] = {
     1024,  1088,  1152,  1216,  1280,  1344,  1408,  1472,
     1536,  1600,  1664,  1728,  1792,  1856,  1920,  1984,
     2048,  2176,  2304,  2432,  2560,  2688,  2816,  2944,
     3072,  3200,  3328,  3456,  3584,  3712,  3840,  3968,
     4096,  4352,  4608,  4864,  5120,  5376,  5632,  5888,
     6144,  6400,  6656,  6912,  7168,  7424,  7680,  7936,
     8192,  8704,  9216,  9728, 10240, 10752, 11264, 11776,
    12288, 12800, 13312, 13824, 14336, 14848, 15360, 15872
};

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
		UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
    UINT16 *feature_data_16 = (UINT16 *) feature_para;
    UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
    UINT32 *feature_data_32 = (UINT32 *) feature_para;
    unsigned long long *feature_data = (unsigned long long *) feature_para;
    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data =
		(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("ov08d10--> feature_id = %d\n", feature_id);
    switch (feature_id) {
    case SENSOR_FEATURE_GET_ANA_GAIN_TABLE:
	if ((void *)(uintptr_t) (*(feature_data + 1)) == NULL
		|| *(feature_data + 0) == 0) {
			*(feature_data + 0) =
				sizeof(ana_gain_table_16x);
	} else {
		memcpy((void *)(uintptr_t) (*(feature_data + 1)),
			(void *)ana_gain_table_16x,
			sizeof(ana_gain_table_16x));
	}
	break;
    case SENSOR_FEATURE_GET_AWB_REQ_BY_SCENARIO:
        *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
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
	switch (*feature_data) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:

	break;
	default:
	    *(feature_data + 2) = 1;
	break;
	}
	break;
    case SENSOR_FEATURE_GET_OFFSET_TO_START_OF_EXPOSURE:
	*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = 0;
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
	set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data + 1));
	break;

    case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
	get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data + 1)));
	break;
    case SENSOR_FEATURE_SET_TEST_PATTERN:
	set_test_pattern_mode((BOOL)*feature_data);
	break;
    case SENSOR_FEATURE_GET_FRAME_CTRL_INFO_BY_SCENARIO:
	/*
	 * 1, if driver support new sw frame sync
	 * set_shutter_frame_length() support third para auto_extend_en
	 */
	*(feature_data + 1) = 1; /* margin info by scenario */
	*(feature_data + 2) = imgsensor_info.margin;
	break;
    /*for factory mode auto testing*/
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
	*feature_return_para_32 = imgsensor_info.checksum_value;
	*feature_para_len = 4;
	break;

    case SENSOR_FEATURE_SET_FRAMERATE:
	LOG_INF("ov08d10-->current fps :%d\n", *feature_data_32);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_fps = (UINT16)*feature_data_32;
	spin_unlock(&imgsensor_drv_lock);
	break;
    case SENSOR_FEATURE_SET_HDR:
	LOG_INF("ov08d10-->ihdr enable :%d\n", *feature_data_32);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.ihdr_mode = (UINT8)*feature_data_32;
	spin_unlock(&imgsensor_drv_lock);
	break;
    case SENSOR_FEATURE_GET_CROP_INFO:
	LOG_INF("ov08d10-->SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);

	wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data + 1));

	switch (*feature_data_32) {
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
	    memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
	break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
	    memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
	break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
	    memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
	break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
	    memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
	break;

	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
	default:
	    memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
	break;
	}
	break;

    case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
	LOG_INF("ov08d10-->SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n", (UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));

	ihdr_write_shutter_gain((UINT16)*feature_data, (UINT16)*(feature_data+1), (UINT16)*(feature_data + 2));
	break;

    case SENSOR_FEATURE_SET_SHUTTER_FRAME_TIME:
	set_shutter_frame_length((UINT16) (*feature_data), (UINT16) (*(feature_data + 1)));
	break;

    case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
	LOG_INF("ov08d10-->This sensor can get temperature 20\n");
	*feature_return_para_32 = 20;
	*feature_para_len = 4;
	break;
    case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
	LOG_INF("ov08d10-->SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
	streaming_control(KAL_FALSE);
	break;
    case SENSOR_FEATURE_SET_STREAMING_RESUME:
	LOG_INF("ov08d10-->SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%llu\n", *feature_data);

	if (*feature_data != 0) {
	    set_shutter(*feature_data);
	}
	streaming_control(KAL_TRUE);
	break;
    case SENSOR_FEATURE_GET_BINNING_TYPE:
        switch (*(feature_data + 1)) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            *feature_return_para_32 = 1; /*BINNING_AVERAGED*/
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            *feature_return_para_32 = 1; /*BINNING_AVERAGED*/
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            *feature_return_para_32 = 1; /*BINNING_AVERAGED*/
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            *feature_return_para_32 = 1; /*BINNING_AVERAGED*/
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            *feature_return_para_32 = 1; /*BINNING_AVERAGED*/
            break;
        default:
            *feature_return_para_32 = 1; /*BINNING_AVERAGED*/
            break;
        }
        LOG_INF("ov08d10-->SENSOR_FEATURE_GET_BINNING_TYPE AE_binning_type:%d,\n",
            *feature_return_para_32);
        *feature_para_len = 4;

        break;
    case SENSOR_FEATURE_GET_AE_EFFECTIVE_FRAME_FOR_LE:
        *feature_return_para_32 = imgsensor.current_ae_effective_frame;
        break;
    case SENSOR_FEATURE_GET_AE_FRAME_MODE_FOR_LE:
        memcpy(feature_return_para_32, &imgsensor.ae_frm_mode,
            sizeof(struct IMGSENSOR_AE_FRM_MODE));
        break;
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

		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
		    rate = imgsensor_info.pre.mipi_pixel_rate;
		break;
	}
	    *(MUINT32 *)(uintptr_t)(*(feature_data + 1)) = rate;
	}
	break;
    case SENSOR_FEATURE_SET_AWB_GAIN:
        break;
    case SENSOR_FEATURE_SET_LSC_TBL:
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

UINT32 OV08D10_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	/* To Do : Check Sensor status here */
    if (pfFunc != NULL) {
		*pfFunc=&sensor_func;
    }
	return ERROR_NONE;
}
