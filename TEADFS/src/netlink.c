#include "netlink.h"
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


#define NETLINK_TEADFS 25

extern struct net init_net;

static struct sock* nlfd;

struct
{
	__u32 pid;
	rwlock_t lock;
}user_proc;

static void teadfs_user_request_kernel(struct teadfs_packet_info* packet_info) {
	struct teadfs_packet_info response_packet_info;

	LOG_DBG("ENTRY\n");
	switch (packet_info->header.msg_type)
	{
	case PR_MSG_HELLO: {
		response_packet_info.header = packet_info->header;
		response_packet_info.header.size = sizeof(struct teadfs_packet_info);
		response_packet_info.data.code.error_code = 0;

		LOG_DBG("hello: client pid :%d\n", packet_info->data.hello.pid);
		teadfs_set_client_pid(packet_info->data.hello.pid);
		teadfs_send_to_user((char*)&response_packet_info, response_packet_info.header.size);
	}
		break;
	case PR_MSG_CLOSE:
		LOG_DBG("close: client\n");
		teadfs_set_client_pid(0);
		break;
	default:
		break;
	}
	LOG_DBG("LEVAL\n");
}


static void teadfs_netlink_receive(struct sk_buff *skb) {
	struct nlmsghdr* nlmsghdr = nlmsg_hdr(skb);
	struct teadfs_msg_ctx *msg_ctx = NULL;
	struct teadfs_packet_info *packet_info = nlmsg_data(nlmsghdr);

	LOG_DBG("ENTRY, size:%d\n", nlmsghdr->nlmsg_len);
	// kernel request to user 
	if (1 == packet_info->header.initiator) {
		teadfs_user_request_kernel(packet_info);
	} else { // user request to kernel
		// ±éÀúÁ´±í
		mutex_lock(&teadfs_get_msg_queue()->mux);
		list_for_each_entry(msg_ctx, &(teadfs_get_msg_queue()->msg_ctx_queue), out_list) {
			if (packet_info->header.msg_id == msg_ctx->msg_id) {
				LOG_DBG("Find Msg\n");
				mutex_lock(&msg_ctx->mux);
				do {
					//msg is success
					msg_ctx->state = TEADFS_MSG_CTX_STATE_DONE;
					msg_ctx->response_msg = teadfs_zalloc(nlmsghdr->nlmsg_len, GFP_KERNEL);
					if (!msg_ctx->response_msg) {
						LOG_ERR("Alloc Mem Fail \n");
						break;
					}
					msg_ctx->response_msg_size = nlmsghdr->nlmsg_len;
					memcpy(msg_ctx->response_msg, packet_info, msg_ctx->response_msg_size);
				} while (0);
				LOG_DBG("Find Msg msg_ctx->state:%d\n", msg_ctx->state);
				mutex_unlock(&msg_ctx->mux);
				break;
			}
		}
		mutex_unlock(&teadfs_get_msg_queue()->mux);
	}
	

	LOG_DBG("LEVAL\n");
}

int teadfs_send_to_user(char* data, int size) {
	int rc = 0;
	unsigned char* old_tail;
	struct sk_buff* skb;
	struct nlmsghdr* nlh;
	struct teadfs_packet_info* packet;

	LOG_DBG("ENTRY\n");
	do {
		//alloc netlink memory
		skb = nlmsg_new(size, GFP_ATOMIC);
		if (!skb) {
			break;
		}
		//copy data
		nlh = nlmsg_put(skb, 0, 0, 0, size, 0);
		nlh->nlmsg_len = NLMSG_LENGTH(size);
		nlh->nlmsg_pid = teadfs_get_client_pid();
		nlh->nlmsg_flags = 0;

		NETLINK_CB(skb).portid = 0;
		NETLINK_CB(skb).dst_group = 0;
		memcpy(nlmsg_data(nlh), data, size);
		//send
		read_lock_bh(&user_proc.lock);
		rc = netlink_unicast(nlfd, skb, teadfs_get_client_pid(), MSG_DONTWAIT);
		read_unlock_bh(&user_proc.lock);
	} while (0);
	LOG_DBG("LEAVE rc = [%d]\n", rc);
	return rc;
}

int teadfs_start_netlink(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = teadfs_netlink_receive
	};

	LOG_DBG("ENTRY\n");
	rwlock_init(&user_proc.lock);

	nlfd = netlink_kernel_create(&init_net, NETLINK_TEADFS, &cfg);
	if (IS_ERR(nlfd))
	{
		LOG_ERR("can not create a netlink socket\n");
		return -1;
	}

	LOG_DBG("LEVAL\n");
	return 0;
}

void teadfs_release_netlink(void)
{
	LOG_DBG("ENTRY\n");
	netlink_kernel_release(nlfd);
	LOG_DBG("LEVAL\n");
}

