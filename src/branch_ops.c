/*
* Description: filesystem operations on branches
*
* License: BSD-style license
*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*            Goswin von Brederlow <brederlo@q-leap.de>
*
*
* The functions below implement simple operations like lstat() or
* open() relative to a branch. This can use either the *at() family of
* functions or simple path concatenation.
*
* In all functions the path argument is made up of the branch number
* and one or more path components and is therefore moved to the end of
* the argument list. The path components MUST be terminated with a
* NULL argument.
*
* The other arguments, return values and error codes are identical to
* the original filesystem function implemented.
*/

#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h> // utimes()
#include <dirent.h>
#include <sys/xattr.h>

#include "general.h"
#include "opts.h"
#include "branch_ops.h"

int va_build_path(char *path, int space, int branch, va_list ap) {
	char *p = path;
	int len;
	(void)branch;

#ifndef HAVE_AT_FAMILY
	len = strlen(uopt.branches[branch].path);
	strcpy(p, uopt.branches[branch].path);
	space -= len;
	p += len;
#endif

        while (1) {
                char *str = va_arg (ap, char *);
                if (!str) break;
		len = strlen(str);
		space -= len;
                if (space <= 0) { // must have space for '\0' terminator
                        usyslog (LOG_WARNING, "%s: Path too long \n", __FUNC__);
                        errno = ENAMETOOLONG;
                        return -errno;
                }

                strcpy (p, str);
		p += len;
        }

	// terminate string
	*p = 0;
        return 0;
}

static int branch_build_path(char *path, int space, int branch, ...) {
	va_list ap;
	int res;

	va_start(ap, branch);
	res = va_build_path(path, space, branch, ap);
	va_end(ap);

	return res;
}

const char *relative(const char *path) {
	while(*path == '/') path++;
	if (*path == '\0') return ".";
	return path;
}

int branch_lstat(struct stat *buf, int branch, ...) {
	va_list ap;
	int res;

	va_start(ap, branch);
	res = branch_va_lstat(buf, branch, ap);
	va_end(ap);

	return res;
}

int branch_va_lstat(struct stat *buf, int branch, va_list ap) {
	int res;
	char path[PATHLEN_MAX];

	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = fstatat(uopt.branches[branch].fd, relative(path), buf, AT_SYMLINK_NOFOLLOW);
#else
	res = lstat(path, buf);
#endif
	return res;
}

int branch_stat(struct stat *buf, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = fstatat(uopt.branches[branch].fd, relative(path), buf, 0);
#else
	res = stat(path, buf);
#endif
	return res;
}

int branch_open(int flags, mode_t mode, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	fprintf(stderr, "fd = %d, path = %s\n", uopt.branches[branch].fd, relative(path));
	res = openat(uopt.branches[branch].fd, relative(path), flags, mode);
#else
	res = open(path, flags, mode);
#endif
	return res;
}

DIR *branch_opendir(int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];
	DIR *dir;

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return NULL;

#ifdef HAVE_AT_FAMILY
	int fd = openat(uopt.branches[branch].fd, relative(path), O_RDONLY | O_DIRECTORY);
	if (fd == -1) return 0;
	dir = fdopendir(fd);
	if (dir == NULL) close(fd);
#else
	dir = opendir(path);
#endif
	return dir;
}

int branch_mkdir(mode_t mode, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = mkdirat(uopt.branches[branch].fd, relative(path), mode);
#else
	res = mkdir(path, mode);
#endif
	return res;
}

int branch_rmdir(int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = unlinkat(uopt.branches[branch].fd, relative(path), AT_REMOVEDIR);
#else
	res = rmdir(path);
#endif
	return res;
}

int branch_unlink(int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = unlinkat(uopt.branches[branch].fd, relative(path), 0);
#else
	res = unlink(path);
#endif
	return res;
}

int branch_lchown(uid_t owner, gid_t group, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = fchownat(uopt.branches[branch].fd, relative(path), owner, group, AT_SYMLINK_NOFOLLOW);
#else
	res = lchown(path, owner, group);
#endif
	return res;
}

int branch_chown(uid_t owner, gid_t group, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = fchownat(uopt.branches[branch].fd, relative(path), owner, group, 0);
#else
	res = chown(path, owner, group);
#endif
	return res;
}

int branch_link(int oldbranch, const char *oldpath, int newbranch, const char *newpath) {
	int res;
	char frompath[PATHLEN_MAX];
	char topath[PATHLEN_MAX];

	res = branch_build_path(frompath, PATHLEN_MAX, oldbranch, oldpath, NULL);
	if (res < 0) return res;

	res = branch_build_path(topath, PATHLEN_MAX, newbranch, newpath, NULL);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = linkat(uopt.branches[oldbranch].fd, relative(frompath),
		     uopt.branches[newbranch].fd, relative(topath), 0);
#else
	res = link(frompath, topath);
#endif
	return res;
}

int branch_chmod(mode_t mode, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = fchmodat(uopt.branches[branch].fd, relative(path), mode, 0);
#else
	res = chmod(path, mode);
#endif
	return res;
}

int branch_creat(mode_t mode, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	fprintf(stderr, "fd = %d, path = %s\n", uopt.branches[branch].fd, relative(path));
	res = openat(uopt.branches[branch].fd, relative(path), O_CREAT | O_WRONLY | O_TRUNC, mode);
#else
	res = creat(path, mode);
#endif
	return res;
}

int branch_mknod(mode_t mode, dev_t dev, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = mknodat(uopt.branches[branch].fd, relative(path), mode, dev);
#else
	res = mknod(path, mode, dev);
#endif
	return res;
}

int branch_mkfifo(mode_t mode, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = mkfifoat(uopt.branches[branch].fd, relative(path), mode);
#else
	res = mkfifo(path, mode);
#endif
	return res;
}

ssize_t branch_readlink(char *buf, size_t bufsiz, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = readlinkat(uopt.branches[branch].fd, relative(path), buf, bufsiz);
#else
	res = readlink(path, buf, bufsiz);
#endif
	return res;
}

int branch_symlink(const char *oldpath, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = symlinkat(oldpath, uopt.branches[branch].fd, relative(path));
#else
	res = symlink(oldpath, path);
#endif
	return res;
}

int branch_rename(int oldbranch, const char *oldpath, int newbranch, const char *newpath) {
	int res;
	char frompath[PATHLEN_MAX];
	char topath[PATHLEN_MAX];
fprintf(stderr, "branch_rename(): building frompath\n");
	res = branch_build_path(frompath, PATHLEN_MAX, oldbranch, oldpath, NULL);
	if (res < 0) return res;

fprintf(stderr, "branch_rename(): building topath\n");
	res = branch_build_path(topath, PATHLEN_MAX, newbranch, newpath, NULL);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
fprintf(stderr, "branch_rename(): renameat (%d: %s) -> (%d: %s)\n", oldbranch, relative(frompath), newbranch, relative(topath));
	res = renameat(uopt.branches[oldbranch].fd, relative(frompath),
		       uopt.branches[newbranch].fd, relative(topath));
#else
fprintf(stderr, "branch_rename(): rename(%s, %s)\n", frompath, topath);
	res = rename(frompath, topath);
#endif
fprintf(stderr, "branch_rename(): res = %d\n", res);
	return res;
}

int branch_statfs(struct statfs *buf, int branch) {
	int res;

#ifdef HAVE_AT_FAMILY
	res = fstatfs(uopt.branches[branch].fd, buf);
#else
	res = statfs(uopt.branches[branch].path, buf);
#endif
	return res;
}

int branch_truncate(off_t length, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	int fd = openat(uopt.branches[branch].fd, relative(path), O_WRONLY);
	if (fd == -1) return -1;
	res = ftruncate(fd, length);
	close(fd);
#else
	res = truncate(path, length);
#endif
	return res;
}

int branch_utimens(const struct timespec times[2], int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = utimensat(uopt.branches[branch].fd, relative(path), times, AT_SYMLINK_NOFOLLOW);
#else
	struct timeval tv[2];
        tv[0].tv_sec  = times[0].tv_sec;
        tv[0].tv_usec = times[0].tv_nsec / 1000;
        tv[1].tv_sec  = times[1].tv_sec;
        tv[1].tv_usec = times[1].tv_nsec / 1000;

	res = utimes(path, tv);
	if (res == -1 && errno == ENOENT) {
		// weird utimes() symlink bug?
		struct stat buf;
		res = lstat(path, &buf);
		if (res || !S_ISLNK(buf.st_mode)) return - ENOENT;
		// nothing we can do something about, seems to be a failure
		// of the underlying filesystem or the syscall itself is not
		// supported on symlinks. utime() also does not work.
		// As the error is annyoing and point to a non-existing error
		// in unionfs-fuse, we ignore it
		// The real solution will be utimensat(0, p, ts, AT_SYMLINK_NOFOLLOW);
		res = 0;
	}
#endif
	return res;
}

#ifdef HAVE_CHFLAGS
int chflags(int flags, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	res = chflagsat(uopt.branches[branch].fd, relative(path), flags);
#else
	res = chflags(path, flags);
#endif
	return res;
}

#endif

#ifdef HAVE_SETXATTR
int branch_lsetxattr(const char *name, const void *value, size_t size, int flags, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	int fd = openat(uopt.branches[branch].fd, relative(path), O_WRONLY | O_NOFOLLOW);
	if (fd == -1) {
	  if (errno == ELOOP) errno = ENOTSUP; // no xattr for symlinks
	  return -1;
	}
	res = fsetxattr(fd, name, value, size, flags);
	close(fd);
#else
	res = lsetxattr(path, name, value, size, flags);
#endif
	return res;
}

ssize_t branch_lgetxattr(const char *name, void *value, size_t size, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	int fd = openat(uopt.branches[branch].fd, relative(path), O_RDONLY | O_NOFOLLOW);
	if (fd == -1) {
	  if (errno == ELOOP) errno = ENOTSUP; // no xattr for symlinks
	  return -1;
	}
	res = fgetxattr(fd, name, value, size);
	close(fd);
#else
	res = lgetxattr(path, name, value, size);
#endif
	return res;
}

ssize_t branch_llistxattr(char *list, size_t size, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	int fd = openat(uopt.branches[branch].fd, relative(path), O_RDONLY | O_NOFOLLOW);
	if (fd == -1) {
	  if (errno == ELOOP) errno = ENOTSUP; // no xattr for symlinks
	  return -1;
	}
	res = flistxattr(fd, list, size);
	close(fd);
#else
	res = llistxattr(path, list, size);
#endif
	return res;
}

int branch_lremovexattr(const char *name, int branch, ...) {
	va_list ap;
	int res;
	char path[PATHLEN_MAX];

	va_start(ap, branch);
	res = va_build_path(path, PATHLEN_MAX, branch, ap);
	va_end(ap);
	if (res < 0) return res;

#ifdef HAVE_AT_FAMILY
	int fd = openat(uopt.branches[branch].fd, relative(path), O_WRONLY | O_NOFOLLOW);
	if (fd == -1) {
	  if (errno == ELOOP) errno = ENOTSUP; // no xattr for symlinks
	  return -1;
	}
	res = fremovexattr(fd, name);
	close(fd);
#else
	res = lremovexattr(path, name);
#endif
	return res;
}
#endif
