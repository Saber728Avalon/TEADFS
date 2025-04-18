#include "netlink.h"
#include "teadfs_log.h"
#include "protocol.h"

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
//#include <asm/semaphore.h>
#include <net/sock.h>

static struct sock* nlfd;

struct
{
	__u32 pid;
	rwlock_t lock;
}user_proc;

static void teadfs_netlink_receive(struct sk_buff *skb)
{
	struct packet_info* nlh = nlmsg_hdr((skb);

	LOG_DBG("ENTRY\n");
	

	LOG_DBG("LEVAL\n");
}

static int send_to_user(struct packet_info* info)
{
	int ret;
	int size;
	unsigned char* old_tail;
	struct sk_buff* skb;
	struct nlmsghdr* nlh;
	struct packet_info* packet;

	size = NLMSG_SPACE(sizeof(*info));

	skb = alloc_skb(size, GFP_ATOMIC);
	old_tail = skb->tail;

	nlh = NLMSG_PUT(skb, 0, 0, IMP2_K_MSG, size - sizeof(*nlh));
	packet = NLMSG_DATA(nlh);
	memset(packet, 0, sizeof(struct packet_info));

	packet->src = info->src;
	packet->dest = info->dest;

	nlh->nlmsg_len = skb->tail - old_tail;
	NETLINK_CB(skb).dst_groups = 0;

	read_lock_bh(&user_proc.lock);
	ret = netlink_unicast(nlfd, skb, user_proc.pid, MSG_DONTWAIT);
	read_unlock_bh(&user_proc.lock);

	return ret;
}

static unsigned int get_icmp(unsigned int hook,
	struct sk_buff** pskb,
	const struct net_device* in,
	const struct net_device* out,
	int (*okfn)(struct sk_buff*))
{
	struct iphdr* iph = (*pskb)->nh.iph;
	struct packet_info info;

	if (iph->protocol == IPPROTO_ICMP)
	{
		read_lock_bh(&user_proc.lock);
		if (user_proc.pid != 0)
		{
			read_unlock_bh(&user_proc.lock);
			info.src = iph->saddr;
			info.dest = iph->daddr;
			send_to_user(&info);
		}
		else
			read_unlock_bh(&user_proc.lock);
	}

	return NF_ACCEPT;
}

static struct nf_hook_ops imp2_ops =
{
  .hook = get_icmp,
  .pf = PF_INET,
  .hooknum = NF_IP_PRE_ROUTING,
  .priority = NF_IP_PRI_FILTER - 1,
};

int start_netlink(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = teadfs_netlink_receive
	};

	LOG_DBG("ENTRY\n");
	rwlock_init(&user_proc.lock);

	nlfd = netlink_kernel_create(NL_IMP2, &cfg);
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

