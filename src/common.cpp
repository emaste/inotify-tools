#include "common.h"
#include "../config.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <inotifytools/inotifytools.h>

#define MAXLEN 4096
#define LIST_CHUNK 1024

static void resize_if_necessary(const int count, int* len, const char*** ptr) {
	if (count >= *len - 1) {
		*len += LIST_CHUNK;
		*ptr = (const char**)realloc(*ptr, sizeof(char*) * *len);
	}
}

void print_event_descriptions() {
	printf("\taccess\t\tfile or directory contents were read\n");
	printf("\tmodify\t\tfile or directory contents were written\n");
	printf("\tattrib\t\tfile or directory attributes changed\n");
	printf(
	    "\tclose_write\tfile or directory closed, after being opened in\n"
	    "\t           \twritable mode\n");
	printf(
	    "\tclose_nowrite\tfile or directory closed, after being opened in\n"
	    "\t           \tread-only mode\n");
	printf(
	    "\tclose\t\tfile or directory closed, regardless of read/write "
	    "mode\n");
	printf("\topen\t\tfile or directory opened\n");
	printf("\tmoved_to\tfile or directory moved to watched directory\n");
	printf(
	    "\tmoved_from\tfile or directory moved from watched directory\n");
	printf(
	    "\tmove\t\tfile or directory moved to or from watched directory\n");
	printf("\tmove_self\t\tA watched file or directory was moved.\n");
	printf(
	    "\tcreate\t\tfile or directory created within watched directory\n");
	printf(
	    "\tdelete\t\tfile or directory deleted within watched directory\n");
	printf("\tdelete_self\tfile or directory was deleted\n");
	printf(
	    "\tunmount\t\tfile system containing file or directory "
	    "unmounted\n");
}

int isdir(char const* path) {
	static struct stat my_stat;

	if (-1 == lstat(path, &my_stat)) {
		if (errno == ENOENT)
			return 0;
		fprintf(stderr, "Stat failed on %s: %s\n", path,
			strerror(errno));
		return 0;
	}

	return S_ISDIR(my_stat.st_mode) && !S_ISLNK(my_stat.st_mode);
}

FileList::FileList(int argc, char** argv)
    : watch_files_(0), exclude_files_(0), argc_(argc), argv_(argv) {}

FileList::~FileList() {
	char* start_of_stack = argv_[0];
	char* end_of_stack = argv_[argc_ - 1];
	for (int i = 0; argv_[i]; ++i) {
		if (argv_[i] < start_of_stack) {
			start_of_stack = argv_[i];
		} else if (argv_[i] > end_of_stack) {
			end_of_stack = argv_[i];
		}
	}

	while (*end_of_stack) {
		++end_of_stack;
	}

	for (int i = 0; watch_files_[i]; ++i) {
		if (watch_files_[i] < start_of_stack ||
		    watch_files_[i] > end_of_stack) {
			free((void*)watch_files_[i]);
		}
	}

	free(watch_files_);

	for (int i = 0; exclude_files_[i]; ++i) {
		if (exclude_files_[i] < start_of_stack ||
		    exclude_files_[i] > end_of_stack) {
			free((void*)exclude_files_[i]);
		}
	}

	free(exclude_files_);
}

void construct_path_list(int argc,
			 char** argv,
			 char const* filename,
			 FileList* list) {
	list->watch_files_ = 0;
	list->exclude_files_ = 0;
	FILE* file = 0;

	if (filename) {
		if (!strcmp(filename, "-")) {
			file = stdin;
		} else {
			file = fopen(filename, "r");
			if (!file) {
				fprintf(stderr, "Couldn't open %s: %s\n",
					filename, strerror(errno));
			}
		}
	}

	int watch_len = LIST_CHUNK;
	int exclude_len = LIST_CHUNK;
	int watch_count = 0;
	int exclude_count = 0;
	list->watch_files_ = (char const**)malloc(sizeof(char*) * LIST_CHUNK);
	list->exclude_files_ = (char const**)malloc(sizeof(char*) * LIST_CHUNK);

	char name[MAXLEN];
	while (file && fgets(name, MAXLEN, file)) {
		if (name[strlen(name) - 1] == '\n')
			name[strlen(name) - 1] = 0;
		if (strlen(name) == 0)
			continue;
		if ('@' == name[0] && strlen(name) == 1)
			continue;
		if ('@' == name[0]) {
			resize_if_necessary(exclude_count, &exclude_len,
					    &list->exclude_files_);
			list->exclude_files_[exclude_count++] =
			    strdup(&name[1]);
		} else {
			resize_if_necessary(watch_count, &watch_len,
					    &list->watch_files_);
			list->watch_files_[watch_count++] = strdup(name);
		}
	}

	if (file && file != stdin)
		fclose(file);

	for (int i = 0; i < argc; ++i) {
		if (strlen(argv[i]) == 0)
			continue;
		if ('@' == argv[i][0] && strlen(argv[i]) == 1)
			continue;
		if ('@' == argv[i][0]) {
			resize_if_necessary(exclude_count, &exclude_len,
					    &list->exclude_files_);
			list->exclude_files_[exclude_count++] = &argv[i][1];
		} else {
			resize_if_necessary(watch_count, &watch_len,
					    &list->watch_files_);
			list->watch_files_[watch_count++] = argv[i];
		}
	}

	list->exclude_files_[exclude_count] = 0;
	list->watch_files_[watch_count] = 0;
}

void warn_inotify_init_error(int fanotify) {
	const char* backend = fanotify ? "fanotify" : "inotify";
	const char* resource = fanotify ? "groups" : "instances";
	int error = inotifytools_error();

	fprintf(stderr, "Couldn't initialize %s: %s\n", backend,
		strerror(error));
	if (error == EMFILE) {
		fprintf(stderr,
			"Try increasing the value of "
			"/proc/sys/fs/%s/max_user_%s\n",
			backend, resource);
	}
	if (fanotify && error == EINVAL) {
		fprintf(stderr,
			"fanotify support for reporting the events with "
			"file names was added in kernel v5.9.\n");
	}
	if (fanotify && error == EPERM) {
		fprintf(stderr, "fanotify watch requires admin privileges\n");
	}
}

bool is_timeout_option_valid(unsigned int* timeout, char* o) {
	if ((o == NULL) || (*o == '\0')) {
		fprintf(stderr,
			"The provided value is not a valid timeout value.\n"
			"Please specify a long int value.\n");
		return false;
	}

	char* timeout_end = NULL;
	errno = 0;
	*timeout = strtol(o, &timeout_end, 10);

	const int err = errno;
	if (err != 0) {
		fprintf(stderr,
			"Something went wrong with the timeout "
			"value you provided.\n");
		fprintf(stderr, "%s\n", strerror(err));
		return false;
	}

	if (*timeout_end != '\0') {
		fprintf(stderr,
			"'%s' is not a valid timeout value.\n"
			"Please specify a long int value.\n",
			o);
		return false;
	}

	return true;
}
