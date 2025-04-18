#ifndef __PROTOCOL_H___
#define __PROTOCOL_H___

#define IMP2_U_PID   0
#define IMP2_K_MSG   1
#define IMP2_CLOSE   2

#define NL_IMP2      31

struct packet_header {
	__u64 msg_id;
	__u8  msg_type;
	__u8 msg_status;
};

struct packet_info
{
	packet_header header;
	__u32 dest;
};


#endif


