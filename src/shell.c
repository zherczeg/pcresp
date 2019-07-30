/*
 *    Stream processing tool
 *
 *    Copyright 2016 Zoltan Herczeg (hzmester@freemail.hu). All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *      conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *      of conditions and the following disclaimer in the documentation and/or other materials
 *      provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pcresp.h"

static const char *do_parse_shell(const char *shell_arg, char **msg)
{
	const int max_args = 1000;
	const char *src = shell_arg;
	size_t args_len = 0;
	size_t str_list_len = 0;
	int in_group = 0;
	int arg0_found = 0;
	char **args;
	char *dst;

	while (IS_SPACE(*src)) {
		src++;
	}

	shell_arg = src;

	if (*src == '\0') {
		return NULL;
	}

	do {
		args_len++;
		if (args_len > max_args) {
			break;
		}

		if (*src == '?' && (src[1] == '\0' || IS_SPACE(src[1]))) {
			if (arg0_found) {
				*msg = "maximum one ? parameter is allowed";
				return src;
			}
			arg0_found = 1;

			src++;
			while (IS_SPACE(*src)) {
				src++;
			}
			continue;
		}

		str_list_len++;
		in_group = 0;

		if (*src == '<') {
			in_group = 1;
			src++;
		}

		while (1) {
			if (*src == '\0') {
				if (in_group) {
					*msg = "missing > at the end";
					return src;
				}
				break;
			}

			if ((!in_group && IS_SPACE(*src)) || (in_group && *src == '>')) {
				src++;
				while (IS_SPACE(*src)) {
					src++;
				}
				break;
			}

			if (*src == '#') {
				src++;
				if (*src != '#' && *src != '<' && *src != '>') {
					*msg = "invalid # (hash mark) sequence";
					return src;
				}
			}
			str_list_len++;
			src++;
		}
	} while (*src != '\0');

	if (!arg0_found) {
		args_len++;
	}

	if (args_len > max_args) {
		*msg = "maximum 1000 arguments are allowed";
		return src;
	}

	shell = (char**)malloc(sizeof(char*) * args_len + str_list_len);
	shell_args = args_len;

	args = shell;
	dst = (char*)(shell + args_len);

	src = shell_arg;

	do {
		if (*src == '?' && (src[1] == '\0' || IS_SPACE(src[1]))) {
			shell_arg0_index = args - shell;
			*args++ = NULL;

			src++;
			while (IS_SPACE(*src)) {
				src++;
			}
			continue;
		}

		*args++ = dst;
		in_group = 0;

		if (*src == '<') {
			in_group = 1;
			src++;
		}

		while (1) {
			if (*src == '\0') {
				break;
			}

			if ((!in_group && IS_SPACE(*src)) || (in_group && *src == '>')) {
				src++;
				while (IS_SPACE(*src)) {
					src++;
				}
				break;
			}

			if (*src == '#') {
				src++;
			}
			*dst++ = *src++;
		}

		*dst++ = '\0';
	} while (*src != '\0');

	if (!arg0_found) {
		shell_arg0_index = args - shell;
		*args++ = NULL;
	}

	return NULL;
}

int parse_shell(const char *shell_arg)
{
	char *err_msg = NULL;
	const char *err_pos;
	size_t err_offs;

	if (shell) {
		free(shell);
		shell = NULL;
		shell_args = 0;
		shell_arg0_index = 0;
	}

	err_pos = do_parse_shell(shell_arg, &err_msg);

	if (err_pos != NULL) {
		err_offs = err_pos - shell_arg;

		fprintf(stderr, "Cannot compile: ");
		fwrite(shell_arg, 1, err_offs, stderr);
		fprintf(stderr, "<< SYNTAX ERROR HERE >>");
		fwrite(err_pos, 1, strlen(shell_arg) - err_offs, stderr);
		fprintf(stderr, "\n    Error at offset %d : %s\n", (int)err_offs, err_msg);
		return 0;
	}
	return 1;
}
