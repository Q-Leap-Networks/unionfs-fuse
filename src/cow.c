/*
*  C Implementation: cow
*
* Copy-on-write functions
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>

#include "opts.h"
#include "findbranch.h"
#include "branch_ops.h"
#include "general.h"
#include "cow.h"
#include "cow_utils.h"
#include "string.h"
#include "debug.h"


/**
 * Actually create the directory here.
 */
static int do_create(const char *path, int nbranch_ro, int nbranch_rw) {
	DBG_IN();

	struct stat buf;
	int res = STAT(&buf, nbranch_rw, path);
	if (res != -1) return 0; // already exists

	if (nbranch_ro == nbranch_rw) {
		// special case nbranch_ro = nbranch_rw, this is if we a create
		// unionfs meta directories, so not directly on cow operations
		buf.st_mode = S_IRWXU | S_IRWXG;
	} else {
		// data from the ro-branch
		res = STAT(&buf, nbranch_ro, path);
		if (res == -1) return 1; // lower level branch removed in the mean time?
	}

	res = MKDIR(buf.st_mode, nbranch_rw, path);
	if (res == -1) {
		usyslog(LOG_DAEMON, "Creating %d %s failed: \n", nbranch_rw, path);
		return 1;
	}

	if (nbranch_ro == nbranch_rw) return 0; // the special case again

	if (setfile(nbranch_rw, path, &buf))  return 1; // directory already removed by another process?

	// TODO: time, but its values are modified by the next dir/file creation steps?

	return 0;
}

/**
 * l_nbranch (lower nbranch than nbranch) is write protected, create the dir path on
 * nbranch for an other COW operation.
 */
int path_create(const char *path, int nbranch_ro, int nbranch_rw) {
	DBG_IN();

	if (!uopt.cow_enabled) return 0;

	struct stat st;
	if (!STAT(&st, nbranch_rw, path)) {
		// path does already exists, no need to create it
		return 0;
	}

	char p[PATHLEN_MAX];
	char *walk = p;
	strncpy(p, path, PATHLEN_MAX - 1);
	p[PATHLEN_MAX - 1] = '\0';

	// first slashes, e.g. we have path = /dir1/dir2/, will set walk = dir1/dir2/
	while (*walk != '\0' && *walk == '/') walk++;

	do {
		// walk over the directory name, walk will now be /dir2
		while (*walk != '\0' && *walk != '/') walk++;

		// save '\0' or '/'
		char c = *walk;
		*walk = '\0';
		int res = do_create(p, nbranch_ro, nbranch_rw);
		if (res) return res; // creating the directory failed
		// restore '\0' or '/'
		*walk = c;

		// as above the do loop, walk over the next slashes, walk = dir2/
		while (*walk != '\0' && *walk == '/') walk++;
	} while (*walk != '\0');

	return 0;
}

/**
 * Same as  path_create(), but ignore the last segment in path,
 * i.e. it might be a filename.
 */
int path_create_cutlast(const char *path, int nbranch_ro, int nbranch_rw) {
	DBG_IN();

	char *dname = u_dirname(path);
	if (dname == NULL)
		return -ENOMEM;
	int ret = path_create(dname, nbranch_ro, nbranch_rw);
	free(dname);

	return ret;
}

/**
 * initiate the cow-copy action
 */
int cow_cp(const char *path, int branch_ro, int branch_rw) {
	DBG_IN();

	// create the path to the file
	path_create_cutlast(path, branch_ro, branch_rw);

	setlocale(LC_ALL, "");

	struct cow cow;

	cow.uid = getuid();

	// Copy the umask for explicit mode setting.
	cow.umask = umask(0);
	umask(cow.umask);

	cow.from_branch = branch_ro;
	cow.from_path = path;
	cow.to_branch = branch_rw;
	cow.to_path = path;

	struct stat buf;
	int res = LSTAT(&buf, cow.from_branch, cow.from_path);
	if (res != 0) {
	  usyslog(LOG_WARNING,   "cow_cp: %s %d -> %d: file disapeared", path, branch_ro, branch_rw);
	  return res;
	}
	cow.stat = &buf;

	switch (buf.st_mode & S_IFMT) {
		case S_IFLNK:
			res = copy_link(&cow);
			break;
		case S_IFDIR:
			res = copy_directory(path, branch_ro, branch_rw);
			break;
		case S_IFBLK:
		case S_IFCHR:
			res = copy_special(&cow);
			break;
		case S_IFIFO:
			res = copy_fifo(&cow);
			break;
		case S_IFSOCK:
			usyslog(LOG_WARNING, "COW of sockets not supported: %d %s\n", cow.from_branch cow.from_path);
			return 1;
		default:
			res = copy_file(&cow);
	}

	return res;
}

/**
 * copy a directory between branches (includes all contents of the directory)
 */
int copy_directory(const char *path, int branch_ro, int branch_rw) {
	DBG_IN();

	/* create the directory on the destination branch */
	int res = path_create(path, branch_ro, branch_rw);
	if (res != 0) {
		return res;
	}

	/* space for path of directory plus file within */
	char p[PATHLEN_MAX];
	int len = snprintf(p, PATHLEN_MAX, "%s/", path);
	char *q = p + len; // start of file name inside dir

	DIR *dp = OPENDIR(branch_ro, path);
	if (dp == NULL) return 1;

	struct dirent *de;
	while ((de = readdir(dp)) != NULL) {
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

		if (len + strlen(de->d_name) >= PATHLEN_MAX) {
			errno = ENAMETOOLONG;
			return 1;
		}
		strcpy(q, de->d_name);
		res = cow_cp(p, branch_ro, branch_rw);
		if (res != 0) return res;
	}

	closedir(dp);
	return 0;
}
