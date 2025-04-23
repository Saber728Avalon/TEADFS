#ifndef __GLOBAL_PARAM_H___
#define __GLOBAL_PARAM_H___


#include <linux/fs.h>

struct comm_msg_queue {
	struct mutex mux;
	struct list_head msg_ctx_queue;
};

//init
int teadfs_init_global_param(void);

//release
void teadfs_release_global_param(void);

// get msg id. id will auto increment
__u64 teadfs_get_next_msg_id(void);

//get list of send to user msg
struct comm_msg_queue* teadfs_get_msg_queue(void);

//user process
pid_t teadfs_get_client_pid(void);
void teadfs_set_client_pid(pid_t pid);

int  teadfs_get_client_connect(void);
void teadfs_set_clinet_connect(int connect);
#endif


