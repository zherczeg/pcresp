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

#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#define HAS_NULL_FLAG 0x1
#define HAS_NO_SH_FLAG 0x2

static const char *do_check_script(const char *script, size_t script_size, char **msg)
{
	const int max_args = 1000;
	const char *src, *src_end, *flag_start;
	PCRE2_SIZE capture_id;
	size_t args_len, flag_len;
	int in_group = 0;
	int flags = 0;

	if (script == NULL || script_size == 0) {
		return NULL;
	}

	src = script;
	src_end = script + script_size;

	while (src < src_end && IS_SPACE(*src)) {
		src++;
	}

	while (*src == '!') {
		src++;

		flag_start = src;
		while (src < src_end && *src != '\0' && !IS_SPACE(*src)) {
			src++;
		}

		flag_len = src - flag_start;
		if (flag_len == 4 && memcmp (flag_start, "null", 4) == 0) {
			if (flags & HAS_NULL_FLAG) {
				*msg = "duplicated !null flag";
				return flag_start - 1;
			}
			flags |= HAS_NULL_FLAG;
		}
		else if (flag_len == 5 && memcmp (flag_start, "no_sh", 5) == 0) {
			if (flags & HAS_NO_SH_FLAG) {
				*msg = "duplicated !no_sh flag";
				return flag_start - 1;
			}
			flags |= HAS_NO_SH_FLAG;
		}
		else {
			*msg = "unknown flag";
			return flag_start - 1;
		}

		while (src < src_end && IS_SPACE(*src)) {
			src++;
		}
	}

	if (src == src_end) {
		return NULL;
	}

	args_len = 1;

	do {
		args_len++;
		if (args_len > max_args) {
			break;
		}

		in_group = 0;

		if (*src == '<') {
			in_group = 1;
			src++;
		}

		while (1) {
			if (src == src_end) {
				if (in_group) {
					*msg = "missing > at the end";
					return src;
				}
				break;
			}

			if ((!in_group && IS_SPACE(*src)) || (in_group && *src == '>')) {
				src++;
				while (src < src_end && IS_SPACE(*src)) {
					src++;
				}
				break;
			}

			src++;

			if (src[-1] != '%') {
				continue;
			}

			if (src >= src_end) {
				*msg = "a character must be present after % sign";
				return src;
			}

			if (*src >= '0' && *src <= '9') {
				capture_id = 0;
				do {
					capture_id = capture_id * 10 + (*src - '0');
					if (capture_id > 65535) {
						*msg = "a decimal number between 0 and 65535 is required";
						return src;
					}

					src++;
				} while (src < src_end && *src >= '0' && *src <= '9');

				continue;
			}

			if (*src == '{') {
				/* Must be a decimal number in parenthesis, e.g: {5} or {38} */
				src++;
				capture_id = 0;

				while (1) {
					if (src >= src_end || *src < '0' || *src > '9') {
						*msg = "a decimal number between 0 and 65535 is required";
						return src;
					}

					capture_id = capture_id * 10 + (*src - '0');

					if (capture_id > 65535) {
						*msg = "a decimal number between 0 and 65535 is required";
						return src;
					}

					src++;
					if (src < src_end && *src == '}') {
						break;
					}
				}
			}
			else if (*src == '[') {
				src++;

				if (src >= src_end || *src == ']') {
					*msg = "string name cannot be empty";
					return src;
				}

				do {
					src++;
					if (src >= src_end) {
						*msg = "string name is not terminated by ']'";
						return src;
					}
				} while (*src != ']');
			}
			else if (*src != '%' && *src != '<' && *src != '>' && *src != 'M') {
				*msg = "invalid % sign sequence";
				return src;
			}

			src++;
		}
	} while (src < src_end);

	if (args_len > max_args) {
		*msg = "maximum 1000 arguments are allowed";
		return src;
	}
	return 0;
}

int check_script(const char *script, size_t script_size)
{
	char *err_msg = NULL;
	const char *err_pos = do_check_script(script, script_size, &err_msg);
	size_t err_offs;

	if (err_pos != NULL) {
		err_offs = err_pos - script;

		fprintf(stderr, "Cannot compile: ");
		fwrite(script, 1, err_offs, stderr);
		fprintf(stderr, "<< SYNTAX ERROR HERE >>");
		fwrite(err_pos, 1, script_size - err_offs, stderr);
		fprintf(stderr, "\n    Error at offset %d : %s\n", (int)err_offs, err_msg);
		return 0;
	}
	return 1;
}

static size_t get_capture_size(PCRE2_SIZE capture_id, PCRE2_SIZE *ovector, const char *script)
{
	if (capture_id >= ovector_size) {
		return 0;
	}

	if (capture_id == 0 && script != default_script) {
		return 0;
	}

	capture_id *= 2;
	if (ovector[capture_id + 1] <= ovector[capture_id]) {
		return 0;
	}

	return ovector[capture_id + 1] - ovector[capture_id];
}

static ext_string * get_ext_string(const char *name, size_t length)
{
	ext_string *current = ext_string_list;
	ext_string *end = ext_string_list + ext_string_count;

	while (current < end) {
		if (current->name_length == length && memcmp(current->name, name, length) == 0) {
			return current;
		}
		current++;
	}
	return NULL;
}

int run_script(const char *script, size_t script_size, const char *buffer, PCRE2_SIZE *ovector, char *mark)
{
	const char *src, *src_end, *src_start;
	char *str_list_dst;
	size_t args_len, str_list_len, flag_len;
	PCRE2_SIZE capture_id, memcpy_size;
	char **args, **args_dst;
	int result, in_group, flags = 0;
	pid_t pid;

	if (script == NULL || script_size == 0) {
		return 0;
	}

	/* Checking syntax and compute the number of string fragments.
	 * Callout strings are ignored in case of a syntax error. */
	src = script;
	src_end = script + script_size;

	while (src < src_end && IS_SPACE(*src)) {
		src++;
	}

	while (*src == '!') {
		src++;

		src_start = src;
		while (src < src_end && *src != '\0' && !IS_SPACE(*src)) {
			src++;
		}

		flag_len = src - src_start;
		if (flag_len == 4) {
			flags |= HAS_NULL_FLAG;
		}
		else if (flag_len == 5) {
			flags |= HAS_NO_SH_FLAG;
		}

		while (src < src_end && IS_SPACE(*src)) {
			src++;
		}
	}

	if (src == src_end) {
		return 0;
	}

	args_len = 2;
	str_list_len = 1;
	in_group = 0;
	src_start = src;

	if (!(flags & HAS_NO_SH_FLAG)) {
		args_len += shell_args;
	}

	if (*src == '<') {
		in_group = 1;
		src++;
	}

	do {
		if ((!in_group && IS_SPACE(*src)) || (in_group && *src == '>')) {
			args_len++;

			src++;
			while (src < src_end && IS_SPACE(*src)) {
				src++;
			}

			in_group = 0;
			if (src == src_end) {
				args_len--;
			}
			else if (*src == '<') {
				in_group = 1;
				src++;
			}
			continue;
		}
		else if (*src == '%')
		{
			src++;
			capture_id = 0;

			if (*src >= '0' && *src <= '9') {
				do {
					capture_id = capture_id * 10 + (*src - '0');
					src++;
				} while (src < src_end && *src >= '0' && *src <= '9');

				/* Increase ID */
				capture_id++;
			}
			else if (*src == '{')
			{
				src++;

				do
				{
					capture_id = capture_id * 10 + (*src - '0');
					src++;
				} while (*src != '}');

				src++;
				/* Increase ID */
				capture_id++;
			}
			else if (*src == '[')
			{
				const char *name = src + 1;
				ext_string *string;

				do
				{
					src++;
				} while (*src != ']');

				string = get_ext_string(name, src - name);

				if (string != NULL) {
					str_list_len += string->chars_length;
				}

				src++;
				continue;
			}
			else if (*src == 'M') {
				if (mark != NULL) {
					str_list_len += strlen(mark);
				}
				src++;
				continue;
			}

			if (capture_id > 0) {
				str_list_len += get_capture_size(capture_id - 1, ovector, script);
				continue;
			}
		}

		src++;
		str_list_len++;
	} while (src < src_end);

	args = (char**)malloc((args_len * sizeof(char*)) + str_list_len);

	if (args == NULL) {
		return 0;
	}

	args_dst = args;
	str_list_dst = (char*)(args + args_len);

	if (!(flags & HAS_NO_SH_FLAG) && shell_args > 0) {
		memcpy(args_dst, shell, shell_args * sizeof(char*));
		args_dst[shell_arg0_index] = str_list_dst;
		args_dst += shell_args;
	}
	else {
		*args_dst++ = str_list_dst;
	}

	src = src_start;
	in_group = 0;

	if (*src == '<') {
		in_group = 1;
		src++;
	}

	do {
		if ((!in_group && IS_SPACE(*src)) || (in_group && *src == '>')) {
			*str_list_dst++ = '\0';
			*args_dst++ = str_list_dst;

			src++;
			while (src < src_end && IS_SPACE(*src)) {
				src++;
			}

			in_group = 0;
			if (src == src_end) {
				str_list_dst--;
				args_dst--;
			}
			else if (src < src_end && *src == '<') {
				in_group = 1;
				src++;
			}
			continue;
		}
		else if (*src == '%')
		{
			src++;
			capture_id = 0;

			if (*src >= '0' && *src <= '9') {
				do {
					capture_id = capture_id * 10 + (*src - '0');
					src++;
				} while (src < src_end && *src >= '0' && *src <= '9');

				/* Increase ID */
				capture_id++;
			}
			else if (*src == '{')
			{
				/* Must be a decimal number in parenthesis, e.g: (5) or (38) */
				src++;

				do
				{
					capture_id = capture_id * 10 + (*src - '0');
					src++;
				} while (*src != '}');

				src++;
				/* Increase ID */
				capture_id++;
			}
			else if (*src == '[')
			{
				const char *name = src + 1;
				ext_string *string;

				do
				{
					src++;
				} while (*src != ']');

				string = get_ext_string(name, src - name);

				if (string != NULL) {
					memcpy_size = string->chars_length;
					memcpy(str_list_dst, string->chars, memcpy_size);
					str_list_dst += memcpy_size;
				}

				src++;
				continue;
			}
			else if (*src == 'M') {
				if (mark != NULL) {
					memcpy_size = strlen(mark);
					memcpy(str_list_dst, mark, memcpy_size);
					str_list_dst += memcpy_size;
				}
				src++;
				continue;
			}

			if (capture_id > 0) {
				capture_id--;
				memcpy_size = get_capture_size(capture_id, ovector, script);
				if (memcpy_size > 0) {
					memcpy(str_list_dst, buffer + ovector[capture_id * 2], memcpy_size);
					str_list_dst += memcpy_size;
				}
				continue;
			}
		}

		*str_list_dst++ = *src++;
	} while (src < src_end);

	*str_list_dst = '\0';
	*args_dst = NULL;

	fflush(stdout);
	pid = fork();

	if (pid == 0) {
		if (flags & HAS_NULL_FLAG) {
			result = open("/dev/null", O_WRONLY);
			if (result > -1) {
				dup2(result, STDOUT_FILENO);
				close(result);
			}
		}

		(void)execv(args[0], args);
		/* Control gets here if there is an error,
		 * e.g. a non-existent program. */
		exit(1);
	}

	if (pid > 0) {
		(void)waitpid(pid, &result, 0);
	}

	/* Currently negative return values are not supported,
	 * only zero (match continues) or non-zero (match fails). */

	free(args);
	return !!result;
}

void match(char *buffer, size_t size)
{
	int result, match_count = 0;
	PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
	PCRE2_SIZE start_offset = 0;
	uint32_t options = 0;

	while (1) {
		result = pcre2_match(re_code, (uint8_t*)buffer, size,
			start_offset, options, match_data, match_context);

		if (result <= 0) {
			break;
		}

		if (ovector[1] < ovector[0]) {
			ovector[1] = ovector[0];
		}

		if (print_text && ovector[0] > start_offset) {
			fwrite(buffer + start_offset, 1, ovector[0] - start_offset, stdout);
		}

		if (default_script == NULL) {
			if (ovector[1] > ovector[0]) {
				fwrite(buffer + ovector[0], 1, ovector[1] - ovector[0], stdout);
				fputs("\n", stdout);
			}
		}
		else {
			run_script(default_script, default_script_size, buffer,
				ovector, (char*)pcre2_get_mark(match_data));
		}

		if (ovector[1] > start_offset) {
			start_offset = ovector[1];
		}
		else {
			start_offset++;
		}

		match_count++;
		if (match_max > 0 && match_count >= match_max) {
			break;
		}

		/* Check UTF only once. */
		options |= PCRE2_NO_UTF_CHECK;
	}

	if (print_text && size > start_offset) {
		fwrite(buffer + start_offset, 1, size - start_offset, stdout);
	}
}
