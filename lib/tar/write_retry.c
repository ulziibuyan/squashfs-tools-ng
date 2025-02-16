/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * write_retry.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "tar.h"

int write_retry(const char *errstr, int fd, const void *data, size_t size)
{
	ssize_t ret;

	while (size > 0) {
		ret = write(fd, data, size);
		if (ret == 0) {
			fprintf(stderr, "%s: write truncated\n", errstr);
			return -1;
		}
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror(errstr);
			return -1;
		}

		data = (const char *)data + ret;
		size -= ret;
	}

	return 0;
}
