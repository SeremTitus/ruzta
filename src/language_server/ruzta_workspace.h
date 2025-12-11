/**************************************************************************/
/*  ruzta_workspace.h                                                  */
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

#pragma once

#include "../ruzta_parser.h"
#include "ruzta_extend_parser.h"
#include "godot_lsp.h" // original: godot_lsp.h

#include <godot_cpp/variant/variant.hpp> // original: core/variant/variant.h
#include <godot_cpp/classes/editor_file_system.hpp> // original: editor/file_system/editor_file_system.h

class RuztaWorkspace : public RefCounted {
	GDCLASS(RuztaWorkspace, RefCounted);

private:
	void _get_owners(EditorFileSystemDirectory *efsd, String p_path, List<String> &owners);
	Node *_get_owner_scene_node(String p_path);

protected:
	static void _bind_methods();
	void remove_cache_parser(const String &p_path);
	bool initialized = false;
	HashMap<StringName, LSP::DocumentSymbol> native_symbols;

	// Absolute paths that are known to point to res://
	HashSet<String> absolute_res_paths;

	const LSP::DocumentSymbol *get_native_symbol(const String &p_class, const String &p_member = "") const;
	const LSP::DocumentSymbol *get_script_symbol(const String &p_path) const;
	const LSP::DocumentSymbol *get_parameter_symbol(const LSP::DocumentSymbol *p_parent, const String &symbol_identifier);
	const LSP::DocumentSymbol *get_local_symbol_at(const ExtendRuztaParser *p_parser, const String &p_symbol_identifier, const LSP::Position p_position);

	void reload_all_workspace_scripts();

	ExtendRuztaParser *get_parse_successed_script(const String &p_path);
	ExtendRuztaParser *get_parse_result(const String &p_path);

	void list_script_files(const String &p_root_dir, List<String> &r_files);

	void apply_new_signal(Object *obj, String function, PackedStringArray args);

public:
	String root;
	String root_uri;

	HashMap<String, ExtendRuztaParser *> scripts;
	HashMap<String, ExtendRuztaParser *> parse_results;
	HashMap<StringName, ClassMembers> native_members;

public:
	Error initialize();

	Error parse_script(const String &p_path, const String &p_content);
	Error parse_local_script(const String &p_path);

	String get_file_path(const String &p_uri);
	String get_file_uri(const String &p_path) const;

	void publish_diagnostics(const String &p_path);
	void completion(const LSP::CompletionParams &p_params, List<RuztaLanguage::CodeCompletionOption> *r_options);

	const LSP::DocumentSymbol *resolve_symbol(const LSP::TextDocumentPositionParams &p_doc_pos, const String &p_symbol_name = "", bool p_func_required = false);
	void resolve_related_symbols(const LSP::TextDocumentPositionParams &p_doc_pos, List<const LSP::DocumentSymbol *> &r_list);
	const LSP::DocumentSymbol *resolve_native_symbol(const LSP::NativeSymbolInspectParams &p_params);
	void resolve_document_links(const String &p_uri, List<LSP::DocumentLink> &r_list);
	Dictionary generate_script_api(const String &p_path);
	Error resolve_signature(const LSP::TextDocumentPositionParams &p_doc_pos, LSP::SignatureHelp &r_signature);
	void didDeleteFiles(const Dictionary &p_params);
	Dictionary rename(const LSP::TextDocumentPositionParams &p_doc_pos, const String &new_name);
	bool can_rename(const LSP::TextDocumentPositionParams &p_doc_pos, LSP::DocumentSymbol &r_symbol, LSP::Range &r_range);
	Vector<LSP::Location> find_usages_in_file(const LSP::DocumentSymbol &p_symbol, const String &p_file_path);
	Vector<LSP::Location> find_all_usages(const LSP::DocumentSymbol &p_symbol);

	RuztaWorkspace();
	~RuztaWorkspace();
};
