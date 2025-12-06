/**************************************************************************/
/*  span_to_string.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                RUZTA                                   */
/*                    https://seremtitus.co.ke/ruzta                      */
/**************************************************************************/
/* Copyright (c) 2025-present Ruzta contributors (see AUTHORS.md).  */
/* Copyright (c) 2014-present Godot Engine contributors                   */
/*                                             (see OG_AUTHORS.md).       */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/core/print_string.hpp>
#include <godot_cpp/variant/variant.hpp>
#include "span.h" // original: span.h

using namespace godot;

void print_unicode_error(const String &p_message, bool p_critical) {
	if (p_critical) {
		print_error(vformat(U"Unicode parsing error, some characters were replaced with � (U+FFFD): %s", p_message));
	} else {
		print_error(vformat("Unicode parsing error: %s", p_message));
	}
}

static constexpr char32_t _replacement_char = 0xfffd;

Error span_to_string(String &s, const Span<char32_t> &p_cstr) {
	if (p_cstr.is_empty()) {
		return OK;
	}

	Error error = OK;

	const int prev_length = s.length();
	s.resize(prev_length + p_cstr.size() + 1);
	const char32_t *src = p_cstr.ptr();
	const char32_t *end = p_cstr.ptr() + p_cstr.size();
	char32_t *dst = s.ptrw() + prev_length;

	// Copy the string, and check for UTF-32 problems.
	for (; src < end; ++src, ++dst) {
		const char32_t chr = *src;
		if (unlikely(chr == U'\0')) {
			// NUL in string is allowed by the unicode standard, but unsupported in our implementation right now.
			print_unicode_error("Unexpected NUL character", true);
			*dst = _replacement_char;
			error = ERR_PARSE_ERROR;
		} else if (unlikely((chr & 0xfffff800) == 0xd800)) {
			print_unicode_error(vformat("Unpaired surrogate (%x)", (uint32_t)chr), true);
			*dst = _replacement_char;
			error = ERR_PARSE_ERROR;
		} else if (unlikely(chr > 0x10ffff)) {
			print_unicode_error(vformat("Invalid unicode codepoint (%x)", (uint32_t)chr), true);
			*dst = _replacement_char;
			error = ERR_PARSE_ERROR;
		} else {
			*dst = chr;
		}
	}
	*dst = 0;
	return error;
}
