#include "request.h"

#include <protocol.h>
#include <memory.h>
#include <atomic>
#include <unistd.h>
#include "netlink.h"



CRequestInfo::CRequestInfo(std::shared_ptr<CNetlinkInfo> ptr) {
	m_ptrNetlink = ptr;
}

CRequestInfo::~CRequestInfo() {

}

std::mutex CRequestInfo::s_mutex;
std::map<uint64_t, response_handler> CRequestInfo::s_map;

uint64_t get_next_msg_id() {
	static std::atomic<uint64_t> g_next_id(0);
	uint64_t u64;
	g_next_id.fetch_add(1);
	u64 = g_next_id.load();
	if (0 == u64) {
		g_next_id.fetch_add(1);
		u64 = g_next_id.load();
	}
	return u64;
}


int CRequestInfo::SendHello(response_handler handler) {
	
	std::shared_ptr<std::string> ptrBuf = std::make_shared<std::string>();
	ptrBuf->resize(sizeof(teadfs_packet_info));

	teadfs_packet_info *p_packet_info = (teadfs_packet_info*)ptrBuf->data();

	p_packet_info->header.size = sizeof(teadfs_packet_info);
	p_packet_info->header.msg_id = get_next_msg_id();
	p_packet_info->header.msg_type = PR_MSG_HELLO;
	p_packet_info->header.initiator = 1;

	p_packet_info->data.hello.pid = getpid();

	{
		std::lock_guard<std::mutex> lock(s_mutex);
		s_map.emplace(p_packet_info->header.msg_id, handler);
	}
	m_ptrNetlink->SendMsg(ptrBuf->size(), ptrBuf->data());

}

 void CRequestInfo::ResponseMsg(uint64_t msd_id, std::shared_ptr<std::string> ptr) {
	 response_handler handler = nullptr;
	 {
		 std::lock_guard<std::mutex> lock(s_mutex);
		 auto iterFind = s_map.find(msd_id);
		 if (iterFind == s_map.end()) {
			 return;
		 }
		 handler = iterFind->second;
		 s_map.erase(iterFind);
	 }
	 if (handler) handler(ptr);
}