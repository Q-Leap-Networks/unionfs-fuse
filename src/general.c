/*
*  C Implementation: general
*
* Description: General functions, not directly related to file system operations
*
* original implementation by Radek Podgorny
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*
*/

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <pthread.h>

#include "unionfs.h"
#include "opts.h"
#include "string.h"
#include "cow.h"
#include "findbranch.h"
#include "branch_ops.h"
#include "general.h"
#include "debug.h"


/**
 * Check if a file or directory with the hidden flag exists.
 */
static bool filedir_hidden(const char *path, int branch) {
	DBG_IN();

	// cow mode disabled, no need for hidden files
	if (!uopt.cow_enabled) return false;
	
	struct stat stbuf;
	int res = LSTAT(&stbuf, branch, METADIR, path, HIDETAG);
	if (res == 0) return true;

	return false;
}

/**
 * check if any dir or file within path is hidden
 */
bool path_hidden(const char *path, int branch) {
	DBG_IN();
	if (!uopt.cow_enabled) return false;

	char p[PATHLEN_MAX];
	char *walk = p;
	strcpy(p, path);

	// first slashes, e.g. we have path = /dir1/dir2/, will set walk = dir1/dir2/
	while (*walk != '\0' && *walk == '/') walk++;

	do {
		// walk over the directory name, walk will now be /dir2
		while (*walk != '\0' && *walk != '/') walk++;

		const char c = *walk; // save '\0' or '/'
		*walk = '\0';
		bool res = filedir_hidden(p, branch);
		if (res) return res; // path is hidden
		*walk = c; // restore '\0' or '/'

		// as above the do loop, walk over the next slashes, walk = dir2/
		while (*walk != '\0' && *walk == '/') walk++;
	} while (*walk != '\0');

	return 0;
}

/**
 * Remove a hide-file in all branches up to maxbranch
 * If maxbranch == -1, try to delete it in all branches.
 */
int remove_hidden(const char *path, int maxbranch) {
	DBG_IN();

	if (!uopt.cow_enabled) return 0;

	if (maxbranch == -1) maxbranch = uopt.nbranches;

	int i;
	for (i = 0; i <= maxbranch; i++) {
		switch (path_is_dir(i, METADIR, path, HIDETAG, NULL)) {
			case IS_FILE: UNLINK(i, METADIR, path, HIDETAG); break;
			case IS_DIR: RMDIR(i, METADIR, path, HIDETAG); break;
			case NOT_EXISTING: continue;
		}
	}

	return 0;
}

/**
 * check if path is a directory
 *
 * return proper types given by filetype_t
 */
filetype_t path_is_dir(int branch, ...) {
	DBG_IN();

	va_list ap;
	int res;
	struct stat buf;

	va_start(ap, branch);
	res = branch_va_lstat(&buf, branch, ap);
	va_end(ap);

	if (res == -1) return NOT_EXISTING;
	
	if (S_ISDIR(buf.st_mode)) return IS_DIR;
	
	return IS_FILE;
}

/**
 * Create a file or directory that hides path below branch_rw
 */
static int do_create_whiteout(const char *path, int branch_rw, enum whiteout mode) {
	DBG_IN();

	char metapath[PATHLEN_MAX];

	if (BUILD_PATH(metapath, METADIR, path))  return -1;

	// p MUST be without path to branch prefix here! 2 x branch_rw is correct here!
	// this creates e.g. branch/.unionfs/some_directory
	path_create_cutlast(metapath, branch_rw, branch_rw);

	int res;
	if (mode == WHITEOUT_FILE) {
		res = OPEN(O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR, branch_rw, metapath, HIDETAG);
		if (res == -1) return -1;
		res = close(res);
	} else {
		res = MKDIR(S_IRWXU, branch_rw, metapath, HIDETAG);
	}

	return res;
}

/**
 * Create a file that hides path below branch_rw
 */
int hide_file(const char *path, int branch_rw) {
	DBG_IN();
	return do_create_whiteout(path, branch_rw, WHITEOUT_FILE);
}

/**
 * Create a directory that hides path below branch_rw
 */
int hide_dir(const char *path, int branch_rw) {
	DBG_IN();
	return do_create_whiteout(path, branch_rw, WHITEOUT_DIR);
}

/**
 * This is called *after* unlink() or rmdir(), create a whiteout file
 * if the same file/dir does exist in a lower branch
 */
int maybe_whiteout(const char *path, int branch_rw, enum whiteout mode) {
	DBG_IN();

	// we are not interested in the branch itself, only if it exists at all
	if (find_rorw_branch(path) != -1) {
		return do_create_whiteout(path, branch_rw, mode);
	}

	return 0;
}

/**
 * Set file owner of after an operation, which created a file.
 */
int set_owner(const char *path, int branch) {
	struct fuse_context *ctx = fuse_get_context();
	if (ctx->uid != 0 && ctx->gid != 0) {
		int res = LCHOWN(ctx->uid, ctx->gid, branch, path);
		if (res) {
			usyslog(LOG_WARNING,
			       ":%s: Setting the correct file owner failed: %s !\n", 
			       __func__, strerror(errno));
			return -errno;
		}
	}
	return 0;
}
