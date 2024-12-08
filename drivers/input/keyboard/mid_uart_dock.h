#ifndef _MID_UART_DOCK_H_
#define _MID_UART_DOCK_H_

struct mid_uart_info {
	struct device *dev;
	/*iggig gpio */
	int dock_in_irq;
	struct gpio_desc *dock_in_gpiod;
	struct gpio_desc *dock_bus_gpiod;
	struct delayed_work dock_in_detcable;
	struct serio serio;
};

#define UART_KEY_A  0x04 //30
#define UART_KEY_B  0x05
#define UART_KEY_C  0x06
#define UART_KEY_D  0x07
#define UART_KEY_E	0x08
#define UART_KEY_F	0x09
#define UART_KEY_G  0x0A
#define UART_KEY_H  0x0B
#define UART_KEY_I  0x0C
#define UART_KEY_J  0x0D
#define UART_KEY_K  0x0E
#define UART_KEY_L  0x0F
#define UART_KEY_M  0x10
#define UART_KEY_N  0x11
#define UART_KEY_O  0x12
#define UART_KEY_P  0x13
#define UART_KEY_Q  0x14
#define UART_KEY_R  0x15
#define UART_KEY_S  0x16
#define UART_KEY_T  0x17
#define UART_KEY_U  0x18
#define UART_KEY_V  0x19
#define UART_KEY_W  0x1A
#define UART_KEY_X  0x1B
#define UART_KEY_Y  0x1C
#define UART_KEY_Z  0x1D

#define UART_KEY_1  0x1E
#define UART_KEY_2  0x1F
#define UART_KEY_3  0x20
#define UART_KEY_4  0x21
#define UART_KEY_5  0x22
#define UART_KEY_6  0x23
#define UART_KEY_7  0x24
#define UART_KEY_8  0x25
#define UART_KEY_9  0x26
#define UART_KEY_0  0x27




#define UART_KEY_ENTER		0x28//一致
#define UART_KEY_ESC        0x29
#define UART_KEY_DELETE			0x2A
#define UART_KEY_TAB				0x2B
#define UART_KEY_SPACE			0x2C



//media key and system control key
#define UART_KEY_BACK     	 		0x224
#define UART_KEY_SEARCH  			0x221
#define UART_KEY_INTERNET_BROWSER 	0x196//暂无对应,上报WWW
//#define KEY_WWW 150
#define UART_KEY_EMAIL 				0x18A
#define UART_KEY_MENU 				0x40
//5-6-7-8  ok

//media music
#define UART_KEY_PREVIOUSSONG 			0xB6
#define UART_KEY_PLAYPAUSE				0xCD//暂停/播放
#define UART_KEY_NEXTSONG				0xB5

#define UART_KEY_MUTE 					0xE2
#define UART_KEY_VOLUMEDOWN				0xEA
#define UART_KEY_VOLUMEUP				0xE9
#define UART_KEY_POWER					0x30
#define UART_KEY_HOME					0x223
  
#define UART_KEY_BRIGHTNESS_UP          0x6F
#define UART_KEY_BRIGHTNESS_DOWN        0x70
#define UART_KEY_MUSIC	                0x183

#define MID_KEY_NULL  0x00          // NULL

/* #define UART_KEY_BACK 0x07 
#define UART_KEY_HOME 0x07
#define UART_KEY_MENU 0x07
#define UART_KEY_VOLUMEUP   0x07 
#define UART_KEY_VOLUMEDOWN 0x07
#define UART_KEY_UP    0x07
#define UART_KEY_DOWN  0x07
#define UART_KEY_LEFT  0x07
#define UART_KEY_RIGHT 0x07 */




#endif