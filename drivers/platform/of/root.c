// SPDX-License-Identifier: GPL-2.0+

/*
 * Copyright (C) 2021 metux IT consult
 * Author: Enrico Weigelt <info@metux.net>
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/libfdt.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include "../../of/of_private.h"

#include "ofboard.h"

#define DECLARE_STRING_PROPERTY(scope, pname, pvalue, pnext)	\
	static struct property prop_##scope##_##pname = {	\
		.name = #pname,					\
		.value = pvalue,				\
		.length = sizeof(pvalue),			\
		.next = &prop_##scope##_##pnext,		\
	};

#define DECLARE_STRING_PROPERTY_1ST(scope, pname, pvalue)	\
	static struct property prop_##scope##_##pname = {	\
		.name = #pname,					\
		.value = pvalue,				\
		.length = sizeof(pvalue),			\
	};

DECLARE_STRING_PROPERTY_1ST(root, compatible, "unknown,acpi");
DECLARE_STRING_PROPERTY(root, model, "any", compatible);

static struct device_node pseudo_symbols = {
	.full_name = "__symbols__",
};

static struct device_node pseudo_root = {
	.full_name = "",
	.properties = &prop_root_model,
	.child = &pseudo_symbols,
};

void init_oftree(void)
{
	struct device_node *np;

	if (of_root)
		return;

	pr_info("OF: no root node - creating one\n");
	of_node_init(&pseudo_root);
	of_node_init(&pseudo_symbols);

	pseudo_symbols.parent = &pseudo_root;

	of_root = &pseudo_root;
	for_each_of_allnodes(np) {
		pr_info("sysfs populating: %s\n", np->full_name);
		__of_attach_node_sysfs(np, NULL);
	}

	of_aliases = of_find_node_by_path("/aliases");
	of_chosen = of_find_node_by_path("/chosen");
}
