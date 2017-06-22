/*
* License: BSD-style license
* Copyright: Radek Podgorny <radek@podgorny.cz>,
*            Bernd Schubert <bernd-schubert@gmx.de>
*/

#ifndef COW_UTILS_H
#define COW_UTILS_H

#define VM_AND_BUFFER_CACHE_SYNCHRONIZED
#define MAXBSIZE 4096

struct cow {
	mode_t umask;
	uid_t uid;

	// source file
	int from_branch;
	const char  *from_path;
	struct stat *stat;

	// destination file
	int to_branch;
	const char *to_path;
};

int setfile(int branch, const char *path, struct stat *fs);
int copy_special(struct cow *cow);
int copy_fifo(struct cow *cow);
int copy_link(struct cow *cow);
int copy_file(struct cow *cow);

#endif
