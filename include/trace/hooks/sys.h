/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM sys
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_SYS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_SYS_H
#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>

#ifdef __GENKSYMS__
struct task_struct;
#else
/* struct task_struct */
#include <linux/sched.h>
#endif /* __GENKSYMS__ */
DECLARE_HOOK(android_vh_syscall_prctl_finished,
	TP_PROTO(int option, struct task_struct *task),
	TP_ARGS(option, task));
#endif

#include <trace/define_trace.h>

#if defined(CONFIG_TRACEPOINTS) && defined(CONFIG_ANDROID_VENDOR_HOOKS)
struct task_struct;
DECLARE_HOOK(android_vh_sys_set_task,
	TP_PROTO(struct task_struct *task),
	TP_ARGS(task));
#else
#define trace_android_vh_sys_set_task(task)
#endif
#endif
