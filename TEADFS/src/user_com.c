#include "user_com.h"
#include "mem.h"
#include "teadfs_log.h"
#include "protocol.h"
#include "global_param.h"
#include "teadfs_header.h"
#include "netlink.h"

#include <linux/fs.h>
#include <linux/sched.h>


// blocked current thead, to wait R3 deal.
static void teadfs_request_wait_answer(struct teadfs_msg_ctx* ctx) {
	int rc = 0;

	LOG_DBG("ENTRY\n");
	do {
		if (TEADFS_MSG_CTX_STATE_NO_REPLY == ctx->state) {
			mutex_lock(&(ctx->mux));
			ctx->state = TEADFS_MSG_CTX_STATE_DONE;
			ctx->response_msg_size = 0;
			ctx->response_msg = NULL;
			mutex_unlock(&(ctx->mux));
			break;
		}
		// wait R3 deal result
		rc = wait_event_timeout(ctx->wait, ctx->state == TEADFS_MSG_CTX_STATE_DONE, 30 * HZ);
		if (!rc) {
			LOG_ERR("wait_event_timeout.\n");
			mutex_lock(&(ctx->mux));
			ctx->state = TEADFS_MSG_CTX_STATE_DONE;
			ctx->response_msg_size = 0;
			ctx->response_msg = NULL;
			mutex_unlock(&(ctx->mux));
			break;
		}
	} while (0);
	LOG_DBG("LEVAL rc:%d\n", rc);
	return;
}

static int teadfs_request_send(__u64 msg_id, size_t request_size, char* request_data, size_t* response_size, char** response_data) {
	int rc = 0;
	struct teadfs_msg_ctx* ctx, *tmp_ctx;

	LOG_DBG("ENTRY\n");
	do {
		// alloc memory
		ctx = teadfs_zalloc(sizeof(struct teadfs_msg_ctx), GFP_KERNEL);
		if (!ctx) {
			rc = -ENOMEM;
			break;
		}
		// init struct teadfs_msg_ctx
		ctx->state = TEADFS_MSG_CTX_STATE_PENDING;
		ctx->msg_id = msg_id;
		ctx->request_msg_size = request_size;
		ctx->request_msg = request_data;
		ctx->response_msg_size = 0;
		ctx->response_msg = NULL;
		mutex_init(&(ctx->mux));
		init_waitqueue_head(&(ctx->wait));

		//add list
		mutex_lock(&teadfs_get_msg_queue()->mux);
		list_add_tail(&ctx->out_list, &teadfs_get_msg_queue()->msg_ctx_queue);
		mutex_unlock(&teadfs_get_msg_queue()->mux);

		teadfs_send_to_user(request_data, request_size);
		//request usr answer
		teadfs_request_wait_answer(ctx);

		//result
		*response_size = ctx->response_msg_size;
		*response_data = ctx->response_msg;

		// delete in list
		mutex_lock(&teadfs_get_msg_queue()->mux);
		list_del(&ctx->out_list);
		mutex_unlock(&teadfs_get_msg_queue()->mux);
		//free mem
		teadfs_free(ctx);
	} while (0);
	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

//add header info
static void teadfs_packet_header(struct teadfs_packet_info* packet, int size, __u8  msg_type, __u8 initiator, pid_t pid, kuid_t uid, kgid_t gid) {
	packet->header.size = size;
	packet->header.msg_id = teadfs_get_next_msg_id();
	packet->header.msg_type = msg_type;
	packet->header.initiator = initiator;
	packet->header.pid = pid;
	packet->header.uid = uid;
	packet->header.gid = gid;
	return;
}

int teadfs_request_open(struct file* file) {
	char* buffer_file_path = NULL;
	char* buffer_packet = NULL;
	int buffer_size = 0;
	char* file_path_start = NULL;
	struct dentry* teadfs_dentry = file->f_path.dentry;
	int rc = 0;
	int file_path_size = 0;
	pid_t kpid = 0;
	struct teadfs_packet_info* packet = NULL;
	char* response_data = NULL;
	size_t response_size = 0;

	LOG_DBG("ENTRY\n");
	do {
		//get current process id
		kpid = task_tgid_vnr(current);
		//ignore client proces
		if (kpid == teadfs_get_client_pid()) {
			rc = -ENOMEM;
			break;
		}
		LOG_DBG("++++++++++++++++cur:%d   client%d++++++++++\n", kpid, teadfs_get_client_pid());
		// get file path
		buffer_file_path = teadfs_zalloc(PATH_MAX, GFP_KERNEL);
		if (!buffer_file_path) {
			rc = -ENOMEM;
			break;
		}
		file_path_start = d_path(&file->f_path, buffer_file_path, PATH_MAX);
		file_path_size = strlen(file_path_start);

		//packet data ro usr
		buffer_size = sizeof(struct teadfs_packet_info) + file_path_size;
		buffer_packet = teadfs_zalloc(sizeof(struct teadfs_packet_info) + file_path_size, GFP_KERNEL);
		if (!buffer_packet) {
			rc = -ENOMEM;
			break;
		}
		packet = (struct teadfs_packet_info *)(buffer_packet);
		//add header info
		teadfs_packet_header(packet, buffer_size, PR_MSG_OPEN, 0, kpid, KUIDT_INIT(0), KGIDT_INIT(0));
		
		packet->data.open.file_id = (__u64)file;
		packet->data.open.file_path.size = file_path_size;
		packet->data.open.file_path.offset = sizeof(struct teadfs_packet_info);
		memcpy(buffer_packet + sizeof(struct teadfs_packet_info), file_path_start, file_path_size);

		LOG_DBG("size:%d, msg_id:0x%llx, msg_type:%d, pid:%d, uid:%d, gid:%d\n"
			, packet->header.size
			, packet->header.msg_id
			, packet->header.msg_type
			, packet->header.pid
			, packet->header.uid
			, packet->header.gid
		);
		LOG_DBG("path:%s", buffer_packet + sizeof(struct teadfs_packet_info));

		//send to usr
		rc = teadfs_request_send(packet->header.msg_id, buffer_size, buffer_packet, &response_size, &response_data);
		if (rc) {
			rc = -ENOMEM;
			break;
		}
		if ((NULL == response_data) || (response_size < sizeof(struct teadfs_packet_info))) {
			LOG_ERR("Get Message Size Error, size:%d\n", response_size);
			rc = -ENOMEM;
			break;
		}
		packet = (struct teadfs_packet_info*)response_data;
		//get file access code. is OPEN_FILE_RESULT
		rc = packet->data.code.error_code;
		//release mem
		if (response_data) {
			teadfs_free(response_data);
		}
		
	} while (0);
	if (buffer_packet) {
		teadfs_free(buffer_packet);
	}
	if (buffer_file_path) {
		teadfs_free(buffer_file_path);
	}
	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

//close file to user mode
int teadfs_request_release(struct file* file) {
	char* buffer_file_path = NULL;
	char* buffer_packet = NULL;
	int buffer_size = 0;
	char* file_path_start = NULL;
	struct dentry* teadfs_dentry = file->f_path.dentry;
	int rc = 0;
	int file_path_size = 0;
	pid_t kpid = 0;
	struct teadfs_packet_info* packet = NULL;
	char* response_data = NULL;
	size_t response_size = 0;

	LOG_DBG("ENTRY\n");
	do {
		//get current process id
		kpid = task_tgid_vnr(current);
		//ignore client proces
		if (kpid == teadfs_get_client_pid()) {
			rc = -ENOMEM;
			break;
		}

		// get file path
		buffer_file_path = teadfs_zalloc(PATH_MAX, GFP_KERNEL);
		if (!buffer_file_path) {
			rc = -ENOMEM;
			break;
		}
		file_path_start = d_path(&file->f_path, buffer_file_path, PATH_MAX);
		file_path_size = strlen(file_path_start);

		//packet data ro usr
		buffer_size = sizeof(struct teadfs_packet_info) + file_path_size;
		buffer_packet = teadfs_zalloc(sizeof(struct teadfs_packet_info) + file_path_size, GFP_KERNEL);
		if (!buffer_packet) {
			rc = -ENOMEM;
			break;
		}
		packet = (struct teadfs_packet_info*)(buffer_packet);
		//add header info
		teadfs_packet_header(packet, buffer_size, PR_MSG_RELEASE, 0, kpid, KUIDT_INIT(0), KGIDT_INIT(0));

		packet->data.release.file_id = (__u64)file;
		packet->data.release.file_path.size = file_path_size;
		packet->data.release.file_path.offset = sizeof(struct teadfs_packet_info);
		memcpy(buffer_packet + sizeof(struct teadfs_packet_info), file_path_start, file_path_size);

		LOG_DBG("size:%d, msg_id:0x%llx, msg_type:%d, pid:%d, uid:%d, gid:%d\n"
			, packet->header.size
			, packet->header.msg_id
			, packet->header.msg_type
			, packet->header.pid
			, packet->header.uid
			, packet->header.gid
		);
		LOG_DBG("path:%s", buffer_packet + sizeof(struct teadfs_packet_info));

		//send to usr
		rc = teadfs_request_send(packet->header.msg_id, buffer_size, buffer_packet, &response_size, &response_data);
		if (rc) {
			rc = -ENOMEM;
			break;
		}
		if ((NULL == response_data) || (response_size < sizeof(struct teadfs_packet_info))) {
			LOG_ERR("Get Message Size Error, size:%d\n", response_size);
			rc = -ENOMEM;
			break;
		}
		packet = (struct teadfs_packet_info*)response_data;
		//get file release code. is RELEASE_FILE_RESULT
		rc = packet->data.code.error_code;
		//release mem
		if (response_data) {
			teadfs_free(response_data);
		}
	} while (0);
	if (buffer_packet) {
		teadfs_free(buffer_packet);
	}
	if (buffer_file_path) {
		teadfs_free(buffer_file_path);
	}
	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}


//close file to user mode
int teadfs_request_read(loff_t offset, const char* src_data, int src_size, char* dst_data, int dst_size) {
	int rc = 0;
	int buffer_size = 0;
	char *buffer = NULL;
	pid_t kpid = 0;
	struct teadfs_packet_info* packet = NULL;
	char* response_data = NULL;
	size_t response_size = 0;

	LOG_DBG("ENTRY\n");
	do {
		//get current process id
		kpid = task_tgid_vnr(current);
		//ignore client proces
		if (kpid == teadfs_get_client_pid()) {
			rc = -ENOMEM;
			break;
		}
		//packet data ro usr
		buffer_size = sizeof(struct teadfs_packet_info) + src_size;
		buffer = teadfs_zalloc(buffer_size, GFP_KERNEL);
		if (!buffer) {
			rc = -ENOMEM;
			break;
		}
		packet = (struct teadfs_packet_info*)(buffer);
		//add header info
		teadfs_packet_header(packet, buffer_size, PR_MSG_READ, 0, kpid, KUIDT_INIT(0), KGIDT_INIT(0));

		packet->data.read.offset = offset;
		packet->data.read.code = 0;
		packet->data.read.read_data.size = src_size;
		packet->data.read.read_data.offset = sizeof(struct teadfs_packet_info);
		memcpy(buffer + sizeof(struct teadfs_packet_info), src_data, src_size);

		LOG_DBG("size:%d, msg_id:0x%llx, msg_type:%d, pid:%d, uid:%d, gid:%d\n"
			, packet->header.size
			, packet->header.msg_id
			, packet->header.msg_type
			, packet->header.pid
			, packet->header.uid
			, packet->header.gid
		);
		//send to usr
		rc = teadfs_request_send(packet->header.msg_id, buffer_size, buffer, &response_size, &response_data);
		if (rc) {
			LOG_ERR("teadfs_request_send, error:%d\n", rc);
			rc = -ENOMEM;
			break;
		}
		if ((NULL == response_data) || (response_size < sizeof(struct teadfs_packet_info))) {
			LOG_ERR("Get Message Size Error, size:%d\n", response_size);
			rc = -ENOMEM;
			break;
		}
		packet = (struct teadfs_packet_info*)response_data;
		//get file release code. is RELEASE_FILE_RESULT
		rc = packet->data.read.code;
		if (rc) {
			LOG_DBG("ENTRY\n");
			rc = -ENOMEM;
			break;
		}
		if (response_size != (sizeof(struct teadfs_packet_info) + packet->data.read.read_data.size)) {
			LOG_DBG("ENTRY response_size:%d, read_data.size:%d\n", response_size, (sizeof(struct teadfs_packet_info) + packet->data.read.read_data.size));
			rc = -ENOMEM;
			break;
		}
		if (dst_size < packet->data.write.write_data.size) {
			LOG_DBG("ENTRY\n");
			rc = -ENOMEM;
			break;
		}
		memcpy(dst_data, (char*)packet + packet->data.read.read_data.offset, packet->data.read.read_data.size);
		//release mem
		if (response_data) {
			teadfs_free(response_data);
		}
		rc = packet->data.read.read_data.size;
	} while (0);

	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}


//close file to user mode
int teadfs_request_write(loff_t offset, const char* src_data, int src_size, char* dst_data, int dst_size) {
	int rc = 0;
	int buffer_size = 0;
	char* buffer = NULL;
	pid_t kpid = 0;
	struct teadfs_packet_info* packet = NULL;
	char* response_data = NULL;
	size_t response_size = 0;

	LOG_DBG("ENTRY\n");
	do {
		//get current process id
		kpid = task_tgid_vnr(current);
		//ignore client proces
		if (kpid == teadfs_get_client_pid()) {
			rc = -ENOMEM;
			break;
		}
		//packet data ro usr
		buffer_size = sizeof(struct teadfs_packet_info) + src_size;
		buffer = teadfs_zalloc(buffer_size, GFP_KERNEL);
		if (!buffer) {
			rc = -ENOMEM;
			break;
		}
		packet = (struct teadfs_packet_info*)(buffer);
		//add header info
		teadfs_packet_header(packet, buffer_size, PR_MSG_WRITE, 0, kpid, KUIDT_INIT(0), KGIDT_INIT(0));

		packet->data.write.offset = offset;
		packet->data.write.code = 0;
		packet->data.write.write_data.size = src_size;
		packet->data.write.write_data.offset = sizeof(struct teadfs_packet_info);
		memcpy(buffer + sizeof(struct teadfs_packet_info), src_data, src_size);

		LOG_DBG("size:%d, msg_id:0x%llx, msg_type:%d, pid:%d, uid:%d, gid:%d\n"
			, packet->header.size
			, packet->header.msg_id
			, packet->header.msg_type
			, packet->header.pid
			, packet->header.uid
			, packet->header.gid
		);
		//send to usr
		rc = teadfs_request_send(packet->header.msg_id, buffer_size, buffer, &response_size, &response_data);
		if (rc) {
			rc = -ENOMEM;
			break;
		}
		if ((NULL == response_data) || (response_size < sizeof(struct teadfs_packet_info))) {
			LOG_ERR("Get Message Size Error, size:%d\n", response_size);
			rc = -ENOMEM;
			break;
		}
		packet = (struct teadfs_packet_info*)response_data;
		//get file release code. is RELEASE_FILE_RESULT
		rc = packet->data.write.code;
		if (rc) {
			rc = -ENOMEM;
			break;
		}
		if (response_size != (sizeof(struct teadfs_packet_info) + packet->data.write.write_data.size)) {
			rc = -ENOMEM;
			break;
		}
		if (dst_size < packet->data.write.write_data.size) {
			rc = -ENOMEM;
			break;
		}
		memcpy(dst_data, (char*)packet + packet->data.write.write_data.offset, packet->data.write.write_data.size);
		//release mem
		if (response_data) {
			teadfs_free(response_data);
		}
		rc = packet->data.write.write_data.size;
	} while (0);

	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}