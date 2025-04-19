#include "netlink.h"
#include "teadfs_log.h"
#include "protocol.h"
#include "mem.h"

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


extern struct net init_net;


static struct sock* nlfd;

struct
{
	__u32 pid;
	rwlock_t lock;
}user_proc;

static void teadfs_netlink_receive(struct sk_buff *skb) {
	struct teadfs_packet_info* nlh = nlmsg_hdr(skb);

	LOG_DBG("ENTRY\n");
	

	LOG_DBG("LEVAL\n");
}

static int send_to_user(char* data, int size) {
	int rc;
	unsigned char* old_tail;
	struct sk_buff* skb;
	struct nlmsghdr* nlh;
	struct packet_info* packet;

	LOG_DBG("ENTRY\n");
	//alloc netlink memory
	skb = nlmsg_new(size, GFP_ATOMIC);
	//copy data
	nlh = nlmsg_put(skb, 0, 0, 0, size, 0);
	memcpy(nlmsg_data(nlh), data, size);
	//send
	read_lock_bh(&user_proc.lock);
	rc = netlink_unicast(nlfd, skb, user_proc.pid, MSG_DONTWAIT);
	read_unlock_bh(&user_proc.lock);

	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}

int start_netlink(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = teadfs_netlink_receive
	};

	LOG_DBG("ENTRY\n");
	rwlock_init(&user_proc.lock);

	nlfd = netlink_kernel_create(&init_net, NETLINK_USERSOCK, &cfg);
	if (IS_ERR(nlfd))
	{
		LOG_ERR("can not create a netlink socket\n");
		return -1;
	}

	LOG_DBG("LEVAL\n");
	return 0;
}

void release_netlink(void)
{
	LOG_DBG("ENTRY\n");
	netlink_kernel_release(nlfd);
	LOG_DBG("LEVAL\n");
}

