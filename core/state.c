/*
 * Author: Christian Storm
 * Copyright (C) 2016, Siemens AG
 *
 * SPDX-License-Identifier:     GPL-2.0-only
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <util.h>
#include <bootloader.h>
#include <state.h>
#include <network_ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include "pctl.h"


#define RST_FILE_NAME "update_rst.txt"

/*
 * This check is to avoid to corrupt the environment
 * An empty key is accepted, but U-Boot reports a corrupted
 * environment/
 */
#define CHECK_STATE_VAR(v) do { \
	if (v[0] == 0) { \
		WARN("Update Status Storage Key " \
			"is empty, setting it to 'ustate'"); \
		v = (char *)"ustate"; \
	} \
} while(0)

int save_update_result(RECOVERY_STATUS status) 
{
	int fd = -1;
	char* result_file = NULL;
	char text[128] ;
	memset(text, 0, sizeof(text));

	if (asprintf(&result_file, "%s%s", get_tmpdir(), RST_FILE_NAME) == ENOMEM_ASPRINTF) {
		ERROR("Path too long: %s%s", get_tmpdir(), RST_FILE_NAME);
		return -1;
	}
	
	fd = open(result_file, O_CREAT | O_WRONLY | O_TRUNC, 0777);
	if (fd < 0) {
		ERROR("open %s fail, errno = %d\n", result_file, errno);
		free(result_file);
		return -1;
	}
	
	if(status == SUCCESS) {
		strlcpy(text, "update images success!\n", 127);
	} else {
		strlcpy(text, "update images failed!\n", 127);
	}

	int w_len = write(fd, text, strlen(text));
	if (w_len <= 0) {
		ERROR("write %s fail, errno = %d\n", result_file, errno);
		free(result_file);
		return -1;
	}
	free(result_file);
	close(fd);
	
	return 0;
}

static int do_save_state(char *key, char* value)
{
	CHECK_STATE_VAR(key);
	if (!value)
		return -EINVAL;
	char c = *value;
	if (c < STATE_OK || c > STATE_LAST)
		return -EINVAL;

	return bootloader_env_set(key, value);
}

int save_state(update_state_t value)
{
	char value_str[2] = {value, '\0'};
	ipc_message msg;
	if (pid == getpid()) {
		memset(&msg, 0, sizeof(msg));
		msg.magic = IPC_MAGIC;
		msg.type = SET_UPDATE_STATE;
		msg.data.msg[0] = (char)value;
		return (ipc_send_cmd(&msg));
	} else {
		/* Main process */
		return do_save_state((char *)STATE_KEY, value_str);
	}
}

static update_state_t read_state(char *key)
{
	CHECK_STATE_VAR(key);

	char *envval = bootloader_env_get(key);
	if (envval == NULL) {
		INFO("Key '%s' not found in Bootloader's environment.", key);
		return STATE_NOT_AVAILABLE;
	}
	/* TODO It's a bit whacky just to cast this but as we're the only */
	/*      ones touching the variable, it's maybe OK for a PoC now. */

	update_state_t val = (update_state_t)*envval;
	/* bootloader get env allocates space for the value */
	free(envval);

	return val;
}

static update_state_t do_get_state(void) {
	update_state_t state = read_state((char *)STATE_KEY);

	if (state == STATE_NOT_AVAILABLE) {
		ERROR("Cannot read stored update state.");
		return STATE_NOT_AVAILABLE;
	}

	if (is_valid_state(state)) {
		TRACE("Read state=%c from persistent storage.", state);
		return state;
	}

	ERROR("Unknown update state=%c", state);
	return STATE_NOT_AVAILABLE;
}

update_state_t get_state(void) {
	if (pid == getpid())
	{
		ipc_message msg;
		memset(&msg, 0, sizeof(msg));

		msg.type = GET_UPDATE_STATE;

		if (ipc_send_cmd(&msg) || msg.type == NACK) {
			ERROR("Failed to get current bootloader update state.");
			return STATE_NOT_AVAILABLE;
		}

		return (update_state_t)msg.data.msg[0];
	} else {
		// Main process
		return do_get_state();
	}
}
