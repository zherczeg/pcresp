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

#define DATA_PAGE_SIZE 4096

typedef struct data_page {
	struct data_page *next;
	uint8_t data[DATA_PAGE_SIZE];
} data_page;

void free_pages(data_page *page)
{
	while (page != NULL) {
		data_page *current = page;
		page = page->next;
		free(current);
	}
}

static void load_and_match(FILE* f, char* file_name)
{
	/* Reads the file into a single buffer */
	size_t size = 0, offset = DATA_PAGE_SIZE;
	data_page *first = NULL, *last = NULL;
	char *full_buffer;

	while (1) {
		if (offset >= DATA_PAGE_SIZE) {
			data_page *new_page = (data_page*)malloc(sizeof(data_page));
			if (new_page == NULL) {
				fprintf(stderr, "Cannot allocate memory\n");
				free_pages(first);
				return;
			}

			new_page->next = NULL;

			if (last == NULL) {
				first = new_page;
			}
			else {
				last->next = new_page;
			}

			last = new_page;
			offset = 0;
		}

		size_t bytes = fread(last->data + offset, 1, DATA_PAGE_SIZE - offset, f);

		offset += bytes;
		size += bytes;

		if (ferror(f)) {
			fprintf(stderr, "Read error when processing '%s'\n", file_name);
			free_pages(first);
			return;
		}

		if (feof(f)) {
			break;
		}
	}

	if (size == 0) {
		match("", 0);
		free_pages(first);
		return;
	}

	full_buffer = malloc(size);

	if (full_buffer != NULL) {
		char *dst = full_buffer;
		data_page *current = first;

		while (current != last) {
			memcpy(dst, current->data, DATA_PAGE_SIZE);
			dst += DATA_PAGE_SIZE;
			current = current->next;
		}

		if (offset > 0) {
			memcpy(dst, last->data, offset);
		}

		match(full_buffer, size);

		free(full_buffer);
	}
	else {
		fprintf(stderr, "Cannot allocate memory\n");
	}

	free_pages(first);
}

void match_file(char* file_name)
{
	FILE *f = fopen(file_name, "r");

	if (f == NULL) {
		fprintf(stderr, "Cannot open file: %s\n", file_name);
		return;
	}

	load_and_match(f, file_name);

	fclose(f);
}

void match_stdin()
{
	load_and_match(stdin, "stdin");
}
