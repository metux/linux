
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/scatterlist.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/user_namespace.h>
#include <linux/mutex.h>
#include <crypto/hash.h>
#include <crypto/sha.h>

/*
 * Plan9 /dev/caphash and /dev/capuse device
 *
 * 2DO: - caphash should only allow one process (per userns)
 *      - support textual user names
 *      - invalidate old caps
 */

#define DEVICE_CAPUSE	"/dev/capuse"
#define DEVICE_CAPHASH	"/dev/caphash"

#define MODNAME		"p9cap"

struct caphash_entry {
	struct list_head list;
	struct user_namespace *user_ns;
	char data[SHA1_DIGEST_SIZE];
};

struct caphash_writer {
	struct list_head list;
	struct user_namespace *user_ns;
};

static dev_t caphash_devid = 0;
static dev_t capuse_devid = 0;

static LIST_HEAD(caphash_entries);
static LIST_HEAD(caphash_writers);

static DEFINE_MUTEX(p9cap_lock);

struct crypto_ahash *p9cap_tfm = NULL;

static int caphash_open(struct inode *inode, struct file *filp)
{
	struct caphash_writer *tmp = NULL;
	struct user_namespace *user_ns = current_user_ns();
	int retval = 0;
	struct list_head *pos, *q;

	/* make sure only one instance per namespace can be opened */
	mutex_lock(&p9cap_lock);

	list_for_each_safe(pos, q, &(caphash_writers)) {
		tmp = list_entry(pos, struct caphash_writer, list);
		if (tmp->user_ns == user_ns) {
			printk(KERN_ERR DEVICE_CAPHASH ": already locked in this namespace\n");
			retval = -EBUSY;
			goto out;
		}
	}

	if (!(tmp = kzalloc(sizeof(struct caphash_writer), GFP_KERNEL))) {
		retval = -ENOMEM;
		goto out;
	}

	tmp->user_ns = get_user_ns(user_ns);
	list_add(&(tmp->list), &caphash_writers);

out:
	mutex_unlock(&p9cap_lock);
	return retval;
}

static int caphash_release(struct inode *inode, struct file *filp)
{
	int retval = 0;
	struct user_namespace *user_ns = current_user_ns();
	struct list_head *pos, *q;
	struct caphash_entry *tmp;

	mutex_lock(&p9cap_lock);

	list_for_each_safe(pos, q, &(caphash_writers)) {
		tmp = list_entry(pos, struct caphash_entry, list);
		if (tmp->user_ns == user_ns) {
			list_del(pos);
			kfree(tmp);
			goto out;
		}
	}

out:
	mutex_unlock(&p9cap_lock);
	return retval;
}

static ssize_t caphash_write(struct file *filp, const char __user *buf,
				   size_t count, loff_t *f_pos)
{
	struct caphash_entry *ent;

	if (count > SHA1_DIGEST_SIZE) {
		printk(KERN_ERR DEVICE_CAPHASH ": too large: %d\n", count);
		return -E2BIG;
	}

	if (!(ent = kzalloc(sizeof(struct caphash_entry), GFP_KERNEL)))
		return -ENOMEM;

	if (copy_from_user(&(ent->data), buf, count)) {
		kfree(ent);
		return -EFAULT;
	}

	ent->user_ns = get_user_ns(current_user_ns());

	mutex_lock(&p9cap_lock);
	list_add(&(ent->list), &caphash_entries);
	mutex_unlock(&p9cap_lock);

	return count;
}

/* called w/ lock held. we can releave this by allocating tfm locally */
static ssize_t hash(const char *src, const char* dst, const char *key, u8 *result)
{
	struct scatterlist sg;
	struct ahash_request *req;
	int retval;
	char *text = NULL;
	size_t text_len;
	int digest_len;
	u8* digest = NULL;

	text_len = strlen(src)+strlen(dst)+1;		/* src@dst\0 */
	digest_len = crypto_ahash_reqsize(p9cap_tfm);

	digest = kzalloc(digest_len, GFP_KERNEL);
	text = kzalloc(text_len+1, GFP_KERNEL);

	if (!digest || !text) {
		retval = -ENOMEM;
		goto out;
	}

	if (!(req = ahash_request_alloc(p9cap_tfm, GFP_KERNEL))) {
		printk(KERN_ERR MODNAME ": failed to alloc ahash_request\n");
		retval = -ENOMEM;
		goto out;
	}

	snprintf(text, text_len+1, "%s@%s", src, dst);
	sg_set_buf(&sg, text, text_len);

	ahash_request_set_callback(req, 0, NULL, NULL);
	ahash_request_set_crypt(req, &sg, digest, text_len);

	if ((retval = crypto_ahash_setkey(p9cap_tfm, key, strlen(key)))) {
		printk(KERN_ERR MODNAME ": crypto_ahash_setkey() failed ret=%d\n", retval);
		goto out;
	}

	if ((retval = crypto_ahash_digest(req))) {
		printk(KERN_ERR MODNAME ": digest() failed ret=%d\n", retval);
		goto out;
	}

	memcpy(result, digest, SHA1_DIGEST_SIZE);

out:
	kfree(text);
	kfree(digest);

	return 0;
}

static inline kuid_t convert_uid(const char* uname)
{
	return make_kuid(current_user_ns(), simple_strtol(uname, NULL, 0));
}

static ssize_t switch_uid(const char *src_uname, const char *dst_uname)
{
	struct cred *creds = prepare_creds();

	kuid_t src_uid = convert_uid(src_uname);
	kuid_t dst_uid = convert_uid(dst_uname);

	if (!uid_eq(src_uid, current_uid())) {
		printk(KERN_INFO DEVICE_CAPUSE ": src uid mismatch\n");
		return -EPERM;
	}

	if (!(creds = prepare_creds()))
		return -ENOMEM;

	creds->uid = dst_uid;
	creds->euid = dst_uid;

	printk(KERN_INFO DEVICE_CAPUSE ": switching from kuid %d to %d\n", src_uid.val, dst_uid.val);
	return commit_creds(creds);
}

static ssize_t try_switch(const char* src_uname, const char* dst_uname, const u8* hashval)
{
	struct list_head *pos;
	list_for_each(pos, &(caphash_entries)) {
		struct caphash_entry *tmp = list_entry(pos, struct caphash_entry, list);
		if ((0 == memcmp(hashval, tmp->data, SHA1_DIGEST_SIZE)) &&
		    (tmp->user_ns == current_user_ns())) {

			int retval;

			if ((retval = switch_uid(src_uname, dst_uname))) {
				printk(KERN_INFO DEVICE_CAPUSE ": uid switch failed\n");
				return retval;
			}

			tmp = list_entry(pos, struct caphash_entry, list);
			list_del(pos);
			put_user_ns(tmp->user_ns);
			kfree(tmp);

			return 0;
		}
	}

	printk(KERN_INFO DEVICE_CAPUSE ": cap not found\n");

	return -ENOENT;
}

static ssize_t capuse_write(struct file *filp, const char __user *buf,
				  size_t count, loff_t *f_pos)
{
	ssize_t retval = count;
	char  *rand_str, *src_uname, *dst_uname;
	u8 hashval[SHA1_DIGEST_SIZE] = { 0 };
	char *cmdbuf;

	if (!(cmdbuf = kzalloc(count, GFP_KERNEL)))
		return -ENOMEM;

	if (copy_from_user(cmdbuf, buf, count)) {
		retval = -EFAULT;
		goto out_free;
	}

	{
		char *walk = cmdbuf;
		src_uname = strsep(&walk, "@");
		dst_uname = strsep(&walk, "@");
		rand_str = walk;
		if (!src_uname || !dst_uname || !rand_str) {
			retval = -EINVAL;
			goto out_free;
		}
	}

	mutex_lock(&p9cap_lock);

	if ((retval = hash(src_uname, dst_uname, rand_str, hashval)))
		goto out_unlock;

	if ((retval = try_switch(src_uname, dst_uname, hashval)))
		goto out_unlock;

	retval = count;

out_unlock:
	mutex_unlock(&p9cap_lock);

out_free:
	kfree(cmdbuf);
	return retval;
}

static const struct file_operations p9cap_caphash_fops = {
	.owner		= THIS_MODULE,
	.write		= caphash_write,
	.open		= caphash_open,
	.release	= caphash_release,
};

static const struct file_operations p9cap_capuse_fops = {
	.owner		= THIS_MODULE,
	.write		= capuse_write,
};

static struct cdev p9cap_dev_caphash;
static struct cdev p9cap_dev_capuse;

static int p9cap_clear(void)
{
	struct caphash_entry *tmp;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &(caphash_entries)) {
		tmp = list_entry(pos, struct caphash_entry, list);
		list_del(pos);
		kfree(tmp);
	}

	return 0;
}

static void p9cap_cleanup_module(void)
{
	p9cap_clear();

	cdev_del(&p9cap_dev_caphash);
	cdev_del(&p9cap_dev_capuse);

	unregister_chrdev_region(caphash_devid, 1);
	unregister_chrdev_region(capuse_devid, 1);

	if (p9cap_tfm)
		crypto_free_ahash(p9cap_tfm);
}

static int p9cap_init_module(void)
{
	int retval;

	p9cap_tfm = crypto_alloc_ahash("hmac(sha1)", 0, CRYPTO_ALG_ASYNC);
	if (IS_ERR(p9cap_tfm)) {
		retval = -PTR_ERR(p9cap_tfm);
		printk(KERN_ERR MODNAME ": failed to load transform for hmac(sha1): %d\n", retval);
		goto fail;
	}

	if ((retval = alloc_chrdev_region(&caphash_devid, 0, 1, DEVICE_CAPHASH)))
		goto fail;

	if ((retval = alloc_chrdev_region(&capuse_devid, 0, 1, DEVICE_CAPUSE)))
		goto fail;

	cdev_init(&p9cap_dev_caphash, &p9cap_caphash_fops);
	p9cap_dev_caphash.owner = THIS_MODULE;
	if ((retval = cdev_add(&p9cap_dev_caphash, caphash_devid, 1)))
		printk(KERN_ERR MODNAME ": failed adding " DEVICE_CAPHASH ": %d\n", retval);

	cdev_init(&p9cap_dev_capuse, &p9cap_capuse_fops);
	p9cap_dev_capuse.owner = THIS_MODULE;
	if ((retval = cdev_add(&p9cap_dev_capuse, capuse_devid, 1)))
		printk(KERN_ERR MODNAME ": failed adding " DEVICE_CAPUSE ": %d\n", retval);

	return 0;

fail:
	p9cap_cleanup_module();
	return retval;
}

MODULE_AUTHOR("Enrico Weigelt, metux IT consult <info@metux.net>");
MODULE_LICENSE("GPLv3");

module_init(p9cap_init_module);
module_exit(p9cap_cleanup_module);
