# AnyKernel3 Ramdisk Mod Script
# osm0sis @ xda-developers
# begin properties
properties() { '
kernel.string=OPPO A37 QC Perf
do.devicecheck=0
do.modules=1
do.systemless=1
do.cleanup=1
do.cleanuponabort=0
device.name1=
device.name2=
device.name3=
device.name4=
device.name5=
supported.versions=
supported.patchlevels=
'; } # end properties

block=/dev/block/bootdevice/by-name/boot;
is_slot_device=0;
ramdisk_compression=auto;

. tools/ak3-core.sh;

chmod -R 750 $ramdisk/*;
chown -R root:root $ramdisk/*;

dump_boot;

ui_print "*******************************************"
ui_print "Updating Kernel and Patching cmdline..."
ui_print "*******************************************"


ui_print "*******************************************"
ui_print "Brought to you by DeepakChaurasia (TG: @deepakchaurasia30)"
ui_print "*******************************************"

write_boot;
