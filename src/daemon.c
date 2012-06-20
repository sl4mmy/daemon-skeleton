/*
 * Copyright (c) 2011, 2012 Kent R. Spillner <kspillner@acm.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "daemon.h"
#include "log.h"

void	 init_child(pid_t, char *);

pid_t
daemonize(char *working_dir)
{
	pid_t	 process_id;

	process_id = fork();
	if (process_id == 0) {
		log_info("Forked daemon: pid=%ld\n", process_id);
		init_child(process_id, working_dir);
	}

	return process_id;
}

void
init_child(pid_t child_id, char *working_dir)
{
	umask(0);

	if (setsid() == -1) {
		log_error("Unable to create new session: %s\n",
		    strerror(errno));
		goto error_out;
	}

	if (chdir(working_dir) == -1) {
		log_error("Unable to change to directory %s: %s\n",
		    working_dir, strerror(errno));
		goto error_out;
	}

	if (close(STDIN_FILENO) == -1) {
		log_error("Unable to close stdin: %s\n", strerror(errno));
		goto error_out;
	}
	if (close(STDOUT_FILENO) == -1) {
		log_error("Unable to close stdout: %s\n", strerror(errno));
		goto error_out;
	}
	if (close(STDERR_FILENO) == -1) {
		log_error("Unable to close stderr: %s\n", strerror(errno));
		goto error_out;
	}

	return;

error_out:
	close_log();
	free(working_dir);
	exit(EXIT_FAILURE);
}
