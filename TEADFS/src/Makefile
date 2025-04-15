MOD = TEADFS
KPATH :=/lib/modules/$(shell uname -r)/build
PWD :=$(shell pwd)
obj-m += $(MOD).o
$(MOD)-objs := teadfs_log.o inode.o super.o lookup.o mmap.o dentry.o file.o

all:
	echo "$($(MOD)-objs)"
	$(MAKE) -C $(KPATH) M=$(PWD) modules

clean:
	$(MAKE) -C $(KPATH) M=$(PWD) clean

insmod: all
	sudo rmmod $(MOD).ko; true
	sudo insmod $(MOD).ko

log: 
	tail -f /var/log/messages