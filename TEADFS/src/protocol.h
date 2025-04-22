#ifndef __PROTOCOL_H___
#define __PROTOCOL_H___

#if defined(__KERNEL__)
	#include <linux/fs.h>
	#include <linux/types.h>
#else
	#include <iostream>
	#include <linux/stat.h>
	
	#define kuid_t uid_t
	#define kgid_t gid_t
#endif


#define PR_MSG_HELLO 1
#define PR_MSG_CLOSE 2

//start self defined type .
#define PR_MSG_USER 3

#define PR_MSG_OPEN			(PR_MSG_USER + 1)
#define PR_MSG_RELEASE		(PR_MSG_USER + 2)
#define PR_MSG_READ			(PR_MSG_USER + 3)
#define PR_MSG_WRITE		(PR_MSG_USER + 4)
#define PR_MSG_CLEANUP		(PR_MSG_USER + 5)



#define ENCRYPT_FILE_HEADER_SIZE 256

enum OPEN_FILE_RESULT {
	OFR_INIT = 1,
	OFR_PROHIBIT,  // prohibit access file
	OFR_ENCRYPT,// file be encrypt, and access by decrypt
	OFR_DECRYPT, // file be encrypt, but access by decrypt

	OFR_COUNT
};

enum RELEASE_FILE_RESULT {
	RFR_NORMAL = 1, // Do not take any action
	RFR_ENCRYPT, // encrypt file.

	RFR_COUNT
};

struct teadfs_protocol_binary {
	__u32 size;
	__u32 offset; //data offset in buffer start
};

struct teadfs_packet_header {
	__u32 size;
	// unique msg id
	__u64 msg_id;
	//type
	__u8  msg_type; 
	//if requester is kernel set 0, if requester is user mode set 1. to support duplex and asynchronous
	__u8 initiator;
	//current process id
	pid_t pid;
	//current process user
	kuid_t uid;
	//current process group
	kgid_t gid;
};

struct teadfs_hello_info {
	//user process pid
	pid_t pid;
};

struct teadfs_open_info {
	//unique open file, likely struct file;
	__u64 file_id;
	// file_path
	struct teadfs_protocol_binary file_path;
};

struct teadfs_release_info {
	//unique open file, likely struct file;
	__u64 file_id;
	// file_path
	struct teadfs_protocol_binary file_path;
};


struct teadfs_read_info {
	int code; //result code
	//read file data offset
	__u64 offset;
	// file data
	struct teadfs_protocol_binary read_data;
};



struct teadfs_write_info {
	int code; //result code
	//write file data offset
	__u64 offset;
	// file data
	struct teadfs_protocol_binary write_data;
};

struct teadfs_cleanup_info {
	//unique open file, likely struct file;
	__u64 file_id;
};

struct teadfs_result_code_info {
	// file data
	int error_code;
};

struct teadfs_packet_info
{
	struct teadfs_packet_header header;
	union data
	{
		struct teadfs_hello_info hello;
		struct teadfs_open_info open;
		struct teadfs_release_info release;
		struct teadfs_read_info read;
		struct teadfs_write_info write;
		struct teadfs_result_code_info code;
		struct teadfs_cleanup_info cleanup;
	} data;
};


#endif


