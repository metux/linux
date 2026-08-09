/*
 * Copyright (c) 2016, Fuzhou Rockchip Electronics Co., Ltd
 * Author: Zain Wang <zain.wang@rock-chips.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * Some ideas are from chrome ec and fairchild GPL fusb302 driver.
 */

#ifndef FUSB302_H
#define FUSB302_H

#include <linux/i2c.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

const char *FUSB_DT_INTERRUPT_INTN =	"fsc_interrupt_int_n";
#define FUSB_DT_GPIO_INTN		"fairchild,int_n"
#define FUSB_DT_GPIO_VBUS_5V		"fairchild,vbus5v"
#define FUSB_DT_GPIO_VBUS_OTHER		"fairchild,vbusOther"

#define FUSB30X_I2C_DRIVER_NAME		"fusb302"
#define FUSB30X_I2C_DEVICETREE_NAME	"fairchild,fusb302"

/* FUSB300 Register Addresses */
#define FUSB_REG_DEVICEID		0x01
#define FUSB_REG_SWITCHES0		0x02
#define FUSB_REG_SWITCHES1		0x03
#define FUSB_REG_MEASURE		0x04
#define FUSB_REG_SLICE			0x05
#define FUSB_REG_CONTROL0		0x06
#define FUSB_REG_CONTROL1		0x07
#define FUSB_REG_CONTROL2		0x08
#define FUSB_REG_CONTROL3		0x09
#define FUSB_REG_MASK			0x0A
#define FUSB_REG_POWER			0x0B
#define FUSB_REG_RESET			0x0C
#define FUSB_REG_OCPREG			0x0D
#define FUSB_REG_MASKA			0x0E
#define FUSB_REG_MASKB			0x0F
#define FUSB_REG_CONTROL4		0x10
#define FUSB_REG_STATUS0A		0x3C
#define FUSB_REG_STATUS1A		0x3D
#define FUSB_REG_INTERRUPTA		0x3E
#define FUSB_REG_INTERRUPTB		0x3F
#define FUSB_REG_STATUS0		0x40
#define FUSB_REG_STATUS1		0x41
#define FUSB_REG_INTERRUPT		0x42
#define FUSB_REG_FIFO			0x43

enum connection_state {
	disabled = 0,
	error_recovery,
	unattached, //2
	attach_wait_sink, //3
	attach_wait_source,
	attached_source,
	attached_sink,

	policy_src_startup, //6
	policy_src_send_caps,//7
	policy_src_discovery,
	policy_src_negotiate_cap,//9
	policy_src_cap_response,//10
	policy_src_transition_supply,
	policy_src_transition_default,

	policy_src_ready,
	policy_src_get_sink_caps, //14

	policy_src_send_softrst,
	policy_src_softrst,
	policy_src_send_hardrst, //17

	policy_snk_startup,
	policy_snk_discovery, //19
	policy_snk_wait_caps,//20
	policy_snk_evaluate_caps,//21
	policy_snk_select_cap,//22
	policy_snk_transition_sink,//23
	policy_snk_ready,//24

	policy_snk_send_softrst,//25
	policy_snk_softrst,
	policy_snk_send_hardrst,

	policy_snk_transition_default,

	/* PR SWAP */
	policy_src_prs_evaluate,
	policy_src_prs_accept, //30
	policy_src_prs_transition_to_off,
	policy_src_prs_source_off,
	policy_src_prs_assert_rd,
	policy_src_prs_reject,
	policy_src_prs_send_swap,//35

	policy_snk_prs_evaluate,
	policy_snk_prs_accept,
	policy_snk_prs_transition_to_off,
	policy_snk_prs_source_on,
	policy_snk_prs_assert_rp,//40
	policy_snk_prs_reject,
	policy_snk_prs_send_swap,

	/* VC SWAP */
	policy_vcs_dfp_send_swap,
	policy_vcs_dfp_wait_for_ufp_vconn,
	policy_vcs_dfp_turn_off_vconn,//45
	policy_vcs_dfp_turn_on_vconn,
	policy_vcs_dfp_send_ps_rdy,

	policy_vcs_ufp_evaluate_swap,
	policy_vcs_ufp_reject,
	policy_vcs_ufp_accept,//50
	policy_vcs_ufp_wait_for_dfp_vconn,
	policy_vcs_ufp_turn_off_vconn,
	policy_vcs_ufp_turn_on_vconn,
	policy_vcs_ufp_send_ps_rdy,

	policy_drs_ufp_evaluate,//55
	policy_drs_ufp_accept,
	policy_drs_ufp_reject,
	policy_drs_ufp_change,
	policy_drs_ufp_send_swap,

	policy_drs_dfp_evaluate,//60
	policy_drs_dfp_accept,
	policy_drs_dfp_reject,
	policy_drs_dfp_change,
	policy_drs_dfp_send_swap,

	attach_try_src,//65
	attach_try_snk,

	attach_wait_audio_acc,
	attached_audio_acc,

	policy_snk_get_pps_status,//69
	policy_max,
};

char connection_state_chr[policy_max + 1][128] = {
	"disabled",
	"error_recovery",
	"unattached", //2
	"attach_wait_sink", //3
	"attach_wait_source",
	"attached_source",
	"attached_sink",

	"policy_src_startup", //6
	"policy_src_send_caps",//7
	"policy_src_discovery",
	"policy_src_negotiate_cap",//9
	"policy_src_cap_response",//10
	"policy_src_transition_supply",
	"policy_src_transition_default",

	"policy_src_ready",
	"policy_src_get_sink_caps", //14

	"policy_src_send_softrst",
	"policy_src_softrst",
	"policy_src_send_hardrst", //17
	
	"policy_snk_startup",
	"policy_snk_discovery", //19
	"policy_snk_wait_caps",//20
	"policy_snk_evaluate_caps",//21
	"policy_snk_select_cap",//22
	"policy_snk_transition_sink",//23
	"policy_snk_ready",//24
	
	"policy_snk_send_softrst",//25
	"policy_snk_softrst",
	"policy_snk_send_hardrst",
	"policy_snk_transition_default",

	/* PR SWAP */
	"policy_src_prs_evaluate",
	"policy_src_prs_accept",
	"policy_src_prs_transition_to_off",
	"policy_src_prs_source_off",
	"policy_src_prs_assert_rd",
	"policy_src_prs_reject",
	"policy_src_prs_send_swap",

	"policy_snk_prs_evaluate",
	"policy_snk_prs_accept",
	"policy_snk_prs_transition_to_off",
	"policy_snk_prs_source_on",
	"policy_snk_prs_assert_rp",
	"policy_snk_prs_reject",
	"policy_snk_prs_send_swap",

	"policy_vcs_dfp_send_swap",
	"policy_vcs_dfp_wait_for_ufp_vconn",
	"policy_vcs_dfp_turn_off_vconn",
	"policy_vcs_dfp_turn_on_vconn",
	"policy_vcs_dfp_send_ps_rdy",

	"policy_vcs_ufp_evaluate_swap",
	"policy_vcs_ufp_reject",
	"policy_vcs_ufp_accept",
	"policy_vcs_ufp_wait_for_dfp_vconn",
	"policy_vcs_ufp_turn_off_vconn",
	"policy_vcs_ufp_turn_on_vconn",
	"policy_vcs_ufp_send_ps_rdy",

	"policy_drs_ufp_evaluate",
	"policy_drs_ufp_accept",
	"policy_drs_ufp_reject",
	"policy_drs_ufp_change",
	"policy_drs_ufp_send_swap",

	"policy_drs_dfp_evaluate",
	"policy_drs_dfp_accept",
	"policy_drs_dfp_reject",
	"policy_drs_dfp_change",
	"policy_drs_dfp_send_swap",

	"attach_try_src",
	"attach_try_snk",

	"attach_wait_audio_acc",
	"attached_audio_acc",
	
	"policy_snk_get_pps_status",
};


enum vdm_state {
	VDM_STATE_DISCOVERY_ID,
	VDM_STATE_DISCOVERY_SVID,
	VDM_STATE_DISCOVERY_MODES,
	VDM_STATE_ENTER_MODE,
	VDM_STATE_UPDATE_STATUS,
	VDM_STATE_DP_CONFIG,
	VDM_STATE_NOTIFY,
	VDM_STATE_READY,
	VDM_STATE_ERR,
};

enum tcpm_rp_value {
	TYPEC_RP_USB = 0,
	TYPEC_RP_1A5 = 1,
	TYPEC_RP_3A0 = 2,
	TYPEC_RP_RESERVED = 3,
};

enum role_mode {
	ROLE_MODE_NONE,
	ROLE_MODE_DRP,
	ROLE_MODE_UFP,
	ROLE_MODE_DFP,
	ROLE_MODE_ASS,
};

#define SBF(s, v)		((s) << (v))
#define SWITCHES0_PDWN1		SBF(1, 0)
#define SWITCHES0_PDWN2		SBF(1, 1)
#define SWITCHES0_MEAS_CC1	SBF(1, 2)
#define SWITCHES0_MEAS_CC2	SBF(1, 3)
#define SWITCHES0_VCONN_CC1	SBF(1, 4)
#define SWITCHES0_VCONN_CC2	SBF(1, 5)
#define SWITCHES0_PU_EN1	SBF(1, 6)
#define SWITCHES0_PU_EN2	SBF(1, 7)

#define SWITCHES1_TXCC1		SBF(1, 0)
#define SWITCHES1_TXCC2		SBF(1, 1)
#define SWITCHES1_AUTO_CRC	SBF(1, 2)
#define SWITCHES1_DATAROLE	SBF(1, 4)
#define SWITCHES1_SPECREV	SBF(3, 5)
#define SWITCHES1_POWERROLE	SBF(1, 7)

#define MEASURE_MDAC		SBF(0x3f, 0)
#define MEASURE_VBUS		SBF(1, 6)

#define SLICE_SDAC		SBF(0x3f, 0)
#define SLICE_SDAC_HYS		SBF(3, 6)

#define CONTROL0_TX_START	SBF(1, 0)
#define CONTROL0_AUTO_PRE	SBF(1, 1)
#define CONTROL0_HOST_CUR	SBF(3, 2)
#define CONTROL0_HOST_CUR_USB		SBF(1, 2)
#define CONTROL0_HOST_CUR_1A5		SBF(2, 2)
#define CONTROL0_HOST_CUR_3A0		SBF(3, 2)
#define CONTROL0_INT_MASK	SBF(1, 5)
#define CONTROL0_TX_FLUSH	SBF(1, 6)

#define CONTROL1_ENSOP1		SBF(1, 0)
#define CONTROL1_ENSOP2		SBF(1, 1)
#define CONTROL1_RX_FLUSH	SBF(1, 2)
#define CONTROL1_BIST_MODE2	SBF(1, 4)
#define CONTROL1_ENSOP1DB	SBF(1, 5)
#define CONTROL1_ENSOP2DB	SBF(1, 6)

#define CONTROL2_TOGGLE		SBF(1, 0)
#define CONTROL2_MODE		SBF(3, 1)
#define CONTROL2_MODE_NONE	0
#define CONTROL2_MODE_DFP	SBF(3, 1)
#define CONTROL2_MODE_UFP	SBF(2, 1)
#define CONTROL2_MODE_DRP	SBF(1, 1)
#define CONTROL2_WAKE_EN	SBF(1, 3)
#define CONTROL2_TOG_RD_ONLY	SBF(1, 5)
#define CONTROL2_TOG_SAVE_PWR1	SBF(1, 6)
#define CONTROL2_TOG_SAVE_PWR2	SBF(1, 7)

#define CONTROL3_AUTO_RETRY	SBF(1, 0)
#define CONTROL3_N_RETRIES	SBF(3, 1)
#define CONTROL3_AUTO_SOFTRESET	SBF(1, 3)
#define CONTROL3_AUTO_HARDRESET	SBF(1, 4)
#define CONTROL3_SEND_HARDRESET	SBF(1, 6)

#define MASK_M_BC_LVL		SBF(1, 0)
#define MASK_M_COLLISION	SBF(1, 1)
#define MASK_M_WAKE		SBF(1, 2)
#define MASK_M_ALERT		SBF(1, 3)
#define MASK_M_CRC_CHK		SBF(1, 4)
#define MASK_M_COMP_CHNG	SBF(1, 5)
#define MASK_M_ACTIVITY		SBF(1, 6)
#define MASK_M_VBUSOK		SBF(1, 7)

#define POWER_PWR		SBF(0xf, 0)

#define RESET_SW_RESET		SBF(1, 0)
#define RESET_PD_RESET		SBF(1, 1)

#define MASKA_M_HARDRST		SBF(1, 0)
#define MASKA_M_SOFTRST		SBF(1, 1)
#define MASKA_M_TXSENT		SBF(1, 2)
#define MASKA_M_HARDSENT	SBF(1, 3)
#define MASKA_M_RETRYFAIL	SBF(1, 4)
#define MASKA_M_SOFTFAIL	SBF(1, 5)
#define MASKA_M_TOGDONE		SBF(1, 6)
#define MASKA_M_OCP_TEMP	SBF(1, 7)

#define MASKB_M_GCRCSEND	SBF(1, 0)

#define CONTROL4_TOG_USRC_EXIT	SBF(1, 0)

#define MDAC_1P6V		0x26

#define STATUS0A_HARDRST	SBF(1, 0)
#define STATUS0A_SOFTRST	SBF(1, 1)
#define STATUS0A_POWER23	SBF(3, 2)
#define STATUS0A_RETRYFAIL	SBF(1, 4)
#define STATUS0A_SOFTFAIL	SBF(1, 5)
#define STATUS0A_TOGDONE	SBF(1, 6)
#define STATUS0A_M_OCP_TEMP	SBF(1, 7)

#define STATUS1A_RXSOP		SBF(1, 0)
#define STATUS1A_RXSOP1DB	SBF(1, 1)
#define STATUS1A_RXSOP2DB	SBF(1, 2)
#define STATUS1A_TOGSS		SBF(7, 3)
#define CC_STATE_TOGSS_CC1	SBF(1, 0)
#define CC_STATE_TOGSS_CC2	SBF(1, 1)
#define CC_STATE_TOGSS_IS_UFP	SBF(1, 2)

#define CC_STATE_TOGSS_IS_DFP	SBF(2, 2)
#define CC_STATE_TOGSS_IS_ACC	SBF(3, 2)
#define CC_STATE_TOGSS_ROLE	SBF(3, 2)

#define INTERRUPTA_HARDRST	SBF(1, 0)
#define INTERRUPTA_SOFTRST	SBF(1, 1)
#define INTERRUPTA_TXSENT	SBF(1, 2)
#define INTERRUPTA_HARDSENT	SBF(1, 3)
#define INTERRUPTA_RETRYFAIL	SBF(1, 4)
#define INTERRUPTA_SOFTFAIL	SBF(1, 5)
#define INTERRUPTA_TOGDONE	SBF(1, 6)
#define INTERRUPTA_OCP_TEMP	SBF(1, 7)

#define INTERRUPTB_GCRCSENT	SBF(1, 0)

#define STATUS0_BC_LVL		SBF(3, 0)
#define STATUS0_WAKE		SBF(1, 2)
#define STATUS0_ALERT		SBF(1, 3)
#define STATUS0_CRC_CHK		SBF(1, 4)
#define STATUS0_COMP		SBF(1, 5)
#define STATUS0_ACTIVITY	SBF(1, 6)
#define STATUS0_VBUSOK		SBF(1, 7)

#define STATUS1_OCP		SBF(1, 0)
#define STATUS1_OVRTEMP		SBF(1, 1)
#define STATUS1_TX_FULL		SBF(1, 2)
#define STATUS1_TX_EMPTY	SBF(1, 3)
#define STATUS1_RX_FULL		SBF(1, 4)
#define STATUS1_RX_EMPTY	SBF(1, 5)
#define STATUS1_RXSOP1		SBF(1, 6)
#define STATUS1_RXSOP2		SBF(1, 7)

#define INTERRUPT_BC_LVL	SBF(1, 0)
#define INTERRUPT_COLLISION	SBF(1, 1)
#define INTERRUPT_WAKE		SBF(1, 2)
#define INTERRUPT_ALERT		SBF(1, 3)
#define INTERRUPT_CRC_CHK	SBF(1, 4)
#define INTERRUPT_COMP_CHNG	SBF(1, 5)
#define INTERRUPT_ACTIVITY	SBF(1, 6)
#define INTERRUPT_VBUSOK	SBF(1, 7)

#define FUSB_TKN_TXON		0xa1
#define FUSB_TKN_SYNC1		0x12
#define FUSB_TKN_SYNC2		0x13
#define FUSB_TKN_SYNC3		0x1b
#define FUSB_TKN_RST1		0x15
#define FUSB_TKN_RST2		0x16
#define FUSB_TKN_PACKSYM	0x80
#define FUSB_TKN_JAMCRC		0xff
#define FUSB_TKN_EOP		0x14
#define FUSB_TKN_TXOFF		0xfe

/* USB PD Control Message Types */
#define CONTROLMESSAGE		0
#define CMT_GOODCRC		1
#define CMT_GOTOMIN		2
#define CMT_ACCEPT		3
#define CMT_REJECT		4
#define CMT_PING		5
#define CMT_PS_RDY		6
#define CMT_GETSOURCECAP	7
#define CMT_GETSINKCAP		8
#define CMT_DR_SWAP		9
#define CMT_PR_SWAP		10
#define CMT_VCONN_SWAP		11
#define CMT_WAIT		12
#define CMT_SOFTRESET		13
//Leo 20230614
#define CMT_DATARESET				14
#define CMT_DATARESET_COMPLETE		15
#define CMT_NOT_SUPPORT				16
#define CMT_GET_SOURCECAP_EXT		17
#define CMT_GETSTATUS				18
#define CMT_FRSWAP					19
#define CMT_GET_PPS_STATUS			20
#define CMT_GET_COUNTRY_CODE		21
#define CMT_GET_SINKCAP_EXT			22

/* USB PD Data Message Types */
#define DATAMESSAGE		1
#define DMT_SOURCECAPABILITIES	1
#define DMT_REQUEST		2
#define DMT_BIST		3
#define DMT_SINKCAPABILITIES	4
#define DMT_VENDERDEFINED	15


/* Extened Message Types*/
enum Extended_Message_Type {
	EMT_RESERVED = 0,
	EMT_SOURCE_CAPABILITES_EXT = 1,
	EMT_STATUS = 2,
	EMT_GET_BATTERY_CAP = 3,
	EMT_GET_BATTERY_STATUS = 4,
	EMT_BATTERY_CAPABILITES = 5,
	EMT_GET_MENUFACTURER_INFO = 6,
	EMT_MENUFACTURER_INFO = 7,
	EMT_SECURITY_REQUEST = 8,
	EMT_SECURITY_RESPONSE = 9,
	EMT_FIRMWARE_UPDATE_REQUEST = 10,
	EMT_FIRMWARE_UPDATE_RESPONSE = 11,
	EMT_PPS_STATUS = 12,
	EMT_COUNTRY_INFO = 13,
	EMT_COUNTRY_CODE = 14,
	EMT_SINK_CAPABILITES_EXT = 15,
	/*RESERVED*/
};


/* VDM Command Types */
#define VDM_DISCOVERY_ID	0X01
#define VDM_DISCOVERY_SVIDS	0X02
#define VDM_DISCOVERY_MODES	0X03
#define VDM_ENTER_MODE		0X04
#define VDM_EXIT_MODE		0X05
#define VDM_ATTENTION		0X06
#define VDM_DP_STATUS_UPDATE	0X10
#define VDM_DP_CONFIG		0X11

#define VDM_TYPE_INIT		0
#define VDM_TYPE_ACK		1
#define VDM_TYPE_NACK		2
#define VDM_TYPE_BUSY		3

/* 200ms at least, 1 cycle about 6ms */
#define N_DEBOUNCE_CNT		33
#define N_CAPS_COUNT		50
#define N_HARDRESET_COUNT	0

#define T_NO_RESPONSE		5000
#define T_SRC_RECOVER		830
#define T_TYPEC_SEND_SOURCECAP	100
#define T_SENDER_RESPONSE	30
#define T_SRC_TRANSITION	30
#define T_TYPEC_SINK_WAIT_CAP	500
#define T_PS_TRANSITION		500
#define T_BMC_TIMEOUT		5
#define T_PS_HARD_RESET_MAX	35
#define T_SAFE_0V		650
#define T_SRC_TURN_ON		275
#define T_SRC_RECOVER_MAX	1000
#define T_PD_SOURCE_OFF		920
#define T_PD_SOURCE_ON		480
#define T_PD_SWAP_SOURCE_START	20
#define T_PD_VCONN_SRC_ON	100
#define T_PD_TRY_DRP		75

#define T_NO_TRIGGER		500
#define T_DISABLED		0xffff

#define PD_30_SUPPORT //Leo 20230615

#define PD_HEADER_CNT(header)		(((header) >> 12) & 7)
#if defined(PD_30_SUPPORT) //Leo 20230615
#define PD_HEADER_TYPE(header)		((header) & 0x1F)
#else
#define PD_HEADER_TYPE(header)		((header) & 0xF)
#endif
#define PD_HEADER_ID(header)		(((header) >> 9) & 7)

#define VDM_HEADER_TYPE(header)		(((header) >> 6) & 3)
#define VDMHEAD_CMD_TYPE_MASK		(3 << 6)
#define VDMHEAD_CMD_MASK		(0x1f << 0)
#define VDMHEAD_STRUCT_TYPE_MASK	BIT(15)

#define GET_VDMHEAD_CMD_TYPE(head)	((head & VDMHEAD_CMD_TYPE_MASK) >> 6)
#define GET_VDMHEAD_CMD(head)		(head & VDMHEAD_CMD_MASK)
#define GET_VDMHEAD_STRUCT_TYPE(head)	((head & VDMHEAD_STRUCT_TYPE_MASK) >> 15)

#define DP_STATUS_MASK			0x000000ff
#define DP_STATUS_HPD_STATE		BIT(7)

#define GET_DP_STATUS(status)		(status & DP_STATUS_MASK)
#define GET_DP_STATUS_HPD(status)	((status & DP_STATUS_HPD_STATE) >> 7)

#define VDM_IDHEAD_USBVID_MASK		(0xffff << 0)
#define VDM_IDHEAD_MODALSUPPORT_MASK	BIT(26)
#define VDM_IDHEAD_PRODUCTTYPE		(7 << 27)
#define VDM_IDHEAD_USBDEVICE		BIT(30)
#define VDM_IDHEAD_USBHOST		BIT(30)

#define CAP_POWER_TYPE(PDO)		((PDO >> 30) & 3)
#define CAP_FPDO_VOLTAGE(PDO)		((PDO >> 10) & 0x3ff)
#define CAP_VPDO_VOLTAGE(PDO)		((PDO >> 20) & 0x3ff)
#define CAP_FPDO_CURRENT(PDO)		((PDO >> 0) & 0x3ff)
#define CAP_VPDO_CURRENT(PDO)		((PDO >> 0) & 0x3ff)



#if 1 //Leo 20230530

#define PDO_TYPE_FIXED    (0 << 30)
#define PDO_TYPE_BATTERY  (1 << 30)
#define PDO_TYPE_VARIABLE (2 << 30)
#define PDO_TYPE_APDO	(3 << 30)
#define PDO_TYPE_MASK     (3 << 30) //mask

#define PDO_FIXED_DUAL_ROLE (1 << 29) /* Dual role device */
#define PDO_FIXED_SUSPEND   (1 << 28) /* USB Suspend supported (SRC)*/
#define PDO_FIXED_HIGH_CAP	(1 << 28) /* Higher Capability (SNK )*/
#define PDO_FIXED_EXTERNAL  (1 << 27) /* Externally powered */
#define PDO_FIXED_COMM_CAP  (1 << 26) /* USB Communications Capable */
#define PDO_FIXED_DATA_SWAP (1 << 25) /* Data role swap command supported */

#define PDO_FIXED_PEAK_CURR(i) \
	((i & 0x03) << 20) /* [21..20] Peak current */
#define PDO_FIXED_VOLT(mv)  \
	((((mv)/50) & 0x3ff) << 10) /* Voltage in 50mV units */
#define PDO_FIXED_CURR(ma)  \
	((((ma)/10) & 0x3ff) << 0)  /* Max current in 10mA units */

#define PDO_TYPE(raw)	(raw & PDO_TYPE_MASK)
#define PDO_TYPE_VAL(raw)	(PDO_TYPE(raw) >> 30)

//Leo FPDO
#define PDO_FIXED_EXTRACT_VOLT_RAW(raw)	(((raw) >> 10) & 0x3ff)
#define PDO_FIXED_EXTRACT_CURR_RAW(raw)	(((raw) >> 0) & 0x3ff)

#define PDO_FIXED_EXTRACT_VOLT(raw)	(PDO_FIXED_EXTRACT_VOLT_RAW(raw) * 50)
#define PDO_FIXED_EXTRACT_CURR(raw)	(PDO_FIXED_EXTRACT_CURR_RAW(raw) * 10)
#define PDO_FIXED_RESET_CURR(raw, ma)	\
	((raw & ~0x3ff) | PDO_FIXED_CURR(ma))

//PDO_FIXED
#define PDO_FIXED(mv, ma, flags) (PDO_FIXED_VOLT(mv) |\
				  PDO_FIXED_CURR(ma) | (flags))

#define PDO_VAR_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_VAR_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_VAR_OP_CURR(ma)  ((((ma) / 10) & 0x3FF) << 0)

#define PDO_VAR_EXTRACT_MAX_VOLT_RAW(raw)	(((raw) >> 20) & 0x3ff)
#define PDO_VAR_EXTRACT_MIN_VOLT_RAW(raw)	(((raw) >> 10) & 0x3ff)
#define PDO_VAR_EXTRACT_CURR_RAW(raw)		(((raw) >> 0) & 0x3ff)

#define PDO_VAR_EXTRACT_MAX_VOLT(raw)	(PDO_VAR_EXTRACT_MAX_VOLT_RAW(raw) * 50)
#define PDO_VAR_EXTRACT_MIN_VOLT(raw)	(PDO_VAR_EXTRACT_MIN_VOLT_RAW(raw) * 50)
#define PDO_VAR_EXTRACT_CURR(raw)	(PDO_VAR_EXTRACT_CURR_RAW(raw) * 10)

#define PDO_VAR_RESET_CURR(raw, ma)	\
	((raw & ~0x3ff) | PDO_VAR_OP_CURR(ma))

//PDO_VAR
#define PDO_VAR(min_mv, max_mv, op_ma) \
				(PDO_VAR_MIN_VOLT(min_mv) | \
				 PDO_VAR_MAX_VOLT(max_mv) | \
				 PDO_VAR_OP_CURR(op_ma)   | \
				 PDO_TYPE_VARIABLE)

#define PDO_BATT_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_BATT_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_BATT_OP_POWER(mw) ((((mw) / 250) & 0x3FF) << 0)

#define PDO_BATT_EXTRACT_MAX_VOLT_RAW(raw)	(((raw) >> 20) & 0x3ff)
#define PDO_BATT_EXTRACT_MIN_VOLT_RAW(raw)	(((raw) >> 10) & 0x3ff)
#define PDO_BATT_EXTRACT_OP_POWER_RAW(raw)	(((raw) >> 0) & 0x3ff)

#define PDO_BATT_EXTRACT_MAX_VOLT(raw)	\
	(PDO_BATT_EXTRACT_MAX_VOLT_RAW(raw) * 50)
#define PDO_BATT_EXTRACT_MIN_VOLT(raw)	\
	(PDO_BATT_EXTRACT_MIN_VOLT_RAW(raw) * 50)
#define PDO_BATT_EXTRACT_OP_POWER(raw)	\
	(PDO_BATT_EXTRACT_OP_POWER_RAW(raw) * 250)

//PDO_BATT
#define PDO_BATT(min_mv, max_mv, op_mw) \
				(PDO_BATT_MIN_VOLT(min_mv) | \
				 PDO_BATT_MAX_VOLT(max_mv) | \
				 PDO_BATT_OP_POWER(op_mw) | \
				 PDO_TYPE_BATTERY)

#define PDO_TYPE(raw)	(raw & PDO_TYPE_MASK)
#define PDO_TYPE_VAL(raw)	(PDO_TYPE(raw) >> 30)
#define PDO_TYPE_FIXED    (0 << 30)
#define PDO_TYPE_BATTERY  (1 << 30)
#define PDO_TYPE_VARIABLE (2 << 30)
#define PDO_TYPE_APDO	(3 << 30)
#define PDO_TYPE_MASK     (3 << 30)
#define APDO_TYPE_MASK		(3 << 28)
#define APDO_TYPE_PPS		(0 << 28)

#define APDO_TYPE(raw)	(raw & APDO_TYPE_MASK)
#define APDO_TYPE_VAL(raw)	(APDO_TYPE(raw) >> 28)

#define APDO_PPS_CURR_FOLDBACK	(1<<26)
#define APDO_PPS_MAX_VOLT(mv) ((((mv) / 100) & 0xff) << 17)
#define APDO_PPS_MIN_VOLT(mv) ((((mv) / 100) & 0xff) << 8)
#define APDO_PPS_CURR(ma) ((((ma) / 50) & 0x7f) << 0)


#define APDO_PPS_EXTRACT_MAX_VOLT_RAW(raw)	(((raw) >> 17) & 0xff)
#define APDO_PPS_EXTRACT_MIN_VOLT_RAW(raw)	(((raw) >> 8) & 0Xff)
#define APDO_PPS_EXTRACT_CURR_RAW(raw)	(((raw) >> 0) & 0x7f)
#define APDO_PPS_EXTRACT_PWR_LIMIT(raw)        ((raw >> 27) & 0x1)

#define APDO_PPS_EXTRACT_MAX_VOLT(raw)	\
	(APDO_PPS_EXTRACT_MAX_VOLT_RAW(raw) * 100)
#define APDO_PPS_EXTRACT_MIN_VOLT(raw)	\
	(APDO_PPS_EXTRACT_MIN_VOLT_RAW(raw) * 100)
#define APDO_PPS_EXTRACT_CURR(raw)	\
	(APDO_PPS_EXTRACT_CURR_RAW(raw) * 50)


//Leo add for set current 20230531
#define APDO_PPS_OBJ_POS_RAW(raw)  ((raw & 0x7) << 28)
#define APDO_PPS_SET_VOLT_RAW(raw) ((((raw) / 20) & 0x7ff) << 9)
#define APDO_PPS_SET_CURR_RAW(raw) (((raw) / 50) & 0x7f)

//APDO_PPS
#define APDO_PPS(min_mv, max_mv, ma, flags)	\
	(APDO_PPS_MIN_VOLT(min_mv)	 | \
	APDO_PPS_MAX_VOLT(max_mv) | \
	APDO_PPS_CURR(ma) | \
	flags | PDO_TYPE_APDO | APDO_TYPE_PPS)

#define PD_EXT_HEADER_DATA_SIZE(header)	(((header) >> 0) & 0x1FF)
#endif


enum CC_ORIENTATION {
	TYPEC_ORIENTATION_NONE,
	TYPEC_ORIENTATION_CC1,
	TYPEC_ORIENTATION_CC2,
};

enum typec_cc_polarity {
	TYPEC_POLARITY_CC1,
	TYPEC_POLARITY_CC2,
};

enum CC_MODE {
	CC_PULL_UP,
	CC_PULL_DOWN,
	CC_PULL_NONE,
};

enum typec_power_role {
	POWER_ROLE_SINK = 0,
	POWER_ROLE_SOURCE,
};

enum typec_data_role {
	DATA_ROLE_UFP = 0,
	DATA_ROLE_DFP,
};

struct notify_info {
	enum CC_ORIENTATION orientation;
	/* 0 UFP : 1 DFP */
	enum typec_power_role power_role;
	enum typec_data_role data_role;

	bool is_cc_connected;
	bool is_pd_connected;
	int is_pps_support;

	bool is_enter_mode;
	int pin_assignment_support;
	int pin_assignment_def;
	bool attention;
	u32 dp_status;
	u32 dp_caps;
};

enum tx_state {
	tx_idle,
	tx_busy,
	tx_failed,
	tx_success
};

struct PD_CAP_INFO {
	u32 peak_current;
	u32 specification_revision;
	u32 externally_powered;
	u32 usb_suspend_support;
	u32 usb_communications_cap;
	u32 dual_role_power;
	u32 data_role_swap;
	u32 supply_type;
};

#define FUSB302_ADAPTER_CAP_MAX_NR 10

#if 1 //Leo 20230609
struct tcpm_power_cap_val {
    uint8_t type;
    uint8_t apdo_type;
    uint8_t pwr_limit;

    int max_mv;
    int min_mv;

    union {
        int uw;
        int ma;
    };
};

struct dpm_pdo_info_t {
	uint8_t type;
	uint8_t apdo_type;
	uint8_t pwr_limit;
	int vmin;
	int vmax;
	int uw;
	int ma;
};

enum tcpm_power_cap_apdo_type {
        TCPM_POWER_CAP_APDO_TYPE_PPS = 1 << 0,
        TCPM_POWER_CAP_APDO_TYPE_PPS_CF = (1 << 7),
};

enum tcpm_power_cap_val_type {
        TCPM_POWER_CAP_VAL_TYPE_FIXED = 0, 
        TCPM_POWER_CAP_VAL_TYPE_BATTERY = 1, 
        TCPM_POWER_CAP_VAL_TYPE_VARIABLE = 2, 
        TCPM_POWER_CAP_VAL_TYPE_AUGMENT = 3, 

        TCPM_POWER_CAP_VAL_TYPE_UNKNOWN = 0xff,
};

#define PD_DATA_OBJ_SIZE		(7)
#define PDO_MAX_NR				PD_DATA_OBJ_SIZE
#define TCPM_APDO_TYPE_MASK		(0x0f)

struct tcpm_power_cap {
	uint8_t cnt;
	uint32_t pdos[PDO_MAX_NR];
};

struct tcpm_remote_power_cap {
	uint8_t selected_cap_idx;
	uint8_t nr;
	int max_mv[PDO_MAX_NR];
	int min_mv[PDO_MAX_NR];
	int ma[PDO_MAX_NR];
	uint8_t type[PDO_MAX_NR];
};

#define DPM_PDO_TYPE_FIXED      TCPM_POWER_CAP_VAL_TYPE_FIXED
#define DPM_PDO_TYPE_VAR        TCPM_POWER_CAP_VAL_TYPE_VARIABLE
#define DPM_PDO_TYPE_BAT        TCPM_POWER_CAP_VAL_TYPE_BATTERY
#define DPM_PDO_TYPE_APDO       TCPM_POWER_CAP_VAL_TYPE_AUGMENT

#define DPM_APDO_TYPE_PPS       (TCPM_POWER_CAP_APDO_TYPE_PPS)
#define DPM_APDO_TYPE_PPS_CF    (TCPM_POWER_CAP_APDO_TYPE_PPS_CF)

enum tcp_dpm_return_code {
	TCP_DPM_RET_SUCCESS = 0,
	TCP_DPM_RET_SENT = 0,
	TCP_DPM_RET_VDM_ACK = 0,

	TCP_DPM_RET_DENIED_UNKNOWN,
	TCP_DPM_RET_DENIED_NOT_READY,
	TCP_DPM_RET_DENIED_LOCAL_CAP,
	TCP_DPM_RET_DENIED_PARTNER_CAP,
	TCP_DPM_RET_DENIED_SAME_ROLE,
	TCP_DPM_RET_DENIED_INVALID_REQUEST,
	TCP_DPM_RET_DENIED_REPEAT_REQUEST,
	TCP_DPM_RET_DENIED_WRONG_DATA_ROLE,
	TCP_DPM_RET_DENIED_PD_REV,

	TCP_DPM_RET_DROP_CC_DETACH,
	TCP_DPM_RET_DROP_SENT_SRESET,
	TCP_DPM_RET_DROP_RECV_SRESET,
	TCP_DPM_RET_DROP_SENT_HRESET,
	TCP_DPM_RET_DROP_RECV_HRESET,
	TCP_DPM_RET_DROP_ERROR_REOCVERY,
	TCP_DPM_RET_DROP_SEND_BIST,
	TCP_DPM_RET_DROP_PE_BUSY,	/* SinkTXNg*/
	TCP_DPM_RET_DROP_DISCARD,
	TCP_DPM_RET_DROP_UNEXPECTED,

	TCP_DPM_RET_WAIT,
	TCP_DPM_RET_REJECT,
	TCP_DPM_RET_TIMEOUT,
	TCP_DPM_RET_VDM_NAK,
	TCP_DPM_RET_NOT_SUPPORT,

	TCP_DPM_RET_BK_TIMEOUT,
	TCP_DPM_RET_NO_RESPONSE,

	TCP_DPM_RET_NR,
};

enum tcpm_error_list {
	TCPM_SUCCESS = 0,
	TCPM_ERROR_UNKNOWN = -1,
	TCPM_ERROR_UNATTACHED = -2,
	TCPM_ERROR_PARAMETER = -3,
	TCPM_ERROR_PUT_EVENT = -4,
	TCPM_ERROR_NO_SUPPORT = -5,
	TCPM_ERROR_NO_PD_CONNECTED = -6,
	TCPM_ERROR_NO_POWER_CABLE = -7,
	TCPM_ERROR_NO_PARTNER_INFORM = -8,
	TCPM_ERROR_NO_SOURCE_CAP = -9,
	TCPM_ERROR_NO_SINK_CAP = -10,
	TCPM_ERROR_NOT_DRP_ROLE = -11,
	TCPM_ERROR_DURING_ROLE_SWAP = -12,
	TCPM_ERROR_NO_EXPLICIT_CONTRACT = -13,
	TCPM_ERROR_ERROR_RECOVERY = -14,
	TCPM_ERROR_NOT_FOUND = -15,
	TCPM_ERROR_INVALID_POLICY = -16,
	TCPM_ERROR_EXPECT_CB2 = -17,
	TCPM_ERROR_POWER_ROLE = -18,
	TCPM_ERROR_PE_NOT_READY = -19,
	TCPM_ERROR_REPEAT_POLICY = -20,
	TCPM_ERROR_CUSTOM_SRC = -21,
	TCPM_ERROR_NO_IMPLEMENT = -22,
	TCPM_ALERT = -23,
};

/* Inquire TCPM status */

enum tcpc_cc_voltage_status {
	TYPEC_CC_VOLT_OPEN = 0,
	TYPEC_CC_VOLT_RA = 1,
	TYPEC_CC_VOLT_RD = 2,

	TYPEC_CC_VOLT_SNK_DFT = 5,
	TYPEC_CC_VOLT_SNK_1_5 = 6,
	TYPEC_CC_VOLT_SNK_3_0 = 7,

	TYPEC_CC_DRP_TOGGLING = 15,
};

enum tcpm_vbus_level {
#ifdef CONFIG_TCPC_VSAFE0V_DETECT
	TCPC_VBUS_SAFE0V = 0,	/* < 0.8V */
	TCPC_VBUS_INVALID,		/* > 0.8V */
	TCPC_VBUS_VALID,		/* > 4.5V */
#else
	TCPC_VBUS_INVALID = 0,
	TCPC_VBUS_VALID,
#endif /* CONFIG_TCPC_VSAFE0V_DETECT */
};

enum typec_role_defination {
	TYPEC_ROLE_UNKNOWN = 0,
	TYPEC_ROLE_SNK,
	TYPEC_ROLE_SRC,
	TYPEC_ROLE_DRP,
	TYPEC_ROLE_TRY_SRC,
	TYPEC_ROLE_TRY_SNK,
	TYPEC_ROLE_NR,
};

enum pd_cable_current_limit {
	PD_CABLE_CURR_UNKNOWN = 0,
	PD_CABLE_CURR_1A5 = 1,
	PD_CABLE_CURR_3A = 2,
	PD_CABLE_CURR_5A = 3,
};

enum typec_attach_type {
	TYPEC_UNATTACHED = 0,
	TYPEC_ATTACHED_SNK,
	TYPEC_ATTACHED_SRC,
	TYPEC_ATTACHED_AUDIO,
	TYPEC_ATTACHED_DEBUG,			/* Rd, Rd */

/* CONFIG_TYPEC_CAP_DBGACC_SNK */
	TYPEC_ATTACHED_DBGACC_SNK,		/* Rp, Rp */

/* CONFIG_TYPEC_CAP_CUSTOM_SRC */
	TYPEC_ATTACHED_CUSTOM_SRC,		/* Same Rp */

/* CONFIG_TYPEC_CAP_NORP_SRC */
	TYPEC_ATTACHED_NORP_SRC,		/* No Rp */
};
#endif

struct fusb30x_chip {
	struct i2c_client *client;
	struct device *dev;
	struct regmap *regmap;
	struct work_struct work;
	struct workqueue_struct *fusb30x_wq;
	struct hrtimer timer_state_machine;
	struct hrtimer timer_mux_machine;
	struct PD_CAP_INFO pd_cap_info;
	struct notify_info notify;
	struct notify_info notify_cmp;
	struct extcon_dev *extcon;
	enum connection_state conn_state;
	struct gpio_desc *gpio_vbus_5v;
	struct gpio_desc *gpio_vbus_other;
	struct gpio_desc *gpio_int;
	struct gpio_desc *gpio_discharge;
	struct gpio_desc *gpio_iddig;
	int timer_state;
	int timer_mux;
	int port_num;
	u32 work_continue;
	spinlock_t irq_lock;
	int gpio_int_irq;
	int enable_irq;
	struct delayed_work request_irq_work;
	struct workqueue_struct *fusb302_irq_wq;

	/*
	 * ---------------------------------
	 * | role 0x03 << 2, | cc_use 0x03 |
	 * | src  1 << 2,    | cc1 1       |
	 * | snk  2 << 2,    | cc2 2       |
	 * ---------------------------------
	 */
	u8 cc_state;
	int cc1;
	int cc2;
	enum typec_cc_polarity cc_polarity;
	u8 val_tmp;
	u8 debounce_cnt;
	int sub_state;
	int caps_counter;
	u32 send_load[7];
	u32 rec_load[7];
	u16 send_head;
	u16 rec_head;
	int msg_id;
	enum tx_state tx_state;
	int hardrst_count;
	u32 source_power_supply[7];
	/* 50mv unit */
	u32 source_max_current[7];
	/* 10ma uint*/
	int pos_power;
	/*
	 * if PartnerCap[0] == 0xffffffff
	 * show Partner Device do not support supply
	 */
	u32 partner_cap[7];
	int n_caps_used;
	int vdm_state;
	int vdm_substate;
	int vdm_send_state;
	u16 vdm_svid[12];
	int vdm_svid_num;
	u32 vdm_id;
	u8 chip_id;
	bool vconn_enabled;
	bool is_pd_support;
	int pd_output_vol;
	int pd_output_cur;
	int cc_meas_high;
	int cc_meas_low;
	bool vbus_begin;

	enum role_mode role;
	bool vconn_supported;
	bool try_role_complete;
	enum role_mode try_role;
	struct input_dev *input;
	bool suspended;
#if 1 //Leo 20230530
	u32 apdo_cap[7];
	struct dpm_pdo_info_t pdo_info[7];
	int typec_attach_new;
	int typec_attach_old;
	int max_vol;
	int max_cur;
	
	int selected_cap_idx;
	int nr;
	uint8_t pdp;
	uint8_t pwr_limit[FUSB302_ADAPTER_CAP_MAX_NR];
	int pps_vmin;
	int pps_vmax;
	int pps_ma;
	int ma[FUSB302_ADAPTER_CAP_MAX_NR];
	int mv[FUSB302_ADAPTER_CAP_MAX_NR];
	int maxwatt[FUSB302_ADAPTER_CAP_MAX_NR];
	int minwatt[FUSB302_ADAPTER_CAP_MAX_NR];
	uint8_t type[FUSB302_ADAPTER_CAP_MAX_NR];
	int info[FUSB302_ADAPTER_CAP_MAX_NR];
	int event_type;

	wait_queue_head_t  pd_task_que;
	struct power_supply  *bat_psy;
	int fast_stop_soc;
	struct task_struct *pd_charger_task;
	struct mutex pd_charger_lock;
#endif
};

#endif /* FUSB302_H */

