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

int print_text;
int match_max;
int ext_string_count;
ext_string* ext_string_list;
pcre2_code *re_code;
pcre2_match_context *match_context;
pcre2_match_data *match_data;
uint32_t ovector_size;
char *default_script;
size_t default_script_size;
char *shell;

static void help(const char *name)
{
	const char *ptr = name;

	while (*ptr != '\0') {
		if (*ptr == '\\' || *ptr == '/') {
			name = ptr + 1;
		}
		ptr++;
	}

	printf("Usage %s [options] pcre_pattern [file list]\n\n"
		"  -h, --help\n"
		"          Prints help and exit\n"
		"  -s, --script script\n"
		"          Executing this script after each successul match\n"
		"  -p, --print\n"
		"          Print text between matched strings\n"
		"  -d, --def-string name string\n"
		"          Define a constant string (see %%[name])\n"
		"  -i\n"
		"          Enable caseless matching\n"
		"  -m\n"
		"          Both ^ and $ match newlines\n"
		"  -x\n"
		"          Ignore white space and # comments (extended regex)\n"
		"  -u\n"
		"          Enable UTF-8\n"
		"  --match-max n\n"
		"          Stop after n successful match (0 - unlimited)\n"
		"  --shell shell_name\n"
		"          Specify default shell for scripts:\n"
		"          shell_name -c [first arg] shell_name [followup args]\n"
		"  --newline-lf\n"
		"          Set newline type to LF (linefeed) only (\\n)\n"
		"  --newline-cr\n"
		"          Set newline type to CR (carriage return) only (\\r)\n"
		"  --newline-crlf\n"
		"          Set newline type to CR followed by LF only (\\r\\n)\n"
		"  --newline-anycrlf\n"
		"          Set newline type to any combination of CR and LF\n"
		"  --newline-any\n"
		"          Set newline type to any Unicode newline sequence\n"
		"  --bsr-anycrlf\n"
		"          Set \\R to match any combination of CR and LF\n"
		"  --bsr-unicode\n"
		"          Set \\R to match any Unicode newline sequence\n"
		"\n  Reads from stdin if file list is empty\n"
		"\nPattern format:\n"
		"\n    Patterns must follow PCRE2 regular expression syntax\n"
		"    Scripts can be executed during matching using (?C) callouts\n"
		"\nScript format:\n"
		"\n  path_to_executable argument1 argument2 ...\n"
		"\n  arguments can be enclosed in <> bracket\n"
		"  if script starts with ! the output is discarded\n"
		"\nSpecial character sequences for scripts:\n"
		"  %%idx     - string value of capture block idx (0-65535)\n"
		"  %%{idx}   - string value of capture block idx (0-65535)\n"
		"  %%[name]  - insert constant string by name\n"
		"  %%M       - current MARK value\n"
		"  %%%%       - %% (percent) sign\n"
		"  %%<       - less-than sign character\n"
		"  %%>       - greater-than sign character\n",
		name);
}

static int add_ext_string(const char *name, const char *chars)
{
	static int ext_string_max = 0;
	ext_string *new_ext_string_list;
	const char *cptr = name;

	if (ext_string_count == 65535) {
		fprintf(stderr, "Maximum number of scripts reached\n");
		return 0;
	}

	if (cptr == NULL || *cptr == '\0') {
		fprintf(stderr, "String name cannot be empty\n");
		return 0;
	}

	while (*cptr != '\0') {
		if (*cptr == ']') {
			fprintf(stderr, "The ']' character is not allowed in string name: %s\n", cptr);
			return 0;
		}
		cptr++;
	}

	if (ext_string_count >= ext_string_max) {
		const int growth = 16;

		new_ext_string_list = (ext_string*)malloc((ext_string_max + growth) * sizeof(ext_string));
		if (new_ext_string_list == NULL) {
			fprintf(stderr, "Cannot allocate memory\n");
			return 0;
		}

		if (ext_string_max > 0) {
			memcpy(new_ext_string_list, ext_string_list, ext_string_max * sizeof(ext_string));
			free(ext_string_list);
		}
		ext_string_list = new_ext_string_list;
		ext_string_max += growth;
	}

	ext_string_list[ext_string_count].name = name;
	ext_string_list[ext_string_count].name_length = strlen(name);
	ext_string_list[ext_string_count].chars = chars;
	ext_string_list[ext_string_count].chars_length = strlen(chars);
	ext_string_count++;
	return 1;
}

static int read_int(const char *str, int max)
{
	const char *char_ptr = str;
	int result;

	do {
		if (*char_ptr < '0' || *char_ptr > '9') {
			fprintf(stderr, "'%s' is not a valid decimal number\n", str);
			return -1;
		}
		char_ptr++;
	} while (*char_ptr != '\0');

	char_ptr = str;
	result = 0;
	do {
		result = result * 10 + *char_ptr++ - '0';

		if (result > max) {
			fprintf(stderr, "'%s' must be less than %d\n", str, max);
			return -1;
		}
	} while (*char_ptr != '\0');

	return result;
}

int callout_function(pcre2_callout_block *callout_block, void *data)
{
	return run_script((char*)callout_block->callout_string, callout_block->callout_string_length,
			(const char*)callout_block->subject, callout_block->offset_vector, (char*)callout_block->mark);
}

int enumerate_callback(pcre2_callout_enumerate_block *callout_block, void *data)
{
	if (!check_script((const char*)callout_block->callout_string, callout_block->callout_string_length)) {
		return 1;
	}
	return 0;
}

static int pcresp_main(int argc, char* argv[])
{
	int arg_index, error_code;
	PCRE2_SIZE error_offset;
	pcre2_compile_context *compile_context;
	pcre2_jit_stack *jit_stack;
	uint32_t options = 0;
	int newline = -1;
	int bsr = -1;

	if (argc <= 1) {
		help(argv[0]);
		return 0;
	}

	arg_index = 1;
	while (arg_index < argc) {
		char *arg = argv[arg_index];

		if (arg[0] != '-') {
			break;
		}

		arg_index++;

		if (arg[1] == '-') {
			arg += 2;
			if (strcmp(arg, "help") == 0) {
				help(argv[0]);
				return 0;
			}
			else if (strcmp(arg, "script") == 0) {
				if (arg_index >= argc) {
					fprintf(stderr, "Script required after --script\n");
					return 1;
				}
				default_script = argv[arg_index++];
				default_script_size = (size_t)strlen(default_script);
				continue;
			}
			else if (strcmp(arg, "print") == 0) {
				print_text = 1;
				continue;
			}
			else if (strcmp(arg, "def-string") == 0) {
				if (arg_index + 1>= argc) {
					fprintf(stderr, "Name and string required after --def-string\n");
					return 1;
				}
				if (!add_ext_string(argv[arg_index], argv[arg_index + 1])) {
					return 1;
				}
				arg_index += 2;
				continue;
			}
			else if (strcmp(arg, "match-max") == 0) {
				if (arg_index >= argc) {
					fprintf(stderr, "Number required after --max-match\n");
					return 1;
				}
				match_max = read_int(argv[arg_index++], 100000000);
				if (match_max == -1) {
					return 1;
				}
				continue;
			}
			else if (strcmp(arg, "shell") == 0) {
				if (arg_index >= argc) {
					fprintf(stderr, "String required after --shell\n");
					return 1;
				}
				shell = argv[arg_index++];
				if (*shell == '\0') {
					shell = NULL;
				}
				continue;
			}
			else if (strcmp(arg, "newline-lf") == 0) {
				newline = PCRE2_NEWLINE_LF;
				continue;
			}
			else if (strcmp(arg, "newline-cr") == 0) {
				newline = PCRE2_NEWLINE_CR;
				continue;
			}
			else if (strcmp(arg, "newline-crlf") == 0) {
				newline = PCRE2_NEWLINE_CRLF;
				continue;
			}
			else if (strcmp(arg, "newline-anycrlf") == 0) {
				newline = PCRE2_NEWLINE_ANYCRLF;
				continue;
			}
			else if (strcmp(arg, "newline-any") == 0) {
				newline = PCRE2_NEWLINE_ANY;
				continue;
			}
			else if (strcmp(arg, "bsr-anycrlf") == 0) {
				bsr = PCRE2_BSR_ANYCRLF;
				continue;
			}
			else if (strcmp(arg, "bsr-unicode") == 0) {
				bsr = PCRE2_BSR_UNICODE;
				continue;
			}
			arg -= 2;
		}
		else if (arg[1] != '\0' && arg[2] == '\0') {
			switch (arg[1]) {
			case 'h':
				help(argv[0]);
				return 0;
			case 's':
				if (arg_index >= argc) {
					fprintf(stderr, "Script required after -s\n");
					return 1;
				}
				default_script = argv[arg_index++];
				default_script_size = (size_t)strlen(default_script);
				continue;
			case 'p':
				print_text = 1;
				continue;
			case 'd':
				if (arg_index + 1>= argc) {
					fprintf(stderr, "Name and string required after -d\n");
					return 1;
				}
				if (!add_ext_string(argv[arg_index], argv[arg_index + 1])) {
					return 1;
				}
				arg_index += 2;
				continue;
			case 'i':
				options |= PCRE2_CASELESS;
				continue;
			case 'm':
				options |= PCRE2_MULTILINE;
				continue;
			case 'x':
				options |= PCRE2_EXTENDED;
				continue;
			case 'u':
				options |= PCRE2_UTF;
				continue;
			}
		}

		fprintf(stderr, "Unknown argument: %s\n", arg);
		return 1;
	}

	if (arg_index >= argc) {
		fprintf(stderr, "Pattern required!\n");
		return 1;
	}

	if (default_script != NULL) {
		if (!check_script(default_script, default_script_size)) {
			return 1;
		}
	}

	compile_context = pcre2_compile_context_create(NULL);
	if (!compile_context) {
		fprintf(stderr, "Cannot create context\n");
		return 1;
	}
	if (newline != -1) {
		pcre2_set_newline(compile_context, (uint32_t)newline);
	}
	if (bsr != -1) {
		pcre2_set_bsr(compile_context, (uint32_t)bsr);
	}

	re_code = pcre2_compile((uint8_t*)(argv[arg_index]), PCRE2_ZERO_TERMINATED, options,
				&error_code, &error_offset, compile_context);
	pcre2_compile_context_free(compile_context);

	if (re_code == NULL) {
		char *buffer = (char *)malloc(256);

		if (buffer != NULL) {
			pcre2_get_error_message(error_code, (uint8_t*)buffer, 256);
		}

		fprintf(stderr, "Cannot compile /%s/\n    Error at offset %d : %s\n",
			argv[arg_index], (int)error_offset, buffer != NULL ? buffer : "<no memory for error string>");

		if (buffer != NULL) {
			free(buffer);
		}

		return 1;
	}

	if (pcre2_callout_enumerate(re_code, enumerate_callback, NULL)) {
		return 1;
	}

	arg_index++;

	match_context = pcre2_match_context_create(NULL);
	match_data = pcre2_match_data_create_from_pattern(re_code, NULL);

	if (match_context == NULL || match_data == NULL) {
		if (match_context != NULL) {
			pcre2_match_context_free(match_context);
		}
		if (match_data != NULL) {
			pcre2_match_data_free(match_data);
		}

		fprintf(stderr, "Cannot create match context\n");
		return 1;
	}

	jit_stack = pcre2_jit_stack_create(64 * 1024, 8 * 1024 * 1024, NULL);

	if (jit_stack != NULL) {
		pcre2_jit_stack_assign(match_context, NULL, jit_stack);

		/* Silently ignored if JIT compilation is failed */
		pcre2_jit_compile(re_code, PCRE2_JIT_COMPLETE);
	}

	pcre2_set_callout(match_context, callout_function, NULL);
	ovector_size = pcre2_get_ovector_count(match_data);

	if (arg_index >= argc) {
		match_stdin();
	}
	else {
		while (arg_index < argc) {
			match_file(argv[arg_index]);
			arg_index++;
		}
	}

	if (jit_stack != NULL) {
		pcre2_jit_stack_free(jit_stack);
	}

	pcre2_match_context_free(match_context);
	pcre2_match_data_free(match_data);
	return 0;
}

int main(int argc, char* argv[])
{
	int result = pcresp_main(argc, argv);

	if (re_code != NULL) {
		pcre2_code_free(re_code);
	}
	if (ext_string_list != NULL) {
		free(ext_string_list);
	}
	return result;
}
