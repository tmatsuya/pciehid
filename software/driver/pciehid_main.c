#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/pci.h>
#include <linux/wait.h>		/* wait_queue_head_t */
#include <linux/sched.h>	/* wait_event_interruptible, wake_up_interruptible */
#include <linux/interrupt.h>
#include <linux/version.h>

//#define DEBUG
//#define	IRQ_ENABLE

#if defined(USE_32BIT) && defined(USE_64BIT)
#error "You have to choose between USE_32BIT and USE_64BIT"
#endif

#ifndef DRV_NAME
#define DRV_NAME	"pciehid"
#endif

//#define	MAX_BOARDS	(10)	// Max numbers of Board

#define	DRV_VERSION	"0.0.1"
#define	pciehid_DRIVER_NAME DRV_NAME ": PCI Express HID Driver - version " DRV_VERSION
#define	CMDP_DMA_BUF_SIZE	(1024*1024)
#define	RESP_DMA_BUF_SIZE	(1024*1024)

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

//static struct pci_dev *pcidev = NULL;

static struct file_operations pciehid_fops[MAX_BOARDS];
/* = {
	.owner		= THIS_MODULE,
	.read		= pciehid_read,
	.write		= pciehid_write,
	.poll		= pciehid_poll,
	.unlocked_ioctl	= pciehid_ioctl,
	.open		= pciehid_open,
	.release	= pciehid_release,
}; */

static struct miscdevice pciehid_dev[MAX_BOARDS];
/*
 {
	.minor = MISC_DYNAMIC_MINOR,
	.name = DRV_NAME,
	.fops = &pciehid_fops,
};
*/


#ifdef IRQ_ENABLE
static irqreturn_t pciehid_interrupt(int irq, void *pdev)
{
	//struct __pciehid_board_conf *board;

	// not my interrupt
	if (pciehid_req_interrupt == 0) {
		return IRQ_NONE;
	}

	pciehid_req_interrupt = 0;

#ifdef DEBUG
	printk("%s\n", __func__);
#endif

//	wake_up_interruptible( &board->read_q );

	return IRQ_HANDLED;
}
#endif

static int pciehid_open(struct inode *inode, struct file *filp)
{
	struct __pciehid_board_conf *board;
	int i;

#ifdef DEBUG
	printk("%s\n", __func__);
#endif

	for (i=0; i<MAX_BOARDS; ++i) {
		if (pciehid_dev[i].fops == filp->f_op) {
#ifdef DEBUG
			printk("Board=%d\n", i);
#endif
			break;
		}
	}

	board = (struct __pciehid_board_conf *)&pciehid_boards[i];
	board->read_flag = 0;
	board->seq = 0;
	board->write_buf[0] = '\000';
	board->write_len = 0;
	filp->private_data = (void *)board;

	return 0;
}

static ssize_t pciehid_read(struct file *filp, char __user *buf,
			   size_t count, loff_t *ppos)
{
	struct __pciehid_board_conf *board = (struct __pciehid_board_conf *)filp->private_data;
	unsigned char tmp[140];
	int copy_len, fpga_rev, dma_queue_max;

#ifdef DEBUG
	printk("%s\n", __func__);
#endif

#if 0
	if ( wait_event_interruptible( &board->read_q, ( pbuf0.rx_read_ptr != pbuf0.rx_write_ptr ) ) )
		return -ERESTARTSYS;
#endif
	board->read_flag = !board->read_flag;
	if (board->read_flag) {
		fpga_rev = (int)*(board->bar_p[0] + 0xc0008/4)&0xff;
		if (fpga_rev == 2)
			dma_queue_max = 2;
		else if (fpga_rev >= 3 && fpga_rev <= 4)
			dma_queue_max = 4;
		else if (fpga_rev >= 5 )
			dma_queue_max = (int)*(board->bar_p[0] + 0xc0004/4)&0xff;
		else
			dma_queue_max = 0;

		sprintf(tmp, "KERNEL DRIVER Version:%s\nFPGA TCAM F/W Version:%x.%x.%x(%08X-%08X)\nBAR0:%X\nBAR2:%X\nBAR4:%X\nDMA_QUEUE_MAX:%d\n",
		 DRV_VERSION ,
		 (int)*(board->bar_p[0] + 0xc0008/4)&0xff, (int)*(board->bar_p[0] + 0xc0008/4)>>16, (int)*(board->bar_p[0] + 0xc000c/4)&0xff,
		(int)*(board->bar_p[0] + 0xc00f0/4), (int)*(board->bar_p[0] + 0xc00f4/4),
		(int)board->bar_start[0], (int)board->bar_start[1], (int)board->bar_start[2],
		dma_queue_max);
		copy_len = strlen(tmp);
	} else {
		copy_len = 0;
	}

	if ( copy_to_user( buf, tmp, copy_len ) ) {
		printk( KERN_INFO "copy_to_user failed\n" );
		return -EFAULT;
	}

	return copy_len;
}

static ssize_t pciehid_write_one( struct __pciehid_board_conf *board )
{
	int rc;
	unsigned char *buf = board->write_buf;
	char mode[32];
	int addr;
	unsigned int data[10], mask[10];
	int disable_cam;

#ifdef DEBUG
	printk("%s\n", __func__);
#endif
#ifdef DEBUG
	printk( "%s", buf );
#endif

	disable_cam = -1;

	rc = sscanf( buf, "%10s %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x", mode, &addr,
		&data[0], &data[1], &data[2], &data[3], &data[4],
		&data[5], &data[6], &data[7], &data[8], &data[9],
		&mask[0], &mask[1], &mask[2], &mask[3], &mask[4],
		&mask[5], &mask[6], &mask[7], &mask[8], &mask[9]);

	if (rc != 22) {
		rc = sscanf( buf, "%10s %x %x %x %x %x %x %x %x %x %x %x", mode, &addr,
			&data[0], &data[1], &data[2], &data[3], &data[4],
			&mask[0], &mask[1], &mask[2], &mask[3], &mask[4]);
	}

	// write TCAM entry
	if ( !strncmp(mode, "W", 1)) {
		if (rc == 12 || rc == 22) {
			if ( !strcmp(mode, "W0" ))
				disable_cam = 2;
			if ( !strcmp(mode, "W1" ))
				disable_cam = 1;
			if ( !strcmp(mode, "W01" ) || !strcmp(buf, "W10" ))
				disable_cam = 0;
			if ( rc == 12 && disable_cam >= 0 && addr >= 0 && addr <= 0xfffff)
				pciehid_wcam160((struct pciehid_board_conf *)board, 0, 3, disable_cam, 0, addr, data, mask);
			if ( rc == 22 && disable_cam >= 0 && addr >= 0 && addr <= 0xfffff)
				pciehid_wcam320((struct pciehid_board_conf *)board, 0, 3, disable_cam, 0, addr, data, mask);
		}
	}

	// disable tcam entry
	if ( !strncmp(mode, "D", 1)) {
		if (rc >= 2) {
			if ( !strcmp(mode, "D0" ))
				disable_cam = 2;
			if ( !strcmp(mode, "D1" ))
				disable_cam = 1;
			if ( !strcmp(mode, "D01" ) || !strcmp(buf, "D10" ))
				disable_cam = 0;
			if ( disable_cam >= 0 && addr >= 0 && addr <= 0xfffff)
				pciehid_wcam160((struct pciehid_board_conf *)board, 0, 4, disable_cam, 0, addr, data, mask);
		}
	}

	return 0;
}

static ssize_t pciehid_write(struct file *filp, const char __user *buf,
			    size_t count, loff_t *ppos)

{
	struct __pciehid_board_conf *board = (struct __pciehid_board_conf *)filp->private_data;
	int i, copy_len;
	char __user *p;
	char tmp[128];

#ifdef DEBUG
	printk("%s\n", __func__);
#endif
	if (count > 128)
		count = 128;
	copy_from_user( tmp, buf, count );

	for ( p = tmp, i = 0; i < count; ++p, ++i ) {
		board->write_buf[ board->write_len++ ] = *p;
		if (*p == '\n' || board->write_len >= 256) {
			board->write_buf[ board->write_len++ ] = '\000';
			pciehid_write_one( board );
			board->write_buf[ 0 ] = '\000';
			board->write_len = 0;
		}	
	}

	copy_len = count;

#if 0
	if ( copy_from_user( board->bar_p[0], buf, copy_len ) ) {
		printk( KERN_INFO "copy_from_user failed\n" );
		return -EFAULT;
	}
#endif

	return copy_len;
}

static int pciehid_release(struct inode *inode, struct file *filp)
{
	//struct __pciehid_board_conf *board = (struct __pciehid_board_conf *)filp->private_data;
#ifdef DEBUG
	printk("%s\n", __func__);
#endif

//	*board->bar_p[0] = 0x00;		/* IRQ clear and not Request receiving PHY#0 */

	return 0;
}

static unsigned int pciehid_poll(struct file *filp, poll_table *wait)
{
#ifdef DEBUG
	printk("%s\n", __func__);
#endif
	return 0;
}


static long pciehid_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	struct __pciehid_board_conf *board = (struct __pciehid_board_conf *)filp->private_data;
	unsigned int *ptr;
	struct tc_buff160 tc_kws160;

#ifdef DEBUG
	printk("%s(cmd=%x)\n", __func__, cmd);
#endif
	switch (cmd) {
		// RESET
		case TCAM_IOCTL_RESET: ptr = (unsigned int *)arg;
			break;
		// GET BAR0 Address
		case TCAM_IOCTL_GET_BAR0_ADDR: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->bar_start[0], sizeof(unsigned long long));
			break;
		// GET BAR0 Size
		case TCAM_IOCTL_GET_BAR0_LEN: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->bar_len[0],   sizeof(int));
			break;
		// GET BAR1 Address
		case TCAM_IOCTL_GET_BAR1_ADDR: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->bar_start[1], sizeof(unsigned long long));
			break;
		// GET BAR1 Size
		case TCAM_IOCTL_GET_BAR1_LEN: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->bar_len[1],   sizeof(int));
			break;
		// GET BAR2 Address
		case TCAM_IOCTL_GET_BAR2_ADDR: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->bar_start[2], sizeof(unsigned long long));
			break;
		// GET BAR2 Size
		case TCAM_IOCTL_GET_BAR2_LEN: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->bar_len[2],   sizeof(int));
			break;
		// SET CMDP Address
		case TCAM_IOCTL_SET_CMDP_ADDR: ptr = (unsigned int *)arg;
			copy_from_user(&board->bar_start[3], (void __user *)ptr, sizeof(unsigned long long));
			board->bar_p[3] = (unsigned int *)board->bar_start[3];
			break;
		// GET CMDP Address
		case TCAM_IOCTL_GET_CMDP_ADDR: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->bar_start[3], sizeof(unsigned long long));
			break;
		// SET CMDP Length
		case TCAM_IOCTL_SET_CMDP_LEN: ptr = (unsigned int *)arg;
			copy_from_user(&board->bar_len[3], (void __user *)ptr, sizeof(unsigned long long));
			break;
		// GET CMDP Length
		case TCAM_IOCTL_GET_CMDP_LEN: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->bar_len[3], sizeof(unsigned long long));
			break;
		// SET RESP Address
		case TCAM_IOCTL_SET_RESP_ADDR: ptr = (unsigned int *)arg;
			copy_from_user(&board->bar_start[4], (void __user *)ptr, sizeof(unsigned long long));
			board->bar_p[4] = (unsigned int *)board->bar_start[4];
			break;
		// GET RESP Address
		case TCAM_IOCTL_GET_RESP_ADDR: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->bar_start[4], sizeof(unsigned long long));
			break;
		// SET RESP Length
		case TCAM_IOCTL_SET_RESP_LEN: ptr = (unsigned int *)arg;
			copy_from_user(&board->bar_len[4], (void __user *)ptr, sizeof(unsigned long long));
			break;
		// GET RESP Length
		case TCAM_IOCTL_GET_RESP_LEN: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->bar_len[4], sizeof(unsigned long long));
			break;
		// SET DMA MODE
		case TCAM_IOCTL_SET_DMA_MODE: ptr = (unsigned int *)arg;
			copy_from_user(&board->dma_mode, (void __user *)ptr, sizeof(int));
			break;
		// GET DMA MODE
		case TCAM_IOCTL_GET_DMA_MODE: ptr = (unsigned int *)arg;
			copy_to_user((void __user *)ptr, &board->dma_mode, sizeof(int));
			break;
		// SET KWS160 Parameters
		case TCAM_IOCTL_SET_KWS160: ptr = (unsigned int *)arg;
			copy_from_user(&tc_kws160, (void __user *)ptr, sizeof(tc_kws160));
#ifdef DEBUG
for (i=0; i<5; ++i)
	printk( "data[%d]=%X", i, tc_kws160.data[0][i]);
#endif
			if (board->dma_mode) {
				pciehid_kws160_dma((struct pciehid_board_conf *)board, 0, 0, &tc_kws160);
			} else {
				pciehid_kws160((struct pciehid_board_conf *)board, 0, 0, &tc_kws160);
				copy_to_user((void __user *)ptr, &tc_kws160, sizeof(tc_kws160));
			}
			break;
		// GET Results
		case TCAM_IOCTL_GET_RESULT: ptr = (unsigned int *)arg;
			if (board->dma_mode) {
				copy_to_user((void __user *)ptr, board->bar_p[4], 64);
			} else {
				pciehid_kws160_dma_result((struct pciehid_board_conf *)board, 0, 0, &tc_kws160);
//				copy_to_user((void __user *)ptr, &tc_kws160, sizeof(tc_kws160));
			}
			break;
		default:
			return -ENOTTY;
	}

	return 0;
}


static int __devinit pciehid_probe (struct pci_dev *pdev,
				       const struct pci_device_id *ent)
{
	struct __pciehid_board_conf *board;
	static char name[16];
	static int board_idx = -1;
	int rc;
	unsigned long mmio0_start, mmio0_end, mmio0_flags, mmio0_len;
	unsigned long mmio2_start, mmio2_end, mmio2_flags, mmio2_len;
	unsigned long mmio4_start, mmio4_end, mmio4_flags, mmio4_len;

	if (++board_idx >= (MAX_BOARDS-1))
		goto err_out;

	board = (struct __pciehid_board_conf *)&pciehid_boards[board_idx];

	board->bar_p[0] = NULL;
	board->bar_p[1] = NULL;
	board->bar_p[2] = NULL;
	board->bar_p[3] = NULL;
	board->bar_start[3] = (dma_addr_t)NULL;
	board->bar_p[4] = NULL;
	board->bar_start[4] = (dma_addr_t)NULL;

	pci_set_drvdata(pdev, board);

	rc = pci_enable_device (pdev);
	if (rc)
		goto err_out;

	rc = pci_request_regions (pdev, DRV_NAME);
	if (rc)
		goto err_out;

	printk( KERN_INFO "Found board_idx: %d\n", board_idx );

	pdev->priv_flags = board_idx;

	pciehid_fops[board_idx].owner	= THIS_MODULE,
	pciehid_fops[board_idx].read	= pciehid_read,
	pciehid_fops[board_idx].write	= pciehid_write,
	pciehid_fops[board_idx].poll	= pciehid_poll,
	pciehid_fops[board_idx].unlocked_ioctl	= pciehid_ioctl,
	pciehid_fops[board_idx].open	= pciehid_open,
	pciehid_fops[board_idx].release	= pciehid_release,

	sprintf( name, "%s/%d", DRV_NAME,  board_idx );
	pciehid_dev[board_idx].minor = MISC_DYNAMIC_MINOR,
	pciehid_dev[board_idx].name = name,
	pciehid_dev[board_idx].fops = &pciehid_fops[board_idx],

	rc = misc_register(&pciehid_dev[board_idx]);
	if (rc) {
		printk("fail to misc_register (MISC_DYNAMIC_MINOR)\n");
		return rc;
	}

	pci_set_master (pdev);		/* set BUS Master Mode */

	mmio0_start = pci_resource_start (pdev, 0);
	mmio0_end   = pci_resource_end   (pdev, 0);
	mmio0_flags = pci_resource_flags (pdev, 0);
	mmio0_len   = pci_resource_len   (pdev, 0);
	board->bar_start[0] = mmio0_start;
	board->bar_len[0]   = mmio0_len;

	printk( KERN_INFO " BAR0: %X-%X\n", (unsigned int)mmio0_start,
					(unsigned int)mmio0_end );

//	board->bar_p[0] = ioremap_wc(mmio0_start, mmio0_len);		// write combined mode
	board->bar_p[0] = ioremap(mmio0_start, mmio0_len);			// normal mode
	if (!board->bar_p[0]) {
		printk(KERN_ERR "cannot ioremap MMIO0 base\n");
		goto err_out;
	}

	mmio2_start = pci_resource_start (pdev, 2);
	mmio2_end   = pci_resource_end   (pdev, 2);
	mmio2_flags = pci_resource_flags (pdev, 2);
	mmio2_len   = pci_resource_len   (pdev, 2);
	board->bar_start[1] = mmio2_start;
	board->bar_len[1]   = mmio2_len;

	printk( KERN_INFO " BAR2: %X-%X\n", (unsigned int)mmio2_start,
					(unsigned int)mmio2_end );

	board->bar_p[1] = ioremap_wc(mmio2_start, mmio2_len);
	if (!board->bar_p[1]) {
		printk(KERN_ERR "cannot ioremap MMIO2 base\n");
		goto err_out;
	}

	mmio4_start = pci_resource_start (pdev, 4);
	mmio4_end   = pci_resource_end   (pdev, 4);
	mmio4_flags = pci_resource_flags (pdev, 4);
	mmio4_len   = pci_resource_len   (pdev, 4);
	board->bar_start[2] = mmio4_start;
	board->bar_len[2]   = mmio4_len;

	printk( KERN_INFO " BAR4: %X-%X\n", (unsigned int)mmio4_start,
					(unsigned int)mmio4_end );

	board->bar_p[2] = ioremap_wc(mmio4_start, mmio4_len);
	if (!board->bar_p[2]) {
		printk(KERN_ERR "cannot ioremap MMIO4 base\n");
		goto err_out;
	}

	// CMDP DMA BUFFER
	board->bar_len[3] = CMDP_DMA_BUF_SIZE;
	board->bar_p[3] = dma_alloc_coherent( &pdev->dev, CMDP_DMA_BUF_SIZE, &board->bar_start[3], GFP_KERNEL);
	if (!board->bar_p[3]) {
		printk(KERN_ERR "cannot dma_alloc_coherent\n");
		goto err_out;
	}

	printk( KERN_INFO " cmdp_virtual  : %llX\n", (long long)board->bar_p[3] );
	printk( KERN_INFO " cmdp_physical : %X (LEN=%X)\n", (int)board->bar_start[3], board->bar_len[3]);

	// RESP DMA BUFFER
	board->bar_len[4] = RESP_DMA_BUF_SIZE;
	board->bar_p[4] = dma_alloc_coherent( &pdev->dev, RESP_DMA_BUF_SIZE, &board->bar_start[4], GFP_KERNEL);
	if (!board->bar_p[4]) {
		printk(KERN_ERR "cannot dma_alloc_coherent\n");
		dma_free_coherent(&pdev->dev, CMDP_DMA_BUF_SIZE, board->bar_p[3], board->bar_start[3]);
		goto err_out;
	}
//	board->bar_start[4] = any_v2p((unsigned long)board->bar_p[4]);

	printk( KERN_INFO " resp_virtual  : %llX\n", (long long)board->bar_p[4] );
	printk( KERN_INFO " resp_physical : %X (LEN=%X)\n", (int)board->bar_start[4], board->bar_len[4]);

	board->dma_mode = 0;

	pciehid_req_interrupt = 0;

	pciehid_init_fpga((struct pciehid_board_conf *)board);		// fpgaini.sh

#ifdef IRQ_ENABLE
	if (request_irq(pdev->irq, pciehid_interrupt, IRQF_SHARED, DRV_NAME, pdev)) {
		printk(KERN_ERR "cannot request_irq\n");
	}
#endif
	
	board->pdev = pdev;

	spin_lock_init( &board->board_lock );
	init_waitqueue_head( &board->write_q );
	init_waitqueue_head( &board->read_q );

	return 0;

err_out:
	pci_release_regions (pdev);
	pci_disable_device (pdev);
	return -1;
}


static void __devexit pciehid_remove (struct pci_dev *pdev)
{
	struct __pciehid_board_conf *board = pci_get_drvdata(pdev);

#ifdef DEBUG
	printk("%s\n", __func__);
#endif
#ifdef IRQ_ENABLE
	disable_irq(pdev->irq);
	free_irq(pdev->irq, pdev);
#endif

	if (board->bar_p[0]) {
		iounmap(board->bar_p[0]);
		board->bar_p[0] = 0L;
	}
	if (board->bar_p[1]) {
		iounmap(board->bar_p[1]);
		board->bar_p[1] = 0L;
	}
	if (board->bar_p[2]) {
		iounmap(board->bar_p[2]);
		board->bar_p[2] = 0L;
	}

	if ( board->bar_p[3] )
		dma_free_coherent(&pdev->dev, CMDP_DMA_BUF_SIZE, board->bar_p[3], board->bar_start[3]);

	if ( board->bar_p[4] )
		dma_free_coherent(&pdev->dev, RESP_DMA_BUF_SIZE, board->bar_p[4], board->bar_start[4]);

	pci_release_regions (pdev);
	pci_disable_device (pdev);
	misc_deregister(&pciehid_dev[pdev->priv_flags]);
}


static struct pci_driver pciehid_pci_driver = {
	.name		= DRV_NAME,
	.id_table	= pciehid_pci_tbl,
	.probe		= pciehid_probe,
	.remove		= __devexit_p(pciehid_remove),
#ifdef CONFIG_PM
//	.suspend	= pciehid_suspend,
//	.resume		= pciehid_resume,
#endif /* CONFIG_PM */
};


static int __init pciehid_init(void)
{

	printk( KERN_INFO "tcam: Copyright (c) 2019-2020 NTT Communications Corporation, KEIO University and ALAXALA Networks Corporation\n" );
#ifdef MODULE
	pr_info(pciehid_DRIVER_NAME "\n");
#endif

#ifdef DEBUG
	printk("%s\n", __func__);
#endif
	return pci_register_driver(&pciehid_pci_driver);
}

static void __exit pciehid_exit(void)
{
#ifdef DEBUG
	printk("%s\n", __func__);
#endif
	pci_unregister_driver(&pciehid_pci_driver);
}

module_init(pciehid_init);
module_exit(pciehid_exit);

MODULE_DESCRIPTION("Keio University, PCIe HID driver");
MODULE_AUTHOR("<macchan@sfc.wide.ad.jp>");
MODULE_LICENSE("GPL");

