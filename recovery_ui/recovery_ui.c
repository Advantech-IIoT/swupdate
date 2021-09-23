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

#include <progress_ipc.h>
#include <recovery_ui.h>

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

static bool silent = false;

void printf_ui_text(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
    fprintf(stdout, fmt,  args);
	ui_print(fmt, args);
    va_end(args);

	fflush(stdout);
}

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

int main(int argc, char **argv)
{
	int connfd;
	struct progress_msg msg;
	unsigned int curstep = 0;
	unsigned int percent = 0;
	const int bar_len = 60;
	char bar[bar_len+1];
	int opt_r = 0;
	int ret;
	bool wait_update = true;

	ui_init();
    ui_set_background(BACKGROUND_ICON_INSTALLING);
	ui_reset_progress();

	connfd = -1;
	while (1) {
		if (connfd < 0) {
			connfd = progress_ipc_connect(true);
		}
		/*
		 * if still fails, try later
		 */
		if (connfd < 0) {
			sleep(1);
			continue;
		}

		if (progress_ipc_receive(&connfd, &msg) <= 0) {
			continue;
		}

		/*
		 * Wait update Start
		 */
		if (wait_update) {
			if (msg.status == START || msg.status == RUN) {
				ui_show_text(1);
				printf_ui_text("Update started !\n");
				printf_ui_text("Interface:  \n");
				switch (msg.source) {
				case SOURCE_UNKNOWN:
					printf_ui_text("UNKNOWN\n\n");
					break;
				case SOURCE_WEBSERVER:
					printf_ui_text("WEBSERVER\n\n");
					break;
				case SOURCE_SURICATTA:
					printf_ui_text("BACKEND\n\n");
					break;
				case SOURCE_DOWNLOADER:
					printf_ui_text("DOWNLOADER\n\n");
					break;
				case SOURCE_LOCAL:
					printf_ui_text("LOCAL\n\n");
					break;
				}
				curstep = 0;
				wait_update = false;
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
			printf_ui_text("INFO : %s\r", msg.info);
		}
		msg.cur_image[sizeof(msg.cur_image) - 1] = '\0';


		if (!wait_update) {

			if (msg.cur_step > 0) {
				if ((msg.cur_step != curstep) && (curstep != 0)){
					ui_reset_progress();
					printf_ui_text("\n");
				}
				fill_progress_bar(bar, sizeof(bar), msg.cur_percent);

				ui_set_progress2(msg.cur_percent);
				printf_ui_text("[ %.*s ] %d of %d %d%% (%s), dwl %d%% of %llu bytes\r",
					bar_len,
					bar,
					msg.cur_step, msg.nsteps, msg.cur_percent,
					msg.cur_image, msg.dwl_percent, msg.dwl_bytes);

				curstep = msg.cur_step;
			}
		}

		switch (msg.status) {
		case SUCCESS:
		case FAILURE:
			if(msg.status != SUCCESS){
				ui_set_background(BACKGROUND_ICON_ERROR);
			}
			textcolor(BRIGHT, GREEN, BLACK);
			printf_ui_text("\n%s !\n", msg.status == SUCCESS
							  ? "SUCCESS"
							  : "FAILURE");
			resetterm();
			ui_show_text(0);
			wait_update = true;
			break;
		case DONE:
			printf_ui_text("\nDONE.\n\n");
			break;
		default:
			break;
		}
	}
}
