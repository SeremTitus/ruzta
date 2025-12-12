/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                RUZTA                                   */
/*                    https://seremtitus.co.ke/ruzta                      */
/**************************************************************************/
//* Copyright (c) 2025-present Ruzta contributors (see AUTHORS.md).        */
/* Copyright (c) 2014-present Godot Engine contributors                   */
/*                                             (see OG_AUTHORS.md). */
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
#ifdef TOOLS_ENABLED
#include "ruzta_editor_plugin.h"
#endif
#include "ruzta_parser.h"
#include "ruzta_utility_functions.h"
#include "ruzta_variant/ruzta_variant_extension.h"
#include "ruzta_variant/core_constants.h" // Original:
#include <godot_cpp/classes/engine.hpp>	 // original:

#ifdef TOOLS_ENABLED
#ifndef RUZTA_NO_LSP
#include "language_server/ruzta_language_server.h"
#endif
#include "tests/test_ruzta.h"
#endif	// TOOLS_ENABLED

#include <godot_cpp/classes/file_access.hpp>	  // original: core/io/file_access.h
#include <godot_cpp/classes/resource_loader.hpp>  // original: core/io/resource_loader.h

#ifdef TOOLS_ENABLED
// TODO: #include "editor/editor_node.h" // original: editor/editor_node.h

#ifndef RUZTA_NO_LSP
#include <godot_cpp/classes/engine.hpp>	 // original: core/config/engine.h
#endif
#endif	// TOOLS_ENABLED

#ifdef TESTS_ENABLED
// TODO: #include "tests/test_macros.h" // original: tests/test_macros.h
#endif

RuztaLanguage* script_language_rz = nullptr;
Ref<ResourceFormatLoaderRuzta> resource_loader_rz;
Ref<ResourceFormatSaverRuzta> resource_saver_rz;
RuztaCache* ruzta_cache = nullptr;

void initialize_ruzta_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		RuztaVariantExtension::register_types();
		CoreConstants::unregister_global_constants();
		RuztaUtilityFunctions::register_functions();
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		GDREGISTER_CLASS(Ruzta);

		script_language_rz = memnew(RuztaLanguage);
		Engine::get_singleton()->register_script_language(script_language_rz);

		resource_loader_rz.instantiate();
		ResourceLoader::get_singleton()->add_resource_format_loader(resource_loader_rz);

		resource_saver_rz.instantiate();
		ResourceSaver::get_singleton()->add_resource_format_saver(resource_saver_rz);

		ruzta_cache = memnew(RuztaCache);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
#ifndef RUZTA_NO_LSP
		register_lsp_types();
		EditorPlugins::add_by_type<RuztaLanguageServer>();
		Engine::get_singleton()->register_singleton("RuztaLanguageProtocol", RuztaLanguageProtocol::get_singleton());
#endif	// !RUZTA_NO_LSP
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		register_plugin_types();
		EditorPlugins::add_by_type<RuztaEditorPlugin>();
	}
#endif	// TOOLS_ENABLED
}

void uninitialize_ruzta_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		RuztaVariantExtension::unregister_types();
		CoreConstants::unregister_global_constants();
		RuztaUtilityFunctions::unregister_functions();
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		Engine::get_singleton()->unregister_script_language(script_language_rz);

		if (ruzta_cache) {
			memdelete(ruzta_cache);
		}

		if (script_language_rz) {
			memdelete(script_language_rz);
		}

		ResourceLoader::get_singleton()->remove_resource_format_loader(resource_loader_rz);
		resource_loader_rz.unref();

		ResourceSaver::get_singleton()->remove_resource_format_saver(resource_saver_rz);
		resource_saver_rz.unref();

		RuztaParser::cleanup();
	}
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

// TODO: Make tests work.

// REGISTER_TEST_COMMAND("ruzta-tokenizer", &test_tokenizer);
// REGISTER_TEST_COMMAND("ruzta-tokenizer-buffer", &test_tokenizer_buffer);
// REGISTER_TEST_COMMAND("ruzta-parser", &test_parser);
// REGISTER_TEST_COMMAND("ruzta-compiler", &test_compiler);
// REGISTER_TEST_COMMAND("ruzta-bytecode", &test_bytecode);
#endif
