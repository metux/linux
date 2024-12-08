/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_RPMB_H_
#define _MTK_RPMB_H_

/**********************************************************
 * Function Declaration                                   *
 **********************************************************/
#if IS_ENABLED(CONFIG_RPMB)
int mmc_rpmb_register(struct mmc_host *mmc);


#if defined(CONFIG_TRUSTKERNEL_TEE_RPMB_SUPPORT)

#define RPMB_DATA_FRAME_SIZE    512u
#define RPMB_DATA_BUFFER_SIZE   (RPMB_DATA_FRAME_SIZE * 8)

#define RPMB_IOCTL_TKCORE_WRITE_DATA    10
#define RPMB_IOCTL_TKCORE_READ_DATA     11
#define RPMB_IOCTL_TKCORE_GET_CNT       12
#define RPMB_IOCTL_TKCORE_GET_WR_SIZE   13

struct rpmb_request {
    unsigned int nr;
    unsigned int rel_wr_sec_c;
    unsigned char *data_frame;
};

#endif

#else
//int mmc_rpmb_register(...);
#endif

#endif
