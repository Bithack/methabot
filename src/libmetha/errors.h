/*-
 * errors.h
 * This file is part of libmetha
 *
 * Copyright (c) 2008, Emil Romanus <emil.romanus@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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
 * 
 * http://bithack.se/projects/methabot/
 */

#ifndef _LM_ERRORS__H_
#define _LM_ERRORS__H_

#define M_FAILED M_ERROR

typedef enum {
    M_OK = 0,
    M_OUT_OF_MEM,
    M_COULD_NOT_OPEN,
    M_IO_ERROR,
    M_SOCKET_ERROR,
    M_NO_CRAWLERS,
    M_NO_FILETYPES,
    M_FT_MULTIDEF,
    M_CR_MULTIDEF,
    M_NO_DEFAULT_CRAWLER,
    M_BAD_OPTION,
    M_BAD_VALUE,
    M_UNKNOWN_FILETYPE,
    M_EXTERNAL_ERROR,
    M_TOO_BIG,
    M_SYNTAX_ERROR,
    M_NOT_READY,
    M_UNKNOWN_CRAWLER,
    M_EVENT_ERROR,
    M_UNRESOLVED,
    M_ERROR,
    M_THREAD_ERROR,
} M_CODE;

const char *lm_strerror(M_CODE c);

#define LM_ERROR(m, ...) ((m)->error_cb((m), __VA_ARGS__))
#define LM_WARNING(m, ...) ((m)->warning_cb((m), __VA_ARGS__))

#endif

