MOD = TEADFS
ifndef KPATH
	KPATH :=/lib/modules/$(shell uname -r)/build
endif
PWD :=$(shell pwd)
obj-m += $(MOD).o
$(MOD)-y := main.o teadfs_log.o inode.o super.o lookup.o mmap.o dentry.o file.o netlink.o user_com.o global_param.o
ccflags-y = -D__KERNEL__ -DMODULE -O0 -Wall -fstack-protector


all:
	echo "$($(MOD)-y)"
	$(MAKE) -C $(KPATH) M=$(PWD) modules

clean:
	$(MAKE) -C $(KPATH) M=$(PWD) clean

insmod: all
	sudo rmmod $(MOD).ko; true
	sudo insmod $(MOD).ko

log: 
	tail -f /var/log/messages