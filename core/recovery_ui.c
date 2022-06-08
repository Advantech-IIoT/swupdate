#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <getopt.h>

#include "progress_ipc.h"
#include "recovery_ui.h"
#include "pctl.h"
#include "sdboot.h"


#define PSPLASH_MSG_SIZE	64

#define RESET		0
#define BRIGHT 		1
#define DIM		2
#define UNDERLINE 	3
#define BLINK		4
#define REVERSE		7
#define HIDDEN		8

#define BLACK 		0
#define RED		1
#define GREEN		2
#define YELLOW		3
#define BLUE		4
#define MAGENTA		5
#define CYAN		6
#define	WHITE		7

static int connfd = -1;
static bool silent = false;
static bool need_reboot = false;

static void resetterm(void)
{
	if (!silent)
		fprintf(stdout, "%c[%dm", 0x1B, RESET);
}

static void textcolor(int attr, int fg, int bg)
{	char command[13];

	/* Command is the control command to the terminal */
	sprintf(command, "%c[%d;%d;%dm", 0x1B, attr, fg + 30, bg + 40);
	if (!silent)
		fprintf(stdout, "%s", command);
}

static void fill_progress_bar(char *bar, size_t size, unsigned int percent)
{
	/* the max len for a bar is size-1 sue to string terminator */
	unsigned int filled_len, remain;

	if (percent > 100)
		percent = 100;
	filled_len = (size - 1) * percent / 100;
	memset(bar, 0, size);

	memset(bar,'=', filled_len);
	remain = (size - 1) - filled_len;
	memset(&bar[filled_len], '-', remain);
}

static void* recoveryUI_loop_thread(void* data){
	struct progress_msg msg;
	unsigned int curstep = 0;
	unsigned int percent = 0;
	const int bar_len = 60;
	char bar[bar_len+1];
	char tmp_image[256]={0};
	int ret;
	bool wait_update = true;

	while (1) {
		if (progress_ipc_receive(&connfd, &msg) <= 0) {
			connfd = progress_ipc_connect(true);
			continue;
		}

		/* Wait update Start*/
		if (wait_update) {
			if (msg.status == START || msg.status == RUN) {
				ui_reset_progress();
				ui_print("Update started !\n");
				ui_print("Interface:  ");
				switch (msg.source) {
				case SOURCE_UNKNOWN:
					ui_print("UNKNOWN\n");
					break;
				case SOURCE_WEBSERVER:
					ui_print("WEBSERVER\n");
					break;
				case SOURCE_SURICATTA:
					ui_print("BACKEND\n");
					break;
				case SOURCE_DOWNLOADER:
					ui_print("DOWNLOADER\n");
					break;
				case SOURCE_LOCAL:
					ui_print("LOCAL\n");
					break;
				}
				curstep = 0;
				wait_update = false;
				ui_print("Checking....\n");
			}
		}

		/*
		 * Be sure that string in message are Null terminated
		 */
		if (msg.infolen > 0) {
			if (msg.infolen >= sizeof(msg.info) - 1) {
				msg.infolen = sizeof(msg.info) - 1;
			}
			msg.info[msg.infolen] = '\0';
		}

		if (!wait_update && (msg.status == PROGRESS)) {
			msg.cur_image[sizeof(msg.cur_image) - 1] = '\0';

			if (msg.cur_step > 0) {
				if (msg.cur_step != curstep){
					ui_reset_progress();
					if (0 == curstep) 
						ui_print("Updating....\n");

					ui_print("[Step %d/%d] %s update ...\n", msg.cur_step, msg.nsteps, msg.cur_image);
					curstep = msg.cur_step;
				}
			}else{
				if (strncmp(tmp_image, msg.cur_image, strlen(msg.cur_image) + 1) != 0 ){	
					ui_reset_progress();
					ui_print("[Step %d/%d] %s checking ...\n", msg.cur_step, msg.nsteps, msg.cur_image);
					curstep = msg.cur_step;
					strncpy(tmp_image, msg.cur_image, sizeof(tmp_image));
				}
			}

			if (0 == curstep) {
				if((msg.cur_percent % 10) != 0 ) 
					continue;
			}else {
				if((msg.cur_percent % 5) != 0 ) 
					continue;
			}

			if(msg.cur_percent == percent)
				continue;

			//refresh  the progress bar.
			fill_progress_bar(bar, sizeof(bar), msg.cur_percent);
			ui_show_progress2(msg.cur_percent);
			printf("[ %.*s ] %d of %d %d%% (%s)\r", bar_len, bar, msg.cur_step, 
				msg.nsteps, msg.cur_percent, msg.cur_image);
			fflush(stdout);
			percent = msg.cur_percent;
		}

		switch (msg.status) {
		case SUCCESS:
		case FAILURE:
			if(msg.status != SUCCESS) {
				ui_set_background(BACKGROUND_ICON_ERROR);
			}
	
			ui_print("%s !\n", msg.status == SUCCESS
							  ? "SUCCESS" : "FAILURE");
			wait_update = true;
			break;
		case DONE:
			ui_print("DONE.\n");
			if (is_boot_from_SD()) {
				sdcard_umount(EX_SDCARD_ROOT);
				/* Updating is finished here, we must print this message
	             * in console, it shows user a specific message that
	             * updating is completely, remove SD CARD and reboot */
	            fflush(stdout);
	            freopen("/dev/console", "w", stdout);
	            printf("\nPlease remove SD CARD!!!, wait for reboot.\n");
				ui_print("Please remove SD CARD!!!, wait for reboot.");
				while( access("/dev/mmcblk1p1", F_OK) == 0 ) { sleep(1); }		
			}
			sleep(1);
			UIthread_finished();
			if(need_reboot) {
				if (system("reboot -f") < 0) { /* It should never happen */
					printf("Please reset the board.\n");
				}
			}
			return NULL;
		default:
			break;
		}
	}
}

/*
*  start_recoveryUI:
*  start a thread to refresh the recovery UI.
*  we need to wait the progress connection is
*  okay.
*/
void start_recoveryUI(bool is_reboot){
	/* ui init. */
	ui_init();
    ui_set_background(BACKGROUND_ICON_INSTALLING);
	ui_show_text(1);

	/* wait for connect to the progress thread. */
	connfd = progress_ipc_connect(true);
	if (connfd < 0) {
		printf("open local socket with err! \r\n");
		return;
	}

	need_reboot = is_reboot;
	//start recovery UI loop thread.
	start_thread(recoveryUI_loop_thread, NULL);
}
