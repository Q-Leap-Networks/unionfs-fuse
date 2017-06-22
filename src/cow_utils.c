/*-
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Bernd Schubert <bernd-schubert@gmx.de>:
 *	This file was taken from OpenBSD and modified to fit the unionfs requirements.
 */


#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "unionfs.h"
#include "branch_ops.h"
#include "cow_utils.h"
#include "debug.h"
#include "general.h"

// BSD seems to know S_ISTXT itself
#ifndef S_ISTXT
#define S_ISTXT S_ISVTX
#endif

/**
 * set the stat() data of a file
 **/
int setfile(int branch, const char *path, struct stat *fs)
{
	DBG_IN();

	struct timespec times[2];
	int rval;

	rval = 0;
	fs->st_mode &= S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO;

	times[0].tv_sec = fs->st_atime;
	times[0].tv_nsec = 0;
        times[1].tv_sec = fs->st_mtime;
        times[1].tv_nsec = 0;

	if (UTIMENS(times, branch, path)) {
		usyslog(LOG_WARNING,   "utimens: %d %s", branch, path);
		rval = 1;
	}
	/*
	* Changing the ownership probably won't succeed, unless we're root
	* or POSIX_CHOWN_RESTRICTED is not set.  Set uid/gid before setting
	* the mode; current BSD behavior is to remove all setuid bits on
	* chown.  If chown fails, lose setuid/setgid bits.
	*/
	if (CHOWN(fs->st_uid, fs->st_gid, branch, path)) {
		if (errno != EPERM) {
			usyslog(LOG_WARNING,   "chown: %d %s", branch, path);
			rval = 1;
		}
		fs->st_mode &= ~(S_ISTXT | S_ISUID | S_ISGID);
	}
	
	if (CHMOD(fs->st_mode, branch, path)) {
		usyslog(LOG_WARNING,   "chown: %d %s", branch, path);
		rval = 1;
	}

#ifdef HAVE_CHFLAGS
		/*
		 * XXX
		 * NFS doesn't support chflags; ignore errors unless there's reason
		 * to believe we're losing bits.  (Note, this still won't be right
		 * if the server supports flags and we were trying to *remove* flags
		 * on a file that we copied, i.e., that we didn't create.)
		 */
		errno = 0;
		if (CHFLAGS(fs->st_flags, branch, path)) {
			if (errno != EOPNOTSUPP || fs->st_flags != 0) {
				usyslog(LOG_WARNING,   "chflags: %d %s", branch, path);
				rval = 1;
			}
		}
#endif
	return rval;
}

/**
 * set the stat() data of a link
 **/
static int setlink(int branch, const char *path, struct stat *fs)
{
	DBG_IN();

	if (LCHOWN(fs->st_uid, fs->st_gid, branch, path)) {
		if (errno != EPERM) {
			usyslog(LOG_WARNING,   "lchown: %d %s", branch, path);
			return (1);
		}
	}
	return (0);
}


/**
 * copy an ordinary file with all of its stat() data
 **/
int copy_file(struct cow *cow)
{
	DBG_IN();

	static char buf[MAXBSIZE];
	struct stat to_stat, *fs;
	int from_fd, rcount, to_fd, wcount;
	int rval = 0;
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
	char *p;
#endif

	if ((from_fd = OPEN(O_RDONLY, 0, cow->from_branch, cow->from_path)) == -1) {
		usyslog(LOG_WARNING, "%d %s", cow->from_branch, cow->from_path);
		return (1);
	}

	fs = cow->stat;

	to_fd = OPEN(O_WRONLY | O_TRUNC | O_CREAT,
	             fs->st_mode & ~(S_ISTXT | S_ISUID | S_ISGID),
		     cow->to_branch, cow->to_path);

	if (to_fd == -1) {
		usyslog(LOG_WARNING, "%d %s", cow->to_branch, cow->to_path);
		(void)close(from_fd);
		return (1);
	}

	/*
	 * Mmap and write if less than 8M (the limit is so we don't totally
	 * trash memory on big files.  This is really a minor hack, but it
	 * wins some CPU back.
	 */
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
	if (fs->st_size > 0 && fs->st_size <= 8 * 1048576) {
		if ((p = mmap(NULL, (size_t)fs->st_size, PROT_READ,
		    MAP_FILE|MAP_SHARED, from_fd, (off_t)0)) == MAP_FAILED) {
			usyslog(LOG_WARNING,   "mmap: %d %s", cow->from_branch, cow->from_path);
			rval = 1;
		} else {
			madvise(p, fs->st_size, MADV_SEQUENTIAL);
			if (write(to_fd, p, fs->st_size) != fs->st_size) {
				usyslog(LOG_WARNING,   "%d %s", cow->to_branch, cow->to_path);
				rval = 1;
			}
			/* Some systems don't unmap on close(2). */
			if (munmap(p, fs->st_size) < 0) {
				usyslog(LOG_WARNING,   "%d %s", cow->from_branch, cow->from_path);
				rval = 1;
			}
		}
	} else
#endif
	{
		while ((rcount = read(from_fd, buf, MAXBSIZE)) > 0) {
			wcount = write(to_fd, buf, rcount);
			if (rcount != wcount || wcount == -1) {
				usyslog(LOG_WARNING,   "%d %s", cow->to_branch, cow->to_path);
				rval = 1;
				break;
			}
		}
		if (rcount < 0) {
			usyslog(LOG_WARNING,   "copy failed: %d %s", cow->from_branch, cow->from_path);
			return 1;
		}
	}

	if (rval == 1) {
		(void)close(from_fd);
		(void)close(to_fd);
		return (1);
	}

	if (setfile(cow->to_branch, cow->to_path, cow->stat))
		rval = 1;
	/*
	 * If the source was setuid or setgid, lose the bits unless the
	 * copy is owned by the same user and group.
	 */
#define	RETAINBITS \
	(S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)
	else if (fs->st_mode & (S_ISUID | S_ISGID) && fs->st_uid == cow->uid) {
		if (fstat(to_fd, &to_stat)) {
			usyslog(LOG_WARNING,   "%d %s", cow->to_branch, cow->to_path);
			rval = 1;
		} else if (fs->st_gid == to_stat.st_gid &&
		    fchmod(to_fd, fs->st_mode & RETAINBITS & ~cow->umask)) {
			usyslog(LOG_WARNING,   "%d %s", cow->to_branch, cow->to_path);
			rval = 1;
		}
	}
	(void)close(from_fd);
	if (close(to_fd)) {
		usyslog(LOG_WARNING,   "%d %s", cow->to_branch, cow->to_path);
		rval = 1;
	}
	
	return (rval);
}

/**
 * copy a link, actually we recreate the link and only copy its stat() data.
 */
int copy_link(struct cow *cow)
{
	DBG_IN();

	int len;
	char link[PATHLEN_MAX];

	if ((len = READLINK(link, sizeof(link)-1, cow->from_branch, cow->from_path)) == -1) {
		usyslog(LOG_WARNING,   "readlink: %d %s", cow->from_branch, cow->from_path);
		return (1);
	}

	link[len] = '\0';
	
	if (SYMLINK(link, cow->to_branch, cow->to_path)) {
		usyslog(LOG_WARNING,   "symlink: %s", link);
		return (1);
	}
	
	return setlink(cow->to_branch, cow->to_path, cow->stat);
}

/**
 * copy a fifo, actually we recreate the fifo and only copy
 * its stat() data
 **/
int copy_fifo(struct cow *cow)
{
	DBG_IN();

	if (MKFIFO(cow->stat->st_mode, cow->to_branch, cow->to_path)) {
		usyslog(LOG_WARNING,   "mkfifo: %d %s", cow->to_branch, cow->to_path);
		return (1);
	}
	return setfile(cow->to_branch, cow->to_path, cow->stat);
}

/**
 * copy a special file, actually we recreate this file and only copy
 * its stat() data
 */
int copy_special(struct cow *cow)
{
	DBG_IN();

	if (MKNOD(cow->stat->st_mode, cow->stat->st_rdev, cow->to_branch, cow->to_path)) {
		usyslog(LOG_WARNING,   "mknod: %d %s", cow->to_branch, cow->to_path);
		return (1);
	}
	return setfile(cow->to_branch, cow->to_path, cow->stat);
}
