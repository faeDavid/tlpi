/* Write a program that logs all file creations, deletions, and renames under 
 * the directory named in its command-line argument.
 */

/* Usage: ./monitor.exe <dirpath> */

#define _XOPEN_SOURCE 600
#include <ftw.h>
#include <fcntl.h>
#include <time.h>
#include <sys/inotify.h>
#include <sys/stat.h>

#include "tlpi_hdr.h"

#ifndef NAME_MAX
#define NAME_MAX 100
#endif // NAME_MAX

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

/* Global variables */
int inotify_fd;
FILE *log_fstream;

void event_handle(struct inotify_event *s) {
	// Allocate a local buffer for formatted strings
	char buf[100];
	char name[50];
	int wd;
	strcpy(name, s->name);

	// sprintf(buf, "Event occured on wd: %d\n", s->wd);
	// fputs(buf, log_fstream);
	// fflush(log_fstream);

	if (s->mask & IN_CREATE) {
		if(s->mask & IN_ISDIR) {
			sprintf(buf, "Created directory %s\n", name);
			fputs(buf, log_fstream);
			fflush(log_fstream);
			// need to somehow get the current working dir name and pre-pend it to the s->path
			if ((wd = inotify_add_watch(inotify_fd, name, IN_ALL_EVENTS)) == -1) {
				err_exit("inotify_add_watch");
			}
		} else {
			sprintf(buf, "Created file%s\n", s->name);
			fputs(buf, log_fstream);
			fflush(log_fstream);
		}
	}

	if (s->mask & IN_DELETE) {
		if (s->mask & IN_ISDIR) {
			sprintf(buf, "Removed directory%s\n", s->name);
			fputs(buf, log_fstream);
			fflush(log_fstream);
			wd = inotify_rm_watch(inotify_fd, s->wd);
			if (wd == -1) {
				err_exit("inotify_rm_watch");
			}
		} else {
			sprintf(buf, "Removed file%s\n", s->name);
			fputs(buf, log_fstream);
			fflush(log_fstream);
		}
	}

	if (s->mask & IN_MOVED_FROM) {
		sprintf(buf, "Moved from %s\n", s->name);
		fputs(buf, log_fstream);
		fflush(log_fstream);
	}

	if (s->mask & IN_MOVED_TO) {
		sprintf(buf, "Moved to %s\n", s->name);
		fputs(buf, log_fstream);
		fflush(log_fstream);
	}
}

static int 
sub_dir_add_watch(const char *pathname, const struct stat *sbuf, int type, struct FTW *stwb) {
	int wd;

	// if a dir, add to the watch list
	if (S_ISDIR(sbuf->st_mode)) {
		if ((wd = inotify_add_watch(inotify_fd, pathname, IN_ALL_EVENTS)) == -1) {
			err_exit("inotify_add_watch");
		}
	}
	
	return 0;
}

// if you want to use the bare bones open(), write(), read()
// instead of the FILE * stream libs, you'll have to figure out 
// how to disable buffering. Even with the FILE streams you have 
// to constantly flush to disk since we may potentially never reach
// the close() call

int main (int argc, char *argv[]) {

	// validate command
	if (argc != 2) {
		usageErr("%s <dirpath>", argv[0]);
	}

	int wd;
	char buf[BUF_LEN] __attribute__((aligned(8)));
	ssize_t num_read;
	char *p;
	struct inotify_event *event;

	// Open the file id for the log
	log_fstream = fopen("monitor.log", "a+");
	if (log_fstream == NULL) {
		err_exit("fopen");
	}

	// Create the inotify instance
	inotify_fd = inotify_init();
	
	// Add the dir in cmd line to the watch
	if ((wd = inotify_add_watch(inotify_fd, argv[1], IN_ALL_EVENTS)) == -1) {
		err_exit("inotify_add_watch");
	}

	// Add all subdirectories to the watch
	if (nftw(argv[1], sub_dir_add_watch, 1, 0) == -1) {
		exit(EXIT_FAILURE);
	}

	// Enter the infinite-loop
	for(;;) {
		num_read = read(inotify_fd, buf, BUF_LEN);
		printf("read %ld bytes\n", num_read);

		if(num_read == 0) {
			fatal("read returned 0");
		}

		if(num_read == -1) {
			err_exit("read returned -1");
		}

		for (p = buf; p < buf + num_read; ) {
			event = (struct inotify_event *)p;
			event_handle(event);

			p += sizeof(struct inotify_event) + event->len;
		}
	}

	return(EXIT_SUCCESS);
}