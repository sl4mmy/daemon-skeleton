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
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "application.h"
#include "log.h"
#include "paths.h"
#include "socket.h"

void	 usage(char *);
char	*working_dir(void);

int
main(int argc, char **argv)
{
	struct application	 application;
	size_t			 control_socket_length, socket_path_length,
				    socket_path_root_length;
	enum message_type	 msg_type;
	int			 bytes_read, control_socket;
	char			*socket_path, *socket_path_root;

	socket_path = NULL;
	socket_path_root = NULL;

	if (argc != 2) {
		usage(argv[0]);
		goto error_out;
	}

	if (!strcmp("status", argv[1])) {
		msg_type = MSG_CTL_STATUS;
	} else if (!strcmp("quit", argv[1])) {
		msg_type = MSG_CTL_QUIT;
	} else {
		usage(argv[0]);
		goto error_out;
	}

	control_socket_length = strlen(APPLICATION_CONTROL_SOCKET);

	socket_path_root = working_dir();
	socket_path_root_length = strlen(socket_path_root);

	socket_path_length = socket_path_root_length + control_socket_length;

	socket_path = (char *)malloc(socket_path_length + 1);
	if (socket_path == NULL) {
		log_error("Unable to allocate enough memory to hold control "
		    "socket path: %s\n", strerror(errno));
		goto free_error_out;
	}

	memset(socket_path, 0, socket_path_length + 1);
	strcpy(socket_path, socket_path_root);
	strcat(socket_path, APPLICATION_CONTROL_SOCKET);

	control_socket = init_control_socket(socket_path, connect);
	if (control_socket == -1) {
		goto free_error_out;
	}

	write(control_socket, &msg_type, sizeof(msg_type));
	memset(&application, 0, sizeof(application));
	bytes_read = read(control_socket, &application, sizeof(application));
	if (bytes_read == -1) {
		log_error("Unable to read response from daemon: %s\n",
		    strerror(errno));
		goto close_error_out;
	}

	close(control_socket);
	free(socket_path_root);
	free(socket_path);

	return EXIT_SUCCESS;

close_error_out:
	close(control_socket);
free_error_out:
	if (socket_path_root != NULL) {
		free(socket_path_root);
	}
	if (socket_path != NULL) {
		free(socket_path);
	}
error_out:
	exit(EXIT_FAILURE);
}

void
close_log(void)
{
	/* NOP */
}

void
init_log(void)
{
	/* NOP */
}

void
log_error(const char *message, ...)
{
	va_list	 argv;

	va_start(argv, message);
	vfprintf(stderr, message, argv);
	va_end(argv);
}

void
log_info(const char *message, ...)
{
	va_list	 argv;

	va_start(argv, message);
	vfprintf(stdout, message, argv);
	va_end(argv);
}

void
usage(char *invoked_as)
{
	log_error("Usage: %s <message type>\n", invoked_as);
	log_error("  message type: status | quit\n");
}

char *
working_dir(void)
{
	struct passwd	*passwd_entry;
	uid_t		 user_id;
	size_t		 home_dir_length, suffix_length, working_dir_length;
	char		*working_dir;

	user_id = getuid();
	passwd_entry = getpwuid(user_id);

	home_dir_length = strlen(passwd_entry->pw_dir);
	suffix_length = strlen(APPLICATION_WORKING_DIR_SUFFIX);
	working_dir_length = home_dir_length + suffix_length;

	working_dir = (char *)malloc(working_dir_length + 1);
	if (working_dir == NULL) {
		log_error("Unable to allocate enough memory to hold name of "
		    "working directory: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	memset(working_dir, 0, working_dir_length + 1);
	strcpy(working_dir, passwd_entry->pw_dir);
	strcat(working_dir, APPLICATION_WORKING_DIR_SUFFIX);

	return working_dir;
}
