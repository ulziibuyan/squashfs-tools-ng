/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_reader_dump.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static int append_block(int fd, const sqfs_block_t *blk)
{
	const unsigned char *ptr = blk->data;
	size_t size = blk->size;
	ssize_t ret;

	while (size > 0) {
		ret = write(fd, ptr, size);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror("writing data block");
		}

		if (ret == 0) {
			fputs("writing data block: unexpected end of file\n",
			      stderr);
		}

		ptr += ret;
		size -= ret;
	}

	return 0;
}

int sqfs_data_reader_dump(const char *name, sqfs_data_reader_t *data,
			  const sqfs_inode_generic_t *inode,
			  int outfd, size_t block_size, bool allow_sparse)
{
	sqfs_block_t *blk;
	sqfs_u64 filesz;
	size_t i, diff;
	int err;

	sqfs_inode_get_file_size(inode, &filesz);

	if (allow_sparse && ftruncate(outfd, filesz))
		goto fail_sparse;

	for (i = 0; i < inode->num_file_blocks; ++i) {
		if (SQFS_IS_SPARSE_BLOCK(inode->block_sizes[i]) &&
		    allow_sparse) {
			if (filesz < block_size) {
				diff = filesz;
				filesz = 0;
			} else {
				diff = block_size;
				filesz -= block_size;
			}

			if (lseek(outfd, diff, SEEK_CUR) == (off_t)-1)
				goto fail_sparse;
		} else {
			err = sqfs_data_reader_get_block(data, inode, i, &blk);
			if (err) {
				sqfs_perror(name, "reading data block", err);
				return -1;
			}

			if (append_block(outfd, blk)) {
				free(blk);
				return -1;
			}

			filesz -= blk->size;
			free(blk);
		}
	}

	if (filesz > 0) {
		err = sqfs_data_reader_get_fragment(data, inode, &blk);
		if (err) {
			sqfs_perror(name, "reading fragment block", err);
			return -1;
		}

		if (append_block(outfd, blk)) {
			free(blk);
			return -1;
		}

		free(blk);
	}

	return 0;
fail_sparse:
	perror("creating sparse output file");
	return -1;
}
