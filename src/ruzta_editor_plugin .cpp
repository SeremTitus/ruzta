/**************************************************************************/
/*  ruzta_editor_plugin.cpp                                               */
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

#include "ruzta_editor_plugin.h"

#include "editor/ruzta_translation_parser_plugin.h"
#include <godot_cpp/classes/file_access.hpp>	  // original:
#include <godot_cpp/classes/editor_interface.hpp> // original:
#include <godot_cpp/classes/script_editor.hpp> // original:
#include <godot_cpp/classes/editor_translation_parser_plugin.hpp> // original:

void EditorExportRuzta::_export_begin(const PackedStringArray &p_features, bool p_is_debug, const String &p_path, uint32_t p_flags)
{
	script_mode = DEFAULT_SCRIPT_MODE;

	const Ref<EditorExportPreset> &preset = get_export_preset();
	if (preset.is_valid())
	{
		script_mode = preset->get_script_export_mode();
	}
}

void EditorExportRuzta::_export_file(const String &p_path, const String &p_type, const PackedStringArray &p_features)
{
	if (p_path.get_extension() != "rz" || script_mode == EditorExportPreset::MODE_SCRIPT_TEXT)
	{
		return;
	}

	PackedByteArray file = FileAccess::get_file_as_bytes(p_path);
	if (file.is_empty())
	{
		return;
	}

	String source = String::utf8(reinterpret_cast<const char *>(file.ptr()), file.size());
	RuztaTokenizerBuffer::CompressMode compress_mode = script_mode == EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED ? RuztaTokenizerBuffer::COMPRESS_ZSTD : RuztaTokenizerBuffer::COMPRESS_NONE;
	Vector<uint8_t> parse = RuztaTokenizerBuffer::parse_code_string(source, compress_mode);
	file = PackedByteArray();
	for (uint8_t byte : parse)
	{
		file.append(byte);
	}
	if (file.is_empty())
	{
		return;
	}

	add_file(p_path.get_basename() + ".rzc", file, true);
}

RuztaEditorPlugin::RuztaEditorPlugin() {
	if (!singleton) {
		singleton = this;
	}
	
	rz_export.instantiate();
	add_export_plugin(rz_export);

	ruzta_translation_parser_plugin.instantiate();
	add_translation_parser_plugin(ruzta_translation_parser_plugin);

	ruzta_syntax_highlighter.instantiate();
	EditorInterface::get_singleton()->get_script_editor()->register_syntax_highlighter(ruzta_syntax_highlighter);
}
RuztaEditorPlugin::~RuztaEditorPlugin() {
	EditorInterface::get_singleton()->get_script_editor()->unregister_syntax_highlighter(ruzta_syntax_highlighter);
	remove_translation_parser_plugin(ruzta_translation_parser_plugin);
	remove_export_plugin(rz_export);
}

void register_plugin_types() {
	GDREGISTER_CLASS(RuztaSyntaxHighlighter);
	GDREGISTER_INTERNAL_CLASS(EditorExportRuzta);
	GDREGISTER_INTERNAL_CLASS(RuztaEditorPlugin);
}