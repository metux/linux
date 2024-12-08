#ifndef __CUST_DRM_H_
#define __CUST_DRM_H_

#define HIGH                        1
#define LOW                         0



#define LCM_5                       5
#define LCM_6                       6
#define LCM_7                       7
#define LCM_8                       8
#define LCM_84                      84
#define LCM_101                     101
#define LCM_1036                    1036
#define LCM_105                     1050
#define LCM_1095                    1095
#define LCM_1145                    1145
#define LCM_1260                    1260

#ifndef BUILD_LK //Leo 20230904
#define CUST_GPIO_RLED              0
#define CUST_GPIO_GLED              1
#define CUST_GPIO_BLED              2
#define CUST_GPIO_LCM_3V3           3
#define CUST_GPIO_LCM_1V8           4
#define CUST_GPIO_LCM_RST           5
#define CUST_GPIO_OTG_5V_EN         6
#define CUST_GPIO_TYPEC_OTG_EN      7
#define CUST_GPIO_DOCKING_EN        8
#define CUST_GPIO_USB_SWITCH        9
#define CUST_GPIO_EXT_3V3_EN        10


#define CUST_GPIO_AUDIO_MI_EN       11
#define CUST_GPIO_MCU_3V3           12
#define CUST_GPIO_MCU_BOOT          13
#define CUST_GPIO_PSAM_5V           14
#define CUST_GPIO_PSAM_3V3          15
#define CUST_GPIO_LCM_BL_EN         16
#define CUST_GPIO_TP_RST            17
#define CUST_GPIO_BIAS_ENP          18
#define CUST_GPIO_BIAS_POS          19
#define CUST_GPIO_BIAS_NEG          20
#define CUST_GPIO_IT6112_EN         21
#define CUST_GPIO_BIAS_EN           22
#define CUST_GPIO_AUHPR_SPK_SW      23
#define CUST_GPIO_USB_HP_SW         24
#endif

/* video mode */
#define MIPI_DSI_MODE_VIDEO		0x01
/* video burst mode */
#define MIPI_DSI_MODE_VIDEO_BURST	0x02
/* video pulse mode */
#define MIPI_DSI_MODE_VIDEO_SYNC_PULSE	0x04

/* enable auto vertical count mode */
#define MIPI_DSI_MODE_VIDEO_AUTO_VERT	0x08
/* enable hsync-end packets in vsync-pulse and v-porch area */
#define MIPI_DSI_MODE_VIDEO_HSE		0x10
/* disable hfront-porch area */
#define MIPI_DSI_MODE_VIDEO_HFP		0x20
/* disable hback-porch area */
#define MIPI_DSI_MODE_VIDEO_HBP		0x40
/* disable hsync-active area */
#define MIPI_DSI_MODE_VIDEO_HSA		0x80
/* flush display FIFO on vsync pulse */
#define MIPI_DSI_MODE_VSYNC_FLUSH	0x100
/* disable EoT packets in HS mode */
#define MIPI_DSI_MODE_EOT_PACKET	0x200
/* device supports non-continuous clock behavior (DSI spec 5.6.1) */
#define MIPI_DSI_CLOCK_NON_CONTINUOUS	0x400
/* transmit data in low power */
#define MIPI_DSI_MODE_LPM		0x800
/* disable BLLP area */
#define MIPI_DSI_MODE_VIDEO_BLLP	0x1000
/* disable EOF BLLP area */
#define MIPI_DSI_MODE_VIDEO_EOF_BLLP	0x2000

















#endif
