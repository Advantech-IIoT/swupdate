/*
* (C) Copyright 2021
* Advantech co.ltd, chang.qing@advantech.com.cn.
*
* SPDX-License-Identifier:     GPL-2.0-only
*/


#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include "usbboot.h"
#include "util.h"

int usb_mount(const char *mntpoint)
{
	int ret;
	char node[64] = {0};
	char* dev_num = "a";
	int vol_id = 1;
	
	snprintf(node, sizeof(node), "/dev/sd%s%d", dev_num, vol_id);
	if (access(mntpoint, F_OK) != 0) {
		ret = mkpath(mntpoint, 0755);
		if (ret < 0) {
			ERROR("I cannot create path %s: %s\n", mntpoint, strerror(errno));
			return;
		}
	}
		
	ret = mount(node, mntpoint, "ext4", 0, NULL);
	if (ret) {
		ERROR("SDcard cannot be mounted : on %s : %s\n", mntpoint, strerror(errno));
	}
	return ret;
}

void usb_umount(const char *mntpoint)
{
	umount(mntpoint);
}

bool is_usb_image_exits(const char * fname) {

   if (access(fname, F_OK) != 0) {
		ERROR("Error! swupdate image not exits\n");
        return false;
    }
    fprintf(stdout, "Software Update will from usb. \n");
    return true;
}

