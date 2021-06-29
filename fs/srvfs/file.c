
#include "srvfs.h"

#include <asm/atomic.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>

static int srvfs_file_open(struct inode *inode, struct file *file)
{
	struct srvfs_fileref *fileref = inode->i_private;
	pr_info("open inode_id=%ld\n", inode->i_ino);
	file->private_data = srvfs_fileref_get(fileref);

	if (fileref->file) {
		pr_info("open inode: already assigned another file\n");
		get_file_rcu(fileref->file);
		file->boxed_file = fileref->file;
	}
	else {
		pr_info("open inode: no file assigned yet\n");
	}

	return 0;
}

static int srvfs_file_release(struct inode *inode, struct file *file)
{
	struct srvfs_fileref *fileref = file->private_data;
	pr_info("closing vanilla control file: inode_id=%ld\n", inode->i_ino);
	srvfs_fileref_put(fileref);
	return 0;
}

static int do_switch(struct file *file, long fd)
{
	struct srvfs_fileref *fileref= file->private_data;
	struct file *newfile = fget(fd);
	pr_info("doing the switch: fd=%ld\n", fd);

	if (!newfile) {
		pr_info("invalid fd passed\n");
		goto setref;
	}

	if (newfile->f_inode == file->f_inode) {
		pr_err("whoops. trying to link inode with itself!\n");
		goto loop;
	}

	if (newfile->f_inode->i_sb == file->f_inode->i_sb) {
		pr_err("whoops. trying to link inode within same fs!\n");
		goto loop;
	}

	pr_info("assigning inode %ld\n", newfile->f_inode->i_ino);
	if (newfile && newfile->f_path.dentry)
		pr_info("target inode fn: %s\n", newfile->f_path.dentry->d_name.name);
	else
		pr_info("target inode fn unknown\n");

setref:
	srvfs_fileref_set(fileref, newfile);
	return 0;

loop:
	fput(newfile);
	return -ELOOP;
}

static ssize_t srvfs_file_write(struct file *file, const char *buf,
				size_t count, loff_t *offset)
{
	char tmp[20];
	long fd;
	int ret;

	if ((*offset != 0) || (count >= sizeof(tmp)))
		return -EINVAL;

	memset(tmp, 0, sizeof(tmp));
	if (copy_from_user(tmp, buf, count))
		return -EFAULT;

	fd = simple_strtol(tmp, NULL, 10);
	ret = do_switch(file, fd);

	if (ret)
		return ret;

	return count;
}

struct file_operations srvfs_file_ops = {
	.owner		= THIS_MODULE,
	.open		= srvfs_file_open,
	.write		= srvfs_file_write,
	.release	= srvfs_file_release,
};

int srvfs_insert_file(struct super_block *sb, struct dentry *dentry)
{
	struct inode *inode;
	struct srvfs_fileref *fileref;
	int mode = S_IFREG | S_IWUSR | S_IRUGO;

	fileref = srvfs_fileref_new();
	if (!fileref)
		goto nomem;

	inode = new_inode(sb);
	if (!inode)
		goto err_inode;

	atomic_set(&fileref->counter, 0);

	inode_init_owner(&init_user_ns, inode, sb->s_root->d_inode, mode);

	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
	inode->i_fop = &srvfs_file_ops;
	inode->i_ino = srvfs_inode_id(inode->i_sb);
	inode->i_private = fileref;

	pr_info("new inode id: %ld\n", inode->i_ino);

	d_drop(dentry);
	d_add(dentry, inode);
	return 0;

err_inode:
	srvfs_fileref_put(fileref);

nomem:
	return -ENOMEM;
}
