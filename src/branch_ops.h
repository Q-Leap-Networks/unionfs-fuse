/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*            Goswin von Brederlow <brederlo@q-leap.de>
*/

#ifndef BRANCH_OPS_H
#define BRANCH_OPS_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/vfs.h>    /* or <sys/statfs.h> */
#include <dirent.h>

int branch_lstat(struct stat *buf, int branch, ...);
int branch_va_lstat(struct stat *buf, int branch, va_list ap);
#define LSTAT(buf, branch, ...) branch_lstat(buf, branch, __VA_ARGS__, NULL)

int branch_stat(struct stat *buf, int branch, ...);
#define STAT(buf, branch, ...) branch_stat(buf, branch, __VA_ARGS__, NULL)

int branch_open(int flags, mode_t mode, int branch, ...);
#define OPEN(flags, mode, branch, ...) branch_open(flags, mode, branch, __VA_ARGS__, NULL)

DIR *branch_opendir(int branch, ...);
#define OPENDIR(branch, ...) branch_opendir(branch, __VA_ARGS__, NULL)

int branch_mkdir(mode_t mode, int branch, ...);
#define MKDIR(mode, branch, ...) branch_mkdir(mode, branch, __VA_ARGS__, NULL)

int branch_rmdir(int branch, ...);
#define RMDIR(branch, ...) branch_rmdir(branch, __VA_ARGS__, NULL)

int branch_unlink(int branch, ...);
#define UNLINK(branch, ...) branch_unlink(branch, __VA_ARGS__, NULL)

int branch_lchown(uid_t owner, gid_t group, int branch, ...);
#define LCHOWN(owner, group, branch, ...) branch_lchown(owner, group, branch, __VA_ARGS__, NULL)

int branch_chown(uid_t owner, gid_t group, int branch, ...);
#define CHOWN(owner, group, branch, ...) branch_chown(owner, group, branch, __VA_ARGS__, NULL)

int branch_link(int oldbranch, const char *oldpath, int newbranch, const char *newpath);
#define LINK(oldbranch, oldpath, newbranch,newpath) branch_link(oldbranch, oldpath, newbranch,newpath);

int branch_chmod(mode_t mode, int branch, ...);
#define CHMOD(mode, branch, ...) branch_chmod(mode, branch, __VA_ARGS__, NULL)

int branch_creat(mode_t mode, int branch, ...);
#define CREAT(mode, branch, ...) branch_creat(mode, branch, __VA_ARGS__, NULL)

int branch_mknod(mode_t mode, dev_t dev, int branch, ...);
#define MKNOD(mode, dev, branch, ...) branch_mknod(mode, dev, branch, __VA_ARGS__, NULL)

int branch_mkfifo(mode_t mode, int branch, ...);
#define MKFIFO(mode, branch, ...) branch_mkfifo(mode, branch, __VA_ARGS__, NULL)

ssize_t branch_readlink(char *buf, size_t bufsiz, int branch, ...);
#define READLINK(buf, bufsiz, branch, ...) branch_readlink(buf, bufsiz, branch, __VA_ARGS__, NULL)

int branch_symlink(const char *oldpath, int branch, ...);
#define SYMLINK(oldpath, branch, ...) branch_symlink(oldpath, branch, __VA_ARGS__, NULL)

int branch_rename(int oldbranch, const char *oldpath, int newbranch, const char *newpath);
#define RENAME(oldbranch, oldpath, newbranch, newpath) branch_rename(oldbranch, oldpath, newbranch, newpath)

int branch_statfs(struct statfs *buf, int branch);
#define STATFS(buf, branch) branch_statfs(buf, branch)

int branch_truncate(off_t length, int branch, ...);
#define TRUNCATE(length, branch, ...) branch_truncate(length, branch, __VA_ARGS__, NULL)

int branch_utimens(const struct timespec times[2], int branch, ...);
#define UTIMENS(times, branch, ...) branch_utimens(times, branch, __VA_ARGS__, NULL)

#ifdef HAVE_CHFLAGS
int branch_chflags(int flags, int branch, ...);
#define CHFLAGS(flags, branch, ...) branch_chflags(flags, branch, __VA_ARGS__, NULL)
#endif

#ifdef HAVE_SETXATTR
int branch_lsetxattr(const char *name, const void *value, size_t size, int flags, int branch, ...);
#define LSETXATTR(name, value, size, flags, branch, ...) branch_lsetxattr(name, value, size, flags, branch, __VA_ARGS__, NULL)

ssize_t branch_lgetxattr(const char *name, void *value, size_t size, int branch, ...);
#define LGETXATTR(name, value, size, branch, ...) branch_lgetxattr(name, value, size, branch, __VA_ARGS__, NULL)

ssize_t branch_llistxattr(char *list, size_t size, int branch, ...);
#define LLISTXATTR(list, size, branch, ...) branch_llistxattr(list, size, branch, __VA_ARGS__, NULL)

int branch_lremovexattr(const char *name, int branch, ...);
#define LREMOVEXATTR(name, branch, ...) branch_lremovexattr(name, branch, __VA_ARGS__, NULL)
#endif

#endif
