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

#ifndef PCRESP_H
#define PCRESP_H

#include "stdio.h"
#include "string.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include "pcre2.h"

#define IS_SPACE(chr) ((chr) == ' ' || (chr) == '\t')

typedef struct ext_string {
	const char *name;
	size_t name_length;
	const char *chars;
	size_t chars_length;
} ext_string;

extern int print_text;
extern int match_max;
extern int ext_string_count;
extern ext_string* ext_string_list;
extern pcre2_code *re_code;
extern pcre2_match_context *match_context;
extern pcre2_match_data *match_data;
extern uint32_t ovector_size;
extern char *default_script;
extern size_t default_script_size;
extern char **shell;
extern int shell_args;
extern int shell_arg0_index;

void match_file(char*);
void match_stdin(void);
void match(char*, size_t);
int check_script(const char *, size_t);
int run_script(const char *, size_t, const char *, PCRE2_SIZE *, char *);
int parse_shell(const char *);

#endif /* PCRESP_H */
