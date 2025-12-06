/**************************************************************************/
/*  ruzta_editor_plugin.h                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                RUZTA                                   */
/*                    https://seremtitus.co.ke/ruzta                      */
/**************************************************************************/
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

#include "ruzta_tokenizer_buffer.h"
#include "editor/ruzta_highlighter.h"
#include "editor/ruzta_translation_parser_plugin.h"

#include <godot_cpp/classes/editor_export_plugin.hpp> // original:
#include <godot_cpp/classes/editor_export_preset.hpp> // original:
#include <godot_cpp/classes/editor_plugin.hpp> // original:
#include <godot_cpp/classes/editor_settings.hpp> // original:
#include <godot_cpp/classes/editor_interface.hpp> // original:

using namespace godot;

class RuztaEditorPlugin : public EditorPlugin {
	GDCLASS(RuztaEditorPlugin, EditorPlugin);
	
	static RuztaEditorPlugin *singleton;

	Ref<EditorExportRuzta> rz_export;
	Ref<RuztaEditorTranslationParserPlugin> ruzta_translation_parser_plugin;
	Ref<RuztaSyntaxHighlighter> ruzta_syntax_highlighter;
public:
	static Ref<EditorSettings> get_editor_settings() {
		if (!singleton) {
			return nullptr;
		}
		return singleton->get_editor_interface()->get_editor_settings();
	}
    RuztaEditorPlugin();
    ~RuztaEditorPlugin();
};

class EditorExportRuzta : public EditorExportPlugin {
	GDCLASS(EditorExportRuzta, EditorExportPlugin);

	static constexpr int DEFAULT_SCRIPT_MODE = EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED;
	int script_mode = DEFAULT_SCRIPT_MODE;

protected:
	virtual void _export_begin(const PackedStringArray &p_features, bool p_is_debug, const String &p_path, uint32_t p_flags) override;
	virtual void _export_file(const String &p_path, const String &p_type, const PackedStringArray &p_features) override;
public:
	virtual String _get_name() const override { return "Ruzta"; }
};

void register_plugin_types();