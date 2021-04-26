#
# Makefile for the linux pstorefs routines.
#

obj-y += pstore.o

pstore-objs += inode.o platform.o
obj-$(CONFIG_PSTORE_FTRACE)	+= ftrace.o
#ifdef VENDOR_EDIT
#//Geliang.Tang@Swdp.Android.OppoDebug.Pstore, 2015/12/14, add pmsg
obj-$(CONFIG_PSTORE_PMSG)    	+= pmsg.o
#endif /* VENDOR_EDIT */

ramoops-objs += ram.o ram_core.o
obj-$(CONFIG_PSTORE_RAM)	+= ramoops.o
