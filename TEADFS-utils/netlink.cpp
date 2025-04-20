#include "netlink.h"

#include <memory.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>
#include <sys/mman.h>

#define NETLINK_TEADFS 25

#define MAX_MSG_SIZE (8 * 1024)

CNetlinkInfo::CNetlinkInfo() {
    //
    m_skfd = -1;
    //
    m_bExist = false;
    //create thread
    m_ptrThread = std::make_shared<std::thread>([&]() {
        ThreadRcv();
        });
    m_ptrThread->detach();
}

CNetlinkInfo::~CNetlinkInfo() {
    CloseNetlink();
}

//start, connect to kernel
int CNetlinkInfo::StartNetlink(netlink_handler handler) {
    //struct nl_mmap_req req;

    m_handler = handler;
    //create socket
    m_skfd = socket(PF_NETLINK, SOCK_RAW, NETLINK_TEADFS);
    if (m_skfd < 0) {
        return -1;
    }
    memset(&m_local, 0, sizeof(m_local));
    m_local.nl_family = AF_NETLINK;
    m_local.nl_pid = getpid();
    m_local.nl_groups = 0;

    if (bind(m_skfd, (struct sockaddr*)&m_local, sizeof(m_local)) != 0) {
        return -2;
    }
    ///* Memory mapped Netlink operation request */
    //req.nm_block_size = r.blk_sz;
    //req.nm_block_nr = (unsigned int)r.ring_sz / r.blk_sz;
    //req.nm_frame_size = NL_FR_SZ;
    //req.nm_frame_nr = r.ring_sz / NL_FR_SZ;
    ////
    //if (setsockopt(m_skfd, SOL_NETLINK, NETLINK_RX_RING, &req, sizeof(req)) < 0)
    //    return -3;
    //if (setsockopt(m_skfd, SOL_NETLINK, NETLINK_TX_RING, &req, sizeof(req)) < 0)
    //    return -4;

    return 0;
}


int  CNetlinkInfo::CloseNetlink() {
    m_bExist = true;
    close(m_skfd);
    m_skfd = -1;
}


//send msg to kernel
int CNetlinkInfo::SendMsg(int nDataSize, const char* pData) {
    struct nlmsghdr* nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(nDataSize));
    if (NULL == nlh) {
        return -1;
    }
    nlh->nlmsg_len = NLMSG_LENGTH(nDataSize);
    nlh->nlmsg_pid = 0;
    nlh->nlmsg_flags = 0;
    memcpy(NLMSG_DATA(nlh), pData, nDataSize);

    struct iovec iov;
    iov.iov_base = nlh;
    iov.iov_len = nlh->nlmsg_len;

    struct sockaddr_nl destAddr;
    destAddr.nl_family = AF_NETLINK;
    destAddr.nl_pid = 0; //B：设置目的端口号
    destAddr.nl_groups = 0;
    sendto(m_skfd, nlh, nlh->nlmsg_len, 0,
        (struct sockaddr*)&destAddr, sizeof(destAddr));
}

void CNetlinkInfo::ThreadRcv() {
    int nRead = 0;
    struct sockaddr_nl destAddr;
    destAddr.nl_family = AF_NETLINK;
    destAddr.nl_pid = 0; //B：设置目的端口号
    destAddr.nl_groups = 0;

    while (!m_bExist)
    {
        try {
            struct pollfd pfd = { 0 };
            pfd.fd = m_skfd;
            pfd.events = POLLIN | POLLERR;

            int nRst = poll(&pfd, 1, 5000);
            if (nRst < 0) { //error
                m_bExist = true;
                break;
            }
            else if (0 == nRst) { //timeout
                continue;
            }
            std::shared_ptr<std::string> ptrMsg = std::make_shared<std::string>();
            if (!ptrMsg) {
                continue;
            }
            struct nlmsghdr* nlh = (struct nlmsghdr*)malloc(NLMSG_SPACE(MAX_MSG_SIZE));
            memset(nlh, 0, sizeof(struct nlmsghdr));
            nlh->nlmsg_len = NLMSG_SPACE(MAX_MSG_SIZE);
            nlh->nlmsg_flags = 0;
            nlh->nlmsg_type = 0;
            nlh->nlmsg_seq = 0;
            nlh->nlmsg_pid = getpid();

            //get msg size
            //int nMsgSize = 0;
            //socklen_t nLocalSize = sizeof(struct sockaddr_nl);
            //nRead = recvfrom(m_skfd, &nMsgSize, sizeof(nMsgSize),
            //    0, (struct sockaddr*)&destAddr, &nLocalSize);
            //if (sizeof(int) != nRead) {
            //    continue;
            //}
            ////get msg
            //ptrMsg->resize(nMsgSize);
            //memcpy((void *)ptrMsg->data(), &nMsgSize, sizeof(nMsgSize));
            //nRead = recvfrom(m_skfd, (void *)ptrMsg->data() + sizeof(nMsgSize), nMsgSize - sizeof(nMsgSize),
            //    0, (struct sockaddr*)&destAddr, &nLocalSize);
            
            socklen_t nLocalSize = sizeof(struct sockaddr_nl);
            nRead = recvfrom(m_skfd, (void *)nlh, nlh->nlmsg_len,
               0, (struct sockaddr*)&destAddr, &nLocalSize);
            //deal msg
            if (m_handler) {
                ptrMsg->resize(nRead);
                memcpy((void *)ptrMsg->data(), NLMSG_DATA(nlh), nRead);
                m_handler(ptrMsg);
            }
        }
        catch (std::exception& e) {

        }
        
    }
}