#pragma once

#include <memory>
#include <thread>
#include <sys/socket.h>
#include <functional>
#include <linux/netlink.h>

typedef std::function<void(std::shared_ptr<std::string>)> netlink_handler;

class CNetlinkInfo
{
public:
	CNetlinkInfo();
	~CNetlinkInfo();
	//start
	int StartNetlink(netlink_handler handler);
	//end
	int  CloseNetlink();

	//send msg to kernel
	int SendMsg(int nDataSize, const char *pData);

private:
	void ThreadRcv();
private:
	//
	int m_skfd;
	//
	std::shared_ptr<std::thread> m_ptrThread;
	//
	bool m_bExist;
	//
	struct sockaddr_nl m_local;
	//
	netlink_handler m_handler;
};

