/*
 * Touchright serial touchscreen driver
 *
 * Copyright (c) 2006 Rick Koch <n1gp@hotmail.com>
 *
 * Based on MicroTouch driver (drivers/input/touchscreen/mtouch.c)
 * Copyright (c) 2004 Vojtech Pavlik
 * and Dan Streetman <ddstreet@ieee.org>
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>

#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#endif
#include <linux/interrupt.h>

#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#define DRIVER_DESC	"MID serial uart dock driver"

#include <linux/miscdevice.h>
#include "mid_uart_dock.h"

#include <linux/fb.h>
#include <linux/notifier.h>

#include <linux/completion.h>



#define LOG_ERR (6)
#define LOG_DEBUG (7)
#define LOG_FULL (8)
#define Enable_LOG LOG_FULL
#define UART_DOCK_DRIVER_NAME "[UART_DOCK]"
#define LOG_UART_DOCK(num, fmt, args...)   \
do {									\
	if (Enable_LOG >= (int)num) {		\
		printk(KERN_ERR UART_DOCK_DRIVER_NAME " <%s> <%d> "fmt"\n", __func__, __LINE__, ##args);	\
	}								   \
} while (0)

//unsigned int dock_irqnr;
//unsigned int GPIO_DOCKING_DET;
//unsigned int GPIO_OTG_LDO_EN;//3.3V dock en
static struct delayed_work dock_check_work;
//static DEFINE_MUTEX(mid_kpd_mutex);
//extern int get_docking_status(void);

static old_real_report_val = 0;
static bool MID_UART_DOCK_PLUG = false;
static struct wakeup_source *mid_uart_wakelock;
//static unsigned char old_mouse_data;

struct mid_kpd {
	unsigned char old[8];
	int idx;
	unsigned char *new;
};
struct mid_kpd *kbd;

static int report_id = 0;
static int kplen = 0;
static int on_off = 0; 

static DEFINE_MUTEX(mid_ctrl_mutex);


static const unsigned char mid_kbd_keycode[256] = {
	  0,  0,  0,  0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,  // 16 -> 15
	 50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44,  2,  3,
	  4,  5,  6,  7,  8,  9, 10, 11, 28,  1, 14, 15, 57, 12, 13, 26,
	 27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 224, 225, 61, 62, 63, 133,
	 135, 165, 164, 163, 113, 114, 99, 70,119,110,102,104,111,107,109,106,
	105,108,103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
	 72, 73, 82, 83, 86,127,116,117,183,184,185,186,187,188,189,190,
	191,192,193,194,134,138,130,132,128,129,131,137,133,135,136,113,
	115,114,  0,  0,  0,121,  0, 89, 93,124, 92, 94, 95,  0,  0,  0,
	122,123, 90, 91, 85,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,//13 row  208 -> 207
	  172,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  115,  152,// HOME(键值)208 、 F13(键值)222 、 F14(键值)223
	 29, 42, 56,125, 97, 54,100,126,164,166,165,163,161,115,114,113,
	150,158,159,128,136,177,178,176,142,152,173,140
};			   
#define MAX_KEY_CNT	(sizeof(mid_kbd_keycode)/sizeof(mid_kbd_keycode[0]))
#define DEVICE_NAME	"mid-input"
static struct input_dev *mid_input;
static struct input_dev *mid_tochpad;

static int mid_gpio_direction_output(struct mid_uart_info *muart, bool is_on);

/*caplock light*/
static unsigned char caplock_light_on[6]  = {0xBB,0xBB,0xBB,0x02,0x01,0xCC};
static unsigned char caplock_light_off[6] = {0xBB,0xBB,0xBB,0x02,0x00,0xCC};
#if  1
static int success_count = 0;  
static int readback_error = 0;  

/*handshake*/
#define HANDSHAKE_RETRIES 3  
#define HANDSHAKE_DELAY_MS 300  

static unsigned char HANDSHAKE_CMD1[4] = {0x03,0x03,0xB1,0xCC};
static unsigned char HANDSHAKE_CMD2[4] = {0x6B,0x03,0x01,0xCC};
static unsigned char HANDSHAKE_ACK[4]  = {0x03,0x03,0x01,0xCC};
static bool cmd1_success = false;

static unsigned char VERSION_READ_CMD[5]  = {0x6B,0x08,0x01,0x02,0x03};
//static unsigned char KEYBOARD_VERSION_RESPONSE[6]  = {0x05,0x08,0x31,0x2E,0x30,0x36};

/*update*/
static unsigned char INIT_PACKAGE_ACK[6]  = {0x05,0x04,0xFF,0xFF,0xFF,0xFF};
static unsigned char EARSE_FLASH_ACK[6]   = {0x05,0x05,0xFF,0xFF,0xFF,0xFF};
static unsigned char UPDATE_STATUS_SUCCESS[6] = {0x05,0x07,0x01,0xFF,0xFF,0xFF};
static unsigned char UPDATE_STATUS_FAIL[6]    = {0x05,0x07,0x00,0xFF,0xFF,0xFF};

#define UPDATE_PACKAGE_LENGTH 38
static unsigned char TEMP_INIT_PACKAGE[6]   = {0x6B,0x04,0xFF,0xFF,0xFF,0xFF};
static unsigned char TEMP_EARSE_FLASH[4]   = {0x6B,0x05,0xFF,0xFF};
static unsigned char TEMP_PACKAGE_LENGTH[6]   = {0x6B,0x07,0x01,0x00,0xFF,0xFF};
unsigned char UPDATE_PACKAGE[38]={0x6B,0x06,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
								  0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
								  0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
								  0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05};

#endif
/*
 * Definitions & global arrays.
 */

#define TR_FORMAT_TOUCH_BIT	0xFF //按键松开检测
//#define TR_FORMAT_STATUS_BYTE	0x40
//#define TR_FORMAT_STATUS_MASK	~TR_FORMAT_TOUCH_BIT

#define TR_LENGTH 14

#define TR_MIN_XC -127
#define TR_MAX_XC 127
#define TR_MIN_YC -127
#define TR_MAX_YC 127

#define MID_SERIO_CONTROL    1

/*
 * Per-touchscreen data.
 */

struct tr {
	struct input_dev *dev;
	struct serio *serio;
	int idx;
	signed char data[TR_LENGTH];
	char phys[32];
	
	unsigned long last;
	unsigned char count;
};

//add by LQ start
#if 0
static int arrays_are_equal(int *array1, int *array2, size_t size) 
{  
	return (size == 0 || memcmp(array1, array2, size * sizeof(int)) == 0); 
}
#endif
//add by LQ end

#ifdef MID_SERIO_CONTROL
static int mid_report_uart_data(int recevice_data)
{
	int real_report_value = 0;
	LOG_UART_DOCK(LOG_DEBUG, " ###########  report case %x#########\n",recevice_data);
	switch(recevice_data)
	{
		case MID_KEY_NULL://无值
		real_report_value = 0;
		break;
		case UART_KEY_SEARCH:
		LOG_UART_DOCK(LOG_DEBUG, " ########### report KEY_SEARCH char ########## \n");
		real_report_value = KEY_SEARCH;
		break;
		
		case UART_KEY_BACK:
		LOG_UART_DOCK(LOG_DEBUG, " ########### report KEY_BACK char ########## \n");
		real_report_value = KEY_BACK;
		break;
		
		case UART_KEY_INTERNET_BROWSER:
		LOG_UART_DOCK(LOG_DEBUG, " ########### report UART_KEY_INTERNET_BROWSER char ########## \n");
		real_report_value = KEY_WWW;
		break;
		
		case UART_KEY_EMAIL:
		LOG_UART_DOCK(LOG_DEBUG, " ########### report KEY_EMAIL char ########## \n");
		real_report_value = KEY_EMAIL;
		break;
		
		case UART_KEY_MENU:
		LOG_UART_DOCK(LOG_DEBUG, " ########### report KEY_MENU char ########## \n");
		real_report_value = KEY_MENU;
		break;
		
		case UART_KEY_PLAYPAUSE:
		real_report_value = KEY_PLAYPAUSE;
		break;
		
		case UART_KEY_NEXTSONG:
		real_report_value = KEY_NEXTSONG;
		break;		
				
		case UART_KEY_PREVIOUSSONG:
		real_report_value = KEY_PREVIOUSSONG;
		break;
		
		case UART_KEY_MUTE:
		real_report_value = KEY_MUTE;
		break;
		
		case UART_KEY_VOLUMEDOWN:
		real_report_value = KEY_VOLUMEDOWN;
		break;
		
		case UART_KEY_VOLUMEUP:
		real_report_value = KEY_VOLUMEUP;
		break;
			
		case UART_KEY_POWER:
		real_report_value = KEY_POWER;
		break;
		
		case UART_KEY_HOME:
		real_report_value = KEY_HOMEPAGE;	/* AC Home */
		break;
		
		case UART_KEY_BRIGHTNESS_UP:
		real_report_value = KEY_BRIGHTNESSUP;
		LOG_UART_DOCK(LOG_DEBUG, " ########### report KEY_BRIGHTNESSUP char ########## \n");
		break;
		
		case UART_KEY_BRIGHTNESS_DOWN:
		real_report_value = KEY_BRIGHTNESSDOWN;
		LOG_UART_DOCK(LOG_DEBUG, " ########### report KEY_BRIGHTNESSDOWN char ########## \n");
		break; 

		case UART_KEY_MUSIC:
		real_report_value = KEY_SOUND;
		LOG_UART_DOCK(LOG_DEBUG, " ########### report KEY_SOUND char ########## \n");
		break; 
		
		default:
			LOG_UART_DOCK(LOG_DEBUG, " ########### report ID index. fatal error #########\n");
		break;
	}

	return real_report_value;
	
}

void mid_special_key_report(int kplen,struct serio *serio)
{
	struct tr *tr = serio_get_drvdata(serio);
	struct input_dev *dev = tr->dev;
	int real_report_val = 0;
	int mid_data;
	
	//42 & 62 report ID only with 2 byte key_data: (data[2]<<8)|data[1];
	mid_data = ((unsigned char)tr->data[2]<<8) | (unsigned char)tr->data[1];
	LOG_UART_DOCK(LOG_FULL, " mid_data:%d\n",mid_data);

	real_report_val = mid_report_uart_data(mid_data);
	LOG_UART_DOCK(LOG_FULL, " report new key real_report_val:%d,old_real_report_val:%d\n",real_report_val,old_real_report_val);
	if(real_report_val != old_real_report_val){//key有变化
		if(real_report_val != 0)//非空,当前是按下状态
		{
			LOG_UART_DOCK(LOG_FULL, " report new key press real_report_val:%d\n",real_report_val);
			input_report_key(dev, real_report_val, 1);
			input_sync(dev);

			
		}else{//松开,上报之前的key
			LOG_UART_DOCK(LOG_FULL, " report old key realease old_real_report_val:%d\n",old_real_report_val);
 			input_report_key(dev, old_real_report_val, 0);
			input_sync(dev);
			

		}
	}
	old_real_report_val = real_report_val;//record old key

	return ;
}


void *mid_scan(unsigned char *data, int c, size_t size)
{
	unsigned char *p = data;
	//LOG_UART_DOCK(LOG_DEBUG, " enter \n");
	while (size) {
		if (*p == c)
		return (void *)p;
		p++;
		size--;
	}
	return (unsigned char *)p;
} 
/* void mid_mouse_report(unsigned char mid_mouse_data,struct serio *serio)
{
	struct tr *tr = serio_get_drvdata(serio);
	//struct input_dev *dev = tr->dev;
	unsigned char new_mouse_data = mid_mouse_data;
	
	LOG_UART_DOCK(LOG_DEBUG, " enter new_mouse_data:%x,old_mouse_data:%x\n",new_mouse_data,old_mouse_data);
	if(old_mouse_data != new_mouse_data)
	{
	
		input_event(mid_tochpad,EV_MSC,MSC_SCAN,1);

		input_event(mid_tochpad,EV_KEY,BTN_MOUSE,new_mouse_data);		
		input_report_rel(mid_tochpad, BTN_LEFT,   new_mouse_data & 0x01);
		input_report_rel(mid_tochpad, BTN_RIGHT,  new_mouse_data & 0x02);
		input_report_rel(mid_tochpad, BTN_MIDDLE, new_mouse_data & 0x04);
	}
	
	old_mouse_data = new_mouse_data;
	
} */

//int serio_write(struct serio *serio, unsigned char data)
#if 0
void set_light_onoff(struct serio *serio, int on_off)
{
	//int i;
	struct completion cmd_done;
	int error;
	printk("LQ >>> %s  \n",__func__);
	if(on_off){
		init_completion(&cmd_done);
	//for(i=0;i<length;i++){
	//	serio_write(serio, data[i]);
		//printk("LQ >>> %s data[%d]=0x%x\n",__func__,i,data[i]);
	//}
		serio_write(serio,0xBB);
		serio_write(serio,0xBB);
		serio_write(serio,0xBB);
		serio_write(serio,0x02);
		serio_write(serio,0x01);
		serio_write(serio,0xCC);

		error = wait_for_completion_killable(&cmd_done);
	}else{
		init_completion(&cmd_done);
		serio_write(serio,0xBB);
		serio_write(serio,0xBB);
		serio_write(serio,0xBB);
		serio_write(serio,0x02);
		serio_write(serio,0x00);
		serio_write(serio,0xCC);
		error = wait_for_completion_killable(&cmd_done);
	}
	
	if (error){
		printk("LQ >>> %s  Serial I/O error ,Close Serial\n",__func__);
		goto bail1;
	}

bail1:
	   serio_close(serio);
	   serio_set_drvdata(serio, NULL);

}
#endif

static irqreturn_t tr_interrupt(struct serio *serio,
		unsigned char data, unsigned int flags)
{
	struct tr *tr = serio_get_drvdata(serio);
	struct input_dev *dev = tr->dev;
	int i;
	struct mid_uart_info *muart = container_of(serio,struct mid_uart_info, serio);
	

	printk("LQ >>> %s  new data=0x%x\n",__func__,data);

	if(tr->idx == 0)
	{
		//printk("LQ >>> data buffer clear \n"); 
		//report_id = (data & 0xe0) >> 5;
		//kplen = data & 0x1F;
		kplen=data;  //获取第一个数据为length
		printk("LQ >>>  kplen=%d \n",kplen);
		if(kplen>9){  //长度大于9 暂时过滤数据  前期调试
			tr->idx = 0;
			return IRQ_HANDLED;
		}	
	}
	//LOG_UART_DOCK(LOG_DEBUG, "enter  data:0x%x kplen:0x%x\n", data, kplen);
	tr->data[tr->idx++] = (signed char)data;   //将收到数据赋值给tr->data  --->  length:data0  type:data1  ... data[kplen+1]
	//printk("LQ >>> %s  tr->idx=%d,data:0x%x\n",__func__,tr->idx,data);
	
	
	if (tr->idx == (kplen+1))  //收到klen个字节后，开始处理解析  
	{
		printk("LQ >>>  get %d  byte data done !!!  start parse data\n",kplen);

		
		goto event_handle;
	}

	return IRQ_HANDLED;

event_handle :	

		
			
	//for(i=0;i<(kplen+1);i++){
	//	printk("LQ >>> print data[%d]=%x\n",i,tr->data[i]);
	//}
				
	report_id = tr->data[1]; // type为  byte2
	printk("LQ >>>  report_id=%d \n",report_id);
		
		

			switch(report_id)
			{
				case 1://data[3-8]      
						
#if 1
						for (i = 0; i < 8; i++) {
							kbd->new[i] = (unsigned char)tr->data[i+2];//将data数组每次缓存到 kbd->new 数组去比较判断  ---》  从data2开始是数据
							//LOG_UART_DOCK(LOG_DEBUG, "current_real_data[0-7] report kbd->new[%d]:%x\n",i,kbd->new[i]);
							
							input_report_key(dev, mid_kbd_keycode[i + 224], (kbd->new[0] >> i) & 1);//检测第i位是否为1，如果为1，则上报按键mid_kbd_keycode[i + 224]
							//LOG_UART_DOCK(LOG_DEBUG, "mid_kbd_keycode[%d]%x\n",i + 224,mid_kbd_keycode[i + 224]);
						}
						for (i = 2; i < 8; i++) {							
					
							if (kbd->old[i] > 3 && mid_scan(kbd->new + 2, kbd->old[i], 6) == kbd->new + 8) {
								if (mid_kbd_keycode[kbd->old[i]]){
									input_report_key(dev, mid_kbd_keycode[kbd->old[i]], 0);//新出现的按键值没有old[i],释放old[i]
									LOG_UART_DOCK(LOG_DEBUG, 
										"New key released.\n",
										kbd->new[i]);
								}else{
									LOG_UART_DOCK(LOG_DEBUG, 
										"Unknown key (scancode %#x) released.\n",
										kbd->old[i]);
								}
							}
						
							if (kbd->new[i] > 3 && mid_scan(kbd->old + 2, kbd->new[i], 6) == kbd->old + 8) {
								if (mid_kbd_keycode[kbd->new[i]])//old[i]对应的input码有意义
								{
									input_report_key(dev, mid_kbd_keycode[kbd->new[i]], 1);//上报新键按下事件
									LOG_UART_DOCK(LOG_DEBUG, 
										"New key (scancode %#x) pressed.\n",
										kbd->new[i]);
																		//caplock light off
								}else{
									LOG_UART_DOCK(LOG_DEBUG, 
										"Unknown key (scancode %#x) pressed.\n",
										kbd->new[i]);
								}
							}
						}
						input_sync(dev);
						memcpy(kbd->old, kbd->new, 8);//记录上一次report数组

					
						//caplock light on/off
						if(kbd->new[2]==0x39){ 
							if(on_off){
								//set_light_onoff(serio,1);	
								//serio_write(serio,0xBB);
								//serio_write(serio,0xBB);
								//serio_write(serio,0xBB);
								//serio_write(serio,0x02);
								//serio_write(serio,0x01);
								//serio_write(serio,0xCC);
								for(i=0;i<6;i++){
									serio_write(serio, caplock_light_on[i]);
								//printk("LQ >>> %s data[%d]=0x%x\n",__func__,i,data[i]);
								}

								on_off=0;
							}else{
								//set_light_onoff(serio,0);
								//serio_write(serio,0xBB);
								//serio_write(serio,0xBB);
								//serio_write(serio,0xBB);
								//serio_write(serio,0x02);
								//serio_write(serio,0x00);
								//serio_write(serio,0xCC);
								for(i=0;i<6;i++){
									serio_write(serio, caplock_light_off[i]);
								//printk("LQ >>> %s data[%d]=0x%x\n",__func__,i,data[i]);
								}
								
								on_off=1;
							}
						}
#endif						

					break;
				case 2: /*touch pad*/
						for (i = 0; i < 6; i++) {
							kbd->new[i] = (unsigned char)tr->data[i];
							LOG_UART_DOCK(LOG_DEBUG, "current touchpad data[0-5] report kbd->new[%d]:0x%x\n",i,kbd->new[i]);
						}
						
						input_report_key(mid_tochpad, BTN_LEFT,   tr->data[2] & 0x01);
						input_report_key(mid_tochpad, BTN_RIGHT,  tr->data[2] & 0x02);
						input_report_key(mid_tochpad, BTN_MIDDLE, tr->data[2] & 0x04);
						
						if(kbd->new[2]==0x00){
							if(kbd->new[5]==0x09){  //上滑
								input_report_rel(mid_tochpad, REL_WHEEL, 0xffffffff);
							}
							if(kbd->new[5]==0x01){	//下滑
								input_report_rel(mid_tochpad, REL_WHEEL, 0x1);
							}
							if(kbd->new[5]==0xEE){	//放大后半部
								input_report_rel(mid_tochpad, REL_WHEEL, 0xFF);
							}
							if(kbd->new[5]==0xFF){	//缩小后半部
								input_report_rel(mid_tochpad, REL_WHEEL, 0xffffffff);
							}
						}
						if(kbd->new[2]==0x08){
							if(kbd->new[5]==0x09){  //左滑
								input_report_rel(mid_tochpad, REL_HWHEEL, 0x1);
							}
							if(kbd->new[5]==0x01){	//右滑
								input_report_rel(mid_tochpad, REL_HWHEEL, 0xffffffff);
							}
						}
						
						//input_report_rel(mid_tochpad, REL_WHEEL, tr->data[5]);//上下滚轮
						//input_report_rel(mid_tochpad, REL_HWHEEL, tr->data[5]);//水平滚轮
						input_report_rel(mid_tochpad, REL_X, (signed char)tr->data[3]);
						input_report_rel(mid_tochpad, REL_Y, (signed char)tr->data[4]);
						input_sync(mid_tochpad);

					break;
				case 3: /*handshake*/
						LOG_UART_DOCK(LOG_DEBUG, " ====  Enter handshake  ===== \n");
						for (i = 0; i < 4; i++) {
							kbd->new[i] = (unsigned char)tr->data[i];
							LOG_UART_DOCK(LOG_DEBUG, "current hand_shake data [0-3] report kbd->new[%d]:0x%x\n",i,kbd->new[i]);
						}
										
						if(!cmd1_success){
							//先处理cmd1
							if (memcmp(kbd->new, HANDSHAKE_CMD1, 4) == 0) {  
								LOG_UART_DOCK(LOG_DEBUG, " have received B1 Packet from keyboard \n");
								cmd1_success = true;
								/*readback to keyboard*/
								for(i=0;i<4;i++){
									serio_write(serio, HANDSHAKE_CMD2[i]);
								}
							}else{
								LOG_UART_DOCK(LOG_ERR, " have not received B1 Packet from keyboard \n");
								//cmd1_success = false;
								//readback_error++;  //正常情况下不会出现
							}													
						}else{
							if (memcmp(kbd->new, HANDSHAKE_ACK, 4) == 0) {  
								LOG_UART_DOCK(LOG_DEBUG, " have received ACK from keyboard \n");
								cmd1_success = false;
								success_count++;
							}else{
								LOG_UART_DOCK(LOG_ERR, " have not received ACK from keyboard \n");
								readback_error++;
							}			
						}
						
						/*Need 3 handshakes*/
						if (success_count >= 3) {   
							LOG_UART_DOCK(LOG_DEBUG, "Handshake success !!!\n");
							success_count = 0;
							/*read keyboard version*/
							for(i=0;i<5;i++){
								serio_write(serio, VERSION_READ_CMD[i]);
							}
						} 
						
						/*if 3 fails, need power off/on again*/
						if(readback_error>=3){  
							LOG_UART_DOCK(LOG_ERR, "Handshake failed, reset keyboard\n");
							
							mid_gpio_direction_output(muart, 0); 
							mdelay(500); 
							mid_gpio_direction_output(muart, 1);
							
							readback_error = 0;
							success_count = 0;
						}  
							
					break;
				case 4:/*init update package*/
					LOG_UART_DOCK(LOG_DEBUG, " ====  Enter init package  ===== \n");
						for (i = 0; i < 6; i++) {
							kbd->new[i] = (unsigned char)tr->data[i];
							LOG_UART_DOCK(LOG_DEBUG, "current init package [0-5] report kbd->new[%d]:0x%x\n",i,kbd->new[i]);
						}

						if (memcmp(kbd->new, INIT_PACKAGE_ACK, 6) == 0) {  
							LOG_UART_DOCK(LOG_DEBUG, " ====  init update package Success !  ===== \n");
							for(i=0;i<4;i++){
								serio_write(serio, TEMP_EARSE_FLASH[i]);
							}	
						}else{
							LOG_UART_DOCK(LOG_ERR, " ====  init update package Success !  ===== \n");
						}
					break;
				case 5:/*earse flash*/
					LOG_UART_DOCK(LOG_DEBUG, " ====  Enter earse flash  ===== \n");
						for (i = 0; i < 6; i++) {
							kbd->new[i] = (unsigned char)tr->data[i];
							LOG_UART_DOCK(LOG_DEBUG, "current init package [0-5] report kbd->new[%d]:0x%x\n",i,kbd->new[i]);
						}

						if (memcmp(kbd->new, EARSE_FLASH_ACK, 6) == 0) {  
							LOG_UART_DOCK(LOG_DEBUG, " ====  earse flash Success !  ===== \n");
							/*begain transfer update package*/
							for(i=0;i<UPDATE_PACKAGE_LENGTH;i++){
								serio_write(serio, UPDATE_PACKAGE[i]);
							}
							/*send package done*/
							for(i=0;i<6;i++){
								serio_write(serio, TEMP_PACKAGE_LENGTH[i]);
							}
						}else{
							LOG_UART_DOCK(LOG_ERR, " ====  earse flash Success !  ===== \n");
						}
					break;
				case 7:/*update status reponse*/
					LOG_UART_DOCK(LOG_DEBUG, " ====  Enter update status  ===== \n");
						for (i = 0; i < 6; i++) {
							kbd->new[i] = (unsigned char)tr->data[i];
							LOG_UART_DOCK(LOG_DEBUG, "current update package [0-5] report kbd->new[%d]:0x%x\n",i,kbd->new[i]);
						}
				
						if (memcmp(kbd->new, UPDATE_STATUS_SUCCESS, 6) == 0) {  
							LOG_UART_DOCK(LOG_DEBUG, " ====  earse flash Success !  ===== \n");
						}else if(memcmp(kbd->new, UPDATE_STATUS_FAIL, 6) == 0){
							LOG_UART_DOCK(LOG_ERR, " ====  earse flash Failed !  ===== \n");
						}else{
							LOG_UART_DOCK(LOG_ERR, " ====  earse flash status invalid !!!  ===== \n");
						}
					break;
				case 8: /*read keyboard version*/
						LOG_UART_DOCK(LOG_DEBUG, " ====  Enter read kerboard version  ===== \n");
						for (i = 0; i < 4; i++) {
							kbd->new[i] = (unsigned char)tr->data[i+2]; //read version  data[2-5]
							LOG_UART_DOCK(LOG_DEBUG, "current kerboard version[0-3] report kbd->new[%d]:0x%x\n",i,kbd->new[i]);
						}
						/*compare keboard version*/
						if(0){/*Need Update*/
							LOG_UART_DOCK(LOG_DEBUG, "keboard version is too low,need update !\n");
							for(i=0;i<6;i++){
								serio_write(serio, TEMP_INIT_PACKAGE[i]);
							}
						}else{
							LOG_UART_DOCK(LOG_DEBUG, "No need to update !\n");
						}
					break;
				case 30:
					mid_special_key_report(kplen,serio);
					LOG_UART_DOCK(LOG_FULL, " ########### report ID 62.report consumer key value #########\\n");
					break;
				case 40:
					break;	
					LOG_UART_DOCK(LOG_FULL, " ########### report ID 42.report system control key value #########\n");
					mid_special_key_report(kplen,serio);					

				default:
					LOG_UART_DOCK(LOG_DEBUG, " ########### report ID index. fatal error #########\n");
					break;	
			}

			tr->idx = 0;

	return IRQ_HANDLED;
}


/*
 * tr_disconnect() is the opposite of tr_connect()
 */

static void tr_disconnect(struct serio *serio)
{
	struct tr *tr = serio_get_drvdata(serio);
	LOG_UART_DOCK(LOG_DEBUG," enter\n");
	input_get_device(tr->dev);
	input_unregister_device(tr->dev);
	serio_close(serio);
	serio_set_drvdata(serio, NULL);
	input_put_device(tr->dev);
	kfree(tr);
}

/*
 * tr_connect() is the routine that is called when someone adds a
 * new serio device that supports the Touchright protocol and registers it as
 * an input device.
 */

static int tr_connect(struct serio *serio, struct serio_driver *drv)
{
	struct tr *tr;
	
	//kzh add for report temp buf start
	struct mid_kpd *mid_kbd;
	unsigned char *mid_new;
	//kzh add for report temp buf end
	
	struct input_dev *input_dev;
	int err,i;
	LOG_UART_DOCK(LOG_DEBUG," enter\n");
	on_off = 1;
	tr = kzalloc(sizeof(struct tr), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!tr || !input_dev) {
		err = -ENOMEM;
		LOG_UART_DOCK(LOG_ERR,"unable to register input_dev.\n");
		goto fail1;
	}
	
	//kzh add for report temp buf start
	mid_kbd = kzalloc(sizeof(struct mid_kpd), GFP_KERNEL);
	if (!mid_kbd ) {
		err = -ENOMEM;
		LOG_UART_DOCK(LOG_ERR,"unable to kzalloc mid_kbd.\n");
		return err;
	}
	mid_new = kzalloc(sizeof(unsigned char), GFP_KERNEL);
	if (!mid_new ) {
		err = -ENOMEM;
		LOG_UART_DOCK(LOG_ERR,"unable to kzalloc mid_new.\n");
		return err;
	}
	mid_kbd->new = mid_new;
	kbd = mid_kbd;
	//kzh add for report temp buf end

	tr->serio = serio;
	tr->dev = input_dev;
	snprintf(tr->phys, sizeof(tr->phys), "%s/input0", serio->phys);

	input_dev->name = "uart_dock";
	input_dev->phys = tr->phys;
	input_dev->id.bustype = SERIO_RS232;
	input_dev->id.vendor = 0x3F;
	input_dev->id.product = 1;
	input_dev->id.version = 0x0110;
	input_dev->dev.parent = &serio->dev;
	
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_SYN, input_dev->evbit);


	for(i = 0; i < 256; i++){
		input_set_capability(input_dev, EV_KEY, mid_kbd_keycode[i]);
	}

	//kzhkzh add for extra media & system control key 	
	input_set_capability(input_dev, EV_KEY, KEY_SEARCH);
	input_set_capability(input_dev, EV_KEY, KEY_WWW);//UART_KEY_INTERNET_BROWSER
	input_set_capability(input_dev, EV_KEY, KEY_EMAIL);
	input_set_capability(input_dev, EV_KEY, KEY_MENU);
	input_set_capability(input_dev, EV_KEY, KEY_HOMEPAGE);
	
	input_set_capability(input_dev, EV_KEY, KEY_BRIGHTNESSUP);
	input_set_capability(input_dev, EV_KEY, KEY_BRIGHTNESSDOWN);
	input_set_capability(input_dev, EV_KEY, KEY_SOUND);
	
	serio_set_drvdata(serio, tr);

	err = serio_open(serio, drv);
	LOG_UART_DOCK(LOG_DEBUG," err:%d\n",err);
	if (err){
		LOG_UART_DOCK(LOG_ERR," faile to open serio \n");
		goto fail2;
	}

	err = input_register_device(tr->dev);
	LOG_UART_DOCK(LOG_DEBUG," err:%d\n",err);
	if (err){
		LOG_UART_DOCK(LOG_ERR," faile to register  input dev \n");
		goto fail3;
	}
	LOG_UART_DOCK(LOG_DEBUG," enter\n");
	return 0;

 fail3:	serio_close(serio);
 fail2:	serio_set_drvdata(serio, NULL);
 fail1:	input_free_device(input_dev);
	kfree(tr);
	return err;
}

/*
 * The serio driver structure.
 */

static const struct serio_device_id tr_serio_ids[] = {
	{
		.type	= SERIO_RS232,
		.proto	= 0x3F,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{ 0 }
};

MODULE_DEVICE_TABLE(serio, tr_serio_ids);



static struct serio_driver mid_serio_drv = {
	.driver		= {
		.name	= "uart_dock",
	},
	.description	= DRIVER_DESC,
	.id_table	= tr_serio_ids,
	.interrupt	= tr_interrupt,
	.connect	= tr_connect,
	.disconnect	= tr_disconnect,
};
#endif



static int get_docking_status(struct mid_uart_info *muart){
	//int id = muart->dock_in_gpiod ?	gpiod_get_value_cansleep(muart->dock_in_gpiod) : 1;
	int id = !gpiod_get_value_cansleep(muart->dock_in_gpiod);
	LOG_UART_DOCK(LOG_DEBUG,"xuchen get dock_in_gpiod %s\n", id ? "on" : "off");

	return id;
}
//EXPORT_SYMBOL_GPL(get_docking_status);

static irqreturn_t dock_eint_interrupt_handler(int irq, void *dev_id)
{
	struct mid_uart_info *muart = dev_id;

/* 	if((get_docking_status(muart) == 1))
		irq_set_irq_type( muart->dock_in_irq, IRQF_TRIGGER_FALLING);
	else if((get_docking_status(muart) == 0))
		irq_set_irq_type( muart->dock_in_irq, IRQF_TRIGGER_RISING);
	
	disable_irq_nosync( muart->dock_in_irq);	 */
	//printk("LQ >>> %s  \n",__func__);
	//schedule_delayed_work(&muart->dock_in_detcable, msecs_to_jiffies(50));
	queue_delayed_work(system_power_efficient_wq, &muart->dock_in_detcable, msecs_to_jiffies(50));
	 return IRQ_HANDLED;
}

static int mid_gpio_direction_output(struct mid_uart_info *muart, bool is_on)
{
	//struct device *dev = muart->dev;

	LOG_UART_DOCK(LOG_DEBUG,"xuchen dock_bus_gpiod turn %s\n", is_on ? "on" : "off");
	if (is_on) {
		gpiod_set_value(muart->dock_bus_gpiod, 1);
	} else {
		gpiod_set_value(muart->dock_bus_gpiod, 0);
	}

	return 0;
}

static void dock_mode_switch(struct work_struct *work)
{
	struct mid_uart_info *muart = container_of(to_delayed_work(work),
		struct mid_uart_info, dock_in_detcable);

	LOG_UART_DOCK(LOG_DEBUG,"dock_mode_switch\n");
	mutex_lock(&mid_ctrl_mutex);
	if((get_docking_status(muart) == 1)&&(MID_UART_DOCK_PLUG == false)){
		printk("LQ >>> %s  plug in \n",__func__);
		mid_gpio_direction_output(muart, 1);
		//kzh add for report new sw event for uart dock 
 		input_report_switch(mid_input, SW_MID_UART_DOCK,1);
		input_sync(mid_input);
		MID_UART_DOCK_PLUG = true;
		__pm_stay_awake(mid_uart_wakelock);
		LOG_UART_DOCK(LOG_DEBUG,"MID_docking 1111 eint trigger,MID_UART_DOCK_PLUG:%d\n",MID_UART_DOCK_PLUG);
	}else if ((get_docking_status(muart) == 0)&&(MID_UART_DOCK_PLUG == true)){
		printk("LQ >>> %s plug out \n",__func__);
		mid_gpio_direction_output(muart, 0);
		//kzh add for report new sw event for uart dock 
		input_report_switch(mid_input, SW_MID_UART_DOCK,0);
		input_sync(mid_input);
		MID_UART_DOCK_PLUG = false;
		__pm_relax(mid_uart_wakelock);
		LOG_UART_DOCK(LOG_DEBUG,"MID_docking 0000 eint trigger,MID_UART_DOCK_PLUG:%d\n",MID_UART_DOCK_PLUG);
	}
	mutex_unlock(&mid_ctrl_mutex);
}

//kzhkzh add for platform driver register
static int mid_get_dts_info(struct mid_uart_info *muart)
{
	//struct device_node *node= NULL;
	int dock_in;
	int ret = 0;

	muart->dock_in_gpiod = devm_gpiod_get(muart->dev, "dock_in", GPIOD_IN);

	if (!muart->dock_in_gpiod || IS_ERR(muart->dock_in_gpiod)) {
		LOG_UART_DOCK(LOG_DEBUG,"gpio_docking_det:%d\n",ret);
		muart->dock_in_gpiod = NULL;
		return -EINVAL;
	}

	muart->dock_in_irq = gpiod_to_irq(muart->dock_in_gpiod);
	if (muart->dock_in_irq < 0) {
		LOG_UART_DOCK(LOG_DEBUG,"failed to get dock_in IRQ\n",ret);
		return muart->dock_in_irq;
	}
	
	muart->dock_bus_gpiod = devm_gpiod_get(muart->dev, "dock_bus", GPIOD_OUT_LOW);
	if (!muart->dock_bus_gpiod || IS_ERR(muart->dock_bus_gpiod)) {
		LOG_UART_DOCK(LOG_DEBUG,"failed to get dock_bus gpio\n",ret);
		return -ENODEV;
	}

	INIT_DELAYED_WORK(&muart->dock_in_detcable, dock_mode_switch);
	//INIT_DELAYED_WORK(&dock_check_work, dock_mode_switch);
	dock_check_work = muart->dock_in_detcable;
	//muart->dock_in_detcable = dock_check_work;

	ret = devm_request_threaded_irq(muart->dev, muart->dock_in_irq, NULL,
			dock_eint_interrupt_handler, IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			dev_name(muart->dev), muart);

	if (ret < 0) {
		LOG_UART_DOCK(LOG_DEBUG,"failed to request handler for dock_in_detcable IRQ\n",ret);
		return ret;
	}

	enable_irq_wake(muart->dock_in_irq);

	dock_in = muart->dock_in_gpiod ? gpiod_get_value_cansleep(muart->dock_in_gpiod) : 1;
	//开机过程中键盘插入检测
	if (!dock_in) {
		printk("LQ >>> %s  detected keyboard plug-in during boot-up\n",__func__);
		queue_delayed_work(system_power_efficient_wq, &muart->dock_in_detcable, msecs_to_jiffies(5000));
	}
	return ret;
}
static const struct of_device_id mid_match_table[] = {
	{.compatible = "mediatek, mid_docking",},
	{}
};
MODULE_DEVICE_TABLE(of, mid_match_table);


static ssize_t mid_input_write(struct file *file, const char __user *buf, size_t count_want, loff_t *ppos)
{
	char kbuf[64] = {0};
	int rc = 0;
	unsigned int ret_value = 0;//获取转换后的数据
	memset(kbuf,0,64);

	rc = copy_from_user(kbuf, buf, count_want);
	if (rc)
	{
		LOG_UART_DOCK(LOG_DEBUG,"copy_from_user fail!\n");
		return -EINVAL;
	}
	LOG_UART_DOCK(LOG_DEBUG,"kernel parse kbuf:%s\n", kbuf);

	mutex_lock(&mid_ctrl_mutex);
	ret_value = simple_strtol(kbuf, NULL, 16);
	mutex_unlock(&mid_ctrl_mutex);
	LOG_UART_DOCK(LOG_DEBUG,"Last ret_value:0x%x\n",ret_value);

	if(1 == ret_value)
	{
		schedule_delayed_work(&dock_check_work, msecs_to_jiffies(1000));//kzhkzh add for System first boot on,need exec
	}

	return count_want;
}


static struct file_operations mid_dev_fops = {
	.owner	=THIS_MODULE,
	.write	=mid_input_write,
	//.read = mid_input_read,
};

static struct miscdevice mid_misc_dev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "mid_uart",
	.fops	= &mid_dev_fops,
};
//kzhkzh add for boot_on

static int mid_input_init(void)
{
	int ret = 0;

	mid_input = input_allocate_device();
	if(!mid_input) 
		return -ENOMEM;
 
	__set_bit(EV_SW, mid_input->evbit);
	__set_bit(EV_SYN, mid_input->evbit);
	__set_bit(SW_MID_UART_DOCK, mid_input->swbit);
	__set_bit(EV_KEY, mid_input->evbit);
	
	input_set_capability(mid_input, EV_KEY, KEY_F10);
	
 
	mid_input->name = "mid_input";
	//mid_input->phys = "mid-input/input0";
 
	mid_input->id.bustype = BUS_HOST;
 
	if(input_register_device(mid_input) != 0)
	{
		LOG_UART_DOCK(LOG_DEBUG," mid-input mid_input register device fail!!\n");
		input_free_device(mid_input);
		return -ENODEV;
	}
	
	ret = misc_register(&mid_misc_dev);
	
	LOG_UART_DOCK(LOG_DEBUG,"enter probe success End,ret:%d!!!!\n",ret);
	return ret;
}

static int mid_mouse_init(void)
{
	mid_tochpad = input_allocate_device();
	if(!mid_tochpad) 
		return -ENOMEM;
 
	__set_bit(EV_SW, mid_tochpad->evbit);
	__set_bit(EV_SYN, mid_tochpad->evbit);
	__set_bit(SW_MID_UART_DOCK, mid_tochpad->swbit);
	__set_bit(EV_KEY, mid_tochpad->evbit);
	
	mid_tochpad->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REL);
	mid_tochpad->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) |
		BIT_MASK(BTN_RIGHT);
	mid_tochpad->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y); 

	//set_bit(mid_tochpad, EV_MSC, MSC_SCAN);
	mid_tochpad->name = "mid_mouse";
	mid_input->phys = "mid_mouse/input0";
	set_bit(BTN_MIDDLE, mid_tochpad->keybit);
	set_bit(BTN_SIDE, mid_tochpad->keybit);
	set_bit(BTN_EXTRA, mid_tochpad->keybit);	
	set_bit(REL_WHEEL, mid_tochpad->relbit);	
	set_bit(REL_HWHEEL, mid_tochpad->relbit);
	
	input_set_capability(mid_tochpad, EV_KEY, KEY_F10);

 	mid_tochpad->id.bustype = BUS_RS232;
	mid_tochpad->id.vendor = 0x3F;
	mid_tochpad->id.product = 0x001e;
	mid_tochpad->id.version = 0x0110;
	if(input_register_device(mid_tochpad) != 0)
	{
		LOG_UART_DOCK(LOG_DEBUG," mid-input mid_tochpad register device fail!!\n");
		input_free_device(mid_tochpad);
		return -ENODEV;
	}
 
	LOG_UART_DOCK(LOG_DEBUG," mid_tochpad register tinitialized\n");
	return 0;
}

static int mid_docking_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mid_uart_info *muart;
	int ret = 0;
	
	muart = devm_kzalloc(&pdev->dev, sizeof(*muart), GFP_KERNEL);
	if (!muart)
		return -ENOMEM;

	muart->dev = dev;
	printk("LQ >>> %s  start\n",__func__);
	mid_get_dts_info(muart);

	
	#ifdef MID_SERIO_CONTROL
	ret = serio_register_driver(&mid_serio_drv);
	LOG_UART_DOCK(LOG_DEBUG,"serio serio_register_driver register-ret:%d\n",ret);
	#endif

	mid_uart_wakelock = wakeup_source_register(&pdev->dev,"mid_uart_wakelock");//mid add for suspend ,need have a wakelock to make sure resume func ok.

	ret = mid_input_init();
    LOG_UART_DOCK(LOG_DEBUG,"mid_input mid_input_init register-ret:%d\n",ret);
	ret = mid_mouse_init();
    LOG_UART_DOCK(LOG_DEBUG,"mid_input mid_mouse_init register-ret:%d\n",ret);
	printk("LQ >>> %s  end\n",__func__);
	return 0;
}

static int mid_uart_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}
static int mid_uart_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mid_docking_driver = {
	.probe = mid_docking_probe,
	.driver = {
		   .name = "mid_docking",
		   .owner = THIS_MODULE,
		   .of_match_table = mid_match_table,
		   },
		   
	.suspend = mid_uart_suspend,
	.resume = mid_uart_resume,	   
};
static int __init mid_init(void)

{
	int ret = 0;
	printk("LQ >>> %s  \n",__func__);
	ret = platform_driver_register(&mid_docking_driver);
    LOG_UART_DOCK(LOG_DEBUG,"[MID_DOCKING] mid_docking_init  !!!! \n");	
	if (ret < 0)
		LOG_UART_DOCK(LOG_ERR,"unable to register mid_docking driver.\n");
	LOG_UART_DOCK(LOG_DEBUG," enter\n");

	return 0;

}

static void __exit  mid_exit(void)

{
	LOG_UART_DOCK(LOG_DEBUG,"%s enter\n",__func__);
	//free_irq(muart->dock_in_irq,NULL);
#ifdef MID_SERIO_CONTROL
	serio_unregister_driver(&mid_serio_drv);
#endif	
	LOG_UART_DOCK(LOG_DEBUG,"%s end\n",__func__);
	return;

}

module_init(mid_init);

module_exit(mid_exit);
//module_serio_driver(mid_serio_drv);

MODULE_AUTHOR("kuangzenghui@szroco.com");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
