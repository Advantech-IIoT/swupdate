<!--
SPDX-FileCopyrightText: 2013 Stefano Babic <sbabic@denx.de>

SPDX-License-Identifier: GPL-2.0-only
-->

<p align ="center"><img src=SWUpdate.svg width=200 height=200 /></p>

SWUpdate - Software Update for Embedded Linux Devices
=====================================================

[![Build Status](https://travis-ci.org/sbabic/swupdate.svg?branch=master)
](https://travis-ci.org/sbabic/swupdate)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/20753/badge.svg)](https://scan.coverity.com/projects/20753)
![License](https://img.shields.io/github/license/sbabic/swupdate)

[SWUpdate](https://swupdate.org) is a Linux Update agent with the goal to
provide an efficient and safe way to update
an embedded Linux system in field. SWUpdate supports local and OTA
updates, multiple update strategies and it is designed with security
in mind.


## Technical documentation

Documentation is part of the project and can be generated, or you access
to the [Online Documentation](https://sbabic.github.io/swupdate/swupdate.html).

## Building

SWUpdate is well integrated in the [Yocto](https://www.yoctoproject.org) build system by adding
the [meta-swupdate](https://layers.openembedded.org/layerindex/branch/master/layer/meta-swupdate/) layer.

`source setup-environment eam9918a1`

`bitbake swupdate-image`

`bitbake imx-image-full`

the result file is located in *tmp/deploy/images/imx8mmeamb9918a1*，

*swupdate-image-imx8mmeamb9918a1-.rootfs.cpio.gz.u-boot* is initrd.img, which is recovery partition image.

LABEL="recovery" BLOCK_SIZE="512" TYPE="vfat" PARTUUID="e191c4ad-02"

*imx-image-full-imx8mmeamb9918a1-.rootfs.tar.bz2* and *imx-image-full-imx8mmeamb9918a1.ext4* is rootfs partition image.

LABEL="rootfs" BLOCK_SIZE="4096" TYPE="ext4" PARTUUID="e191c4ad-03"

use `swupdate-tools/mkupdateimg.sh ../imx-image-full-imx8mmeamb9918a1.ext4` to make `*.swu`, which is the upgrade file.

## Running

- #### Webserver update

In normal mode, execute the follow command `swupdate --update -w "-r /www" -g --reboot` 

`--update`,  environment variables `recovery_command` and `recovery_status` will be updated

`-w`, upgrade use  web server， `"-r /www"`,  web server options

`-g,--gui`, upgrade process will display on the lcd screen

`--reboot`, after device  restarted , device enter recovery mode and `/usr/bin/swupdate` run in background

Link to the target device use the following url：*http://<target_ip>:8080*

- #### Local update

If boot with emmc, mount /dev/mmcblk2p4 on /userdata, else if boot with sdcard , mount /dev/mmcblk1p4 on /userdata. Then copy the upgrade file to /userdata path, and execute the follow command in normal mode, 

`swupdate --update -g -i "/userdata/swupdate-image_1.0.swu" --reboot`

`-i,--image`, upgrade file location

`-D,--delete`, delete upgrade file swupdate-image_1.0.swu or not

*--update,-g,--reboot, reference above*

- #### SDCard update

Use the `fdisk` and `mkfs.vfat` tools to create sdcard partiton `/dev/mmcblk1p1`,   copy the upgrade file to the partition, location is `/dev/mmcblk1p1/swupdate-image.swu`. 

When the device is power on with the above sdcard,  uboot phase will check the file of `/dev/mmcblk1p1/swupdate-image.swu` exist or not. For debug, you can enter uboot command line mode and execute `fatls mmc 1:1` to check the file exist or not. If the file exist,  will also verify the validity of the file. If the validity of the file is verified in the meantime, append `swupdate` to environment variable `bootargs` and update `recovery_status` value is 'in_process'. Finally, device enter recovery mode, execute `/usr/sbin/swupdate` in background. 

In swupdate,  read `/proc/cmdline` to check `swupdate`  key words exist or not.  If `swupdate` exist, start sdcard upgrade process,  which samed as local upgrade process. When the process of upgrade finished, prompt user to remove sdcard which prevent  device from doing repeated upgrade. When  sdcard removed ,  reboot the device. 

Note: 
1. upgrade file name must be renamed as swupdate-image.swu.
2. /dev/mmcblk1p1 must be formated as vfat.

## Result

update_rst.txt file record the result of the upgrade process.  If success,  the content is update images is success, if failed, the content is update images failed.

where is the location of the update_rst.txt？ If defined environment TMPDIR, then use getenv("TMPDIR") to read the directory which is used to  save update_rst.txt, else access the directory /userdata exist or not, if not, use /tmp, otherwise use /userdata.

