#include "global_param.h"
#include "teadfs_log.h"

struct global_param {
	struct mutex mux;
	__u64 unique_id;
	pid_t pid

} global_param;


static struct comm_msg_queue g_comm_msg_queue;


//init
int teadfs_init_global_param(void) {
	int rc = 0;

	LOG_DBG("ENTRY\n");

	mutex_init(&(global_param.mux));
	// 0 cann't use
	global_param.unique_id = 1;


	mutex_init(&(g_comm_msg_queue.mux));
	INIT_LIST_HEAD(&(g_comm_msg_queue.msg_ctx_queue));

	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

//release
void teadfs_release_global_param(void) {

}

__u64 teadfs_get_next_msg_id(void) {
	__u64 msg_id;
	mutex_lock(&(global_param.mux));
	global_param.unique_id++;

	if (0 == global_param.unique_id) {
		global_param.unique_id = 1;
	}
	msg_id = global_param.unique_id;
	mutex_unlock(&(global_param.mux));
	return msg_id;
}

struct comm_msg_queue* teadfs_get_msg_queue(void) {
	return &g_comm_msg_queue;
}

//user process
pid_t teadfs_get_client_pid(void) {
	pid_t pid;
	mutex_lock(&(global_param.mux));
	pid = global_param.pid ;
	mutex_unlock(&(global_param.mux));
	return  pid;
}

void teadfs_set_client_pid(pid_t pid) {
	mutex_lock(&(global_param.mux));
	global_param.pid = pid;
	mutex_unlock(&(global_param.mux));
}