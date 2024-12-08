/***************************************************************************** 
** common/oem/config.c
** 
** Copyright (c) 2009~2014 ShangHai Infotm Ltd all rights reserved. 
** 
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** Description: OEM configurations parser.
**
** Author:
**     Warits   <warits.wang@infotm.com>
**      
** Revision History: 
** ----------------- 
** 1.1  XXX 04/01/2012 XXX	draft
*****************************************************************************/



#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
//#include <linux/bootmem.h>
#include <mt-plat/csci.h>
#include <linux/slab.h>

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
//#include <linux/earlysuspend.h>

//#include <linux/hwmsensor.h>
//#include <linux/hwmsen_dev.h>
//#include <linux/sensors_io.h>
#include <linux/proc_fs.h>

//#include <mach/mt_devs.h>
//#include <mach/mt_typedefs.h>
//#include <mach/mt_gpio.h>
//#include <mach/mt_pm_ldo.h>


#define printf(x...) printk(KERN_ERR x)
//#define csci_dbg(x...) printf(x)
#define csci_dbg(x...)

#define strtol simple_strtol
struct imap_csci csci[CSCI_MAX_COUNT];
static char *csci_backup  = NULL, *csci_mem;
static int   csci_backlen = 0;
static char list_buf[CSCI_MAX_COUNT*CSCI_MAX_LEN] = {'\0'};
static int   csci_list_len = 0;
int csci_real_count = 0;
int list_init(char *buf);

#define PARSE_LIMIT 256

inline static const char *
skip_white(const char *s) { int c = 0;
	csci_dbg("\n%s: ", __func__);
	for(; (*s == ' ' || *s == '\t')
			&& c < PARSE_LIMIT; s++, c++)
	  csci_dbg("%c ", *s);
	return (c == PARSE_LIMIT)? NULL: s;
}

inline static const char *
skip_linef(const char *s) { int c = 0;
	for(; (*s == LINEF0 || *s == LINEF1) &&
				c < PARSE_LIMIT; s++, c++)
	  csci_dbg("%c ", *s);
	return (c == PARSE_LIMIT)? NULL: s;
}

inline static const char *
skip_line(const char *s) { int c = 0;
	csci_dbg("\n%s: ", __func__);
	for(; *s != LINEF0 && *s != LINEF1 &&
				c < PARSE_LIMIT; s++, c++)
	  csci_dbg("%c ", *s);
	return (c == PARSE_LIMIT)? NULL: skip_linef(s);
}

inline static const char *
cut_string(char *s) { char *p = s, c = 0;
    for(; *p && *p != '.' && *p != LINEF0 && *p != LINEF1
            && c < CSCI_MAX_LEN; p++, c++);
    *p = 0;
    return s;
}

static const char *__copy_(char *buf, const char *src) 
{
	int c = 0;

	csci_dbg("\n%s: ", __func__);
	for(; (*src != ' ') && (*src != '\t') && (*src != LINEF0) && (*src != LINEF1)&& (c < PARSE_LIMIT);
				src++, buf++, c++) {
		*buf = *src;
		csci_dbg("%c ", *buf);
	}

	*buf = '\0';

	return src;
}

static const char *_parse_line(struct imap_csci *t, const char *s)
{
	s = skip_white(s);
	if(s) s = skip_linef(s);
	if(s && *s != '#') {
		if(strncmp(s, "csci.end", 9) == 0) {
			//while(*s != '\0')/* add by fxi */
			//	s++;
		#if defined(CONFIG_ARM64)
			csci_backlen = (uint64_t)s - (uint64_t)csci_mem + 10;
		#else
			csci_backlen = (uint32_t)s - (uint32_t)csci_mem + 10;
		#endif
			return NULL;
		}
		/* check for key */
		s = __copy_(t->key, s);
		if(s) {
			s = skip_white(s);
			if(*s == LINEF0 || *s == LINEF1)
			  goto skip;
			if(s) s = skip_linef(s);
			/* check for string */
			s = __copy_(t->string, s);
		}
	}
skip:
	s = skip_line(s);
	return s;
}

#if defined(__KERNEL__) && !defined(__U_BOOT__)
static int csci_pos = 0;

int csci_export(char *buf, int len)
{
	int _l;

	if(!buf) {
	    csci_pos = 0;
	    return 0;
	}

	if(csci_pos >= csci_backlen)
	    return 0;

	_l = (len > (csci_backlen - csci_pos))?
	    (csci_backlen - csci_pos): len;

	if(_l) {
	    memcpy(buf, csci_backup + csci_pos, _l);
	    csci_pos += _l;
	}

	return _l;
}
#endif

int csci_init(char *mem, int len)
{
	const char *s = mem;
	int i;
	
	csci_dbg("%s\n", __func__);
	csci_mem    = mem;
	for(i = 0; i < CSCI_MAX_COUNT
				&& s < mem + len;)
	{
		csci[i].key[0] = csci[i].string[0] = 0;
		s = _parse_line(&csci[i], s);

		if(!s) break;
		if(!csci[i].key[0])
		  continue ;

		i++;
	}
	csci_real_count = i; // mid Add
	csci_dbg("%s\n", __func__);

#if 1// defined(__KERNEL__) && !defined(__U_BOOT__)
	if(!csci_backup) {
		csci_backup = kmalloc((len + 0xfff) & ~0xfff,GFP_KERNEL);

		if(!csci_backup)
			printf("failed to allocate memory for backup\n");
		else {
			memcpy(csci_backup, mem, len);
			printf("csci length: %d\n", csci_backlen);
		}
	}
#endif
	list_init(list_buf);
	if(i == CSCI_MAX_COUNT)
	  printf("truncated config csci: %d\n", i);
	return i;
}

void csci_list(const char *subset)
{
	struct imap_csci *t = csci;

	for(; t->key[0]; t++)
	  if(!subset || strncmp(t->key, subset,
		  strnlen(subset, CSCI_MAX_LEN)) == 0)
	#if defined(CONFIG_ARM64)
		printf("%3ld: %-24s  %s\n", (t - csci), t->key, t->string);
	#else
		printf("%3d: %-24s  %s\n", (t - csci), t->key, t->string);
	#endif
}

int list_init(char *buf)
{
	// char buf[CSCI_MAX_LEN*CSCI_MAX_COUNT];
	char tmp[256];
	char *p;
	char *subset = NULL;
	struct imap_csci *t = csci;
	int ret = 0;
	for(; t->key[0]; t++){
		if(!subset || strncmp(t->key, subset,strnlen(subset, CSCI_MAX_LEN)) == 0){
			// printf("%3d: %-24s  %s\n", (t - csci), t->key, t->string);
			sprintf(tmp,"%-24s  %s\n", t->key, t->string);
			ret += strlen(tmp);
			//  printf("ret = %d\n",ret);
			p = strcat(buf,tmp);
		}        	
	}
	strcat(buf,"csci.end\n");
	csci_list_len = strlen(buf);
	return 0;
}
static int list_pos = 0;

int read_list(char __user *buf, int len)
{
#if 0
	int _l;

	if(!buf) {
	    list_pos = 0;
	    return 0;
	}

	if(list_pos >= csci_list_len){
	    return 0;
	}

	_l = (len > (csci_list_len - list_pos))?
	    (csci_list_len - list_pos): len;

	if(_l) {
	    memcpy(buf, list_buf + list_pos, _l);
	    list_pos += _l;
	}

	return _l;
#else
	char tmp[256] = {0};
	int rv = 0;
	if(!buf || list_pos >= csci_real_count) {
	    list_pos = 0;
	    return 0;
	}else{
		if(csci[list_pos].key[0] != '\0'){
			sprintf(tmp,"%s %s\n", csci[list_pos].key, csci[list_pos].string);
			rv = strlen(tmp);
	//		printk("csci: %s \n", tmp);
			//memcpy(buf, tmp, rv);	
			if (copy_to_user(buf,tmp,rv)){
				rv = -EFAULT;
			}	
		}
		list_pos++;
		return rv;
	}	
#endif
}

void add_csci(struct imap_csci *t)
{
	if (csci_real_count < CSCI_MAX_COUNT-1) {
		memset(&csci[csci_real_count -1], 0x00, sizeof(csci[0])*2);
		sprintf(csci[csci_real_count -1].key, "%s", t->key);
		sprintf(csci[csci_real_count -1].string, "%s", t->string);
		sprintf(csci[csci_real_count].key, "%s", "csci.end");
		csci_real_count += 1;

		pr_info("%s: key:%s string:%s \n",__func__, csci[csci_real_count - 2].key, csci[csci_real_count - 2].string);
	}
}

void update_string(struct imap_csci *t)
{
	int i;

	for (i = 0; i < csci_real_count; i++) {
		if ((csci[i].key[0] != '\0')
			&& (strcmp(csci[i].key, t->key) == 0)) {
			memset(csci[i].string, 0x00, CSCI_MAX_LEN);
			sprintf(csci[i].string,"%s", t->string);
			pr_info("%s: key:%s string:%s \n",__func__, csci[i].key, csci[i].string);
			break;
		}
	}

	if (i == csci_real_count) {
		add_csci(t);
	}
}

int csci_exist(const char *key)
{
	struct imap_csci *t = csci;
    int len = strnlen(key, CSCI_MAX_LEN);
	for(; t->key[0]; t++){
        if(strncmp(t->key, key, len) == 0 &&
                    (t->key[len] == 0 || t->key[len] == '.')){
        	return 1;
        }		
	}
	return 0; 
}

EXPORT_SYMBOL(csci_exist);

const char *__string_sec(const char *str, int sec)
{
	const char *p = str;
	int c = 0;

	for(; *p != '\0' && c <= sec; p++) {
		if(*p == '.') {c++; p++; }
		if( c == sec)  {return p;}
	}

	return NULL;
}

int csci_string(char *buf, const char *key, int section)
{
	struct imap_csci *t = csci;
	const char *s;
    int cut = 1;
    if(section < 0){
        section = cut = 0;
    }

	for(; t->key[0]; t++){
	  if(strncmp(t->key, key, CSCI_MAX_LEN) == 0) {
		  s = __string_sec(t->string, section);
		  if(s) {
			  strncpy(buf, s, CSCI_MAX_LEN);
              if(cut){
              	cut_string(buf);
              }
			  return 0;
		  }
	  }		
	}
	return -EFAULT;
}
EXPORT_SYMBOL(csci_string);

int csci_integer(const char *key, int section)
{
	char buf[CSCI_MAX_LEN];
	int ret;

	ret = csci_string(buf, key, section);
	return ret? -CSCI_EINT: strtol(buf, NULL, 10);
}
EXPORT_SYMBOL(csci_integer);

static int notp = 0;
void tp_disable(void) {
	notp = 1;
}
EXPORT_SYMBOL(tp_disable);

int csci_equal(const char *key, const char *value, int section)
{
	char buf[CSCI_MAX_LEN];
	int ret;

	if(notp && strncmp(key, "ts.model", 8) == 0){
		return 0;
	}

	ret = csci_string(buf, key, section);
	return ret? 0: !strncmp(buf, value, CSCI_MAX_LEN);
}

EXPORT_SYMBOL(csci_equal);

int csci_string_csci(char *buf, const char *string, int section)
{
	struct imap_csci *t = csci;
	const char *s;

	for(; t->key[0]; t++){
	  if(strncmp(t->string, string, CSCI_MAX_LEN) == 0) {
		  s = __string_sec(t->key, section);
		  if(s) {
			  strncpy(buf, s, CSCI_MAX_LEN);
			  return 0;
		  }
	  }		
	}
	return -EFAULT;
}

