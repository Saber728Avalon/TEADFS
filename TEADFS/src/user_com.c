#include "user_com.h"
#include "mem.h"
#include "teadfs_log.h"

#include <linux/fs.h>



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
		rc = wait_event_timeout(&ctx->wait, ctx->state == TEADFS_MSG_CTX_STATE_DONE, msecs_to_jiffies(30000));
		if (!rc) {
			LOG_ERR("wait_event_timeout.\n");
			mutex_lock(&(ctx->mux));
			ctx->state = TEADFS_MSG_CTX_STATE_DONE;
			ctx->response_msg_size = 0;
			ctx->response_msg = NULL;
			mutex_unlock(&(ctx->mux));
			break;
		}
	}
	LOG_DBG("LEVAL rc:%d\n", rc);
	return;
}

static int teadfs_request_send(size_t request_size, char* request_data, size_t* response_size, char** response_data) {
	int rc = 0;

	LOG_DBG("ENTRY\n");
	do {
		// alloc memory
		struct teadfs_msg_ctx* ctx = teadfs_zalloc(sizeof(struct teadfs_msg_ctx), GFP_KERNEL);
		if (ctx) {
			rc = -ENOMEM;
			break;
		}
		// init struct teadfs_msg_ctx
		ctx->state = TEADFS_MSG_CTX_STATE_PENDING;
		ctx->request_msg_size = request_size;
		ctx->request_msg = request_data;
		ctx->response_msg_size = 0;
		ctx->response_msg = NULL;
		mutex_init(&(ctx->mux));
		init_waitqueue_head(&(ctx->wait));

		//request R3 answer
		teadfs_request_wait_answer(ctx);

		//result
		*response_size = ctx->response_msg_size;
		*response_data = ctx->response_msg;
	} while (0);
	LOG_DBG("LEVAL rc:%d\n", rc);
	return rc;
}

int teadfs_request_open(struct file* file) {

}