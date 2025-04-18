#ifndef __NETLINK_H___
#define __NETLINK_H___


struct teadfs_msg_ctx {
#define TEADFS_MSG_CTX_STATE_FREE     0x01
#define TEADFS_MSG_CTX_STATE_PENDING  0x02
#define TEADFS_MSG_CTX_STATE_DONE     0x03
#define TEADFS_MSG_CTX_STATE_NO_REPLY 0x04
	u8 state;
	size_t msg_size;
	char* msg;
	struct list_head node;
	struct list_head daemon_out_list;
	struct mutex mux;
};

// create netlink. R3 communite
int start_netlink(void);

// release netlink
void release_netlink(void);
#endif


