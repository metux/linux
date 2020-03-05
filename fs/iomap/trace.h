/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2009-2019 Christoph Hellwig
 *
 * NOTE: none of these tracepoints shall be consider a stable kernel ABI
 * as they can change at any time.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM iomap

#if !defined(_IOMAP_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _IOMAP_TRACE_H

#include <linux/tracepoint.h>

struct inode;

DECLARE_EVENT_CLASS(iomap_readpage_class,
	TP_PROTO(struct inode *inode, int nr_pages),
	TP_ARGS(inode, nr_pages),
	TP_STRUCT__entry(
		__field(dev_t, dev)
		__field(u64, ino)
		__field(int, nr_pages)
	),
	TP_fast_assign(
		__entry->dev = inode->i_sb->s_dev;
		__entry->ino = inode->i_ino;
		__entry->nr_pages = nr_pages;
	),
	TP_printk("dev %d:%d ino 0x%llx nr_pages %d",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  __entry->ino,
		  __entry->nr_pages)
)

#define DEFINE_READPAGE_EVENT(name)		\
DEFINE_EVENT(iomap_readpage_class, name,	\
	TP_PROTO(struct inode *inode, int nr_pages), \
	TP_ARGS(inode, nr_pages))
DEFINE_READPAGE_EVENT(iomap_readpage);
DEFINE_READPAGE_EVENT(iomap_readpages);

DECLARE_EVENT_CLASS(iomap_range_class,
	TP_PROTO(struct inode *inode, unsigned long off, unsigned int len),
	TP_ARGS(inode, off, len),
	TP_STRUCT__entry(
		__field(dev_t, dev)
		__field(u64, ino)
		__field(loff_t, size)
		__field(unsigned long, offset)
		__field(unsigned int, length)
	),
	TP_fast_assign(
		__entry->dev = inode->i_sb->s_dev;
		__entry->ino = inode->i_ino;
		__entry->size = i_size_read(inode);
		__entry->offset = off;
		__entry->length = len;
	),
	TP_printk("dev %d:%d ino 0x%llx size 0x%llx offset %lx "
		  "length %x",
		  MAJOR(__entry->dev), MINOR(__entry->dev),
		  __entry->ino,
		  __entry->size,
		  __entry->offset,
		  __entry->length)
)

#define DEFINE_RANGE_EVENT(name)		\
DEFINE_EVENT(iomap_range_class, name,	\
	TP_PROTO(struct inode *inode, unsigned long off, unsigned int len),\
	TP_ARGS(inode, off, len))
DEFINE_RANGE_EVENT(iomap_writepage);
DEFINE_RANGE_EVENT(iomap_releasepage);
DEFINE_RANGE_EVENT(iomap_invalidatepage);

#endif /* _IOMAP_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace
#include <trace/define_trace.h>
