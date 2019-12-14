/*
 * This file contains internal functions and definitions for the VFS
 * that are shared between multiple files in the fs/ directory.
 */
#ifndef _FS_INTERNAL_H
#define _FS_INTERNAL_H

#include <linux/dcache.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/list.h>

/* dentry.c */
extern void __d_free(struct dentry *dentry);
extern void __d_drop(struct dentry *dentry);
extern void __d_instantiate(struct dentry *dentry, struct inode *inode);
extern void __d_entry_unhash(struct dentry *dentry);
extern int __d_rehash(struct dentry *dentry);
extern struct dentry *d_hash(struct dentry *dentry, const struct qstr *name);
extern struct dentry *d_alloc(struct dentry *parent, const struct qstr *name);
extern struct dentry *d_alloc_name(struct dentry *parent, const char *name);
extern void d_invalidate(struct dentry *dentry);
extern int d_delete(struct dentry *dentry);
extern void d_rehash(struct dentry *entry);
extern void d_add(struct dentry *entry, struct inode *inode);
extern struct dentry *d_splice_alias(struct inode *inode, struct dentry *dentry);
extern struct dentry *d_materialise_unique(struct dentry *dentry, struct inode *inode);
extern void d_prune_aliases(struct inode *inode);
extern void guard_bio_eod(struct bio *bio);
extern int d_set_mounted(struct dentry *dentry);
extern long prune_dcache_sb(struct super_block *sb, struct shrink_control *sc);
extern struct dentry *d_alloc_cursor(struct dentry *);
extern struct dentry * d_alloc_pseudo(struct super_block *, const struct qstr *);
extern char *simple_dname(struct dentry *, char *, int);
extern void dput_to_list(struct dentry *, struct list_head *);
extern void shrink_dentry_list(struct list_head *);

/* file.c */
extern struct file *__dentry_open(struct dentry *, struct vfsmount *, struct file *);
extern void __filemap_set_wb_err(struct address_space *mapping, int err);
extern int __filemap_check_wb_err(struct address_space *mapping);
extern int __generic_file_open(struct inode *, struct file *);
extern int generic_file_open(struct inode *, struct file *);
extern void finish_open(struct file *file, struct dentry *dentry, int (*open)(struct inode *, struct file *), int *opened);
extern int finish_no_open(struct file *file, struct dentry *dentry);

/* read_write.c */
extern ssize_t __vfs_read(struct file *, char __user *, size_t, loff_t *);
extern ssize_t __vfs_write(struct file *, const char __user *, size_t, loff_t *);
extern ssize_t __vfs_readv(struct file *, const struct iovec __user *, unsigned long, loff_t *);
extern ssize_t __vfs_writev(struct file *, const struct iovec __user *, unsigned long, loff_t *);
extern ssize_t vfs_copy_file_range(struct file *, loff_t, struct file *, loff_t, size_t, unsigned int);
extern ssize_t generic_file_read_iter(struct kiocb *, struct iov_iter *);
extern ssize_t generic_file_write_iter(struct kiocb *, struct iov_iter *);
extern ssize_t generic_perform_write(struct file *, struct iov_iter *, loff_t);
extern ssize_t generic_write_checks(struct kiocb *, struct iov_iter *);
extern ssize_t iter_file_splice_write(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);

/* inode.c */
extern struct inode *new_inode(struct super_block *sb);
extern struct inode *new_inode_pseudo(struct super_block *sb);
extern void free_inode_nonrcu(struct inode *inode);
extern void inode_init_once(struct inode *inode);
extern void inode_init_always(struct super_block *sb, struct inode *inode);
extern void inode_sb_list_add(struct inode *inode);
extern void iput(struct inode *inode);
extern int generic_delete_inode(struct inode *inode);
extern int generic_drop_inode(struct inode *inode);
extern void inode_hashtable_init(void);
extern void inode_add_lru(struct inode *inode);
extern void inode_lru_list_add(struct inode *inode);
extern void inode_lru_list_del(struct inode *inode);
extern struct inode *inode_insert5(struct inode *inode, unsigned long hashval,
				    int (*test)(struct inode *, void *), void *data);
extern struct inode *igrab(struct inode *inode);
extern struct inode *ilookup5_nowait(struct super_block *sb, unsigned long hashval,
				     int (*test)(struct inode *, void *), void *data);
extern struct inode *ilookup5(struct super_block *sb, unsigned long hashval,
			      int (*test)(struct inode *, void *), void *data);
extern struct inode *ilookup(struct super_block *sb, unsigned long ino);
extern struct inode *find_inode(struct super_block *sb, unsigned long hashval,
				int (*test)(struct inode *, void *), void *data);
extern void __insert_inode_hash(struct inode *inode, unsigned long hashval);
extern void remove_inode_hash(struct inode *inode);
extern void evict_inodes(struct super_block *sb);
extern void invalidate_inodes(struct super_block *sb);
extern int can_nocow_extent(struct inode *inode, loff_t offset, size_t len, int *nocow);
extern int filemap_write_and_wait(struct address_space *mapping);
extern int filemap_write_and_wait_range(struct address_space *mapping,
					loff_t lstart, loff_t lend);
extern int __filemap_fdatawrite_range(struct address_space *mapping,
				      loff_t start, loff_t end, int sync_mode);
extern int filemap_fdatawrite(struct address_space *mapping);
extern int filemap_fdatawrite_range(struct address_space *mapping,
				    loff_t start, loff_t end);
extern int filemap_fdatawait_range(struct address_space *mapping,
				   loff_t start, loff_t end);
extern int filemap_fdatawait(struct address_space *mapping);
extern int wait_on_page_writeback_range(struct address_space *mapping,
					loff_t start, loff_t end);
extern int wait_on_page_writeback_range_RA(struct address_space *mapping,
					   loff_t start, loff_t end);
extern int filemap_check_errors(struct address_space *mapping);
extern int filemap_check_errors_after_write(struct address_space *mapping);
extern int invalidate_inode_pages2(struct address_space *mapping);
extern int invalidate_inode_pages2_range(struct address_space *mapping,
					 loff_t start, loff_t end);
extern int invalidate_inode_pages_range(struct address_space *mapping,
					loff_t start, loff_t end);
extern int truncate_inode_pages(struct address_space *mapping, loff_t lstart);
extern int truncate_inode_pages_range(struct address_space *mapping,
				      loff_t lstart, loff_t lend);
extern int truncate_inode_pages_final(struct address_space *mapping);
extern int sync_inode(struct inode *inode, struct writeback_control *wbc);
extern int sync_inode_metadata(struct inode *inode, int wait);
extern int write_inode_now(struct inode *inode, int sync);
extern int mark_inode_dirty(struct inode *inode);
extern void __mark_inode_dirty(struct inode *inode, int flags);
extern int inode_needs_sync(struct inode *inode);
extern int __set_page_dirty_nobuffers(struct page *page);
extern int set_page_dirty(struct page *page);
extern int set_page_dirty_lock(struct page *page);
extern void __set_page_dirty(struct page *page);
extern int filemap_dirty_folio(struct address_space *mapping, struct folio *folio);
extern void account_page_dirtied(struct page *page, struct address_space *mapping);
extern int redirty_page_for_writepage(struct writeback_control *wbc, struct page *page);

/* super.c */
extern void __super_block_add(struct super_block *sb);
extern void generic_shutdown_super(struct super_block *sb);
extern void __invalidate_super_block(struct super_block *sb);
extern int super_setup_bdi_name(struct super_block *sb, char *name);
extern struct super_block *sget(struct file_system_type *type,
				int (*test)(struct super_block *, void *),
				int (*set)(struct super_block *, void *),
				int flags, void *data);
extern struct super_block *sget_userns(struct file_system_type *type,
				       int (*test)(struct super_block *, void *),
				       int (*set)(struct super_block *, void *),
				       int flags, void *data,
				       struct user_namespace *user_ns);
extern void super_block_init(struct super_block *sb, struct file_system_type *type,
			     int (*test)(struct super_block *, void *),
			     int (*set)(struct super_block *, void *),
			     int flags, void *data);
extern int get_sb_bdev(struct file_system_type *type, int flags,
		       const char *dev, void *data, struct super_block **res);
extern int get_sb_bdev_excl(struct file_system_type *type, int flags,
			    const char *dev, void *data, struct super_block **res);
extern int get_sb_nodev(struct file_system_type *type, int flags,
			void *data, struct super_block **res);
extern int get_sb_single(struct file_system_type *type, int flags,
			 void *data, struct super_block **res);
extern int get_sb_pseudo(struct file_system_type *type, int flags,
			 void *data, struct super_block **res);
extern int sb_prepare_to_die(struct super_block *sb);
extern void deactivate_locked_super(struct super_block *sb);
extern void deactivate_super(struct super_block *sb);
extern void iterate_supers(void (*f)(struct super_block *, void *), void *arg);
extern void iterate_supers_type(struct file_system_type *type,
				void (*f)(struct super_block *, void *), void *arg);

/* mount.c */
extern struct vfsmount *vfs_kern_mount(struct file_system_type *type,
				       int flags, const char *name, void *data);
extern void mntput(struct vfsmount *mnt);
extern struct vfsmount *mntget(struct vfsmount *mnt);
extern int vfs_path_lookup(struct dentry *dentry, struct vfsmount *mnt,
			   const char *name, unsigned int flags,
			   struct path *path);
extern int path_parent_directory(struct dentry *dentry, struct path *path);
extern int mount_capable(struct vfsmount *mnt);
extern int vfs_mountroot(struct super_block *sb);
extern void mnt_drop_write(struct vfsmount *mnt);
extern int mnt_want_write(struct vfsmount *mnt);

/* namespace.c */
extern struct vfsmount *get_mnt_ns(struct nsproxy *ns);
extern struct nsproxy *switch_mnt_ns(struct nsproxy *to);
extern void mnt_destroy(struct mnt_namespace *ns);
extern int __init mnt_init(void);
extern int sys_unshare(unsigned long flags);

/* dcache.c */
extern void __d_dentry_rcu_walk(struct dentry *dentry, struct rcu_head *head,
				void (*func)(struct dentry *, struct rcu_head *));
extern void d_walk(struct dentry *parent, void *data,
		   int (*enter)(void *, struct dentry *),
		   int (*finish)(void *, struct dentry *));
extern int __d_lookup_rcu(struct dentry *parent, struct qstr *name,
			  struct dentry **dentry);
extern int d_lookup_rcu(struct dentry *parent, struct qstr *name,
			struct dentry **dentry);

/* file_table.c */
extern int files_init(void);
extern void files_deinit(void);
extern void files_free(struct files_struct *files);
extern int files_stat_fd(struct files_struct *files, struct kstat *stat);
extern int files_fstat_fd(struct files_struct *files, int fd, struct kstat *stat);

/* select.c */
extern int do_select(int n, fd_set __user *inp, fd_set __user *outp,
		     fd_set __user *exp, struct timeval __user *tvp);

/* mq.c */
extern void mq_clear_clocks(void);

/* pid.c */
extern void pid_destroy(struct pid *pid);
extern int pid_getattr(struct vfsmount *mnt, struct dentry *dentry,
		       struct kstat *stat);
extern int pid_revalidate(struct dentry *dentry, unsigned int flags);
extern int pid_dentry_delete(struct dentry *dentry);

/* proc.c */
extern void proc_init(void);
extern void proc_deinit(void);

/* sysfs.c */
extern int sysfs_create_dir(struct kobject *kobj);
extern void sysfs_remove_dir(struct kobject *kobj);
extern int sysfs_create_file(struct kobject *kobj, const struct attribute *attr);
extern void sysfs_remove_file(struct kobject *kobj, const struct attribute *attr);
extern int sysfs_create_link(struct kobject *kobj, struct kobject *target,
			     const char *name);
extern void sysfs_remove_link(struct kobject *kobj, const char *name);

/* block_dev.c */
extern int blkdev_get(struct block_device *bdev, fmode_t mode, void *holder);
extern int blkdev_put(struct block_device *bdev, fmode_t mode);
extern int __blkdev_get(struct block_device *bdev, fmode_t mode, int for_partition,
			void *holder);
extern int __blkdev_put(struct block_device *bdev, fmode_t mode, int for_partition);
extern int blkdev_issue_zeroout(struct block_device *bdev, sector_t sector,
				sector_t nr_sects, gfp_t gfp_mask);
extern int blkdev_issue_discard(struct block_device *bdev, sector_t sector,
				sector_t nr_sects, gfp_t gfp_mask);
extern int blkdev_issue_flush(struct block_device *bdev, gfp_t gfp_mask);

/* fs-writeback.c */
extern long do_blockdev_direct_IO(struct kiocb *, struct iov_iter *);
extern void generic_write_sync(struct kiocb *iocb);

/* char_dev.c */
extern int chrdev_open(struct inode *, struct file *);
extern int chrdev_release(struct inode *, struct file *);

/* char_dev.c - cdev functions */
extern int cdev_add(struct cdev *, dev_t, unsigned);
extern void cdev_del(struct cdev *);
extern struct cdev *cdev_alloc(void);
extern void cdev_init(struct cdev *, const struct file_operations *);
extern struct cdev *cdev_default(void);

/* dmap.c */
extern int dmap_init(void);

/* dma.c */
extern void dma_direct_map_page(struct device *dev, struct page *page,
				unsigned long offset, size_t size,
				enum dma_data_direction dir,
				struct dma_attrs *attrs);

/* fsnotify.c */
extern void __fsnotify_update_child_dentry_flags(struct dentry *dentry);
extern void fsnotify(struct inode *inode, __u32 mask, void *obj, int type,
		     const char *name, int isdir);
extern void fsnotify_inode(struct inode *inode, __u32 mask, void *obj,
			   int type, const char *name, int isdir);
extern void fsnotify_mkdir(struct inode *inode, struct dentry *dentry);
extern void fsnotify_create(struct inode *inode, struct dentry *dentry);
extern void fsnotify_access(struct inode *inode, struct dentry *dentry);
extern void fsnotify_modify(struct inode *inode, struct dentry *dentry);
extern void fsnotify_open(struct inode *inode, struct dentry *dentry);
extern void fsnotify_close(struct inode *inode, struct dentry *dentry);
extern void fsnotify_move(struct inode *inode, struct dentry *old_dentry,
			  struct dentry *new_dentry);
extern void fsnotify_link(struct inode *inode, struct dentry *dentry);
extern void fsnotify_unlink(struct inode *inode, struct dentry *dentry);
extern void fsnotify_unmount(struct vfsmount *mnt);

/* fanotify.c */
extern int fanotify_init(struct fanotify_data *fd, unsigned int flags);
extern int fanotify_mark(struct fanotify_data *fd, unsigned int mask,
			 unsigned int flags, struct fsnotify_mark *mark,
			 struct path *path, unsigned int mask);

/* timerfd.c */
extern int timerfd_create(struct file *file, struct timerfd_ctx *ctx);

/* eventfd.c */
extern int eventfd_signal(struct eventfd_ctx *ctx, __u64 n);
extern int eventfd_release(struct file *file);

/* inotify.c */
extern int inotify_init1(int flags);
extern int inotify_add_watch(int fd, const char *pathname, uint32_t mask);
extern int inotify_rm_watch(int fd, int wd);

/* compat.c */
extern int compat_do_readv_writev(int rw, int fd, struct iovec *iov, unsigned long nr_segs);
extern int compat_do_ioctl(int fd, unsigned int cmd, unsigned long arg);

/* fs/sync.c */
extern int vfs_fsync_range(struct file *file, loff_t start, loff_t end, int datasync);

/* fs/compat.c */
extern int compat_file_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
extern int compat_ptr_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/* fs/open.c */
extern int do_filp_open(int dfd, const struct path *path, const struct open_how *how);
extern struct file *filp_open(const char *filename, int flags, umode_t mode);
extern int do_sys_open(int dfd, const char __user *filename, int flags, umode_t mode);
extern int do_sys_openat(int dfd, const char __user *filename, int flags, umode_t mode);

/* fs/stat.c */
extern int vfs_statx(int dfd, const char __user *filename, int flags,
		     unsigned int mask, struct statx *statx);

/* fs/fcntl.c */
extern int do_fcntl(int fd, unsigned int cmd, unsigned long arg);

/* fs/ioctl.c */
extern int vfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

/* fs/pipe.c */
extern int pipe_read(struct file *, char __user *, size_t, loff_t *);
extern int pipe_write(struct file *, const char __user *, size_t, loff_t *);
extern int pipe_poll(struct file *file, struct poll_table_struct *);

/* fs/eventpoll.c */
extern int eventpoll_release(struct file *file);
extern int eventpoll_poll(struct file *file, struct poll_table_struct *);

/* fs/select.c */
extern int select_read(struct file *, char __user *, size_t, loff_t *);
extern int select_write(struct file *, const char __user *, size_t, loff_t *);

/* fs/aio.c */
extern int io_setup(unsigned nr_events, aio_context_t __user *ctx);
extern int io_destroy(aio_context_t ctx);
extern int io_submit(aio_context_t ctx, struct iocb __user *iocb);
extern int io_getevents(aio_context_t ctx, long min_nr, long max_nr,
			struct io_event __user *events, long min_nr);
extern int io_cancel(aio_context_t ctx, struct iocb *iocb, struct io_event __user *);

/* fs/signalfd.c */
extern int signalfd_create(struct file *file, struct signalfd_ctx *ctx);

/* fs/timerfd.c */
extern int timerfd_create(struct file *file, struct timerfd_ctx *ctx);

/* fs/eventfd.c */
extern int eventfd_signal(struct eventfd_ctx *ctx, __u64 n);

/* fs/inotify.c */
extern int inotify_init1(int flags);
extern int inotify_add_watch(int fd, const char *pathname, uint32_t mask);
extern int inotify_rm_watch(int fd, int wd);

/* fs/fanotify.c */
extern int fanotify_init(struct fanotify_data *fd, unsigned int flags);
extern int fanotify_mark(struct fanotify_data *fd, unsigned int mask,
			 unsigned int flags, struct fsnotify_mark *mark,
			 struct path *path, unsigned int mask);

/* fs/namespace.c */
extern int user_mount(const char __user *dev_name, const char __user *dir_name,
		      int flags, const char __user *type_name, void __user *data);

/* fs/dco.c */
extern void fs_dco_free(struct fs_dco *dco);

/* fs/binfmt_flat.c */
extern int init_flat_binary(struct linux_binprm *);

/* fs/utsname.c */
extern int sys_newuname(struct new_utsname *name);
extern int sys_sethostname(const char *name, size_t len);
extern int sys_setdomainname(const char *name, size_t len);

/* fs/pipe.c */
extern int do_pipe_flags(int *fd, int flags);

/* fs/fcntl.c */
extern int vfs_fcntl(int fd, unsigned int cmd, unsigned long arg);

/* fs/dco.c */
extern void fs_dco_free(struct fs_dco *dco);

/*
 * fs/stat.c:
 */
unsigned vfs_stat_set_lookup_flags(unsigned *lookup_flags, int flags);
int cp_statx(const struct kstat *stat, struct statx __user *buffer);

#endif /* _FS_INTERNAL_H */
