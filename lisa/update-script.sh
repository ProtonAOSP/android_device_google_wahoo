#!/bin/bash

build_kernel() {
    echo "==========Building kernel image=========="
    cd $ANDROID_BUILD_TOP
    source build/envsetup.sh
    lunch walleye-userdebug
    cd $LOCAL_KERNEL_HOME
    . ./build.config
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} ${DEFCONFIG}
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} -j32
    cp $LOCAL_KERNEL_HOME/arch/arm64/boot/Image.lz4-dtb arch/arm64/boot/dtbo.img `find . -name '*.ko'` $ANDROID_BUILD_TOP/device/google/wahoo-kernel
}

build_image() {
    cd $ANDROID_BUILD_TOP
    source build/envsetup.sh
    lunch walleye-userdebug
    if [ "$1" = "bootimage" ]; then
        echo "==========Building bootimage=========="
        make -j32 vendorimage-nodeps
        make -j32 vbmetaimage-nodeps
        make -j32 bootimage
    else
        echo "==========Building complete image=========="
        make -j32
    fi
}

wait_for_fastboot() {
    # wait for device to enter fastboot, max wait is 200secs
    local i=0
    while [ $i -lt 100 ]
    do
        if [ -n "`fastboot devices`"  ]; then
            break
        else
            sleep 2
            i=$((i+1))
        fi
    done
}

flash_android() {
    # reboot the device if it's online
    if [ "`adb devices`" != "List of devices attached" ]; then
        echo "==========Rebooting the device into fastboot=========="
        adb reboot bootloader
    fi

    echo "==========Waiting for device to enter fastboot=========="
    wait_for_fastboot

    if [ -z "`fastboot devices`" ]; then
        echo "==========Device failed to enter fastboot=========="
        exit
    fi

    # flash the device
    if [ "$1" = "bootimage" ]; then
        echo "==========Flashing bootimage=========="
        fastboot flash vbmeta
        fastboot flash vendor
        fastboot flash boot
        fastboot reboot
    else
        echo "==========Flashing complete image=========="
        fastboot flashall
    fi

    echo "==========Waiting for device to come online=========="
    # wait for device to boot
    adb wait-for-device
}

# check input parameters
if [ "$1" != "kernel" ] && [ "$1" != "all" ]; then
    echo "First parameter \"$1\" is invalid. Should be \"kernel\" or \"all\"."
    exit
fi

if [ "$2" != "build" ] && [ "$2" != "flash" ]; then
    echo "Second parameter \"$2\" is invalid. Should be \"build\" or \"flash\"."
    exit
fi

if [ -z "$ANDROID_BUILD_TOP" ]; then
    echo "ANDROID_BUILD_TOP environment variable is not set."
    exit
fi

if [ -z "$LOCAL_KERNEL_HOME" ]; then
    echo "LOCAL_KERNEL_HOME environment variable is not set."
    exit
fi

if [ "$2" = "build" ]; then
    build_kernel
    if [ "$1" = "kernel" ]; then
        build_image bootimage
    else
        build_image
    fi
else
    if [ "$1" = "kernel" ]; then
        flash_android bootimage
    else
        flash_android
    fi
fi

