#!/bin/bash
BUILD_START=$(date +"%s")
tcdir=${HOME}/android/TOOLS/GCC

[ -d "out" ] && rm -rf out && mkdir -p out || mkdir -p out

[ -d $tcdir ] && \
echo "ARM64 TC Present." || \
echo "ARM64 TC Not Present. Downloading..." | \
git clone https://bitbucket.org/UBERTC/arm-linux-androideabi-4.9.git $tcdir/uber

echo -e "\e[1;31m***********************************************"
echo "          Compiling A37f Kernel          "
echo -e "***********************************************\e[0m"

make O=out ARCH=arm64 msm-perf_defconfig

PATH="$tcdir/uber/bin:${PATH}" \
make    O=out \
        ARCH=arm64 \
        CC="ccache $tcdir/uber/bin/aarch64-linux-android-gcc" \
        CROSS_COMPILE=aarch64-linux-android- \
        CONFIG_NO_ERROR_ON_MISMATCH=y \
        CONFIG_DEBUG_SECTION_MISMATCH=y \
        -j$(nproc --all) || exit

echo -e "\e[1;32m**** Copying Image ****\e[0m"
cp out/arch/arm64/boot/Image anykernel3

cc anykernel3/dtbtool.c -o out/arch/arm64/boot/dts/dtbtool
echo -e "\e[1;35m*******Building DTB********\e[0m"
( cd out/arch/arm64/boot/dts; ./dtbtool -v -s 2048 -o dt.img )
echo -e "\e[1;33m**** Copying dtb ****\e[0m"
( cp out/arch/arm64/boot/dts/dt.img anykernel3 )

echo -e "\e[1;36m**** Time to zip up! ****\e[0m"
( cd anykernel3; zip -r ../out/A37F_KERNEL_`date +%d\.%m\.%Y_%H\:%M\:%S`.zip . -x 'LICENSE' 'README.md' 'dtbtool.c' )

BUILD_END=$(date +"%s")
DIFF=$(($BUILD_END - $BUILD_START))
echo -e "\e[1;32m**** Enjoy New Shit!! ****\e[0m"
echo -e "\e[1;42mBuild completed in $(($DIFF / 60)) minute(s) and $(($DIFF % 60)) seconds.\e[0m"
