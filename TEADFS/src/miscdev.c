#include "miscdev.h"
#include "teadfs_log.h"
#include "protocol.h"
#include "mem.h"
#include "global_param.h"
#include "teadfs_header.h"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/netfilter_ipv4.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/netlink.h>
#include <linux/spinlock.h>
#include <net/sock.h>



static int
teadfs_miscdev_open(struct inode* inode, struct file* file) {
	int rc = 0;

	LOG_DBG("ENTRY \n");
	do {
		rc = teadfs_get_client_connect();
		if (rc) {
			rc = -EBUSY;
			break;
		}
		teadfs_set_clinet_connect(1);
	} while (0);

	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}


static int
teadfs_miscdev_release(struct inode* inode, struct file* file)
{
	int rc = 0;

	LOG_DBG("ENTRY \n");
	do {
		teadfs_set_clinet_connect(0);
	} while (0);

	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}

static ssize_t
teadfs_miscdev_read(struct file* file, char __user* buf, size_t count,
	loff_t* ppos) {
	int rc = -EFAULT;
	LOG_DBG("ENTRY \n");
	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}

static ssize_t
teadfs_miscdev_write(struct file* file, const char __user* buf,
	size_t count, loff_t* ppos) {
	int rc = -EFAULT;
	LOG_DBG("ENTRY \n");
	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}

static unsigned int
teadfs_miscdev_poll(struct file* file, poll_table* pt) {
	int rc = 0;
	LOG_DBG("ENTRY \n");
	LOG_DBG("LEVAL rc : [%d]\n", rc);
	return rc;
}
static const struct file_operations teadfs_miscdev_fops = {
	.owner = THIS_MODULE,
	.open = teadfs_miscdev_open,
	.poll = teadfs_miscdev_poll,
	.release = teadfs_miscdev_release,
	.read = teadfs_miscdev_read,
	.write = teadfs_miscdev_write,
	.llseek = noop_llseek
};

static struct miscdevice teadfs_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "teadfs",
	.fops = &teadfs_miscdev_fops
};

int teadfs_init_miscdev(void)
{
	int rc;

	LOG_DBG("ENTRY \n");
	teadfs_set_clinet_connect(0);
	rc = misc_register(&teadfs_miscdev);
	if (rc)
		LOG_ERR( "%s: Failed to register miscellaneous device "
			"for communications with userspace daemons; rc = [%d]\n",
			__func__, rc);
	LOG_DBG("LEAVE \n");
	return rc;
}

/**
 * teadfs_destroy_miscdev
 *
 * All of the daemons must be exorcised prior to calling this
 * function.
 */
void teadfs_destroy_miscdev(void)
{
	LOG_DBG("ENTRY \n");
	misc_deregister(&teadfs_miscdev);
	LOG_DBG("LEAVE \n");
}
