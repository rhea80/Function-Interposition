#define _GNU_SOURCE
#include <string.h>
#include <stdarg.h>	// handle variable args in open
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>	// for setting an error code
#include <dlfcn.h>	// for dynamic loading


// interpose the open call
// open takes an optional third argument - the permissions for creating a file
// we don't care what it's set to but will have to handle the variable # of args
//	int open(const char *pathname, int flags)
//	int open(const char *pathname, int flags, mode_t mode)
//
int open(const char *pathname, int flags, ...)
{
	va_list args;
    va_start(args, flags);
    mode_t mode = 0;

	if (flags & 0100) {
        mode = va_arg(args, int);
    }
    va_end(args);

    const char *blocked = getenv("BLOCKED");
    if (blocked != NULL) {
        char *blocked_copy = strdup(blocked);
        char *suffix = strtok(blocked_copy, ":");

        while (suffix != NULL) {
            size_t path_len = strlen(pathname);
            size_t suffix_len = strlen(suffix);

            if (path_len >= suffix_len &&
                strcmp(pathname + path_len - suffix_len, suffix) == 0) {
                free(blocked_copy);
                errno = EACCES;
                return -1;
            }

            suffix = strtok(NULL, ":");
        }
        free(blocked_copy);
	}
}

// interpose the readdir call
// This isn't the system call but is used by many programs that need to read directories, 
// like find and ls.

struct dirent *readdir(DIR *dirp)
{
	struct dirent *(*real_readdir)(DIR *) = dlsym(RTLD_NEXT, "readdir");
	const char *hidden = getenv("HIDDEN");

	if (hidden == NULL) {
		return real_readdir(dirp);
	}

	char *hidden_copy = strdup(hidden);
	char *hidden_names[100]; 
	int hidden_count = 0;

	char *token = strtok(hidden_copy, ":");
	while (token != NULL && hidden_count < 100) {
		hidden_names[hidden_count++] = token;
		token = strtok(NULL, ":");
	}

	struct dirent *entry;
	while ((entry = real_readdir(dirp)) != NULL) {
		int is_hidden = 0;
		for (int i = 0; i < hidden_count; i++) {
			if (strcmp(entry->d_name, hidden_names[i]) == 0) {
				is_hidden = 1;
				break;
			}
		}
		if (!is_hidden) {
			free(hidden_copy);
			return entry;
		}
	}

	free(hidden_copy);
	return NULL;
}
