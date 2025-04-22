
#include "thread_pool.h"
#include "netlink.h"
#include "request.h"
#include "lib_tead_fs.h"

#include <iostream>
#include <protocol.h>
#include <memory.h>


#define DATA_EXTENSION_SIZE 32


std::shared_ptr<TEAD::CThreadPool<std::string>> g_ptrThreadPool;

std::shared_ptr<CNetlinkInfo> g_ptrNetlink;

struct TEAFS_DEAL_CB g_deal_db;

static void deal_teadfs_msg(teadfs_packet_info* pPacketInfo) {
	teadfs_packet_info* pResponsePacketInfo;
	int nRstSize = 0;
	std::string binResponseData;

	switch (pPacketInfo->header.msg_type) {
	case PR_MSG_OPEN: {
		int nCode;
		std::string strFilePath;
		strFilePath.resize(pPacketInfo->data.open.file_path.size + 1);
		strFilePath.at(pPacketInfo->data.open.file_path.size) = '\0';
		strFilePath.resize(pPacketInfo->data.open.file_path.size);
		strFilePath.assign((char*)pPacketInfo + pPacketInfo->data.open.file_path.offset, pPacketInfo->data.open.file_path.size);
		if (g_deal_db.open) {
			nCode = g_deal_db.open(pPacketInfo->data.open.file_id
				, pPacketInfo->header.pid
				, (char*)strFilePath.data()
			);
		}
		binResponseData.resize(sizeof(teadfs_packet_info));
		pResponsePacketInfo = (teadfs_packet_info*)binResponseData.data();
		pResponsePacketInfo->header = pPacketInfo->header;
		pResponsePacketInfo->data.code.error_code = nCode;
	}
		break;
	case PR_MSG_RELEASE: {
		int nCode;
		std::string strFilePath;
		strFilePath.resize(pPacketInfo->data.open.file_path.size + 1);
		strFilePath.at(pPacketInfo->data.open.file_path.size) = '\0';
		strFilePath.resize(pPacketInfo->data.open.file_path.size);
		strFilePath.assign((char*)pPacketInfo + pPacketInfo->data.open.file_path.offset, pPacketInfo->data.open.file_path.size);
		if (g_deal_db.release) {
			nCode = g_deal_db.release(pPacketInfo->data.release.file_id
				, pPacketInfo->header.pid
				, (char*)strFilePath.data()
			);
		}
		binResponseData.resize(sizeof(teadfs_packet_info));
		pResponsePacketInfo = (teadfs_packet_info*)binResponseData.data();
		pResponsePacketInfo->header = pPacketInfo->header;
		pResponsePacketInfo->data.code.error_code = nCode;
	}
		break;
	case PR_MSG_READ: {
		std::string binDstData;
		uint32_t nDstSize = pPacketInfo->data.write.write_data.size + DATA_EXTENSION_SIZE;
		binDstData.resize(nDstSize);
		if (g_deal_db.read) g_deal_db.read(pPacketInfo->data.read.offset
			, pPacketInfo->data.read.read_data.size
			, (char*)pPacketInfo + pPacketInfo->data.read.read_data.offset
			, &nDstSize
			, (char*)binDstData.data()
		);
		binResponseData.resize(sizeof(teadfs_packet_info) + nDstSize);
		pResponsePacketInfo = (teadfs_packet_info*)binResponseData.data();
		pResponsePacketInfo->header = pPacketInfo->header;
		pResponsePacketInfo->data.read.code = 0;
		pResponsePacketInfo->data.read.read_data.size = nDstSize;
		pResponsePacketInfo->data.read.read_data.offset = sizeof(teadfs_packet_info);
		memcpy((char *)binResponseData.data() + sizeof(teadfs_packet_info), binDstData.data(), nDstSize);
	}
		break;
	case PR_MSG_WRITE: {
		std::string binDstData;
		uint32_t nDstSize = pPacketInfo->data.write.write_data.size + DATA_EXTENSION_SIZE;
		binDstData.resize(nDstSize);
		if (g_deal_db.write) g_deal_db.write(pPacketInfo->data.write.offset
			, pPacketInfo->data.write.write_data.size,
			(char*)pPacketInfo + pPacketInfo->data.write.write_data.offset
			, &nDstSize
			, (char*)binDstData.data()
		);
		binResponseData.resize(sizeof(teadfs_packet_info) + nDstSize);
		pResponsePacketInfo = (teadfs_packet_info*)binResponseData.data();
		pResponsePacketInfo->header = pPacketInfo->header;
		pResponsePacketInfo->data.write.code = 0;
		pResponsePacketInfo->data.write.write_data.size = nDstSize;
		pResponsePacketInfo->data.write.write_data.offset = sizeof(teadfs_packet_info);
		memcpy((char *)binResponseData.data() + sizeof(teadfs_packet_info), binDstData.data(), nDstSize);
	}
		break;
	case PR_MSG_CLEANUP:
		if (g_deal_db.cleanup) g_deal_db.cleanup(pPacketInfo->data.cleanup.file_id);
		break;
	default:
		break;
	}
	g_ptrNetlink->SendMsg(binResponseData.size(), binResponseData.data());
}

static void thread_pool_cb_func(std::shared_ptr<std::string> ptr) {
	printf("dddd\n");
	teadfs_packet_info* pPacketInfo = (teadfs_packet_info*)ptr->data();
	//user mode request kernel
	if (1 == pPacketInfo->header.initiator) {
		CRequestInfo::ResponseMsg(pPacketInfo->header.msg_id, ptr);
	} else {
		deal_teadfs_msg(pPacketInfo);
	}
	return;
}

void netlink_rcv_cb_func(std::shared_ptr<std::string> ptr) {
	g_ptrThreadPool->AddTask(ptr);
}

int StartTEADFS(struct TEAFS_DEAL_CB cb) {
	int nRst = 0;
	// set deal function
	g_deal_db = cb;
	// start thead pool. and set thread pool callback
	g_ptrThreadPool = std::make_shared<TEAD::CThreadPool<std::string>>(thread_pool_cb_func);

	//start netlink to kernel. and set get message callback function
	g_ptrNetlink = std::make_shared<CNetlinkInfo>();
	nRst = g_ptrNetlink->StartNetlink(netlink_rcv_cb_func);
	if (nRst < 0) {
		return -1;
	}
	//send hello to kernel
	CRequestInfo requestInfo(g_ptrNetlink);
	requestInfo.SendHello(nullptr);

	return 1;
}