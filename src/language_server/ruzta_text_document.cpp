/**************************************************************************/
/*  ruzta_text_document.cpp                                            */
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

#include "ruzta_text_document.h"

#include "../ruzta.h"
#include "ruzta_extend_parser.h"
#include "ruzta_language_protocol.h"

// TODO: #include "editor/script/script_text_editor.h" // original: editor/script/script_text_editor.h
#include <godot_cpp/classes/editor_settings.hpp> // original: editor/settings/editor_settings.h
#include <godot_cpp/classes/display_server.hpp> // original: servers/display/display_server.h

void RuztaTextDocument::_bind_methods() {
	ClassDB::bind_method(D_METHOD("didOpen"), &RuztaTextDocument::didOpen);
	ClassDB::bind_method(D_METHOD("didClose"), &RuztaTextDocument::didClose);
	ClassDB::bind_method(D_METHOD("didChange"), &RuztaTextDocument::didChange);
	ClassDB::bind_method(D_METHOD("willSaveWaitUntil"), &RuztaTextDocument::willSaveWaitUntil);
	ClassDB::bind_method(D_METHOD("didSave"), &RuztaTextDocument::didSave);
	ClassDB::bind_method(D_METHOD("nativeSymbol"), &RuztaTextDocument::nativeSymbol);
	ClassDB::bind_method(D_METHOD("documentSymbol"), &RuztaTextDocument::documentSymbol);
	ClassDB::bind_method(D_METHOD("completion"), &RuztaTextDocument::completion);
	ClassDB::bind_method(D_METHOD("resolve"), &RuztaTextDocument::resolve);
	ClassDB::bind_method(D_METHOD("rename"), &RuztaTextDocument::rename);
	ClassDB::bind_method(D_METHOD("prepareRename"), &RuztaTextDocument::prepareRename);
	ClassDB::bind_method(D_METHOD("references"), &RuztaTextDocument::references);
	ClassDB::bind_method(D_METHOD("foldingRange"), &RuztaTextDocument::foldingRange);
	ClassDB::bind_method(D_METHOD("codeLens"), &RuztaTextDocument::codeLens);
	ClassDB::bind_method(D_METHOD("documentLink"), &RuztaTextDocument::documentLink);
	ClassDB::bind_method(D_METHOD("colorPresentation"), &RuztaTextDocument::colorPresentation);
	ClassDB::bind_method(D_METHOD("hover"), &RuztaTextDocument::hover);
	ClassDB::bind_method(D_METHOD("definition"), &RuztaTextDocument::definition);
	ClassDB::bind_method(D_METHOD("declaration"), &RuztaTextDocument::declaration);
	ClassDB::bind_method(D_METHOD("signatureHelp"), &RuztaTextDocument::signatureHelp);
	ClassDB::bind_method(D_METHOD("show_native_symbol_in_editor"), &RuztaTextDocument::show_native_symbol_in_editor);
}

void RuztaTextDocument::didOpen(const Variant &p_param) {
	LSP::TextDocumentItem doc = load_document_item(p_param);
	sync_script_content(doc.uri, doc.text);
}

void RuztaTextDocument::didClose(const Variant &p_param) {
	// Left empty on purpose. Godot does nothing special on closing a document,
	// but it satisfies LSP clients that require didClose be implemented.
}

void RuztaTextDocument::didChange(const Variant &p_param) {
	LSP::TextDocumentItem doc = load_document_item(p_param);
	Dictionary dict = p_param;
	Array contentChanges = dict["contentChanges"];
	for (int i = 0; i < contentChanges.size(); ++i) {
		LSP::TextDocumentContentChangeEvent evt;
		evt.load(contentChanges[i]);
		doc.text = evt.text;
	}
	sync_script_content(doc.uri, doc.text);
}

void RuztaTextDocument::willSaveWaitUntil(const Variant &p_param) {
	LSP::TextDocumentItem doc = load_document_item(p_param);

	String path = RuztaLanguageProtocol::get_singleton()->get_workspace()->get_file_path(doc.uri);
	Ref<Script> scr = ResourceLoader::get_singleton()->load(path);
	if (scr.is_valid()) {
		ScriptEditor::get_singleton()->clear_docs_from_script(scr);
	}
}

void RuztaTextDocument::didSave(const Variant &p_param) {
	LSP::TextDocumentItem doc = load_document_item(p_param);
	Dictionary dict = p_param;
	String text = dict["text"];

	sync_script_content(doc.uri, text);

	String path = RuztaLanguageProtocol::get_singleton()->get_workspace()->get_file_path(doc.uri);
	Ref<Ruzta> scr = ResourceLoader::get_singleton()->load(path);
	if (scr.is_valid() && (scr->load_source_code(path) == OK)) {
		if (scr->is_tool()) {
			scr->get_language()->reload_tool_script(scr, true);
		} else {
			scr->reload(true);
		}

		scr->update_exports();

		if (!Thread::is_main_thread()) {
			callable_mp(this, &RuztaTextDocument::reload_script).call_deferred(scr);
		} else {
			reload_script(scr);
		}
	}
}

void RuztaTextDocument::reload_script(Ref<Ruzta> p_to_reload_script) {
	ScriptEditor::get_singleton()->reload_scripts(true);
	ScriptEditor::get_singleton()->update_docs_from_script(p_to_reload_script);
	ScriptEditor::get_singleton()->trigger_live_script_reload(p_to_reload_script->get_path());
}

LSP::TextDocumentItem RuztaTextDocument::load_document_item(const Variant &p_param) {
	LSP::TextDocumentItem doc;
	Dictionary params = p_param;
	doc.load(params["textDocument"]);
	return doc;
}

void RuztaTextDocument::notify_client_show_symbol(const LSP::DocumentSymbol *symbol) {
	ERR_FAIL_NULL(symbol);
	RuztaLanguageProtocol::get_singleton()->notify_client("ruzta/show_native_symbol", symbol->to_json(true));
}

void RuztaTextDocument::initialize() {
	if (RuztaLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {
		for (const KeyValue<StringName, ClassMembers> &E : RuztaLanguageProtocol::get_singleton()->get_workspace()->native_members) {
			const ClassMembers &members = E.value;

			for (const KeyValue<String, const LSP::DocumentSymbol *> &F : members) {
				const LSP::DocumentSymbol *symbol = members.get(F.key);
				LSP::CompletionItem item = symbol->make_completion_item();
				item.data = JOIN_SYMBOLS(String(E.key), F.key);
				native_member_completions.push_back(item.to_json());
			}
		}
	}
}

Variant RuztaTextDocument::nativeSymbol(const Dictionary &p_params) {
	Variant ret;

	LSP::NativeSymbolInspectParams params;
	params.load(p_params);

	if (const LSP::DocumentSymbol *symbol = RuztaLanguageProtocol::get_singleton()->get_workspace()->resolve_native_symbol(params)) {
		ret = symbol->to_json(true);
		notify_client_show_symbol(symbol);
	}

	return ret;
}

Array RuztaTextDocument::documentSymbol(const Dictionary &p_params) {
	Dictionary params = p_params["textDocument"];
	String uri = params["uri"];
	String path = RuztaLanguageProtocol::get_singleton()->get_workspace()->get_file_path(uri);
	Array arr;
	if (HashMap<String, ExtendRuztaParser *>::ConstIterator parser = RuztaLanguageProtocol::get_singleton()->get_workspace()->scripts.find(path)) {
		LSP::DocumentSymbol symbol = parser->value->get_symbols();
		arr.push_back(symbol.to_json(true));
	}
	return arr;
}

Array RuztaTextDocument::completion(const Dictionary &p_params) {
	Array arr;

	LSP::CompletionParams params;
	params.load(p_params);
	Dictionary request_data = params.to_json();

	List<RuztaLanguage::CodeCompletionOption> options;
	RuztaLanguageProtocol::get_singleton()->get_workspace()->completion(params, &options);

	if (!options.is_empty()) {
		int i = 0;
		arr.resize(options.size());

		for (const RuztaLanguage::CodeCompletionOption &option : options) {
			LSP::CompletionItem item;
			item.label = option.display;
			item.data = request_data;
			item.insertText = option.insert_text;

			switch (option.kind) {
				case RuztaLanguage::CODE_COMPLETION_KIND_ENUM:
					item.kind = LSP::CompletionItemKind::Enum;
					break;
				case RuztaLanguage::CODE_COMPLETION_KIND_CLASS:
					item.kind = LSP::CompletionItemKind::Class;
					break;
				case RuztaLanguage::CODE_COMPLETION_KIND_MEMBER:
					item.kind = LSP::CompletionItemKind::Property;
					break;
				case RuztaLanguage::CODE_COMPLETION_KIND_FUNCTION:
					item.kind = LSP::CompletionItemKind::Method;
					break;
				case RuztaLanguage::CODE_COMPLETION_KIND_SIGNAL:
					item.kind = LSP::CompletionItemKind::Event;
					break;
				case RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT:
					item.kind = LSP::CompletionItemKind::Constant;
					break;
				case RuztaLanguage::CODE_COMPLETION_KIND_VARIABLE:
					item.kind = LSP::CompletionItemKind::Variable;
					break;
				case RuztaLanguage::CODE_COMPLETION_KIND_FILE_PATH:
					item.kind = LSP::CompletionItemKind::File;
					break;
				case RuztaLanguage::CODE_COMPLETION_KIND_NODE_PATH:
					item.kind = LSP::CompletionItemKind::Snippet;
					break;
				case RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT:
					item.kind = LSP::CompletionItemKind::Text;
					break;
				default: {
				}
			}

			arr[i] = item.to_json();
			i++;
		}
	}
	return arr;
}

Dictionary RuztaTextDocument::rename(const Dictionary &p_params) {
	LSP::TextDocumentPositionParams params;
	params.load(p_params);
	String new_name = p_params["newName"];

	return RuztaLanguageProtocol::get_singleton()->get_workspace()->rename(params, new_name);
}

Variant RuztaTextDocument::prepareRename(const Dictionary &p_params) {
	LSP::TextDocumentPositionParams params;
	params.load(p_params);

	LSP::DocumentSymbol symbol;
	LSP::Range range;
	if (RuztaLanguageProtocol::get_singleton()->get_workspace()->can_rename(params, symbol, range)) {
		return Variant(range.to_json());
	}

	// `null` -> rename not valid at current location.
	return Variant();
}

Array RuztaTextDocument::references(const Dictionary &p_params) {
	Array res;

	LSP::ReferenceParams params;
	params.load(p_params);

	const LSP::DocumentSymbol *symbol = RuztaLanguageProtocol::get_singleton()->get_workspace()->resolve_symbol(params);
	if (symbol) {
		Vector<LSP::Location> usages = RuztaLanguageProtocol::get_singleton()->get_workspace()->find_all_usages(*symbol);
		res.resize(usages.size());
		int declaration_adjustment = 0;
		for (int i = 0; i < usages.size(); i++) {
			LSP::Location usage = usages[i];
			if (!params.context.includeDeclaration && usage.range == symbol->range) {
				declaration_adjustment++;
				continue;
			}
			res[i - declaration_adjustment] = usages[i].to_json();
		}

		if (declaration_adjustment > 0) {
			res.resize(res.size() - declaration_adjustment);
		}
	}

	return res;
}

Dictionary RuztaTextDocument::resolve(const Dictionary &p_params) {
	LSP::CompletionItem item;
	item.load(p_params);

	LSP::CompletionParams params;
	Variant data = p_params["data"];

	const LSP::DocumentSymbol *symbol = nullptr;

	if (data.get_type() == Variant::DICTIONARY) {
		params.load(p_params["data"]);
		symbol = RuztaLanguageProtocol::get_singleton()->get_workspace()->resolve_symbol(params, item.label, item.kind == LSP::CompletionItemKind::Method || item.kind == LSP::CompletionItemKind::Function);

	} else if (data.is_string()) {
		String query = data;

		Vector<String> param_symbols = query.split(SYMBOL_SEPARATOR, false);

		if (param_symbols.size() >= 2) {
			StringName class_name = param_symbols[0];
			const String &member_name = param_symbols[param_symbols.size() - 1];
			String inner_class_name;
			if (param_symbols.size() >= 3) {
				inner_class_name = param_symbols[1];
			}

			if (const ClassMembers *members = RuztaLanguageProtocol::get_singleton()->get_workspace()->native_members.getptr(class_name)) {
				if (const LSP::DocumentSymbol *const *member = members->getptr(member_name)) {
					symbol = *member;
				}
			}

			if (!symbol) {
				if (HashMap<String, ExtendRuztaParser *>::ConstIterator E = RuztaLanguageProtocol::get_singleton()->get_workspace()->scripts.find(class_name)) {
					symbol = E->value->get_member_symbol(member_name, inner_class_name);
				}
			}
		}
	}

	if (symbol) {
		item.documentation = symbol->render();
	}

	if (item.kind == LSP::CompletionItemKind::Event) {
		if (params.context.triggerKind == LSP::CompletionTriggerKind::TriggerCharacter && (params.context.triggerCharacter == "(")) {
			const String quote_style = RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/completion/use_single_quotes") ? "'" : "\"";
			item.insertText = item.label.quote(quote_style);
		}
	}

	if (item.kind == LSP::CompletionItemKind::Method) {
		bool is_trigger_character = params.context.triggerKind == LSP::CompletionTriggerKind::TriggerCharacter;
		bool is_quote_character = params.context.triggerCharacter == "\"" || params.context.triggerCharacter == "'";

		if (is_trigger_character && is_quote_character && item.insertText.is_quoted()) {
			item.insertText = item.insertText.unquote();
		}
	}

	return item.to_json(true);
}

Array RuztaTextDocument::foldingRange(const Dictionary &p_params) {
	return Array();
}

Array RuztaTextDocument::codeLens(const Dictionary &p_params) {
	return Array();
}

Array RuztaTextDocument::documentLink(const Dictionary &p_params) {
	Array ret;

	LSP::DocumentLinkParams params;
	params.load(p_params);

	List<LSP::DocumentLink> links;
	RuztaLanguageProtocol::get_singleton()->get_workspace()->resolve_document_links(params.textDocument.uri, links);
	for (const LSP::DocumentLink &E : links) {
		ret.push_back(E.to_json());
	}
	return ret;
}

Array RuztaTextDocument::colorPresentation(const Dictionary &p_params) {
	return Array();
}

Variant RuztaTextDocument::hover(const Dictionary &p_params) {
	LSP::TextDocumentPositionParams params;
	params.load(p_params);

	const LSP::DocumentSymbol *symbol = RuztaLanguageProtocol::get_singleton()->get_workspace()->resolve_symbol(params);
	if (symbol) {
		LSP::Hover hover;
		hover.contents = symbol->render();
		hover.range.start = params.position;
		hover.range.end = params.position;
		return hover.to_json();

	} else if (RuztaLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {
		Dictionary ret;
		Array contents;
		List<const LSP::DocumentSymbol *> list;
		RuztaLanguageProtocol::get_singleton()->get_workspace()->resolve_related_symbols(params, list);
		for (const LSP::DocumentSymbol *&E : list) {
			if (const LSP::DocumentSymbol *s = E) {
				contents.push_back(s->render().value);
			}
		}
		ret["contents"] = contents;
		return ret;
	}

	return Variant();
}

Array RuztaTextDocument::definition(const Dictionary &p_params) {
	LSP::TextDocumentPositionParams params;
	params.load(p_params);
	List<const LSP::DocumentSymbol *> symbols;
	return find_symbols(params, symbols);
}

Variant RuztaTextDocument::declaration(const Dictionary &p_params) {
	LSP::TextDocumentPositionParams params;
	params.load(p_params);
	List<const LSP::DocumentSymbol *> symbols;
	Array arr = find_symbols(params, symbols);
	if (arr.is_empty() && !symbols.is_empty() && !symbols.front()->get()->native_class.is_empty()) { // Find a native symbol
		const LSP::DocumentSymbol *symbol = symbols.front()->get();
		if (RuztaLanguageProtocol::get_singleton()->is_goto_native_symbols_enabled()) {
			String id;
			switch (symbol->kind) {
				case LSP::SymbolKind::Class:
					id = "class_name:" + symbol->name;
					break;
				case LSP::SymbolKind::Constant:
					id = "class_constant:" + symbol->native_class + ":" + symbol->name;
					break;
				case LSP::SymbolKind::Property:
				case LSP::SymbolKind::Variable:
					id = "class_property:" + symbol->native_class + ":" + symbol->name;
					break;
				case LSP::SymbolKind::Enum:
					id = "class_enum:" + symbol->native_class + ":" + symbol->name;
					break;
				case LSP::SymbolKind::Method:
				case LSP::SymbolKind::Function:
					id = "class_method:" + symbol->native_class + ":" + symbol->name;
					break;
				default:
					id = "class_global:" + symbol->native_class + ":" + symbol->name;
					break;
			}
			callable_mp(this, &RuztaTextDocument::show_native_symbol_in_editor).call_deferred(id);
		} else {
			notify_client_show_symbol(symbol);
		}
	}
	return arr;
}

Variant RuztaTextDocument::signatureHelp(const Dictionary &p_params) {
	Variant ret;

	LSP::TextDocumentPositionParams params;
	params.load(p_params);

	LSP::SignatureHelp s;
	if (OK == RuztaLanguageProtocol::get_singleton()->get_workspace()->resolve_signature(params, s)) {
		ret = s.to_json();
	}

	return ret;
}

RuztaTextDocument::RuztaTextDocument() {
	file_checker = FileAccess::create(FileAccess::ACCESS_RESOURCES);
}

void RuztaTextDocument::sync_script_content(const String &p_path, const String &p_content) {
	String path = RuztaLanguageProtocol::get_singleton()->get_workspace()->get_file_path(p_path);
	RuztaLanguageProtocol::get_singleton()->get_workspace()->parse_script(path, p_content);
}

void RuztaTextDocument::show_native_symbol_in_editor(const String &p_symbol_id) {
	callable_mp(ScriptEditor::get_singleton(), &ScriptEditor::goto_help).call_deferred(p_symbol_id);

	DisplayServer::get_singleton()->window_move_to_foreground();
}

Array RuztaTextDocument::find_symbols(const LSP::TextDocumentPositionParams &p_location, List<const LSP::DocumentSymbol *> &r_list) {
	Array arr;
	const LSP::DocumentSymbol *symbol = RuztaLanguageProtocol::get_singleton()->get_workspace()->resolve_symbol(p_location);
	if (symbol) {
		LSP::Location location;
		location.uri = symbol->uri;
		if (!location.uri.is_empty()) {
			location.range = symbol->selectionRange;
			const String &path = RuztaLanguageProtocol::get_singleton()->get_workspace()->get_file_path(symbol->uri);
			if (file_checker->file_exists(path)) {
				arr.push_back(location.to_json());
			}
		}
		r_list.push_back(symbol);
	} else if (RuztaLanguageProtocol::get_singleton()->is_smart_resolve_enabled()) {
		List<const LSP::DocumentSymbol *> list;
		RuztaLanguageProtocol::get_singleton()->get_workspace()->resolve_related_symbols(p_location, list);
		for (const LSP::DocumentSymbol *&E : list) {
			if (const LSP::DocumentSymbol *s = E) {
				if (!s->uri.is_empty()) {
					LSP::Location location;
					location.uri = s->uri;
					location.range = s->selectionRange;
					arr.push_back(location.to_json());
					r_list.push_back(s);
				}
			}
		}
	}
	return arr;
}
