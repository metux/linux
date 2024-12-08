
#ifndef _IMAP_CSCI_H_
#define	_IMAP_CSCI_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CSCI_MAX_LEN   64
#define CSCI_MAX_COUNT 200

#define CSCI_SIZE_EMBEDDED		0x0002000
#define CSCI_SIZE_NORMAL		0x0004000

#define LINEF0     10
#define LINEF1     13

int csci_init(char *mem, int len);
void csci_list(const char *subset);
int csci_exist(const char *csci);
int csci_equal(const char *csci, const char *value, int section);
int csci_string(char *buf, const char *csci, int section);
int csci_integer(const char *csci, int section);
int csci_export(char *buf, int len);

int csci_string_csci(char *buf, const char *csci, int section);

int init_config(void);

/* not implemented in uboot0 */
//const char *csci_string_find(const char *str);

struct imap_csci {

	char key[CSCI_MAX_LEN];
	char string[CSCI_MAX_LEN];
};

//#define csci_dbg(x...) printf(x)

#define CSCI_EINT  9999
#define CSCI_MAJOR 167
#define CSCI_MAGIC 'i'
#define CSCI_IOCMAX 10
#define CSCI_NAME "csci"
#define CSCI_NODE "/dev/csci"

#define CSCI_EXIST		_IOR(CSCI_MAGIC, 1, unsigned long)
#define CSCI_STRING		_IOR(CSCI_MAGIC, 2, unsigned long)
#define CSCI_INTEGER		_IOR(CSCI_MAGIC, 3, unsigned long)
#define CSCI_EQUAL		_IOR(CSCI_MAGIC, 4, unsigned long)
#define CSCI_CSCI		_IOR(CSCI_MAGIC, 5, unsigned long)
#define CSCI_REINIT		_IOR(CSCI_MAGIC, 6, unsigned long)
#define CSCI_GETTR		_IOR(CSCI_MAGIC, 7, unsigned long)
#define CSCI_LOWBASE		0x3c000000

struct csci_query {

	char   key[CSCI_MAX_LEN];
	char value[CSCI_MAX_LEN];
	char    fb[CSCI_MAX_LEN];
	int    section;
};
  


#ifdef __cplusplus
}
#endif

#endif /* _IMAP_CSCI_H_ */

