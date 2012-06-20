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

#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "application.h"
#include "daemon.h"
#include "log.h"
#include "paths.h"
#include "socket.h"

#ifndef MAX
#define MAX(left, right)	(((left) >= (right)) ? (left) : (right))
#endif

int	 main_loop(struct application *);
char	*working_dir(void);

int
main(int argc, char **argv)
{
	struct application	*application;
	time_t			 started_at;
	int			 control_socket, result;
	char			*cwd;

	started_at = time(NULL);

	init_log();

	cwd = working_dir();
	switch (daemonize(cwd)) {
	case -1:
		log_error("Unable to daemonize: %s\n", strerror(errno));
		goto error_out;
	case 0:
		/* In the child process. */
		break;
	default:
		/* In the parent process. */
		close_log();
		free(cwd);
		exit(EXIT_SUCCESS);
	}

	if (!access(APPLICATION_CONTROL_SOCKET, F_OK) &&
	    (unlink(APPLICATION_CONTROL_SOCKET) == -1)) {
		log_error("Unable to remove previous control socket: %s\n",
		    strerror(errno));
		goto error_out;
	}

	application = (struct application *)malloc(sizeof(struct application));
	if (application == NULL) {
		log_error("Unable to allocate enough memory to hold "
		    "information about this instance of application: %s\n",
		    strerror(errno));
		goto error_out;
	}
	memset(application, 0, sizeof(struct application));
	application->process_id = getpid();
	application->started_at = started_at;

	control_socket = init_control_socket(APPLICATION_CONTROL_SOCKET, bind);
	if (control_socket == -1) {
		goto free_error_out;
	}
	application->control_socket = control_socket;

	result = main_loop(application);

	if (unlink(APPLICATION_CONTROL_SOCKET) == -1) {
		log_error("Unable to remove control socket: %s\n",
		    strerror(errno));
		goto close_error_out;
	}

	if (close(application->control_socket) == -1) {
		log_error("Unable to close control socket: %s\n",
		    strerror(errno));
		goto free_error_out;
	}

	close_log();
	free(cwd);
	free(application);
	exit(result);

close_error_out:
	close(application->control_socket);
free_error_out:
	free(application);
error_out:
	free(cwd);
	close_log();
	exit(EXIT_FAILURE);
}

int
main_loop(struct application *application)
{
	fd_set				 fds;
	enum message_type		 msg_type;
	int				 connection, highest_fd;

	if (listen(application->control_socket, MAX_PENDING_CONNECTIONS) == -1) {
		log_error("Unable to listen on control socket: %s\n",
		    strerror(errno));
		return EXIT_FAILURE;
	}

	highest_fd = 0;
	for (;;) {
		FD_ZERO(&fds);
		FD_SET(application->control_socket, &fds);
		highest_fd = MAX(highest_fd, application->control_socket);

		switch(select(highest_fd + 1, &fds, NULL, NULL, NULL)) {
		case -1:
			log_error("Unable to wait for data: %s\n",
			    strerror(errno));
			return EXIT_FAILURE;
		default:
			if (FD_ISSET(application->control_socket, &fds)) {
				connection = accept(application->control_socket,
				    NULL, NULL);
				if (connection == -1) {
					log_error("Unable to accept new "
					    "connection: %s\n",
					    strerror(errno));
					return EXIT_FAILURE;
				}

				msg_type = MSG_CTL_NONE;
				if (read(connection, &msg_type,
				    sizeof(msg_type))) {
					switch (msg_type) {
					case MSG_CTL_NONE:
						break;
					case MSG_CTL_STATUS:
						write(connection, application,
						    sizeof(struct application));
						break;
					case MSG_CTL_QUIT:
						log_info("Shutting down\n");
						close(connection);
						return EXIT_SUCCESS;
					}
				}
				close(connection);
			}
		}
	}

	return EXIT_SUCCESS;
}

char *
working_dir(void)
{
	struct passwd	*passwd_entry;
	size_t		 home_dir_length, suffix_length, working_dir_length;
	uid_t		 user_id;
	char		*working_dir;

	user_id = getuid();
	passwd_entry = getpwuid(user_id);

	home_dir_length = strlen(passwd_entry->pw_dir);
	suffix_length = strlen(APPLICATION_WORKING_DIR_SUFFIX);
	working_dir_length = home_dir_length + suffix_length;

	working_dir = (char *)malloc(working_dir_length + 1);
	if (working_dir == NULL) {
		log_error("Unable to allocate enough memory to hold "
		    "path to working directory: %s\n", strerror(errno));
		close_log();
		exit(EXIT_FAILURE);
	}

	memset(working_dir, 0, working_dir_length + 1);
	strcpy(working_dir, passwd_entry->pw_dir);
	strcat(working_dir, APPLICATION_WORKING_DIR_SUFFIX);

	return working_dir;
}
