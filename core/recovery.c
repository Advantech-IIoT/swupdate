/*
* (C) Copyright 2021
* Advantech co.ltd, chang.qing@advantech.com.cn.
*
* SPDX-License-Identifier:     GPL-2.0-only
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>

#include "network_ipc.h"
#include "util.h"
#include "installer.h"
#include "installer_priv.h"
#include "parsers.h"
#include "swupdate.h"
#include "state.h"
#ifdef CONFIG_MTD
#include "flash.h"
#endif
#include "progress.h"
#include "bootloader.h"

enum {
	IMAGE_EXTRACT_DESCRIPTION,
	IMAGE_PARSE_AND_CHECK,
	IMAGE_EXTRACT_AND_INSTALL,
	IMAGE_INSTALL_END
};

static struct installer inst;

void set_update_mode(char *image, bool is_delete, bool is_reboot, bool gui_enabled, bool web_enabled){
	char commond[1024] = {0};
	const char* swupdatebin = "/usr/bin/swupdate";
	char * web_args= "--document-root=/www";

	if(image != NULL){
		if(gui_enabled && is_delete) {
			sprintf(commond, "%s\n-g\n-D\n--image=%s\n", swupdatebin, image);
		} else if(gui_enabled && !is_delete) {
			sprintf(commond, "%s\n-g\n--image=%s\n", swupdatebin, image);
		} else if(!gui_enabled && is_delete) {
			sprintf(commond, "%s\n-D\n--image=%s\n", swupdatebin, image);
		} else {
			sprintf(commond, "%s\n--image=%s\n", swupdatebin, image);
		}
	}else{
		if(web_enabled){
			if(gui_enabled)
				sprintf(commond, "%s\n-g\n--webserver=%s\n", swupdatebin, web_args);
			else
				sprintf(commond, "%s\n--webserver=%s\n", swupdatebin, web_args);
		}
	}

	//Set system enter update/recovery mode.
	bootloader_env_set(BOOTVAR_TRANSSTATUS, get_state_string(STATE_IN_PROGRESS));
	bootloader_env_set(BOOTVAR_TRANSACTION, commond);

	printf("Reboot will enter recovery\r\n");
	printf("recovery_command = %s \r\n", commond);
	printf("[Done!]\r\n");

	if(is_reboot){
		system("reboot -f");
	}
}
static struct img_type* find_image_type(struct swupdate_cfg *software, struct filehdr *pfdh){
	struct img_type *img;
	struct imglist *list[] = {&software->images,
							  &software->scripts,
							  &software->bootscripts};

	for (unsigned int i = 0; i < ARRAY_SIZE(list); i++) {
		LIST_FOREACH(img, list[i], next) {
			if (strcmp(pfdh->filename, img->fname) == 0) {
				return img;
			}
		}
	}

	return NULL;
}

static int extract_file_to_tmp(int fd, const char *fname, unsigned long *poffs, bool encrypted)
{
	char output_file[MAX_IMAGE_FNAME];
	struct filehdr fdh;
	int fdout;
	uint32_t checksum;
	const char* TMPDIR = get_tmpdir();

	if(fd < 0)
		return -5;

	if (extract_cpio_header(fd, &fdh, poffs)) {
		return -1;
	}
	if (strcmp(fdh.filename, fname)) {
		TRACE("description file name not the first of the list: %s instead of %s",
			fdh.filename,
			fname);
		return -1;
	}
	if (snprintf(output_file, sizeof(output_file), "%s%s", TMPDIR,
		     fdh.filename) >= (int)sizeof(output_file)) {
		ERROR("Path too long: %s%s", TMPDIR, fdh.filename);
		return -1;
	}
	TRACE("Found file");
	TRACE("\tfilename %s", fdh.filename);
	TRACE("\tsize %u", (unsigned int)fdh.size);

	fdout = openfileoutput(output_file);
	if (fdout < 0)
		return -1;

	if (copyfile(fd, &fdout, fdh.size, poffs, 0, 0, 0, &checksum, NULL,
		     encrypted, NULL, NULL) < 0) {
		close(fdout);
		return -1;
	}
	if (!swupdate_verify_chksum(checksum, fdh.chksum)) {
		close(fdout);
		return -1;
	}
	close(fdout);

	return 0;
}

static int do_image_parse(int fd, unsigned long *offset, struct swupdate_cfg *software){
	const char* TMPDIR = get_tmpdir();
	char output_file[MAX_IMAGE_FNAME];

#ifdef CONFIG_SIGNED_IMAGES
	snprintf(output_file, sizeof(output_file), "%s.sig", SW_DESCRIPTION_FILENAME);
	if (extract_file_to_tmp(fd, output_file, offset, false) < 0 )
		return -1;
#endif

	snprintf(output_file, sizeof(output_file), "%s%s", TMPDIR, SW_DESCRIPTION_FILENAME);
	if (parse(software, output_file)) {
		ERROR("Compatible SW not found");
		return -1;
	}

	if (check_hw_compatibility(software)) {
		ERROR("SW not compatible with hardware");
		return -1;
	}
	if (preupdatecmd(software)) {
		return -1;
	}

	return 0;
}

//do image checksum and setting
static int do_image_check(int fd, unsigned long *offset, struct swupdate_cfg *software) {
	int fdout = -1;
	int skip_file = 1;
	uint32_t checksum;
	struct filehdr fdh;
	struct img_type* img;

	while(1){
		if (extract_cpio_header(fd, &fdh, offset)) {
			ERROR("CPIO HEADER");
			return -1;
		}

		if (strcmp("TRAILER!!!", fdh.filename) == 0) {
			/*
			* Keep reading the cpio padding, if any, up
			* to 512 bytes from the socket until the
		 	* client stops writing
			*/
			extract_padding(fd, offset);
			break;
		}

		img = find_image_type(software, &fdh);
		if(img == NULL) continue;

		//we mark the offset in cpio file since to use it later.
		img->offset = (off_t)*offset;
		img->provided = 1;
		img->size = (unsigned int)fdh.size;

		TRACE("Found file");
		TRACE("\tfilename %s", fdh.filename);
		TRACE("\tsize %u required", (unsigned int)fdh.size);

		//compare checksum;
		if(img->is_script){
			/*
			* If the image is scripts or bootscripts, then, we will extract it into /tmp
			* directory. Or, we just do  checksum.
			*/
			skip_file = 0;
			if (snprintf(img->extract_file, sizeof(img->extract_file), "%s%s", get_tmpdir(),
		     fdh.filename) >= (int)sizeof(img->extract_file)) {
				ERROR("Path too long: %s%s", get_tmpdir(), fdh.filename);
				return -1;
			}

			fdout = openfileoutput(img->extract_file);
			if (fdout < 0){
				return -1;
			}
		}else {
			fdout = -1;	
			skip_file = 1;
			/* do  image check process. */
			swupdate_progress_on_step0(fdh.filename);
		}

		if (copyfile(fd, &fdout, fdh.size, offset, 0, skip_file, 
					0, &checksum, img->sha256, false, NULL, NULL) < 0) {
			if(fdout > 0) close(fdout);
			return -1;
		}
		if (!swupdate_verify_chksum(checksum, fdh.chksum)) {
			if(fdout > 0) close(fdout);
			return -1;
		}

		if(fdout > 0) close(fdout);
	}

	return 0;
}

static int do_images_install(int fd, struct swupdate_cfg *software){
	int ret;

	TRACE("Valid image found: copying to FLASH");

	/*
	* If an image is loaded, the install must be successful.
	* Set we have initiated an update
	*/
	if (!software->parms.dry_run && software->bootloader_transaction_marker) {
		bootloader_env_set(BOOTVAR_TRANSSTATUS, get_state_string(STATE_IN_PROGRESS));
	}
	notify(RUN, RECOVERY_NO_ERROR, INFOLEVEL, "Installation in progress");

	ret = install_images_from_fd(fd, software);
	if (ret != 0) {
		if (!software->parms.dry_run && software->bootloader_transaction_marker) {
			bootloader_env_set(BOOTVAR_TRANSSTATUS, get_state_string(STATE_FAILED));
		}

		notify(FAILURE, RECOVERY_ERROR, ERRORLEVEL, "Installation failed !");
		inst.last_install = FAILURE;
		inst.last_error = ERROR_INSTALL_FAILED;

		if (!software->parms.dry_run
				&& software->bootloader_state_marker
				&& save_state(STATE_FAILED) != SERVER_OK) {
			WARN("Cannot persistently store FAILED update state.");
		}
	} else {
		/*
		* Clear the recovery variable to indicate to bootloader
		* that it is not required to start recovery again
		*/
		if (!software->parms.dry_run && software->bootloader_transaction_marker) {
			bootloader_env_unset(BOOTVAR_TRANSSTATUS);
			bootloader_env_unset(BOOTVAR_TRANSACTION);
		}
		inst.last_error = ERROR_INSTALL_SUCCESS;
		if (!software->parms.dry_run
			&& software->bootloader_state_marker
			&& save_state(STATE_INSTALLED) != SERVER_OK) {
			ERROR("Cannot persistently store INSTALLED update state.");
			notify(FAILURE, RECOVERY_ERROR, ERRORLEVEL, "Installation failed !");
			inst.last_install = FAILURE;
		} else {
			notify(SUCCESS, RECOVERY_NO_ERROR, INFOLEVEL, "SWUPDATE successful !");
			inst.last_install = SUCCESS;
		}
	}

	return 0;
}

static int do_finish_install(int fd, struct swupdate_cfg *software){
	int result = inst.last_install == FAILURE ? -1 : 0;

	if(inst.last_install == FAILURE &&
		inst.last_error != ERROR_INSTALL_FAILED) {
		/*
		* The  inst.last_error == ERROR_INSTALL_FAILED, the system can 
		*  broken and can't boot from, So we just only enter recovery and wait
		* update from network. Other case, just the image is not correct and
		* system can boot successfuly, and we unset bootloader env and set
		* the update result.
		*/
		/*
		* Clear the recovery variable to indicate to bootloader
		* that it is not required to start recovery again
		*/
		if (!software->parms.dry_run && software->bootloader_transaction_marker) {
			bootloader_env_unset(BOOTVAR_TRANSSTATUS);
			bootloader_env_unset(BOOTVAR_TRANSACTION);
		}

		notify(FAILURE, RECOVERY_ERROR, ERRORLEVEL, "Installation failed !");

		result = 1;
		if (!software->parms.dry_run
				&& software->bootloader_state_marker
				&& save_state(STATE_FAILED) != SERVER_OK) {
			WARN("Cannot persistently store FAILED update state.");
		}
	}

	//report the result.
	swupdate_progress_end(inst.last_install);
	//save the result
	save_update_result(inst.last_install);
	//run post-up command.
	if (software) {
		if (software->parms.dry_run) {
			DEBUG("Dry run, skipping Post-update command");
		} else {
			DEBUG("Running Post-update command");
			run_system_cmd(software->postupdatecmd);
		}
	}
	swupdate_progress_done(NULL);
	inst.status = IDLE;

	/* release temp files we may have created */
	cleanup_files(software);

	swupdate_remove_directory(SCRIPTS_DIR_SUFFIX);
	swupdate_remove_directory(DATADST_DIR_SUFFIX);

	return result;
}

/*
*  do_recovery from filestream, and not generate the tmp image expect scripts.
*  and we hope to take more samll storage spaces.
*  dry_run: just for test and no install.
*  software: the goloable configuration for this recovery work sequence.
*/
int do_recovery(int fd, bool dry_run, struct swupdate_cfg *software) {
	int ret = 0;
	int status = IMAGE_EXTRACT_DESCRIPTION;
	unsigned long offset = 0;
	struct filehdr fdh;
	const char* TMPDIR = get_tmpdir();
	bool encrypted_sw_desc = false;
	struct swupdate_request req;

	TRACE("Software update started");
	memset(&inst, 0, sizeof(inst));
	inst.fd = fd;
	inst.status = IDLE;
	inst.software = software;

	set_default_installer(&inst);
	// build a dummy request for send some
	// infos to UI progress.
	swupdate_prepare_req(&inst.req);
	inst.req.source = SOURCE_LOCAL;

	software->parms.dry_run = dry_run;
	//we need to set uboot env by default.
	software->bootloader_transaction_marker = true;

	/* Create directories for scripts/datadst */
	swupdate_create_directory(SCRIPTS_DIR_SUFFIX);
	swupdate_create_directory(DATADST_DIR_SUFFIX);

#ifdef CONFIG_MTD
	mtd_cleanup();
	scan_mtd_devices();
#endif

#ifdef CONFIG_ENCRYPTED_SW_DESCRIPTION
	encrypted_sw_desc = true;
#endif

#ifdef CONFIG_UBIVOL
	mtd_init();
	ubi_init();
#endif

	for (;;) {
		switch (status) {
		/* Waiting for the first Header */
		case IMAGE_EXTRACT_DESCRIPTION:
			ret = extract_file_to_tmp(fd, SW_DESCRIPTION_FILENAME, &offset, encrypted_sw_desc);
			if (ret < 0 ){
				inst.last_install = FAILURE;
				inst.last_error = ret == -5 ? ERROR_FILE_NOT_EXISTS : ERROR_PARSER_DESCRIPTION;
				status = IMAGE_INSTALL_END;
			}else {
				status = IMAGE_PARSE_AND_CHECK;
			}
			break;

		case IMAGE_PARSE_AND_CHECK:
			if(do_image_parse(fd, &offset, software) < 0){
				inst.last_install = FAILURE;
				inst.last_error = ERROR_PARSER_FAILED;
				status = IMAGE_INSTALL_END;
			}else {
				if(do_image_check(fd, &offset, software) < 0){
					inst.last_install = FAILURE;
					inst.last_error = ERROR_CHECKSUM_FAILED;
					status = IMAGE_INSTALL_END;
				}else
					status = IMAGE_EXTRACT_AND_INSTALL;
			}
			break;

		case IMAGE_EXTRACT_AND_INSTALL:
			do_images_install(fd, software);
			status = IMAGE_INSTALL_END;
			break;

		case IMAGE_INSTALL_END:
			return do_finish_install(fd, software);
		default:
			return -100;
		}
	}
}
