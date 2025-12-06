/**************************************************************************/
/*  ruzta_translation.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                RUZTA                                   */
/*                    https://seremtitus.co.ke/ruzta                      */
/**************************************************************************/
//* Copyright (c) 2025-present Ruzta contributors (see AUTHORS.md).       */
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

#include <godot_cpp/variant/variant.hpp>
// TODO: Get translation actually work
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/classes/translation_domain.hpp>
#include "ruzta_version.h"

using namespace godot;


String RTR(const String &p_text, const String &p_context = "") {
	if (TranslationServer::get_singleton()) {
#ifdef TOOLS_ENABLED
		String rtr = TranslationServer::get_singleton()->get_or_add_domain("Ruzta")->translate(p_text, p_context);
		if (!rtr.is_empty() && rtr != p_text) {
			return rtr;
		}
#endif // TOOLS_ENABLED
		return TranslationServer::get_singleton()->translate(p_text, p_context);
	}

	return p_text;
}

String DTR(const String &p_text, const String &p_context = "") {
	// Comes straight from the XML, so remove indentation and any trailing whitespace.
	const String text = p_text.dedent().strip_edges();

	if (TranslationServer::get_singleton()) {
		return String(TranslationServer::get_singleton()->get_or_add_domain("Ruzta")->translate(text, p_context)).replace("$DOCS_URL", RUZTA_VERSION_DOCS_URL);
	}

	return text.replace("$DOCS_URL", RUZTA_VERSION_DOCS_URL);
}
