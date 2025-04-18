#ifndef __PROTOCOL_H___
#define __PROTOCOL_H___


#define PR_MSG_HELLO 1
#define PR_MSG_CLOSE 2

//start self defined type .
#define PR_MSG_USER 3

#define PR_MSG_OPEN			(PR_MSG_USER + 1)
#define PR_MSG_RELEASE		(PR_MSG_USER + 2)
#define PR_MSG_READ			(PR_MSG_USER + 3)
#define PR_MSG_WRITE		(PR_MSG_USER + 4)



enum OPEN_FILE_RESULT {
	OFR_INIT,
	OFR_ENCRYPT,// file be encrypt, and access by decrypt
	OFR_DECRYPT, // file be encrypt, but access by decrypt

	OFR_COUNT
};

struct protocol_binary {
	__u32 size;
	char* data;
};

struct packet_header {
	// unique msg id
	__u64 msg_id;
	//type
	__u8  msg_type;
	//current process id
	pid_t pid;
	//current process user
	kuid_t uid;
	//current process group
	kgid_t gid;
};


struct open_info {
	//unique open file, likely struct file;
	__u64 file_id;
	// file_path
	struct protocol_binary file_path;
};

struct release_info {
	//unique open file, likely struct file;
	__u64 file_id;
	// file_path
	struct protocol_binary file_path;
};


struct read_info {
	// file data
	struct protocol_binary read_data;
};



struct write_info {
	// file data
	struct protocol_binary write_data;
};

struct packet_info
{
	struct packet_header header;
	union data
	{
		struct open_info open;
		struct release_info release;
		struct read_info read;
		struct write_info write;
	};
};


#endif


