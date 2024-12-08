#include <linux/pm.h>
#include <linux/bug.h>
#include <linux/memblock.h>

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
//#include <mach/mt_typedefs.h>
#include <linux/timer.h>
//#include <mt-plat/mt_pmic_wrap.h>
#include <linux/syscore_ops.h>
#include <asm/io.h>
#include <mt-plat/csci.h>
#include <asm/setup.h>
#include <linux/of_fdt.h>
#include <linux/version.h>
//#include <mt-plat/mtk_memcfg.h>

/*
extern u64 pmem_start;  // pmem_start is inited in mt_fixup
extern u64 kernel_mem_sz;       // kernel_mem_sz is inited in mt_fixup
extern u64 RESERVED_MEM_SIZE_FOR_TEST_3D ;
extern u64 FB_SIZE_EXTERN ;
extern u64 RESERVED_MEM_SIZE_FOR_FB_MAX;
extern u64 RESERVED_MEM_SIZE_FOR_PMEM;

u64 RESERVED_MEM_SIZE_FOR_PMEM = 0x0;
u64 pmem_start = 0x12345678;  // pmem_start is inited in mt_fixup
u64 kernel_mem_sz = 0x0;       // kernel_mem_sz is inited in mt_fixup
u64 RESERVED_MEM_SIZE_FOR_TEST_3D = 0x0;
u64 FB_SIZE_EXTERN = 0x0;
u64 RESERVED_MEM_SIZE_FOR_FB_MAX = 0x1500000;


#define TOTAL_RESERVED_MEM_SIZE (RESERVED_MEM_SIZE_FOR_PMEM + \
                                 RESERVED_MEM_SIZE_FOR_FB)

#define MAX_PFN        ((max_pfn << PAGE_SHIFT) + PHYS_OFFSET)

#define PMEM_MM_START  (pmem_start)
#define PMEM_MM_SIZE   (RESERVED_MEM_SIZE_FOR_PMEM)

#define TEST_3D_START  (PMEM_MM_START + PMEM_MM_SIZE)
#define TEST_3D_SIZE   (RESERVED_MEM_SIZE_FOR_TEST_3D)

#define FB_START      (TEST_3D_START + RESERVED_MEM_SIZE_FOR_TEST_3D)
#define FB_SIZE       (RESERVED_MEM_SIZE_FOR_FB)
*/
//#define CSCI_START    0x7fec0000  //FB_START-0x100000

u64 csci_start;
u32 csci_size;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0)
struct mem_desc {
        u64 start;
        u64 size;
};
#endif

static int __init csci_map(void)
{
	unsigned long* va = NULL;
	//unsigned long* pa;
	char *csci;
	//void *va;
	printk("csci: csci_map  csci_start = 0x%llx\n",csci_start);
	//va =  ioremap_nocache((phys_addr_t)csci_start, 0x100000);
	va =  ioremap((phys_addr_t)csci_start, 0x100000);
	printk("csci va=0x%08lx pa=%0x\n",(long unsigned int)va,(u32)csci_start);

	csci = (char*)va;
	csci_init(csci,CSCI_SIZE_NORMAL);
	csci_list(NULL);
    return 0;
}

static int dt_scan_memory(unsigned long node, const char *uname,
		int depth, void *data)
{
	const char *type = of_get_flat_dt_prop(node, "device_type", NULL);
//	int i;
	int l;

	struct mem_desc *mem_desc;
	const __be32 *reg, *endp;


	/* We are scanning "memory" nodes only */
	if (type == NULL) {
		/*
		 * The longtrail doesn't have a device_type on the
		 * /memory node, so look for the node called /memory@0.
		 */
		if (depth != 1 || strcmp(uname, "memory@0") != 0)
			return 0;
	} else if (strcmp(type, "memory") != 0) {
		return 0;
	}

		reg = of_get_flat_dt_prop(node, "reg", &l);
	if (reg == NULL)
		return 0;

	endp = reg + (l / sizeof(__be32));

	while ((endp - reg) >= (dt_root_addr_cells + dt_root_size_cells)) {
		u64 base, size;

		base = dt_mem_next_cell(dt_root_addr_cells, &reg);
		size = dt_mem_next_cell(dt_root_size_cells, &reg);

		if (size == 0)
			continue;
	}
	
	/* csci reserved memory */
	mem_desc = (struct mem_desc *)of_get_flat_dt_prop(node,
			"csci_reserved_mem", NULL);
	if (mem_desc && mem_desc->size) {
		pr_info(
			"[PHY layout]csci_reserved_mem   :   0x%08llx - 0x%08llx (0x%llx)\n",
			mem_desc->start,
			mem_desc->start +
			(long long unsigned int)0x100000 - 1,
			(long long unsigned int)0x100000
			);
//		dram_sz += mem_desc->size;
	}
	csci_start = mem_desc->start;
	csci_size =  mem_desc->size;
	printk("csci: of_get_flat_dt_prop  mem_desc->start = 0x%0x\n",(u32)mem_desc->start);
	printk("csci: of_get_flat_dt_prop  mem_desc->size = 0x%0x\n",(u32)mem_desc->size);

	return node;
}


extern int csci_early_memory_info(void)
{
	int node;
	node = of_scan_flat_dt(dt_scan_memory, NULL);
	csci_map();
	return 0;
}
pure_initcall(csci_early_memory_info);
postcore_initcall(csci_map);

MODULE_AUTHOR("jerry he <jerry.he@elink.com>");
MODULE_DESCRIPTION("csci Driver");
MODULE_LICENSE("GPL");

