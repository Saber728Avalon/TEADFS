# TEADFS

english:

	linux  Transparent Encryption and Decryption
	Labeling the file header position
	The main functions achieved are as follows:
		File Transparent Encryption and Decryption
		double file page cache
		make label in release
		Does not affect files or directories opened before mountin
		
	directory
		TEADFS: Transparent Encryption and Decryption Kernel Module
		TEADFS-utils: Client and kernel module communiate
		test: test demo.  we can make label file. and tell kernel module file access type.

	usage:
		load kernel module And Mount directory:
			insmod TEADFS.ko 
			mount -t teadfs /test /test
		client application:
			 ./test


chinese:
	在文件头位置打上标签的方式,实现对文件的透明加解密。主要参考了fuse encryptfs.
	主要实现了如下功能:
		文件透明加解密
		文件双缓冲
		关闭打标
		不影响挂载前打开的文件或者目录

	目录
		TEADFS: 透明加解密驱动
		TEADFS-utils: 与驱动通讯功能。
		test:测试代码.包含关闭文件的打标。以及提示内核文件的访问访问方式.

	使用方式
		加载挂载驱动:
		 insmod TEADFS.ko 
		 mount -t teadfs /test /test  
		 
		启用用户层:
		 ./test