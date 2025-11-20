/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
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

#include "register_types.h"

#include "ruzta.h"
#include "ruzta_cache.h"
#include "ruzta_parser.h"
#include "ruzta_tokenizer_buffer.h"
#include "ruzta_utility_functions.h"

#ifdef TOOLS_ENABLED
#include "editor/ruzta_highlighter.h"
#include "editor/ruzta_translation_parser_plugin.h"

#ifndef RUZTA_NO_LSP
#include "language_server/ruzta_language_server.h"
#endif
#endif // TOOLS_ENABLED

#ifdef TESTS_ENABLED
#include "tests/test_ruzta.h"
#endif

#include <godot_cpp/classes/file_access.hpp> // original: core/io/file_access.h
#include <godot_cpp/classes/resource_loader.hpp> // original: core/io/resource_loader.h

#ifdef TOOLS_ENABLED
// TODO: #include "editor/editor_node.h" // original: editor/editor_node.h
// TODO: #include "editor/export/editor_export.h" // original: editor/export/editor_export.h
// TODO: #include "editor/translations/editor_translation_parser.h" // original: editor/translations/editor_translation_parser.h

#ifndef RUZTA_NO_LSP
#include <godot_cpp/classes/engine.hpp> // original: core/config/engine.h
#endif
#endif // TOOLS_ENABLED

#ifdef TESTS_ENABLED
// TODO: #include "tests/test_macros.h" // original: tests/test_macros.h
#endif

RuztaLanguage *script_language_rz = nullptr;
Ref<ResourceFormatLoaderRuzta> resource_loader_rz;
Ref<ResourceFormatSaverRuzta> resource_saver_rz;
RuztaCache *ruzta_cache = nullptr;

#ifdef TOOLS_ENABLED

Ref<RuztaEditorTranslationParserPlugin> ruzta_translation_parser_plugin;

class EditorExportRuzta : public EditorExportPlugin {
	GDCLASS(EditorExportRuzta, EditorExportPlugin);

	static constexpr int DEFAULT_SCRIPT_MODE = EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED;
	int script_mode = DEFAULT_SCRIPT_MODE;

protected:
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override {
		script_mode = DEFAULT_SCRIPT_MODE;

		const Ref<EditorExportPreset> &preset = get_export_preset();
		if (preset.is_valid()) {
			script_mode = preset->get_script_export_mode();
		}
	}

	virtual void _export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) override {
		if (p_path.get_extension() != "rz" || script_mode == EditorExportPreset::MODE_SCRIPT_TEXT) {
			return;
		}

		Vector<uint8_t> file = FileAccess::get_file_as_bytes(p_path);
		if (file.is_empty()) {
			return;
		}

		String source = String::utf8(reinterpret_cast<const char *>(file.ptr()), file.size());
		RuztaTokenizerBuffer::CompressMode compress_mode = script_mode == EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED ? RuztaTokenizerBuffer::COMPRESS_ZSTD : RuztaTokenizerBuffer::COMPRESS_NONE;
		file = RuztaTokenizerBuffer::parse_code_string(source, compress_mode);
		if (file.is_empty()) {
			return;
		}

		add_file(p_path.get_basename() + ".rzc", file, true);
	}

public:
	virtual String get_name() const override { return "Ruzta"; }
};

static void _editor_init() {
	Ref<EditorExportRuzta> rz_export;
	rz_export.instantiate();
	EditorExport::get_singleton()->add_export_plugin(rz_export);

#ifdef TOOLS_ENABLED
	Ref<RuztaSyntaxHighlighter> ruzta_syntax_highlighter;
	ruzta_syntax_highlighter.instantiate();
	ScriptEditor::get_singleton()->register_syntax_highlighter(ruzta_syntax_highlighter);
#endif

#ifndef RUZTA_NO_LSP
	register_lsp_types();
	RuztaLanguageServer *lsp_plugin = memnew(RuztaLanguageServer);
	EditorNode::get_singleton()->add_editor_plugin(lsp_plugin);
	Engine::get_singleton()->add_singleton(Engine::Singleton("RuztaLanguageProtocol", RuztaLanguageProtocol::get_singleton()));
#endif // !RUZTA_NO_LSP
}

#endif // TOOLS_ENABLED

void initialize_ruzta_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		GDREGISTER_CLASS(Ruzta);

		script_language_rz = memnew(RuztaLanguage);
		ScriptServer::register_language(script_language_rz);

		resource_loader_rz.instantiate();
		ResourceLoader::add_resource_format_loader(resource_loader_rz);

		resource_saver_rz.instantiate();
		ResourceSaver::add_resource_format_saver(resource_saver_rz);

		ruzta_cache = memnew(RuztaCache);

		RuztaUtilityFunctions::register_functions();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		EditorNode::add_init_callback(_editor_init);

		ruzta_translation_parser_plugin.instantiate();
		EditorTranslationParser::get_singleton()->add_parser(ruzta_translation_parser_plugin, EditorTranslationParser::STANDARD);
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(RuztaSyntaxHighlighter);
	}
#endif // TOOLS_ENABLED
}

void uninitialize_ruzta_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		ScriptServer::unregister_language(script_language_rz);

		if (ruzta_cache) {
			memdelete(ruzta_cache);
		}

		if (script_language_rz) {
			memdelete(script_language_rz);
		}

		ResourceLoader::remove_resource_format_loader(resource_loader_rz);
		resource_loader_rz.unref();

		ResourceSaver::remove_resource_format_saver(resource_saver_rz);
		resource_saver_rz.unref();

		RuztaParser::cleanup();
		RuztaUtilityFunctions::unregister_functions();
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorTranslationParser::get_singleton()->remove_parser(ruzta_translation_parser_plugin, EditorTranslationParser::STANDARD);
		ruzta_translation_parser_plugin.unref();
	}
#endif // TOOLS_ENABLED
}

#ifdef TESTS_ENABLED
void test_tokenizer() {
	RuztaTests::test(RuztaTests::TestType::TEST_TOKENIZER);
}

void test_tokenizer_buffer() {
	RuztaTests::test(RuztaTests::TestType::TEST_TOKENIZER_BUFFER);
}

void test_parser() {
	RuztaTests::test(RuztaTests::TestType::TEST_PARSER);
}

void test_compiler() {
	RuztaTests::test(RuztaTests::TestType::TEST_COMPILER);
}

void test_bytecode() {
	RuztaTests::test(RuztaTests::TestType::TEST_BYTECODE);
}

REGISTER_TEST_COMMAND("ruzta-tokenizer", &test_tokenizer);
REGISTER_TEST_COMMAND("ruzta-tokenizer-buffer", &test_tokenizer_buffer);
REGISTER_TEST_COMMAND("ruzta-parser", &test_parser);
REGISTER_TEST_COMMAND("ruzta-compiler", &test_compiler);
REGISTER_TEST_COMMAND("ruzta-bytecode", &test_bytecode);
#endif
