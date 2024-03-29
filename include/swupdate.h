/*
 * (C) Copyright 2012-2014
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de.
 *
 * SPDX-License-Identifier:     GPL-2.0-only
 */

#ifndef _SWUPDATE_H
#define _SWUPDATE_H

#include <sys/types.h>
#include <stdbool.h>
#include "bsdqueue.h"
#include "globals.h"
#include "mongoose_interface.h"
#include "swupdate_dict.h"

#define BOOTVAR_TRANSSTATUS "recovery_status"
#define BOOTVAR_TRANSACTION "recovery_command"
#define BOOTVAR_FACSTATUS "factory_reset_mode"
#define BOOTVAR_BOOTCOUNT "bootcount"
#define BOOTVAR_UPGRADDE_AVAILABLE "upgrade_available"
/*
 * swupdate uses SHA256 hashes
 */
#define SHA256_HASH_LENGTH	32

typedef enum {
	FLASH,
	UBI,
	FILEDEV,
	PARTITION,
	SCRIPT
} imagetype_t;

/*
 * this is used to indicate if a file
 * in the .swu image is required for the
 * device, or can be skipped
 */
typedef enum {
	COPY_FILE,
	SKIP_FILE,
	INSTALL_FROM_STREAM
} swupdate_file_t;

typedef enum {
	SKIP_NONE=0,
	SKIP_SAME,
	SKIP_HIGHER,
	SKIP_SCRIPT
} skip_t;

enum {
  COMPRESSED_FALSE,
  COMPRESSED_TRUE,
  COMPRESSED_ZLIB,
  COMPRESSED_ZSTD,
};

struct sw_version {
	char name[SWUPDATE_GENERAL_STRING_SIZE];
	char version[SWUPDATE_GENERAL_STRING_SIZE];
	int install_if_different;
	int install_if_higher;
	LIST_ENTRY(sw_version) next;
};

LIST_HEAD(swver, sw_version);

struct img_type {
	struct sw_version id;		/* This is used to compare versions */
	char type[SWUPDATE_GENERAL_STRING_SIZE]; /* Handler name */
	char fname[MAX_IMAGE_FNAME];	/* Filename in CPIO archive */
	char volname[MAX_VOLNAME];	/* Useful for UBI	*/
	char device[MAX_VOLNAME];	/* device associated with image if any */
	char path[MAX_IMAGE_FNAME];	/* Path where image must be installed */
	char mtdname[MAX_IMAGE_FNAME];	/* MTD device where image must be installed */
	char type_data[SWUPDATE_GENERAL_STRING_SIZE];	/* Data for handler */
	char extract_file[MAX_IMAGE_FNAME];
	char filesystem[MAX_IMAGE_FNAME];
	unsigned long long seek;
	skip_t skip;
	int provided;
	int compressed;
	int preserve_attributes; /* whether to preserve attributes in archives */
	bool is_encrypted;
	char ivt_ascii[33];
	int install_directly;
	int is_script;
	int is_partitioner;
	struct dict properties;
	struct dict *bootloader; /* pointer to swupdate_cfg's bootloader dict for handlers to modify */
	long long partsize;
	int fdin;	/* Used for streaming file */
	off_t offset;	/* offset in cpio file */
	long long size;
	unsigned int checksum;
	unsigned char sha256[SHA256_HASH_LENGTH];	/* SHA-256 is 32 byte */
	LIST_ENTRY(img_type) next;
};

LIST_HEAD(imglist, img_type);

struct hw_type {
	char boardname[SWUPDATE_GENERAL_STRING_SIZE];
	char revision[SWUPDATE_GENERAL_STRING_SIZE];
	LIST_ENTRY(hw_type) next;
};

LIST_HEAD(hwlist, hw_type);

struct extproc {
	char name[SWUPDATE_GENERAL_STRING_SIZE];
	char exec[SWUPDATE_GENERAL_STRING_SIZE];
	char options[SWUPDATE_GENERAL_STRING_SIZE];
	LIST_ENTRY(extproc) next;
};

LIST_HEAD(proclist, extproc);

enum {
	SCRIPT_NONE,
	SCRIPT_PREINSTALL,
	SCRIPT_POSTINSTALL
};

struct swupdate_parms {
	bool dry_run;
	char software_set[SWUPDATE_GENERAL_STRING_SIZE];
	char running_mode[SWUPDATE_GENERAL_STRING_SIZE];
};

struct swupdate_cfg {
	char name[SWUPDATE_GENERAL_STRING_SIZE];
	char description[SWUPDATE_UPDATE_DESCRIPTION_STRING_SIZE];
	char version[SWUPDATE_GENERAL_STRING_SIZE];
	bool bootloader_transaction_marker;
	bool bootloader_state_marker;
	char output[SWUPDATE_GENERAL_STRING_SIZE];
	char publickeyfname[SWUPDATE_GENERAL_STRING_SIZE];
	char aeskeyfname[SWUPDATE_GENERAL_STRING_SIZE];
	char postupdatecmd[SWUPDATE_GENERAL_STRING_SIZE];
	char preupdatecmd[SWUPDATE_GENERAL_STRING_SIZE];
	char minimum_version[SWUPDATE_GENERAL_STRING_SIZE];
	char maximum_version[SWUPDATE_GENERAL_STRING_SIZE];
	char current_version[SWUPDATE_GENERAL_STRING_SIZE];
	char mtdblacklist[SWUPDATE_GENERAL_STRING_SIZE];
	char forced_signer_name[SWUPDATE_GENERAL_STRING_SIZE];
	bool syslog_enabled;
	bool no_downgrading;
	bool no_reinstalling;
	bool no_transaction_marker;
	bool no_state_marker;
	bool check_max_version;
	int verbose;
	int loglevel;
	int cert_purpose;
	struct hw_type hw;
	struct hwlist hardware;
	struct swver installed_sw_list;
	struct imglist images;
	struct imglist scripts;
	struct imglist bootscripts;
	struct dict bootloader;
	struct dict accepted_set;
	struct proclist extprocs;
	void *dgst;	/* Structure for signed images */
	struct swupdate_parms parms;
	const char *embscript;
};

#define SEARCH_FILE(img, list, found, offs) do { \
	if (!found) { \
		for (img = list.lh_first; img != NULL; \
			img = img->next.le_next) { \
			if (strcmp(img->fname, fdh.filename) == 0) { \
				found = 1; \
				img->offset = offs; \
				img->provided = 1; \
				img->size = fdh.size; \
			} \
		} \
		if (!found) \
			img = NULL; \
	} \
} while(0)

int cpio_scan(int fd, struct swupdate_cfg *cfg, off_t start);
struct swupdate_cfg *get_swupdate_cfg(void);
void free_image(struct img_type *img);
/*
* Maybe it's best that no args with do recovery, and we will fix this
* in future.
*/
int do_recovery(int fd, bool dry_run, struct swupdate_cfg *software);
void start_recoveryUI(bool is_reboot, bool web_enabled);
void set_update_mode(char *image, bool is_delete, bool is_reboot, bool gui_enabled, bool web_enabled , char *collections);

#endif
