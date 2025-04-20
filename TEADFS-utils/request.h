#pragma once


#include "netlink.h"

#include <memory>
#include <thread>
#include <sys/socket.h>
#include <functional>
#include <map>
#include <mutex>

typedef std::function<void(std::shared_ptr<std::string> ptr)> response_handler;

class CRequestInfo
{
public:
	CRequestInfo(std::shared_ptr<CNetlinkInfo> ptr);
	~CRequestInfo();

public:
	int SendHello(response_handler handler);

	static void ResponseMsg(uint64_t u64, std::shared_ptr<std::string> ptr);
private:
	std::shared_ptr<CNetlinkInfo> m_ptrNetlink;
	
	static std::mutex s_mutex;
	static std::map<uint64_t, response_handler> s_map;
};

