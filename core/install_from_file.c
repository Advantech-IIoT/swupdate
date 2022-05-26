/*
 * (C) Copyright 2020
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de.
 *
 * SPDX-License-Identifier:     GPL-2.0-only
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>

#include "swupdate.h"

int install_from_file(const char *filename, bool check, struct swupdate_cfg *software)
{
	int ret;
	int fd = -1;

	if (filename && (fd = open(filename, O_RDONLY)) < 0) {
		fprintf(stderr, "Unable to open %s\n", filename);
	}

	/*do recovery work from filestream. */
	ret = do_recovery(fd, check, software);
	if (filename && fd > 0) {
		close(fd);
	}

	return ret;
}
