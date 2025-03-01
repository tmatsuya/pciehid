#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/version.h>

#define	LOOPS	(10000)
#define	DMA_BUF_MAX	(4*1024*1024)

#ifndef DRV_NAME
#define DRV_NAME	"pciehid"
#endif

#define	DRV_VERSION	"0.0.1"
#define	pciehid_DRIVER_NAME	DRV_NAME " PCIe HID driver " DRV_VERSION
#define	DMA_BUF_SIZE	(1024*1024)

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,8,0)
#define	__devinit
#define	__devexit
#define	__devexit_p
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
static const struct pci_device_id pciehid_pci_tbl[] = {
#else
static DEFINE_PCI_DEVICE_TABLE(pciehid_pci_tbl) = {
#endif
        {0x3776, 0x8020, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
        {0,}
};
MODULE_DEVICE_TABLE(pci, pciehid_pci_tbl);



static unsigned char *mmio0_ptr = 0L, *mmio1_ptr = 0L, *mmio1wc_ptr;
static unsigned long mmio0_start, mmio0_end, mmio0_flags, mmio0_len;
static unsigned long mmio1_start, mmio1_end, mmio1_flags, mmio1_len;
static unsigned long *dma_ptr = 0L;
static int parameter_length = 0;
static int parameter_test = 0;
static struct pci_dev *pcidev = NULL;
static unsigned int tsc0[LOOPS], tsc1[LOOPS], tsc2[LOOPS];


static int pciehid_open(struct inode *inode, struct file *filp)
{
	printk("%s\n", __func__);

//	*mmio0_ptr = 0x02;		/* IRQ clear and Request receiving PHY#0 */

	return 0;
}

static ssize_t pciehid_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *ppos)
{
	int copy_len, i, j, len;
	unsigned long long s[3], e[3];
	unsigned long long min[3], max[3], tscs, tsce;
	long long avg[3], stdd[3];
	char tmp[256];
	unsigned char *ptr, *dptr;

#ifdef DEBUG
	printk("%s\n", __func__);
#endif

	copy_len = count;

	/* read 4 byte */
	i = 0;
	ptr = mmio1_ptr;
	dptr = (unsigned char *)dma_ptr;
	if (parameter_test == 0 || parameter_test == 1) {
		mb();
		s[0] = tscs;
		while (i<LOOPS) {
//			mb();
			memcpy(dptr, ptr, parameter_length);
			ptr+=parameter_length;
//			dptr+=parameter_length;
			tsce = 0; //rdtsc();
			tsc0[i] = tsce-tscs;
			tscs = tsce;
			++i;
		}
		e[0] = 0; //rdtsc();
	} else {
		s[0] = e[0] = 0;
	}

	/* write */
	i = 0;
	ptr = mmio1_ptr;
	dptr = (unsigned char *)dma_ptr;
	if (parameter_test == 0 || parameter_test == 2) {
		mb();
		s[1] = tscs = 0; //rdtsc();
		while (i<LOOPS) {
//			mb();
			memcpy(ptr, dptr, parameter_length);
			ptr+=parameter_length;
//			dptr+=parameter_length;
			tsce = 0; //rdtsc();
			tsc1[i] = tsce-tscs;
			tscs = tsce;
			++i;
		}
		e[1] = 0; //rdtsc();
	} else {
		s[1] = e[1] = 0;
	}

	/* write with wc */
	i = 0;
	ptr = mmio1wc_ptr;
	dptr = (unsigned char *)dma_ptr;
	if (parameter_test == 0 || parameter_test == 3) {
		mb();
		s[2] = tscs = 0; //rdtsc();
		while (i<LOOPS) {
//			mb();
			memcpy(ptr, dptr, parameter_length);
			ptr+=parameter_length;
//			dptr+=parameter_length;
			tsce = 0; //rdtsc();
			tsc2[i] = tsce-tscs;
			tscs = tsce;
			++i;
		}
		e[2] = 0; //rdtsc();
	} else {
		s[2] = e[2] = 0;
	}

	// calculate max and min
	for (i=0; i<3; ++i) {
		avg[i] = 0;
		max[i] = 0;
		min[i] = ~max[i];
	}
	for (i=0; i<LOOPS; ++i) {
		avg[0] += tsc0[i];
		avg[1] += tsc1[i];
		avg[2] += tsc2[i];
		if (min[0] > tsc0[i])
			min[0] = tsc0[i];
		if (max[0] < tsc0[i])
			max[0] = tsc0[i];
		if (min[1] > tsc1[i])
			min[1] = tsc1[i];
		if (max[1] < tsc1[i])
			max[1] = tsc1[i];
		if (min[2] > tsc2[i])
			min[2] = tsc2[i];
		if (max[2] < tsc2[i])
			max[2] = tsc2[i];
	}
	for (i=0; i<3; ++i) {
		stdd[i] = 0;
		avg[i] /= LOOPS;
	}
	for (i=0; i<LOOPS; ++i) {
		stdd[0] += abs((long long)tsc0[i] - (long long)avg[0]) * abs((long long)tsc0[i] - (long long)avg[0]);
		stdd[1] += abs((long long)tsc1[i] - (long long)avg[1]) * abs((long long)tsc1[i] - (long long)avg[1]);
		stdd[2] += abs((long long)tsc2[i] - (long long)avg[2]) * abs((long long)tsc2[i] - (long long)avg[2]);
	}
	for (i=0; i<3; ++i) {
		stdd[i] /= LOOPS;
	}
//	sprintf(tmp, "%02x,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n", (char)*mmio0_ptr, parameter_length, LOOPS, e[0]-s[0], e[1]-s[1], e[2]-s[2], min[0], min[1], min[2], max[0], max[1], max[2], stdd[0], stdd[1], stdd[2]);
	sprintf(tmp, "%02x,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n", (char)*mmio0_ptr, parameter_length, LOOPS, avg[0], avg[1], avg[2], min[0], min[1], min[2], max[0], max[1], max[2], stdd[0], stdd[1], stdd[2]);
	len = strlen(tmp);

	copy_len = len;

	if ( copy_to_user( buf, tmp, copy_len ) ) {
		printk( KERN_INFO "copy_to_user failed\n" );
		return -EFAULT;
	}

	return copy_len;
}

static ssize_t pciehid_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *ppos)

{
	char tmp[256];
	int copy_len, i, j, k;

	if (count <= 256)
		copy_len = count;
	else
		copy_len = 256;

#ifdef DEBUG
	printk("%s\n", __func__);
#endif

	if ( copy_from_user( tmp, buf, copy_len ) ) {
		printk( KERN_INFO "copy_from_user failed\n" );
		return -EFAULT;
	}
	if ( copy_len >= 2) {
		sscanf(tmp, "%02X,%d,%d", &i, &j, &k);
		if (j>0 && j<65536)
			parameter_length = j;
		if (k>=0 && k<=3)
			parameter_test = k;
		printk("PCIe link mode is %02x.\n", i);
		*mmio0_ptr = i;		/* set PCIe link mode */
	}

	return copy_len;
}

static int pciehid_release(struct inode *inode, struct file *filp)
{
	printk("%s\n", __func__);

//	*mmio0_ptr = 0x00;		/* IRQ clear and not Request receiving PHY#0 */

	return 0;
}

static unsigned int pciehid_poll(struct file *filp, poll_table *wait)
{
	printk("%s\n", __func__);
	return 0;
}


static long pciehid_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	unsigned long *ptr, ret;
	printk("%s(cmd=%x)\n", __func__, cmd);

	return -ENOTTY;
}

static struct file_operations pciehid_fops = {
	.owner		= THIS_MODULE,
	.read		= pciehid_read,
	.write		= pciehid_write,
	.poll		= pciehid_poll,
	.unlocked_ioctl	= pciehid_ioctl,
	.open		= pciehid_open,
	.release	= pciehid_release,
};

static struct miscdevice pciehid_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DRV_NAME,
	.fops = &pciehid_fops,
};


static int __devinit pciehid_init_one (struct pci_dev *pdev,
				       const struct pci_device_id *ent)
{
	int rc;
	static char name[16];
	static int board_idx = -1;

	mmio0_ptr = 0L;
	mmio1_ptr = 0L;

	rc = pci_enable_device (pdev);
	if (rc)
		goto err_out;

	rc = pci_request_regions (pdev, DRV_NAME);
	if (rc)
		goto err_out;

	++board_idx;

	printk( KERN_INFO "board_idx: %d\n", board_idx );

	pci_set_master (pdev);		/* set BUS Master Mode */

	mmio0_start = pci_resource_start (pdev, 0);
	mmio0_end   = pci_resource_end   (pdev, 0);
	mmio0_flags = pci_resource_flags (pdev, 0);
	mmio0_len   = pci_resource_len   (pdev, 0);

	parameter_length = 4;		/* read length */
	parameter_test = 0;		/* test pattern = all */

	printk( KERN_INFO "mmio0_start: %X\n", (unsigned int)mmio0_start );
	printk( KERN_INFO "mmio0_end  : %X\n", (unsigned int)mmio0_end   );
	printk( KERN_INFO "mmio0_flags: %X\n", (unsigned int)mmio0_flags );
	printk( KERN_INFO "mmio0_len  : %X\n", (unsigned int)mmio0_len   );

	mmio0_ptr = ioremap(mmio0_start, mmio0_len);
	if (!mmio0_ptr) {
		printk(KERN_ERR "cannot ioremap MMIO0 base\n");
		goto err_out;
	}

	mmio1_start = pci_resource_start (pdev, 2);
	mmio1_end   = pci_resource_end   (pdev, 2);
	mmio1_flags = pci_resource_flags (pdev, 2);
	mmio1_len   = pci_resource_len   (pdev, 2);

	printk( KERN_INFO "mmio1_start: %X\n", (unsigned int)mmio1_start );
	printk( KERN_INFO "mmio1_end  : %X\n", (unsigned int)mmio1_end   );
	printk( KERN_INFO "mmio1_flags: %X\n", (unsigned int)mmio1_flags );
	printk( KERN_INFO "mmio1_len  : %X\n", (unsigned int)mmio1_len   );

	if ( ( dma_ptr = kmalloc(DMA_BUF_MAX, GFP_KERNEL) ) == 0 ) {
		printk("fail to kmalloc\n");
		goto err_out;
	}

	mmio1_ptr = ioremap(mmio1_start, mmio1_len/2);
	mmio1wc_ptr = ioremap_wc(mmio1_start+mmio1_len/2, mmio1_len/2);
	if (!mmio1_ptr) {
		printk(KERN_ERR "cannot ioremap MMIO1 base\n");
		goto err_out;
	}
	if (!mmio1wc_ptr) {
		printk(KERN_ERR "cannot ioremap MMIO1wc base\n");
		goto err_out;
	}

	pcidev = pdev;

	/* reset board */
//	*mmio0_ptr = 0x02;	/* Request receiving PHY#1 */

	sprintf( name, "%s/%d", DRV_NAME,  board_idx );
	pciehid_dev.name = name,
	rc = misc_register(&pciehid_dev);
	if (rc) {
		printk("fail to misc_register (MISC_DYNAMIC_MINOR)\n");
		return rc;
	}


	return 0;

err_out:
	if (dma_ptr != 0L)
		kfree(dma_ptr);
	pci_release_regions (pdev);
	pci_disable_device (pdev);
	return -1;
}


static void __devexit pciehid_remove_one (struct pci_dev *pdev)
{
	if (dma_ptr != 0L)
		kfree(dma_ptr);
	if (mmio0_ptr) {
		iounmap(mmio0_ptr);
		mmio0_ptr = 0L;
	}
	if (mmio1_ptr) {
		iounmap(mmio1_ptr);
		mmio1_ptr = 0L;
	}
	if (mmio1wc_ptr) {
		iounmap(mmio1wc_ptr);
		mmio1wc_ptr = 0L;
	}
	pci_release_regions (pdev);
	pci_disable_device (pdev);
	printk("%s\n", __func__);
	misc_deregister(&pciehid_dev);
}


static struct pci_driver pciehid_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= pciehid_pci_tbl,
	.probe		= pciehid_init_one,
	.remove		= __devexit_p(pciehid_remove_one),
#ifdef CONFIG_PM
//	.suspend	= pciehid_suspend,
//	.resume		= pciehid_resume,
#endif /* CONFIG_PM */
};


static int __init pciehid_init(void)
{

#ifdef MODULE
	pr_info(pciehid_DRIVER_NAME "\n");
#endif

	printk("%s\n", __func__);
	return pci_register_driver(&pciehid_pci_driver);
}

static void __exit pciehid_cleanup(void)
{
	printk("%s\n", __func__);
	pci_unregister_driver(&pciehid_pci_driver);
}

MODULE_LICENSE("GPL");
module_init(pciehid_init);
module_exit(pciehid_cleanup);

MODULE_DESCRIPTION("Keio University, PCIe HID driver");
MODULE_AUTHOR("<macchan@sfc.wide.ad.jp>");
MODULE_LICENSE("GPL");

