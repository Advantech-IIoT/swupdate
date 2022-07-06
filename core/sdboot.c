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
#include "sdboot.h"
#include "util.h"


bool is_boot_from_SD(void) {
    bool bSDBoot = false;
    char param[1024] = {0};
    int fd, ret;
    char *s = NULL;
    
    memset(param,0,1024);

    fd = open("/proc/cmdline", O_RDONLY);
    ret = read(fd, (char*)param, 1023);

    s = strstr(param, "sdfwupdate");
    if (s != NULL) {
        bSDBoot = true;
        fprintf(stdout, ">>> Boot from SDcard\n");
    } else {
        bSDBoot = false;
        fprintf(stdout, ">>> Boot from non-SDcard\n");
    }

    close(fd);
    return bSDBoot;
}

void sdcard_mount(const char *mntpoint)
{
	int ret;
	char node[64] = {0};
	int dev_num = 1;
	int vol_id = 1;
	
	snprintf(node, sizeof(node), "/dev/mmcblk%dp%d", dev_num, vol_id);
	if (access(EX_SDCARD_ROOT, F_OK) != 0) {
		ret = mkpath(EX_SDCARD_ROOT, 0755);
		if (ret < 0) {
			ERROR("I cannot create path %s: %s\n", EX_SDCARD_ROOT, strerror(errno));
			return;
		}
	}
		
	ret = mount(node, mntpoint, "vfat", 0, NULL);
	if (ret) {
		ERROR("SDcard cannot be mounted : on %s : %s\n", mntpoint, strerror(errno));
	}
	return;
}

void sdcard_umount(const char *mntpoint)
{
	umount(mntpoint);
}

bool is_image_exits(void) {
	char imageFile[128] = {0};

    strcpy(imageFile, EX_SDCARD_ROOT);
    strcat(imageFile, EX_SDCARD_FILE);
	fprintf(stdout, "%s image = %s \n", __func__, imageFile);
	
    if (access(imageFile, F_OK) != 0) {
		ERROR("Error! swupdate image not exits\n");
        return false;
    }

    fprintf(stdout, "Software Update will from SDcard. \n");
    return true;
}

