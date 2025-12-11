/**************************************************************************/
/*  ruzta_editor.cpp                                                   */
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

#include "ruzta.h"
#include "ruzta_analyzer.h"
#include "ruzta_editor_plugin.h"
#include "ruzta_parser.h"
#include "ruzta_script_server.h"
#include "ruzta_tokenizer.h"
#include "ruzta_utility_functions.h"

#ifdef TOOLS_ENABLED
#include "editor/ruzta_docgen.h"
// TODO: #include "editor/script_templates/templates.gen.h" // original: editor/script_templates/templates.gen.h
#endif

#include <godot_cpp/classes/engine.hpp>			   // original: core/config/engine.h
#include <godot_cpp/classes/expression.hpp>		   // original: core/math/expression.h
#include <godot_cpp/classes/file_access.hpp>	   // original: core/io/file_access.h
#include <godot_cpp/classes/global_constants.hpp>  // original:
#include <godot_cpp/classes/os.hpp>				   // original:

#include "ruzta_variant/core_constants.h"  // original: core/core_constants.h
// TODO: #include "core/variant/container_type_validate.h" // original: core/variant/container_type_validate.h

#ifdef TOOLS_ENABLED
#include <godot_cpp/classes/project_settings.hpp>  // original: core/config/project_settings.h
// TODO: #include "editor/editor_node.h" // original: editor/editor_node.h
// TODO: #include "editor/editor_string_names.h" // original: editor/editor_string_names.h
#include <godot_cpp/classes/editor_file_system.hpp>			   // original: editor/file_system/editor_file_system.h
#include <godot_cpp/classes/editor_file_system_directory.hpp>  // original: editor/file_system/editor_file_system.h
#include <godot_cpp/classes/editor_settings.hpp>			   // original: editor/settings/editor_settings.h
#endif

String quote_string(const String& str, const String& quote_char = "\"") {
	return quote_char + str + quote_char;
}

String unquote_string(const String& str) {
	if (str.length() >= 2) {
		char32_t first = str[0];
		char32_t last = str[str.length() - 1];
		if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
			return str.substr(1, str.length() - 2);
		}
	}
	return str;
}

bool is_string_quoted(const String& str) {
	if (str.length() >= 2) {
		char32_t first = str[0];
		char32_t last = str[str.length() - 1];
		return (first == '"' && last == '"') || (first == '\'' && last == '\'');
	}
	return false;
}

bool string_contains_char(const String& str, char32_t c) {
	return str.find(String(&c)) != -1;
}

bool string_contains_char(const StringName& str, char32_t c) {
	return String(str).find(String(&c)) != -1;
}

// Variant helper functions
String variant_to_string(const Variant& v) {
	return v.stringify();
}

bool variant_is_num(const Variant& v) {
	Variant::Type t = v.get_type();
	return t == Variant::INT || t == Variant::FLOAT;
}

// Type conversion helpers
PackedStringArray vector_to_packed_string_array(const Vector<String>& vec) {
	PackedStringArray arr;
	for (int i = 0; i < vec.size(); i++) {
		arr.append(vec[i]);
	}
	return arr;
}

Vector<String> packed_string_array_to_vector(const PackedStringArray& arr) {
	Vector<String> vec;
	for (int i = 0; i < arr.size(); i++) {
		vec.push_back(arr[i]);
	}
	return vec;
}

TypedArray<int> RuztaLanguage::CodeCompletionOption::get_option_characteristics(const String& p_base) {
	// Return characteristics of the match found by order of importance.
	// Matches will be ranked by a lexicographical order on the vector returned by this function.
	// The lower values indicate better matches and that they should go before in the order of appearance.
	if (!matches_dirty) {
		return charac;
	}
	charac.clear();
	// Ensure base is not empty and at the same time that matches is not empty too.
	if (p_base.length() == 0) {
		matches_dirty = false;
		charac.push_back(location);
		return charac;
	}
	charac.push_back(matches.size());
	charac.push_back((matches[0].first == 0) ? 0 : 1);
	const char32_t* target_char = &p_base[0];
	int bad_case = 0;
	for (const Pair<int, int>& match_segment : matches) {
		const char32_t* string_to_complete_char = &display[match_segment.first];
		for (int j = 0; j < match_segment.second; j++, string_to_complete_char++, target_char++) {
			if (*string_to_complete_char != *target_char) {
				bad_case++;
			}
		}
	}
	charac.push_back(bad_case);
	charac.push_back(location);
	charac.push_back(matches[0].first);
	matches_dirty = false;
	return charac;
}

void RuztaLanguage::CodeCompletionOption::clear_characteristics() {
	charac = TypedArray<int>();
}

TypedArray<int> RuztaLanguage::CodeCompletionOption::get_option_cached_characteristics() const {
	// Only returns the cached value and warns if it was not updated since the last change of matches.
	if (matches_dirty) {
		WARN_PRINT("Characteristics are not up to date.");
	}

	return charac;
}

PackedStringArray RuztaLanguage::_get_comment_delimiters() const {
	static const PackedStringArray delimiters = {"#"};
	return delimiters;
}

PackedStringArray RuztaLanguage::_get_doc_comment_delimiters() const {
	static const PackedStringArray delimiters = {"##"};
	return delimiters;
}

PackedStringArray RuztaLanguage::_get_string_delimiters() const {
	static const PackedStringArray delimiters = {
		"\" \"",
		"' '",
		"\"\"\" \"\"\"",
		"''' '''",
	};
	// NOTE: StringName, NodePath and r-strings are not listed here.
	return delimiters;
}

bool RuztaLanguage::_is_using_templates() {
	return true;
}

Ref<Script> RuztaLanguage::_make_template(const String& p_template, const String& p_class_name, const String& p_base_class_name) const {
	Ref<Ruzta> scr;
	scr.instantiate();

	String processed_template = p_template;

#ifdef TOOLS_ENABLED
	const bool type_hints = RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/completion/add_type_hints");
#else
	const bool type_hints = true;
#endif

	if (!type_hints) {
		processed_template = processed_template.replace(": int", "")
								 .replace(": Shader.Mode", "")
								 .replace(": VisualShader.Type", "")
								 .replace(": float", "")
								 .replace(": String", "")
								 .replace(": Array[String]", "")
								 .replace(": Node", "")
								 .replace(": CharFXTransform", "")
								 .replace(":=", "=")
								 .replace(" -> void", "")
								 .replace(" -> bool", "")
								 .replace(" -> int", "")
								 .replace(" -> PortType", "")
								 .replace(" -> String", "")
								 .replace(" -> Object", "");
	}

	processed_template = processed_template.replace("_BASE_", p_base_class_name)
							 .replace("_CLASS_SNAKE_CASE_", p_class_name.to_snake_case())
							 .replace("_CLASS_", p_class_name.to_pascal_case())
							 .replace("_TS_", _get_indentation());
	scr->set_source_code(processed_template);

	return scr;
}

TypedArray<Dictionary> RuztaLanguage::_get_built_in_templates(const StringName& p_object) const {
	TypedArray<Dictionary> templates;
#ifdef TOOLS_ENABLED
	// TODO
	// for (int i = 0; i < TEMPLATES_ARRAY_SIZE; i++) {
	// for (int i = 0; i < -1; i++) {
	// if (TEMPLATES[i].inherit == p_object) {
	// 	Dictionary template_dict;
	// 	template_dict["inherit"] = TEMPLATES[i].inherit;
	// 		template_dict["name"] = TEMPLATES[i].name;
	// 		template_dict["description"] = TEMPLATES[i].description;
	// 		template_dict["content"] = TEMPLATES[i].content;
	// 		template_dict["id"] = TEMPLATES[i].id;
	// 		template_dict["origin"] = (int)TEMPLATES[i].origin;
	// 		templates.append(template_dict);
	// 	}
	// }
#endif
	return templates;
}

static void get_function_names_recursively(const RuztaParser::ClassNode* p_class, const String& p_prefix, HashMap<int, String>& r_funcs) {
	for (int i = 0; i < p_class->members.size(); i++) {
		if (p_class->members[i].type == RuztaParser::ClassNode::Member::FUNCTION) {
			const RuztaParser::FunctionNode* function = p_class->members[i].function;
			r_funcs[function->start_line] = p_prefix.is_empty() ? String(function->identifier->name) : p_prefix + String(".") + String(function->identifier->name);
		} else if (p_class->members[i].type == RuztaParser::ClassNode::Member::CLASS) {
			String new_prefix = p_class->members[i].m_class->identifier->name;
			get_function_names_recursively(p_class->members[i].m_class, p_prefix.is_empty() ? new_prefix : p_prefix + String(".") + new_prefix, r_funcs);
		}
	}
}

Dictionary RuztaLanguage::_validate(const String& p_script, const String& p_path, bool p_validate_functions, bool p_validate_errors, bool p_validate_warnings, bool p_validate_safe_lines) const {
	Dictionary result;
	RuztaParser parser;
	RuztaAnalyzer analyzer(&parser);

	Error err = parser.parse(p_script, p_path, false);
	if (err == OK) {
		err = analyzer.analyze();
	}

	Array errors_array;
	Array warnings_array;
	Array functions_array;
	Array safe_lines_array;

#ifdef DEBUG_ENABLED
	if (p_validate_warnings) {
		for (const RuztaWarning& E : parser.get_warnings()) {
			const RuztaWarning& warn = E;
			Dictionary w;
			w["start_line"] = warn.start_line;
			w["end_line"] = warn.end_line;
			w["code"] = (int)warn.code;
			w["string_code"] = RuztaWarning::get_name_from_code(warn.code);
			w["message"] = warn.get_message();
			warnings_array.push_back(w);
		}
	}
#endif

	if (err) {
		if (p_validate_errors) {
			for (const RuztaParser::ParserError& pe : parser.get_errors()) {
				Dictionary e;
				e["path"] = p_path;
				e["line"] = pe.line;
				e["column"] = pe.column;
				e["message"] = pe.message;
				errors_array.push_back(e);
			}

			for (KeyValue<String, Ref<RuztaParserRef>> E : parser.get_depended_parsers()) {
				RuztaParser* depended_parser = E.value->get_parser();
				for (const RuztaParser::ParserError& pe : depended_parser->get_errors()) {
					Dictionary e;
					e["path"] = E.key;
					e["line"] = pe.line;
					e["column"] = pe.column;
					e["message"] = pe.message;
					errors_array.push_back(e);
				}
			}
		}
		result["valid"] = false;
	} else {
		if (p_validate_functions) {
			const RuztaParser::ClassNode* cl = parser.get_tree();
			HashMap<int, String> funcs;

			get_function_names_recursively(cl, "", funcs);

			for (const KeyValue<int, String>& E : funcs) {
				functions_array.push_back(E.value + String(":") + itos(E.key));
			}
		}

#ifdef DEBUG_ENABLED
		if (p_validate_safe_lines) {
			const HashSet<int>& unsafe_lines = parser.get_unsafe_lines();
			for (int i = 1; i <= parser.get_last_line_number(); i++) {
				if (!unsafe_lines.has(i)) {
					safe_lines_array.push_back(i);
				}
			}
		}
#endif
		result["valid"] = true;
	}

	result["errors"] = errors_array;
	result["warnings"] = warnings_array;
	result["functions"] = functions_array;
	result["safe_lines"] = safe_lines_array;

	return result;
}

bool RuztaLanguage::_supports_builtin_mode() const {
	return true;
}

bool RuztaLanguage::_supports_documentation() const {
	return true;
}

String RuztaLanguage::_validate_path(const String& p_path) const {
	// Ruzta doesn't have special path validation requirements
	return "";
}

bool RuztaLanguage::_has_named_classes() const {
	return true;
}

bool RuztaLanguage::_can_make_function() const {
	return true;
}

Error RuztaLanguage::_open_in_external_editor(const Ref<Script>& p_script, int32_t p_line, int32_t p_column) {
	return ERR_UNAVAILABLE;	 // Not supported by default
}

bool RuztaLanguage::_overrides_external_editor() {
	return false;
}

ScriptLanguage::ScriptNameCasing RuztaLanguage::_preferred_file_name_casing() const {
	return ScriptLanguage::SCRIPT_NAME_CASING_SNAKE_CASE;
}

void RuztaLanguage::_thread_enter() {
	// No special thread initialization needed
}

void RuztaLanguage::_thread_exit() {
	// No special thread cleanup needed
}

int32_t RuztaLanguage::_find_function(const String& p_function, const String& p_code) const {
	RuztaTokenizerText tokenizer;
	tokenizer.set_source_code(p_code);
	int indent = 0;
	RuztaTokenizer::Token current = tokenizer.scan();
	while (current.type != RuztaTokenizer::Token::TK_EOF && current.type != RuztaTokenizer::Token::ERROR) {
		if (current.type == RuztaTokenizer::Token::INDENT) {
			indent++;
		} else if (current.type == RuztaTokenizer::Token::DEDENT) {
			indent--;
		}
		if (indent == 0 && current.type == RuztaTokenizer::Token::FUNC) {
			current = tokenizer.scan();
			if (current.is_identifier()) {
				String identifier = current.get_identifier();
				if (identifier == p_function) {
					return current.start_line;
				}
			}
		}
		current = tokenizer.scan();
	}
	return -1;
}

Object* RuztaLanguage::_create_script() const {
	return memnew(Ruzta);
}

/* DEBUGGER FUNCTIONS */

thread_local int RuztaLanguage::_debug_parse_err_line = -1;
thread_local String RuztaLanguage::_debug_parse_err_file;
thread_local String RuztaLanguage::_debug_error;

bool RuztaLanguage::debug_break_parse(const String& p_file, int p_line, const String& p_error) {
	// break because of parse error

	if (EngineDebugger::get_singleton()->is_active() && OS::get_singleton()->get_thread_caller_id() == OS::get_singleton()->get_main_thread_id()) {
		_debug_parse_err_line = p_line;
		_debug_parse_err_file = p_file;
		_debug_error = p_error;
		EngineDebugger::get_singleton()->debug(false, true);
		// Because this is thread local, clear the memory afterwards.
		_debug_parse_err_file = String();
		_debug_error = String();
		return true;
	} else {
		return false;
	}
}

bool RuztaLanguage::debug_break(const String& p_error, bool p_allow_continue) {
	if (EngineDebugger::get_singleton()->is_active()) {
		_debug_parse_err_line = -1;
		_debug_parse_err_file = "";
		_debug_error = p_error;
		bool is_error_breakpoint = p_error != "Breakpoint";
		EngineDebugger::get_singleton()->debug(p_allow_continue, is_error_breakpoint);
		// Because this is thread local, clear the memory afterwards.
		_debug_parse_err_file = String();
		_debug_error = String();
		return true;
	} else {
		return false;
	}
}

String RuztaLanguage::_debug_get_error() const {
	return _debug_error;
}

int32_t RuztaLanguage::_debug_get_stack_level_count() const {
	if (_debug_parse_err_line >= 0) {
		return 1;
	}

	return _call_stack_size;
}

int32_t RuztaLanguage::_debug_get_stack_level_line(int32_t p_level) const {
	if (_debug_parse_err_line >= 0) {
		return _debug_parse_err_line;
	}

	ERR_FAIL_INDEX_V(p_level, (int)_call_stack_size, -1);

	return *(_get_stack_level(p_level)->line);
}

String RuztaLanguage::_debug_get_stack_level_function(int32_t p_level) const {
	if (_debug_parse_err_line >= 0) {
		return "";
	}

	ERR_FAIL_INDEX_V(p_level, (int)_call_stack_size, "");
	RuztaFunction* func = _get_stack_level(p_level)->function;
	return func ? String(func->get_name()) : "";
}

String RuztaLanguage::_debug_get_stack_level_source(int32_t p_level) const {
	if (_debug_parse_err_line >= 0) {
		return _debug_parse_err_file;
	}

	ERR_FAIL_INDEX_V(p_level, (int)_call_stack_size, "");
	return _get_stack_level(p_level)->function->get_source();
}

Dictionary RuztaLanguage::_debug_get_stack_level_locals(int32_t p_level, int32_t p_max_subitems, int32_t p_max_depth) {
	Dictionary locals_dict;
	if (_debug_parse_err_line >= 0) {
		return locals_dict;
	}

	ERR_FAIL_INDEX_V(p_level, (int)_call_stack_size, locals_dict);

	CallLevel* cl = _get_stack_level(p_level);
	RuztaFunction* f = cl->function;

	List<Pair<StringName, int>> locals;

	f->debug_get_stack_member_state(*cl->line, &locals);
	for (const Pair<StringName, int>& E : locals) {
		locals_dict[E.first] = cl->stack[E.second];
	}
	return locals_dict;
}

Dictionary RuztaLanguage::_debug_get_stack_level_members(int32_t p_level, int32_t p_max_subitems, int32_t p_max_depth) {
	Dictionary members_dict;
	if (_debug_parse_err_line >= 0) {
		return members_dict;
	}

	ERR_FAIL_INDEX_V(p_level, (int)_call_stack_size, members_dict);

	CallLevel* cl = _get_stack_level(p_level);
	RuztaInstance* instance = cl->instance;

	if (!instance) {
		return members_dict;
	}

	Ref<Ruzta> scr = instance->get_script();
	ERR_FAIL_COND_V(scr.is_null(), members_dict);

	const HashMap<StringName, Ruzta::MemberInfo>& mi = scr->debug_get_member_indices();

	for (const KeyValue<StringName, Ruzta::MemberInfo>& E : mi) {
		members_dict[E.key] = instance->debug_get_member_by_index(E.value.index);
	}
	return members_dict;
}

void* RuztaLanguage::_debug_get_stack_level_instance(int32_t p_level) {
	if (_debug_parse_err_line >= 0) {
		return nullptr;
	}

	ERR_FAIL_INDEX_V(p_level, (int)_call_stack_size, nullptr);

	return _get_stack_level(p_level)->instance;
}

Dictionary RuztaLanguage::_debug_get_globals(int32_t p_max_subitems, int32_t p_max_depth) {
	Dictionary globals_dict;
	const HashMap<StringName, int>& name_idx = RuztaLanguage::get_singleton()->get_global_map();
	const Variant* gl_array = RuztaLanguage::get_singleton()->get_global_array();

	// We can't easily get public constants as a List<Pair> anymore since _get_public_constants returns a Dictionary.
	// But we can just check the dictionary.
	Dictionary constants = _get_public_constants();

	for (const KeyValue<StringName, int>& E : name_idx) {
		if (ClassDB::class_exists(E.key) || Engine::get_singleton()->has_singleton(E.key)) {
			continue;
		}

		if (constants.has(E.key)) {
			continue;
		}

		const Variant& var = gl_array[E.value];
		const Object* obj = var.get_validated_object();
		if (obj && Object::cast_to<RuztaNativeClass>(obj)) {
			continue;
		}

		bool skip = false;
		for (int i = 0; i < CoreConstants::get_global_constant_count(); i++) {
			if (String(E.key) == CoreConstants::get_global_constant_name(i)) {
				skip = true;
				break;
			}
		}
		if (skip) {
			continue;
		}

		globals_dict[E.key] = var;
	}
	return globals_dict;
}

String RuztaLanguage::_debug_parse_stack_level_expression(int32_t p_level, const String& p_expression, int32_t p_max_subitems, int32_t p_max_depth) {
	Dictionary locals_dict = _debug_get_stack_level_locals(p_level, p_max_subitems, p_max_depth);

	PackedStringArray name_vector;
	Array value_array;

	Array keys = locals_dict.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		name_vector.push_back(key);
		value_array.push_back(locals_dict[key]);
	}

	Expression expression;
	if (expression.parse(p_expression, name_vector) == OK) {
		ScriptInstance* instance = static_cast<ScriptInstance*>(_debug_get_stack_level_instance(p_level));
		if (instance) {
			Variant return_val = expression.execute(value_array, instance->get_owner());
			return variant_to_string(return_val);
		}
	}

	return String();
}

PackedStringArray RuztaLanguage::_get_recognized_extensions() const {
	PackedStringArray extensions;
	extensions.push_back("rz");
	return extensions;
}

TypedArray<Dictionary> RuztaLanguage::_get_public_functions() const {
	TypedArray<Dictionary> functions;
	List<StringName> function_list;
	RuztaUtilityFunctions::get_function_list(&function_list);

	for (const StringName& E : function_list) {
		functions.push_back(RuztaUtilityFunctions::get_function_info(E).operator Dictionary());
	}

	// Not really "functions", but show in documentation.
	{
		MethodInfo mi;
		mi.name = "preload";
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "path"));
		mi.return_val = PropertyInfo(Variant::OBJECT, "", PROPERTY_HINT_RESOURCE_TYPE, "Resource");
		functions.push_back(mi.operator Dictionary());
	}
	{
		MethodInfo mi;
		mi.name = "assert";
		mi.return_val.type = Variant::NIL;
		mi.arguments.push_back(PropertyInfo(Variant::BOOL, "condition"));
		mi.arguments.push_back(PropertyInfo(Variant::STRING, "message"));
		mi.default_arguments.push_back(String());
		functions.push_back(mi.operator Dictionary());
	}
	return functions;
}

Dictionary RuztaLanguage::_get_public_constants() const {
	Dictionary constants;
	constants["PI"] = Math_PI;
	constants["TAU"] = Math_TAU;
	constants["INF"] = Math_INF;
	constants["NAN"] = Math_NAN;
	return constants;
}

TypedArray<Dictionary> RuztaLanguage::_get_public_annotations() const {
	TypedArray<Dictionary> annotations_array;
	RuztaParser parser;
	List<MethodInfo> annotations;
	parser.get_annotation_list(&annotations);

	for (const MethodInfo& E : annotations) {
		annotations_array.push_back(E.operator Dictionary());
	}
	return annotations_array;
}

String RuztaLanguage::_make_function(const String& p_class_name, const String& p_function_name, const PackedStringArray& p_function_args) const {
#ifdef TOOLS_ENABLED
	const bool type_hints = RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/completion/add_type_hints");
#else
	const bool type_hints = true;
#endif

	String result = "func " + p_function_name + "(";
	if (p_function_args.size()) {
		for (int i = 0; i < p_function_args.size(); i++) {
			if (i > 0) {
				result += ", ";
			}

			const String name_unstripped = p_function_args[i].get_slicec(':', 0);
			result += name_unstripped.strip_edges();

			if (type_hints) {
				const String type_stripped = p_function_args[i].substr(name_unstripped.length() + 1).strip_edges();
				if (!type_stripped.is_empty()) {
					result += ": " + type_stripped;
				}
			}
		}
	}
	result += String(")") + (type_hints ? " -> void" : "") + ":\n" +
			  _get_indentation() + "pass # Replace with function body.\n";

	return result;
}

//////// COMPLETION //////////

#ifdef TOOLS_ENABLED

#define COMPLETION_RECURSION_LIMIT 200

struct RuztaCompletionIdentifier {
	RuztaParser::DataType type;
	String enumeration;
	Variant value;
	const RuztaParser::ExpressionNode* assigned_expression = nullptr;
};

// LOCATION METHODS
// These methods are used to populate the `CodeCompletionOption::location` integer.
// For these methods, the location is based on the depth in the inheritance chain that the property
// appears. For example, if you are completing code in a class that inherits Node2D, a property found on Node2D
// will have a "better" (lower) location "score" than a property that is found on CanvasItem.
bool ClassDB_has_property(const StringName& p_class, const StringName& p_property, bool p_no_inheritance = false) {
	bool has_property = false;
	godot::TypedArray<Dictionary> property_list = ClassDB::class_get_property_list(p_class, p_no_inheritance);
	for (Dictionary property : property_list) {
		if (property.has("name") && property["name"] == p_property) {
			has_property = true;
		}
	}
	return has_property;
};

static int _get_property_location(const StringName& p_class, const StringName& p_property) {
	if (!ClassDB_has_property(p_class, p_property)) {
		return RuztaLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (!class_test.is_empty() && !ClassDB_has_property(class_test, p_property, true)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | RuztaLanguage::LOCATION_PARENT_MASK;
}

static int _get_property_location(Ref<Script> p_script, const StringName& p_property) {
	// TODO: Script::get_member_line not available in godot-cpp
	// Returning LOCATION_OTHER as fallback
	return RuztaLanguage::LOCATION_OTHER;

	int depth = 0;
	/*
	Ref<Script> scr = p_script;
	while (scr.is_valid()) {
		if (scr->get_member_line(p_property) != -1) {
			return depth | RuztaLanguage::LOCATION_PARENT_MASK;
		}
		depth++;
		scr = scr->get_base_script();
	}
	*/
	return depth + _get_property_location(p_script->get_instance_base_type(), p_property);
}

static int _get_constant_location(const StringName& p_class, const StringName& p_constant) {
	if (!ClassDB::class_has_integer_constant(p_class, p_constant)) {
		return RuztaLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (!class_test.is_empty() && !ClassDB::class_has_integer_constant(class_test, p_constant)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | RuztaLanguage::LOCATION_PARENT_MASK;
}

static int _get_constant_location(Ref<Script> p_script, const StringName& p_constant) {
	// TODO: Script::get_member_line not available in godot-cpp
	return RuztaLanguage::LOCATION_OTHER;

	/*
	int depth = 0;
	Ref<Script> scr = p_script;
	while (scr.is_valid()) {
		if (scr->get_member_line(p_constant) != -1) {
			return depth | RuztaLanguage::LOCATION_PARENT_MASK;
		}
		depth++;
		scr = scr->get_base_script();
	}
	return depth + _get_constant_location(p_script->get_instance_base_type(), p_constant);
	*/
}

static int _get_signal_location(const StringName& p_class, const StringName& p_signal) {
	if (!ClassDB::class_has_signal(p_class, p_signal)) {
		return RuztaLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (!class_test.is_empty() && !ClassDB::class_has_signal(class_test, p_signal)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | RuztaLanguage::LOCATION_PARENT_MASK;
}

static int _get_signal_location(Ref<Script> p_script, const StringName& p_signal) {
	// TODO: Script::get_member_line not available in godot-cpp
	return RuztaLanguage::LOCATION_OTHER;
	/*
	int depth = 0;
	Ref<Script> scr = p_script;
	while (scr.is_valid()) {
		if (scr->get_member_line(p_signal) != -1) {
			return depth | RuztaLanguage::LOCATION_PARENT_MASK;
		}
		depth++;
		scr = scr->get_base_script();
	}

	return depth + _get_signal_location(p_script->get_instance_base_type(), p_signal);
	*/
}

static int _get_method_location(const StringName& p_class, const StringName& p_method) {
	if (!ClassDB::class_has_method(p_class, p_method)) {
		return RuztaLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (!class_test.is_empty() && !ClassDB::class_has_method(class_test, p_method)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | RuztaLanguage::LOCATION_PARENT_MASK;
}

static int _get_enum_constant_location(const StringName& p_class, const StringName& p_enum_constant) {
	if (!ClassDB::class_get_integer_constant_enum(p_class, p_enum_constant)) {
		return RuztaLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (!class_test.is_empty() && !ClassDB::class_get_integer_constant_enum(class_test, p_enum_constant, true)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | RuztaLanguage::LOCATION_PARENT_MASK;
}

static int _get_enum_location(const StringName& p_class, const StringName& p_enum) {
	if (!ClassDB::class_has_enum(p_class, p_enum)) {
		return RuztaLanguage::LOCATION_OTHER;
	}

	int depth = 0;
	StringName class_test = p_class;
	while (!class_test.is_empty() && !ClassDB::class_has_enum(class_test, p_enum)) {
		class_test = ClassDB::get_parent_class(class_test);
		depth++;
	}

	return depth | RuztaLanguage::LOCATION_PARENT_MASK;
}

// END LOCATION METHODS

static String _trim_parent_class(const String& p_class, const String& p_base_class) {
	if (p_base_class.is_empty()) {
		return p_class;
	}
	PackedStringArray names = p_class.split(".", false, 1);
	if (names.size() == 2) {
		const String& first = names[0];
		if (ClassDB::class_exists(p_base_class) && ClassDB::class_exists(first) && ClassDB::is_parent_class(p_base_class, first)) {
			const String& rest = names[1];
			return rest;
		}
	}
	return p_class;
}

static String _get_visual_datatype(const PropertyInfo& p_info, bool p_is_arg, const String& p_base_class = "") {
	String class_name = p_info.class_name;
	bool is_enum = p_info.type == Variant::INT && p_info.usage & PROPERTY_USAGE_CLASS_IS_ENUM;
	// PROPERTY_USAGE_CLASS_IS_BITFIELD: BitField[T] isn't supported (yet?), use plain int.

	if ((p_info.type == Variant::OBJECT || is_enum) && !class_name.is_empty()) {
		if (is_enum && CoreConstants::is_global_enum(p_info.class_name)) {
			return class_name;
		}
		return _trim_parent_class(class_name, p_base_class);
	} else if (p_info.type == Variant::ARRAY && p_info.hint == PROPERTY_HINT_ARRAY_TYPE && !p_info.hint_string.is_empty()) {
		return "Array[" + _trim_parent_class(p_info.hint_string, p_base_class) + "]";
	} else if (p_info.type == Variant::DICTIONARY && p_info.hint == PROPERTY_HINT_DICTIONARY_TYPE && !p_info.hint_string.is_empty()) {
		const String key = p_info.hint_string.get_slicec(';', 0);
		const String value = p_info.hint_string.get_slicec(';', 1);
		return "Dictionary[" + _trim_parent_class(key, p_base_class) + ", " + _trim_parent_class(value, p_base_class) + "]";
	} else if (p_info.type == Variant::NIL) {
		if (p_is_arg || (p_info.usage & PROPERTY_USAGE_NIL_IS_VARIANT)) {
			return "Variant";
		} else {
			return "void";
		}
	}

	return Variant::get_type_name(p_info.type);
}

static String _make_arguments_hint(const MethodInfo& p_info, int p_arg_idx, bool p_is_annotation = false) {
	String arghint;
	if (!p_is_annotation) {
		arghint += _get_visual_datatype(p_info.return_val, false) + " ";
	}
	arghint += p_info.name + String("(");

	int def_args = p_info.arguments.size() - p_info.default_arguments.size();
	int i = 0;
	for (const PropertyInfo& E : p_info.arguments) {
		if (i > 0) {
			arghint += ", ";
		}

		if (i == p_arg_idx) {
			arghint += String::chr(0xFFFF);
		}
		arghint += E.name + String(": ") + _get_visual_datatype(E, true);

		if (i - def_args >= 0) {
			arghint += String(" = ") + variant_to_string(p_info.default_arguments[i - def_args]);
		}

		if (i == p_arg_idx) {
			arghint += String::chr(0xFFFF);
		}

		i++;
	}

	if (p_info.flags & METHOD_FLAG_VARARG) {
		if (p_info.arguments.size() > 0) {
			arghint += ", ";
		}
		if (p_arg_idx >= p_info.arguments.size()) {
			arghint += String::chr(0xFFFF);
		}
		arghint += "...args: Array";  // `MethodInfo` does not support the rest parameter name.
		if (p_arg_idx >= p_info.arguments.size()) {
			arghint += String::chr(0xFFFF);
		}
	}

	arghint += ")";

	return arghint;
}

static String _make_arguments_hint(const RuztaParser::FunctionNode* p_function, int p_arg_idx, bool p_just_args = false) {
	String arghint;

	if (p_just_args) {
		arghint = "(";
	} else {
		if (p_function->get_datatype().builtin_type == Variant::NIL) {
			arghint = "void " + p_function->identifier->name + "(";
		} else {
			arghint = p_function->get_datatype().to_string() + " " + p_function->identifier->name + "(";
		}
	}

	for (int i = 0; i < p_function->parameters.size(); i++) {
		if (i > 0) {
			arghint += ", ";
		}

		if (i == p_arg_idx) {
			arghint += String::chr(0xFFFF);
		}
		const RuztaParser::ParameterNode* par = p_function->parameters[i];
		if (!par->get_datatype().is_hard_type()) {
			arghint += String(par->identifier->name) + String(": Variant");
		} else {
			arghint += String(par->identifier->name) + ": " + par->get_datatype().to_string();
		}

		if (par->initializer) {
			String def_val = "<unknown>";
			switch (par->initializer->type) {
				case RuztaParser::Node::LITERAL: {
					const RuztaParser::LiteralNode* literal = static_cast<const RuztaParser::LiteralNode*>(par->initializer);
					def_val = variant_to_string(literal->value);
				} break;
				case RuztaParser::Node::IDENTIFIER: {
					const RuztaParser::IdentifierNode* id = static_cast<const RuztaParser::IdentifierNode*>(par->initializer);
					def_val = String(id->name);
				} break;
				case RuztaParser::Node::CALL: {
					const RuztaParser::CallNode* call = static_cast<const RuztaParser::CallNode*>(par->initializer);
					if (call->is_constant && call->reduced) {
						def_val = variant_to_string(call->reduced_value);
					} else if (call->get_callee_type() == RuztaParser::Node::IDENTIFIER) {
						def_val = String(call->function_name) + (call->arguments.is_empty() ? "()" : "(...)");
					}
				} break;
				case RuztaParser::Node::ARRAY: {
					const RuztaParser::ArrayNode* arr = static_cast<const RuztaParser::ArrayNode*>(par->initializer);
					if (arr->is_constant && arr->reduced) {
						def_val = variant_to_string(arr->reduced_value);
					} else {
						def_val = arr->elements.is_empty() ? "[]" : "[...]";
					}
				} break;
				case RuztaParser::Node::DICTIONARY: {
					const RuztaParser::DictionaryNode* dict = static_cast<const RuztaParser::DictionaryNode*>(par->initializer);
					if (dict->is_constant && dict->reduced) {
						def_val = variant_to_string(dict->reduced_value);
					} else {
						def_val = dict->elements.is_empty() ? "{}" : "{...}";
					}
				} break;
				case RuztaParser::Node::SUBSCRIPT: {
					const RuztaParser::SubscriptNode* sub = static_cast<const RuztaParser::SubscriptNode*>(par->initializer);
					if (sub->is_attribute && sub->datatype.kind == RuztaParser::DataType::ENUM && !sub->datatype.is_meta_type) {
						def_val = sub->get_datatype().to_string() + "." + sub->attribute->name;
					} else if (sub->is_constant && sub->reduced) {
						def_val = variant_to_string(sub->reduced_value);
					}
				} break;
				default:
					break;
			}
			arghint += " = " + def_val;
		}
		if (i == p_arg_idx) {
			arghint += String::chr(0xFFFF);
		}
	}

	if (p_function->is_vararg()) {
		if (!p_function->parameters.is_empty()) {
			arghint += ", ";
		}
		if (p_arg_idx >= p_function->parameters.size()) {
			arghint += String::chr(0xFFFF);
		}
		const RuztaParser::ParameterNode* rest_param = p_function->rest_parameter;
		arghint += "..." + rest_param->identifier->name + ": " + rest_param->get_datatype().to_string();
		if (p_arg_idx >= p_function->parameters.size()) {
			arghint += String::chr(0xFFFF);
		}
	}

	arghint += ")";

	return arghint;
}

static void _get_directory_contents(EditorFileSystemDirectory* p_dir, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_list, const StringName& p_required_type = StringName()) {
	const String quote_style = RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/completion/use_single_quotes") ? "'" : "\"";
	const bool requires_type = !p_required_type.is_empty();

	for (int i = 0; i < p_dir->get_file_count(); i++) {
		if (requires_type && !ClassDB::is_parent_class(p_dir->get_file_type(i), p_required_type)) {
			continue;
		}
		RuztaLanguage::CodeCompletionOption option(quote_string(p_dir->get_file_path(i), quote_style), RuztaLanguage::CODE_COMPLETION_KIND_FILE_PATH);
		r_list.insert(option.display, option);
	}

	for (int i = 0; i < p_dir->get_subdir_count(); i++) {
		_get_directory_contents(p_dir->get_subdir(i), r_list, p_required_type);
	}
}

static void _find_annotation_arguments(const RuztaParser::AnnotationNode* p_annotation, int p_argument, const String p_quote_style, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result, String& r_arghint) {
	ERR_FAIL_NULL(p_annotation);

	if (p_annotation->info != nullptr) {
		r_arghint = _make_arguments_hint(p_annotation->info->info, p_argument, true);
	}
	if (p_annotation->name == StringName("@export_range")) {
		if (p_argument == 3 || p_argument == 4 || p_argument == 5) {
			// Slider hint.
			RuztaLanguage::CodeCompletionOption slider1("or_greater", RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			slider1.insert_text = quote_string(slider1.display, p_quote_style);
			r_result.insert(slider1.display, slider1);
			RuztaLanguage::CodeCompletionOption slider2("or_less", RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			slider2.insert_text = quote_string(slider2.display, p_quote_style);
			r_result.insert(slider2.display, slider2);
			RuztaLanguage::CodeCompletionOption slider3("prefer_slider", RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			slider3.insert_text = quote_string(slider3.display, p_quote_style);
			r_result.insert(slider3.display, slider3);
			RuztaLanguage::CodeCompletionOption slider4("hide_control", RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			slider4.insert_text = quote_string(slider4.display, p_quote_style);
			r_result.insert(slider4.display, slider4);
		}
	} else if (p_annotation->name == StringName("@export_exp_easing")) {
		if (p_argument == 0 || p_argument == 1) {
			// Easing hint.
			RuztaLanguage::CodeCompletionOption hint1("attenuation", RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			hint1.insert_text = quote_string(hint1.display, p_quote_style);
			r_result.insert(hint1.display, hint1);
			RuztaLanguage::CodeCompletionOption hint2("inout", RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			hint2.insert_text = quote_string(hint2.display, p_quote_style);
			r_result.insert(hint2.display, hint2);
		}
	} else if (p_annotation->name == StringName("@export_node_path")) {
		RuztaLanguage::CodeCompletionOption node("Node", RuztaLanguage::CODE_COMPLETION_KIND_CLASS);
		node.insert_text = quote_string(node.display, p_quote_style);
		r_result.insert(node.display, node);

		// TODO: godot-cpp returns PackedStringArray
		PackedStringArray native_classes = ClassDB::get_inheriters_from_class("Node");
		for (int i = 0; i < native_classes.size(); i++) {
			StringName E = native_classes[i];
			// if (!ClassDB::is_class_exposed(E)) {
			// 	continue;
			// }
			RuztaLanguage::CodeCompletionOption option(E, RuztaLanguage::CODE_COMPLETION_KIND_CLASS);
			option.insert_text = quote_string(option.display, p_quote_style);
			r_result.insert(option.display, option);
		}

#if 0  // TODO: Fix global class list for GDExtension
		LocalVector<StringName> global_script_classes;
		RuztaScriptServer::get_global_class_list(global_script_classes);
		for (const StringName& class_name : global_script_classes) {
			if (!ClassDB::is_parent_class(RuztaScriptServer::get_global_class_native_base(class_name), "Node")) {
				continue;
			}
			RuztaLanguage::CodeCompletionOption option(class_name, RuztaLanguage::CODE_COMPLETION_KIND_CLASS);
			option.insert_text = quote_string(option.display, p_quote_style);
			r_result.insert(option.display, option);
		}
#endif
	} else if (p_annotation->name == StringName("@export_tool_button")) {
		if (p_argument == 1) {
#if 0  // TODO: EditorNode/Theme/EditorIcons not available in godot-cpp
			const Ref<Theme> theme = EditorNode::get_singleton()->get_editor_theme();
			if (theme.is_valid()) {
				List<StringName> icon_list;
				theme->get_icon_list(EditorStringName(EditorIcons), &icon_list);
				for (const StringName& E : icon_list) {
					RuztaLanguage::CodeCompletionOption option(E, RuztaLanguage::CODE_COMPLETION_KIND_CLASS);
					option.insert_text = quote_string(option.display, p_quote_style);
					r_result.insert(option.display, option);
				}
			}
#endif
		}
	} else if (p_annotation->name == StringName("@export_custom")) {
		switch (p_argument) {
			case 0: {
				static HashMap<StringName, int64_t> items;
				if (unlikely(items.is_empty())) {
					CoreConstants::get_enum_values(StringName("PropertyHint"), &items);
				}
				for (const KeyValue<StringName, int64_t>& item : items) {
					RuztaLanguage::CodeCompletionOption option(item.key, RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT);
					r_result.insert(option.display, option);
				}
			} break;
			case 2: {
				static HashMap<StringName, int64_t> items;
				if (unlikely(items.is_empty())) {
					CoreConstants::get_enum_values(StringName("PropertyUsageFlags"), &items);
				}
				for (const KeyValue<StringName, int64_t>& item : items) {
					RuztaLanguage::CodeCompletionOption option(item.key, RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT);
					r_result.insert(option.display, option);
				}
			} break;
		}
	} else if (p_annotation->name == StringName("@warning_ignore") || p_annotation->name == StringName("@warning_ignore_start") || p_annotation->name == StringName("@warning_ignore_restore")) {
		for (int warning_code = 0; warning_code < RuztaWarning::WARNING_MAX; warning_code++) {
#ifndef DISABLE_DEPRECATED
			if (warning_code >= RuztaWarning::FIRST_DEPRECATED_WARNING) {
				break;	// Don't suggest deprecated warnings as they are never produced.
			}
#endif	// DISABLE_DEPRECATED
			RuztaLanguage::CodeCompletionOption warning(RuztaWarning::get_name_from_code((RuztaWarning::Code)warning_code).to_lower(), RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
			warning.insert_text = quote_string(warning.display, p_quote_style);
			r_result.insert(warning.display, warning);
		}
	} else if (p_annotation->name == StringName("@rpc")) {
		if (p_argument == 0 || p_argument == 1 || p_argument == 2) {
			static const char* options[7] = {"call_local", "call_remote", "any_peer", "authority", "reliable", "unreliable", "unreliable_ordered"};
			for (int i = 0; i < 7; i++) {
				RuztaLanguage::CodeCompletionOption option(options[i], RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
				option.insert_text = quote_string(option.display, p_quote_style);
				r_result.insert(option.display, option);
			}
		}
	}
}

static void _find_built_in_variants(HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result, bool exclude_nil = false) {
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		if (!exclude_nil && Variant::Type(i) == Variant::Type::NIL) {
			RuztaLanguage::CodeCompletionOption option("null", RuztaLanguage::CODE_COMPLETION_KIND_CLASS);
			r_result.insert(option.display, option);
		} else {
			RuztaLanguage::CodeCompletionOption option(Variant::get_type_name(Variant::Type(i)), RuztaLanguage::CODE_COMPLETION_KIND_CLASS);
			r_result.insert(option.display, option);
		}
	}
}

static void _find_global_enums(HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result) {
	List<StringName> global_enums;
	CoreConstants::get_global_enums(&global_enums);
	for (const StringName& enum_name : global_enums) {
		RuztaLanguage::CodeCompletionOption option(enum_name, RuztaLanguage::CODE_COMPLETION_KIND_ENUM, RuztaLanguage::LOCATION_OTHER);
		r_result.insert(option.display, option);
	}
}

static void _list_available_types(bool p_inherit_only, RuztaParser::CompletionContext& p_context, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result) {
	// Built-in Variant Types
	_find_built_in_variants(r_result, true);

	PackedStringArray native_types = ClassDB::get_class_list();
	for (const StringName& type : native_types) {
		if (/* ClassDB::is_class_exposed(type) && */ !Engine::get_singleton()->has_singleton(type)) {
			RuztaLanguage::CodeCompletionOption option(type, RuztaLanguage::CODE_COMPLETION_KIND_CLASS);
			r_result.insert(option.display, option);
		}
	}

	// TODO: Unify with _find_identifiers_in_class.
	if (p_context.current_class) {
		if (!p_inherit_only && p_context.current_class->base_type.is_set()) {
			// Native enums from base class
			List<StringName> enums;
			ClassDB::class_get_enum_list(p_context.current_class->base_type.native_type, &enums);
			for (const StringName& E : enums) {
				RuztaLanguage::CodeCompletionOption option(E, RuztaLanguage::CODE_COMPLETION_KIND_ENUM);
				r_result.insert(option.display, option);
			}
		}
		// Check current class for potential types.
		// TODO: Also check classes the current class inherits from.
		const RuztaParser::ClassNode* current = p_context.current_class;
		int location_offset = 0;
		while (current) {
			for (int i = 0; i < current->members.size(); i++) {
				const RuztaParser::ClassNode::Member& member = current->members[i];
				switch (member.type) {
					case RuztaParser::ClassNode::Member::CLASS: {
						RuztaLanguage::CodeCompletionOption option(member.m_class->identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_CLASS, RuztaLanguage::LOCATION_LOCAL + location_offset);
						r_result.insert(option.display, option);
					} break;
					case RuztaParser::ClassNode::Member::ENUM: {
						if (!p_inherit_only) {
							RuztaLanguage::CodeCompletionOption option(member.m_enum->identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_ENUM, RuztaLanguage::LOCATION_LOCAL + location_offset);
							r_result.insert(option.display, option);
						}
					} break;
					case RuztaParser::ClassNode::Member::CONSTANT: {
						if (member.constant->get_datatype().is_meta_type) {
							RuztaLanguage::CodeCompletionOption option(member.constant->identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_CLASS, RuztaLanguage::LOCATION_LOCAL + location_offset);
							r_result.insert(option.display, option);
						}
					} break;
					default:
						break;
				}
			}
			location_offset += 1;
			current = current->outer;
		}
	}

	// Global scripts
	LocalVector<StringName> global_classes;
	RuztaScriptServer::get_global_class_list(global_classes);
	for (const StringName& class_name : global_classes) {
		RuztaLanguage::CodeCompletionOption option(class_name, RuztaLanguage::CODE_COMPLETION_KIND_CLASS, RuztaLanguage::LOCATION_OTHER_USER_CODE);
		r_result.insert(option.display, option);
	}

	// Global enums
	if (!p_inherit_only) {
		_find_global_enums(r_result);
	}

	// Todo: Autoload singletons
	// HashMap<StringName, ProjectSettings::AutoloadInfo> autoloads = ProjectSettings::get_singleton()->get_autoload_list();

	// for (const KeyValue<StringName, ProjectSettings::AutoloadInfo>& E : autoloads) {
	// 	const ProjectSettings::AutoloadInfo& info = E.value;
	// 	if (!info.is_singleton || !info.path.has_extension("rz")) {
	// 		continue;
	// 	}
	// 	RuztaLanguage::CodeCompletionOption option(info.name, RuztaLanguage::CODE_COMPLETION_KIND_CLASS, RuztaLanguage::LOCATION_OTHER_USER_CODE);
	// 	r_result.insert(option.display, option);
	// }
}

static void _find_identifiers_in_suite(const RuztaParser::SuiteNode* p_suite, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result, int p_recursion_depth = 0) {
	for (int i = 0; i < p_suite->locals.size(); i++) {
		RuztaLanguage::CodeCompletionOption option;
		int location = p_recursion_depth == 0 ? RuztaLanguage::LOCATION_LOCAL : (p_recursion_depth | RuztaLanguage::LOCATION_PARENT_MASK);
		if (p_suite->locals[i].type == RuztaParser::SuiteNode::Local::CONSTANT) {
			option = RuztaLanguage::CodeCompletionOption(p_suite->locals[i].name, RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
			option.default_value = p_suite->locals[i].constant->initializer->reduced_value;
		} else {
			option = RuztaLanguage::CodeCompletionOption(p_suite->locals[i].name, RuztaLanguage::CODE_COMPLETION_KIND_VARIABLE, location);
		}
		r_result.insert(option.display, option);
	}
	if (p_suite->parent_block) {
		_find_identifiers_in_suite(p_suite->parent_block, r_result, p_recursion_depth + 1);
	}
}

static void _find_identifiers_in_base(const RuztaCompletionIdentifier& p_base, bool p_only_functions, bool p_types_only, bool p_add_braces, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result, int p_recursion_depth);

static void _find_identifiers_in_class(const RuztaParser::ClassNode* p_class, bool p_only_functions, bool p_types_only, bool p_static, bool p_parent_only, bool p_add_braces, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result, int p_recursion_depth) {
	ERR_FAIL_COND(p_recursion_depth > COMPLETION_RECURSION_LIMIT);

	if (!p_parent_only) {
		bool outer = false;
		const RuztaParser::ClassNode* clss = p_class;
		int classes_processed = 0;
		while (clss) {
			for (int i = 0; i < clss->members.size(); i++) {
				const int location = p_recursion_depth == 0 ? classes_processed : (p_recursion_depth | RuztaLanguage::LOCATION_PARENT_MASK);
				const RuztaParser::ClassNode::Member& member = clss->members[i];
				RuztaLanguage::CodeCompletionOption option;
				switch (member.type) {
					case RuztaParser::ClassNode::Member::VARIABLE:
						if (p_types_only || p_only_functions || outer || (p_static && !member.variable->is_static)) {
							continue;
						}
						option = RuztaLanguage::CodeCompletionOption(member.variable->identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_MEMBER, location);
						break;
					case RuztaParser::ClassNode::Member::CONSTANT:
						if ((p_types_only && !member.constant->datatype.is_meta_type) || p_only_functions) {
							continue;
						}
						if (r_result.has(member.constant->identifier->name)) {
							continue;
						}
						option = RuztaLanguage::CodeCompletionOption(member.constant->identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
						if (member.constant->initializer) {
							option.default_value = member.constant->initializer->reduced_value;
						}
						break;
					case RuztaParser::ClassNode::Member::CLASS:
						if (p_only_functions) {
							continue;
						}
						option = RuztaLanguage::CodeCompletionOption(member.m_class->identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_CLASS, location);
						break;
					case RuztaParser::ClassNode::Member::ENUM_VALUE:
						if (p_types_only || p_only_functions) {
							continue;
						}
						option = RuztaLanguage::CodeCompletionOption(member.enum_value.identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
						break;
					case RuztaParser::ClassNode::Member::ENUM:
						if (p_only_functions) {
							continue;
						}
						option = RuztaLanguage::CodeCompletionOption(member.m_enum->identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_ENUM, location);
						break;
					case RuztaParser::ClassNode::Member::FUNCTION:
						if (p_types_only || outer || (p_static && !member.function->is_static) || String(member.function->identifier->name).begins_with("@")) {
							continue;
						}
						option = RuztaLanguage::CodeCompletionOption(member.function->identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_FUNCTION, location);
						if (p_add_braces) {
							if (member.function->parameters.size() > 0 || (member.function->info.flags & METHOD_FLAG_VARARG)) {
								option.insert_text += "(";
								option.display += U"(\u2026)";
							} else {
								option.insert_text += "()";
								option.display += "()";
							}
						}
						break;
					case RuztaParser::ClassNode::Member::SIGNAL:
						if (p_types_only || p_only_functions || outer || p_static) {
							continue;
						}
						option = RuztaLanguage::CodeCompletionOption(member.signal->identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_SIGNAL, location);
						break;
					case RuztaParser::ClassNode::Member::GROUP:
						break;	// No-op, but silences warnings.
					case RuztaParser::ClassNode::Member::UNDEFINED:
						break;
				}
				r_result.insert(option.display, option);
			}
			if (p_types_only) {
				break;	// Otherwise, it will fill the results with types from the outer class (which is undesired for that case).
			}

			outer = true;
			clss = clss->outer;
			classes_processed++;
		}
	}

	// Parents.
	RuztaCompletionIdentifier base_type;
	base_type.type = p_class->base_type;
	base_type.type.is_meta_type = p_static;

	_find_identifiers_in_base(base_type, p_only_functions, p_types_only, p_add_braces, r_result, p_recursion_depth + 1);
}

static void _find_identifiers_in_base(const RuztaCompletionIdentifier& p_base, bool p_only_functions, bool p_types_only, bool p_add_braces, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result, int p_recursion_depth) {
	ERR_FAIL_COND(p_recursion_depth > COMPLETION_RECURSION_LIMIT);

	RuztaParser::DataType base_type = p_base.type;

	if (!p_types_only && base_type.is_meta_type && base_type.kind != RuztaParser::DataType::BUILTIN && base_type.kind != RuztaParser::DataType::ENUM) {
		RuztaLanguage::CodeCompletionOption option("new", RuztaLanguage::CODE_COMPLETION_KIND_FUNCTION, RuztaLanguage::LOCATION_LOCAL);
		if (p_add_braces) {
			option.insert_text += "(";
			option.display += U"(\u2026)";
		}
		r_result.insert(option.display, option);
	}

	while (!base_type.has_no_type()) {
		switch (base_type.kind) {
			case RuztaParser::DataType::CLASS: {
				_find_identifiers_in_class(base_type.class_type, p_only_functions, p_types_only, base_type.is_meta_type, false, p_add_braces, r_result, p_recursion_depth);
				// This already finds all parent identifiers, so we are done.
				base_type = RuztaParser::DataType();
			} break;
			case RuztaParser::DataType::SCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					if (p_types_only) {
						// TODO: Need to implement Script::get_script_enum_list and retrieve the enum list from a script.
					} else if (!p_only_functions) {
						if (!base_type.is_meta_type) {
							// godot-cpp: get_script_property_list returns TypedArray<Dictionary>
							TypedArray<Dictionary> members_array = scr->get_script_property_list();
							for (int i = 0; i < members_array.size(); i++) {
								PropertyInfo E = PropertyInfo::from_dict(members_array[i]);
								if (E.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_INTERNAL)) {
									continue;
								}
								if (string_contains_char(String(E.name), '/')) {
									continue;
								}
								int location = p_recursion_depth + _get_property_location(scr, E.name);
								RuztaLanguage::CodeCompletionOption option(E.name, RuztaLanguage::CODE_COMPLETION_KIND_MEMBER, location);
								r_result.insert(option.display, option);
							}

							// godot-cpp: get_script_signal_list returns TypedArray<Dictionary>
							TypedArray<Dictionary> signals_array = scr->get_script_signal_list();
							for (int i = 0; i < signals_array.size(); i++) {
								MethodInfo E = MethodInfo::from_dict(signals_array[i]);
								if (E.name.begins_with("_")) {
									continue;
								}
								int location = p_recursion_depth + _get_signal_location(scr, E.name);
								RuztaLanguage::CodeCompletionOption option(E.name, RuztaLanguage::CODE_COMPLETION_KIND_SIGNAL, location);
								r_result.insert(option.display, option);
							}
						}
						// TODO: Script::get_constants not available in godot-cpp
#if 0
						HashMap<StringName, Variant> constants;
						scr->get_constants(&constants);
						for (const KeyValue<StringName, Variant>& E : constants) {
							int location = p_recursion_depth + _get_constant_location(scr, E.key);
							RuztaLanguage::CodeCompletionOption option(String(E.key), RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
							r_result.insert(option.display, option);
						}
#endif
					}

					if (!p_types_only) {
						godot::TypedArray<Dictionary> methods = scr->get_script_method_list();
						for (const Dictionary& info : methods) {
							MethodInfo E = MethodInfo::from_dict(info);
							if (E.name.begins_with("@")) {
								continue;
							}
							int location = p_recursion_depth + _get_method_location(scr->get_class(), E.name);
							RuztaLanguage::CodeCompletionOption option(E.name, RuztaLanguage::CODE_COMPLETION_KIND_FUNCTION, location);
							if (p_add_braces) {
								if (E.arguments.size() || (E.flags & METHOD_FLAG_VARARG)) {
									option.insert_text += "(";
									option.display += U"(\u2026)";
								} else {
									option.insert_text += "()";
									option.display += "()";
								}
							}
							r_result.insert(option.display, option);
						}
					}

					Ref<Script> base_script = scr->get_base_script();
					if (base_script.is_valid()) {
						base_type.script_type = base_script;
					} else {
						base_type.kind = RuztaParser::DataType::NATIVE;
						base_type.builtin_type = Variant::OBJECT;
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					return;
				}
			} break;
			case RuztaParser::DataType::NATIVE: {
				StringName type = base_type.native_type;
				if (!ClassDB::class_exists(type)) {
					return;
				}

				List<StringName> enums;
				ClassDB::class_get_enum_list(type, &enums);
				for (const StringName& E : enums) {
					int location = p_recursion_depth + _get_enum_location(type, E);
					RuztaLanguage::CodeCompletionOption option(E, RuztaLanguage::CODE_COMPLETION_KIND_ENUM, location);
					r_result.insert(option.display, option);
				}

				if (p_types_only) {
					return;
				}

				if (!p_only_functions) {
					List<String> constants;
					ClassDB::class_get_integer_constant_list(type, &constants);
					for (const String& E : constants) {
						int location = p_recursion_depth + _get_constant_location(type, StringName(E));
						RuztaLanguage::CodeCompletionOption option(E, RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
						r_result.insert(option.display, option);
					}

					if (!base_type.is_meta_type || Engine::get_singleton()->has_singleton(type)) {
						// TODO: ClassDB::get_property_list is not available in godot-cpp
						// Need to find GDExtension alternative for property list retrieval
#if 0
						List<PropertyInfo> pinfo;
						ClassDB::get_property_list(type, &pinfo);
						for (const PropertyInfo& E : pinfo) {
							if (E.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_INTERNAL)) {
								continue;
							}
							if (string_contains_char(E.name, '/')) {
								continue;
							}
							int location = p_recursion_depth + _get_property_location(type, E.name);
							RuztaLanguage::CodeCompletionOption option(E.name, RuztaLanguage::CODE_COMPLETION_KIND_MEMBER, location);
							r_result.insert(option.display, option);
						}

						// TODO: ClassDB::get_signal_list is not available in godot-cpp
						List<MethodInfo> signals;
						ClassDB::get_signal_list(type, &signals);
						for (const MethodInfo& E : signals) {
							if (E.name.begins_with("_")) {
								continue;
							}
							int location = p_recursion_depth + _get_signal_location(type, StringName(E.name));
							RuztaLanguage::CodeCompletionOption option(E.name, RuztaLanguage::CODE_COMPLETION_KIND_SIGNAL, location);
							r_result.insert(option.display, option);
						}
#endif
					}

					bool only_static = base_type.is_meta_type && !Engine::get_singleton()->has_singleton(type);

					// TODO: ClassDB::get_method_list is not available in godot-cpp
#if 0
				List<MethodInfo> methods;
				ClassDB::get_method_list(type, &methods, false, true);
				for (const MethodInfo& E : methods) {
					if (only_static && (E.flags & METHOD_FLAG_STATIC) == 0) {
						continue;
					}
					if (E.name.begins_with("_")) {
						continue;
					}
					int location = p_recursion_depth + _get_method_location(type, E.name);
					RuztaLanguage::CodeCompletionOption option(E.name, RuztaLanguage::CODE_COMPLETION_KIND_FUNCTION, location);
					if (p_add_braces) {
						if (E.arguments.size() || (E.flags & METHOD_FLAG_VARARG)) {
							option.insert_text += "(";
							option.display += U"(\u2026)";
						} else {
							option.insert_text += "()";
							option.display += "()";
						}
					}
					r_result.insert(option.display, option);
				}
#endif
					return;
				}
			} break;
			case RuztaParser::DataType::ENUM: {
				if (p_types_only) {
					return;
				}

				String type_str = base_type.native_type;

				if (string_contains_char(type_str, '.')) {
					StringName type = type_str.get_slicec('.', 0);
					StringName type_enum = base_type.enum_type;

#if 0  // TODO: Fix Enum constants for GDExtension
					List<StringName> enum_values;

					ClassDB::class_get_enum_constants(type, type_enum, &enum_values);

					for (const StringName& E : enum_values) {
						int location = p_recursion_depth + _get_enum_constant_location(type, E);
						RuztaLanguage::CodeCompletionOption option(E, RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
						r_result.insert(option.display, option);
					}
				} else if (CoreConstants::is_global_enum(base_type.enum_type)) {
					HashMap<StringName, int64_t> enum_values;
					CoreConstants::get_enum_values(base_type.enum_type, &enum_values);

					for (const KeyValue<StringName, int64_t>& enum_value : enum_values) {
						int location = p_recursion_depth + RuztaLanguage::LOCATION_OTHER;
						RuztaLanguage::CodeCompletionOption option(enum_value.key, RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT, location);
						r_result.insert(option.display, option);
					}
#endif
				}
			}
				[[fallthrough]];
			case RuztaParser::DataType::BUILTIN: {
				if (p_types_only) {
					return;
				}

				GDExtensionCallError err;
				Variant tmp;
				RuztaVariantExtension::construct(base_type.builtin_type, tmp, nullptr, 0, err);
				if (err.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
					return;
				}

				int location = RuztaLanguage::LOCATION_OTHER;

				if (!p_only_functions) {
					List<PropertyInfo> members;
					Variant base_value;
					if (p_base.value.get_type() != Variant::NIL) {
						base_value = p_base.value;
					} else {
						base_value = tmp;
					}
					RuztaVariantExtension::get_property_list(&base_value, &members);

					for (const PropertyInfo& E : members) {
						if (E.usage & (PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_INTERNAL)) {
							continue;
						}
						if (!string_contains_char(String(E.name), '/')) {
							RuztaLanguage::CodeCompletionOption option(E.name, RuztaLanguage::CODE_COMPLETION_KIND_MEMBER, location);
							if (base_type.kind == RuztaParser::DataType::ENUM) {
								// Sort enum members in their declaration order.
								location += 1;
							}
							if (RuztaParser::theme_color_names.has(E.name)) {
								option.theme_color_name = RuztaParser::theme_color_names[E.name];
							}
							r_result.insert(option.display, option);
						}
					}
				}

				List<MethodInfo> methods;
				RuztaVariantExtension::get_method_list(&tmp, &methods);
				for (const MethodInfo& E : methods) {
					if (base_type.kind == RuztaParser::DataType::ENUM && base_type.is_meta_type && !(E.flags & METHOD_FLAG_CONST)) {
						// Enum types are static and cannot change, therefore we skip non-const dictionary methods.
						continue;
					}
					RuztaLanguage::CodeCompletionOption option(E.name, RuztaLanguage::CODE_COMPLETION_KIND_FUNCTION, location);
					if (p_add_braces) {
						if (E.arguments.size() || (E.flags & METHOD_FLAG_VARARG)) {
							option.insert_text += "(";
							option.display += U"(\u2026)";
						} else {
							option.insert_text += "()";
							option.display += "()";
						}
					}
					r_result.insert(option.display, option);
				}

				return;
			} break;
			default: {
				return;
			} break;
		}
	}
}

static void _find_identifiers(const RuztaParser::CompletionContext& p_context, bool p_only_functions, bool p_add_braces, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result, int p_recursion_depth) {
	if (!p_only_functions && p_context.current_suite) {
		// This includes function parameters, since they are also locals.
		_find_identifiers_in_suite(p_context.current_suite, r_result);
	}

	if (p_context.current_class) {
		_find_identifiers_in_class(p_context.current_class, p_only_functions, false, (!p_context.current_function || p_context.current_function->is_static), false, p_add_braces, r_result, p_recursion_depth);
	}

	List<StringName> functions;
	RuztaUtilityFunctions::get_function_list(&functions);

	for (const StringName& E : functions) {
		MethodInfo function = RuztaUtilityFunctions::get_function_info(E);
		RuztaLanguage::CodeCompletionOption option(String(E), RuztaLanguage::CODE_COMPLETION_KIND_FUNCTION);
		if (p_add_braces) {
			if (function.arguments.size() || (function.flags & METHOD_FLAG_VARARG)) {
				option.insert_text += "(";
				option.display += U"(\u2026)";
			} else {
				option.insert_text += "()";
				option.display += "()";
			}
		}
		r_result.insert(option.display, option);
	}

	if (p_only_functions) {
		return;
	}

	_find_built_in_variants(r_result);

	static const char* _keywords[] = {
		"true", "false", "PI", "TAU", "INF", "NAN", "null", "self", "super",
		"break", "breakpoint", "continue", "pass", "return",
		nullptr};

	const char** kw = _keywords;
	while (*kw) {
		RuztaLanguage::CodeCompletionOption option(*kw, RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
		r_result.insert(option.display, option);
		kw++;
	}

	static const char* _keywords_with_space[] = {
		"and", "not", "or", "in", "as", "class", "class_name", "extends", "is", "func", "signal", "await",
		"const", "enum", "static", "var", "if", "elif", "else", "for", "match", "when", "while",
		nullptr};

	const char** kws = _keywords_with_space;
	while (*kws) {
		RuztaLanguage::CodeCompletionOption option(*kws, RuztaLanguage::CODE_COMPLETION_KIND_PLAIN_TEXT);
		option.insert_text += " ";
		r_result.insert(option.display, option);
		kws++;
	}

	static const char* _keywords_with_args[] = {
		"assert", "preload",
		nullptr};

	const char** kwa = _keywords_with_args;
	while (*kwa) {
		RuztaLanguage::CodeCompletionOption option(*kwa, RuztaLanguage::CODE_COMPLETION_KIND_FUNCTION);
		if (p_add_braces) {
			option.insert_text += "(";
			option.display += U"(\u2026)";
		}
		r_result.insert(option.display, option);
		kwa++;
	}

	List<StringName> utility_func_names;
	RuztaVariantExtension::get_utility_function_list(&utility_func_names);

	for (const StringName& util_func_name : utility_func_names) {
		RuztaLanguage::CodeCompletionOption option(util_func_name, RuztaLanguage::CODE_COMPLETION_KIND_FUNCTION);
		if (p_add_braces) {
			option.insert_text += "(";
			option.display += U"(\u2026)";	// As all utility functions contain an argument or more, this is hardcoded here.
		}
		r_result.insert(option.display, option);
	}

	// TODO: ProjectSettings::AutoloadInfo and get_autoload_list are not available in godot-cpp
#if 0
		for (const KeyValue<StringName, ProjectSettings::AutoloadInfo>& E : ProjectSettings::get_singleton()->get_autoload_list()) {
			if (!E.value.is_singleton) {
				continue;
			}
			RuztaLanguage::CodeCompletionOption option(E.key, RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT);
			r_result.insert(option.display, option);
		}
#endif

	// Native classes and global constants.
	for (const KeyValue<StringName, int>& E : RuztaLanguage::get_singleton()->get_global_map()) {
		RuztaLanguage::CodeCompletionOption option;
		if (ClassDB::class_exists(E.key) || Engine::get_singleton()->has_singleton(E.key)) {
			option = RuztaLanguage::CodeCompletionOption(String(E.key), RuztaLanguage::CODE_COMPLETION_KIND_CLASS);
		} else {
			option = RuztaLanguage::CodeCompletionOption(String(E.key), RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT);
		}
		r_result.insert(option.display, option);
	}

	// Global enums
	_find_global_enums(r_result);

	// Global classes
	LocalVector<StringName> global_classes;
	RuztaScriptServer::get_global_class_list(global_classes);
	for (const StringName& class_name : global_classes) {
		RuztaLanguage::CodeCompletionOption option(class_name, RuztaLanguage::CODE_COMPLETION_KIND_CLASS, RuztaLanguage::LOCATION_OTHER_USER_CODE);
		r_result.insert(option.display, option);
	}
}

static RuztaCompletionIdentifier _type_from_variant(const Variant& p_value, RuztaParser::CompletionContext& p_context) {
	RuztaCompletionIdentifier ci;
	ci.value = p_value;
	ci.type.is_constant = true;
	ci.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
	ci.type.kind = RuztaParser::DataType::BUILTIN;
	ci.type.builtin_type = p_value.get_type();

	if (ci.type.builtin_type == Variant::OBJECT) {
		Object* obj = p_value.operator Object*();
		if (!obj) {
			return ci;
		}
		ci.type.native_type = obj->get_class();
		Ref<Script> scr = p_value;
		if (scr.is_valid()) {
			ci.type.is_meta_type = true;
		} else {
			ci.type.is_meta_type = false;
			scr = obj->get_script();
		}
		if (scr.is_valid()) {
			ci.type.script_path = scr->get_path();
			ci.type.script_type = scr;
			ci.type.native_type = scr->get_instance_base_type();
			ci.type.kind = RuztaParser::DataType::SCRIPT;

			if (scr->get_path().ends_with(".rz")) {
				Ref<RuztaParserRef> parser = p_context.parser->get_depended_parser_for(scr->get_path());
				if (parser.is_valid() && parser->raise_status(RuztaParserRef::INTERFACE_SOLVED) == OK) {
					ci.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
					ci.type.class_type = parser->get_parser()->get_tree();
					ci.type.kind = RuztaParser::DataType::CLASS;
					return ci;
				}
			}
		} else {
			ci.type.kind = RuztaParser::DataType::NATIVE;
		}
	}

	return ci;
}

static RuztaCompletionIdentifier _type_from_property(const PropertyInfo& p_property) {
	RuztaCompletionIdentifier ci;

	if (p_property.type == Variant::NIL) {
		// Variant
		ci.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
		ci.type.kind = RuztaParser::DataType::VARIANT;
		return ci;
	}

	if (p_property.usage & (PROPERTY_USAGE_CLASS_IS_ENUM | PROPERTY_USAGE_CLASS_IS_BITFIELD)) {
		ci.enumeration = p_property.class_name;
	}

	ci.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
	ci.type.builtin_type = p_property.type;
	if (p_property.type == Variant::OBJECT) {
		if (RuztaScriptServer::is_global_class(p_property.class_name)) {
			ci.type.kind = RuztaParser::DataType::SCRIPT;
			ci.type.script_path = RuztaScriptServer::get_global_class_path(p_property.class_name);
			ci.type.native_type = RuztaScriptServer::get_global_class_native_base(p_property.class_name);

			Ref<Script> scr = ResourceLoader::get_singleton()->load(RuztaScriptServer::get_global_class_path(p_property.class_name));
			if (scr.is_valid()) {
				ci.type.script_type = scr;
			}
		} else {
			ci.type.kind = RuztaParser::DataType::NATIVE;
			ci.type.native_type = p_property.class_name == StringName() ? "Object" : p_property.class_name;
		}
	} else {
		ci.type.kind = RuztaParser::DataType::BUILTIN;
	}
	return ci;
}

static RuztaCompletionIdentifier _callable_type_from_method_info(const MethodInfo& p_method) {
	RuztaCompletionIdentifier ci;
	ci.type.kind = RuztaParser::DataType::BUILTIN;
	ci.type.builtin_type = Variant::CALLABLE;
	ci.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
	ci.type.is_constant = true;
	ci.type.method_info = p_method;
	return ci;
}

#define MAX_COMPLETION_RECURSION 100
struct RecursionCheck {
	int* counter;
	_FORCE_INLINE_ bool check() {
		return (*counter) > MAX_COMPLETION_RECURSION;
	}
	RecursionCheck(int* p_counter) : counter(p_counter) {
		(*counter)++;
	}
	~RecursionCheck() {
		(*counter)--;
	}
};

static bool _guess_identifier_type(RuztaParser::CompletionContext& p_context, const RuztaParser::IdentifierNode* p_identifier, RuztaCompletionIdentifier& r_type);
static bool _guess_identifier_type_from_base(RuztaParser::CompletionContext& p_context, const RuztaCompletionIdentifier& p_base, const StringName& p_identifier, RuztaCompletionIdentifier& r_type);
static bool _guess_method_return_type_from_base(RuztaParser::CompletionContext& p_context, const RuztaCompletionIdentifier& p_base, const StringName& p_method, RuztaCompletionIdentifier& r_type);

static bool _is_expression_named_identifier(const RuztaParser::ExpressionNode* p_expression, const StringName& p_name) {
	if (p_expression) {
		switch (p_expression->type) {
			case RuztaParser::Node::IDENTIFIER: {
				const RuztaParser::IdentifierNode* id = static_cast<const RuztaParser::IdentifierNode*>(p_expression);
				if (id->name == p_name) {
					return true;
				}
			} break;
			case RuztaParser::Node::CAST: {
				const RuztaParser::CastNode* cn = static_cast<const RuztaParser::CastNode*>(p_expression);
				return _is_expression_named_identifier(cn->operand, p_name);
			} break;
			default:
				break;
		}
	}

	return false;
}

// Creates a map of exemplary results for some functions that return a structured dictionary.
// Setting this example as value allows autocompletion to suggest the specific keys in some cases.
static HashMap<String, Dictionary> make_structure_samples() {
	HashMap<String, Dictionary> res;
	const Array arr;

	{
		Dictionary d;
		d.set("major", 0);
		d.set("minor", 0);
		d.set("patch", 0);
		d.set("hex", 0);
		d.set("status", String());
		d.set("build", String());
		d.set("hash", String());
		d.set("timestamp", 0);
		d.set("string", String());
		res["Engine::get_version_info"] = d;
	}

	{
		Dictionary d;
		d.set("lead_developers", arr);
		d.set("founders", arr);
		d.set("project_managers", arr);
		d.set("developers", arr);
		res["Engine::get_author_info"] = d;
	}

	{
		Dictionary d;
		d.set("platinum_sponsors", arr);
		d.set("gold_sponsors", arr);
		d.set("silver_sponsors", arr);
		d.set("bronze_sponsors", arr);
		d.set("mini_sponsors", arr);
		d.set("gold_donors", arr);
		d.set("silver_donors", arr);
		d.set("bronze_donors", arr);
		res["Engine::get_donor_info"] = d;
	}

	{
		Dictionary d;
		d.set("physical", -1);
		d.set("free", -1);
		d.set("available", -1);
		d.set("stack", -1);
		res["OS::get_memory_info"] = d;
	}

	{
		Dictionary d;
		d.set("year", 0);
		d.set("month", 0);
		d.set("day", 0);
		d.set("weekday", 0);
		d.set("hour", 0);
		d.set("minute", 0);
		d.set("second", 0);
		d.set("dst", 0);
		res["Time::get_datetime_dict_from_system"] = d;
	}

	{
		Dictionary d;
		d.set("year", 0);
		d.set("month", 0);
		d.set("day", 0);
		d.set("weekday", 0);
		d.set("hour", 0);
		d.set("minute", 0);
		d.set("second", 0);
		res["Time::get_datetime_dict_from_unix_time"] = d;
	}

	{
		Dictionary d;
		d.set("year", 0);
		d.set("month", 0);
		d.set("day", 0);
		d.set("weekday", 0);
		res["Time::get_date_dict_from_system"] = d;
		res["Time::get_date_dict_from_unix_time"] = d;
	}

	{
		Dictionary d;
		d.set("hour", 0);
		d.set("minute", 0);
		d.set("second", 0);
		res["Time::get_time_dict_from_system"] = d;
		res["Time::get_time_dict_from_unix_time"] = d;
	}

	{
		Dictionary d;
		d.set("bias", 0);
		d.set("name", String());
		res["Time::get_time_zone_from_system"] = d;
	}

	return res;
}

static const HashMap<String, Dictionary> structure_examples = make_structure_samples();

static bool _guess_expression_type(RuztaParser::CompletionContext& p_context, const RuztaParser::ExpressionNode* p_expression, RuztaCompletionIdentifier& r_type) {
	bool found = false;

	if (p_expression == nullptr) {
		return false;
	}

	static int recursion_depth = 0;
	RecursionCheck recursion(&recursion_depth);
	if (unlikely(recursion.check())) {
		ERR_FAIL_V_MSG(false, "Reached recursion limit while trying to guess type.");
	}

	if (p_expression->is_constant) {
		// Already has a value, so just use that.
		r_type = _type_from_variant(p_expression->reduced_value, p_context);
		switch (p_expression->get_datatype().kind) {
			case RuztaParser::DataType::ENUM:
			case RuztaParser::DataType::CLASS:
				r_type.type = p_expression->get_datatype();
				break;
			default:
				break;
		}
		found = true;
	} else {
		switch (p_expression->type) {
			case RuztaParser::Node::IDENTIFIER: {
				const RuztaParser::IdentifierNode* id = static_cast<const RuztaParser::IdentifierNode*>(p_expression);
				found = _guess_identifier_type(p_context, id, r_type);
			} break;
			case RuztaParser::Node::DICTIONARY: {
				// Try to recreate the dictionary.
				const RuztaParser::DictionaryNode* dn = static_cast<const RuztaParser::DictionaryNode*>(p_expression);
				Dictionary d;
				bool full = true;
				for (int i = 0; i < dn->elements.size(); i++) {
					RuztaCompletionIdentifier key;
					if (_guess_expression_type(p_context, dn->elements[i].key, key)) {
						if (!key.type.is_constant) {
							full = false;
							break;
						}
						RuztaCompletionIdentifier value;
						if (_guess_expression_type(p_context, dn->elements[i].value, value)) {
							if (!value.type.is_constant) {
								full = false;
								break;
							}
							d[key.value] = value.value;
						} else {
							full = false;
							break;
						}
					} else {
						full = false;
						break;
					}
				}
				if (full) {
					r_type.value = d;
					r_type.type.is_constant = true;
				}
				r_type.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
				r_type.type.kind = RuztaParser::DataType::BUILTIN;
				r_type.type.builtin_type = Variant::DICTIONARY;
				found = true;
			} break;
			case RuztaParser::Node::ARRAY: {
				// Try to recreate the array
				const RuztaParser::ArrayNode* an = static_cast<const RuztaParser::ArrayNode*>(p_expression);
				Array a;
				bool full = true;
				a.resize(an->elements.size());
				for (int i = 0; i < an->elements.size(); i++) {
					RuztaCompletionIdentifier value;
					if (_guess_expression_type(p_context, an->elements[i], value)) {
						if (value.type.is_constant) {
							a[i] = value.value;
						} else {
							full = false;
							break;
						}
					} else {
						full = false;
						break;
					}
				}
				if (full) {
					// If not fully constant, setting this value is detrimental to the inference.
					r_type.value = a;
					r_type.type.is_constant = true;
				}
				r_type.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
				r_type.type.kind = RuztaParser::DataType::BUILTIN;
				r_type.type.builtin_type = Variant::ARRAY;
				found = true;
			} break;
			case RuztaParser::Node::CAST: {
				const RuztaParser::CastNode* cn = static_cast<const RuztaParser::CastNode*>(p_expression);
				RuztaCompletionIdentifier value;
				if (_guess_expression_type(p_context, cn->operand, r_type)) {
					r_type.type = cn->get_datatype();
					found = true;
				}
			} break;
			case RuztaParser::Node::CALL: {
				const RuztaParser::CallNode* call = static_cast<const RuztaParser::CallNode*>(p_expression);
				RuztaParser::CompletionContext c = p_context;
				c.current_line = call->start_line;

				RuztaParser::Node::Type callee_type = call->get_callee_type();

				RuztaCompletionIdentifier base;
				if (callee_type == RuztaParser::Node::IDENTIFIER || call->is_super) {
					// Simple call, so base is 'self'.
					if (p_context.current_class) {
						if (call->is_super) {
							base.type = p_context.current_class->base_type;
							base.value = p_context.base;
						} else {
							base.type.kind = RuztaParser::DataType::CLASS;
							base.type.type_source = RuztaParser::DataType::INFERRED;
							base.type.is_constant = true;
							base.type.class_type = p_context.current_class;
							base.value = p_context.base;
						}
					} else {
						break;
					}
				} else if (callee_type == RuztaParser::Node::SUBSCRIPT && static_cast<const RuztaParser::SubscriptNode*>(call->callee)->is_attribute) {
					if (!_guess_expression_type(c, static_cast<const RuztaParser::SubscriptNode*>(call->callee)->base, base)) {
						found = false;
						break;
					}
				} else {
					break;
				}

				// Apply additional behavior aware inference that the analyzer can't do.
				if (base.type.is_set()) {
					// Maintain type for duplicate methods.
					if (call->function_name == StringName("duplicate")) {
						if (base.type.builtin_type == Variant::OBJECT && (ClassDB::is_parent_class(base.type.native_type, StringName("Resource")) || ClassDB::is_parent_class(base.type.native_type, StringName("Node")))) {
							r_type.type = base.type;
							found = true;
							break;
						}
					}

					// Simulate generics for some typed array methods.
					if (base.type.builtin_type == Variant::ARRAY && base.type.has_container_element_types() && (call->function_name == StringName("back") || call->function_name == StringName("front") || call->function_name == StringName("get") || call->function_name == StringName("max") || call->function_name == StringName("min") || call->function_name == StringName("pick_random") || call->function_name == StringName("pop_at") || call->function_name == StringName("pop_back") || call->function_name == StringName("pop_front"))) {
						r_type.type = base.type.get_container_element_type(0);
						found = true;
						break;
					}

					// Insert example values for functions which a structured dictionary response.
					if (!base.type.is_meta_type) {
						const Dictionary* example = structure_examples.getptr(String(base.type.native_type) + String("::") + call->function_name);
						if (example != nullptr) {
							r_type = _type_from_variant(*example, p_context);
							found = true;
							break;
						}
					}
				}

				if (!found) {
					found = _guess_method_return_type_from_base(c, base, call->function_name, r_type);
				}
			} break;
			case RuztaParser::Node::SUBSCRIPT: {
				const RuztaParser::SubscriptNode* subscript = static_cast<const RuztaParser::SubscriptNode*>(p_expression);
				if (subscript->is_attribute) {
					RuztaParser::CompletionContext c = p_context;
					c.current_line = subscript->start_line;

					RuztaCompletionIdentifier base;
					if (!_guess_expression_type(c, subscript->base, base)) {
						found = false;
						break;
					}

					if (base.value.get_type() == Variant::DICTIONARY && base.value.operator Dictionary().has(String(subscript->attribute->name))) {
						Variant value = base.value.operator Dictionary()[String(subscript->attribute->name)];
						r_type = _type_from_variant(value, p_context);
						found = true;
						break;
					}

					const RuztaParser::DictionaryNode* dn = nullptr;
					if (subscript->base->type == RuztaParser::Node::DICTIONARY) {
						dn = static_cast<const RuztaParser::DictionaryNode*>(subscript->base);
					} else if (base.assigned_expression && base.assigned_expression->type == RuztaParser::Node::DICTIONARY) {
						dn = static_cast<const RuztaParser::DictionaryNode*>(base.assigned_expression);
					}

					if (dn) {
						for (int i = 0; i < dn->elements.size(); i++) {
							RuztaCompletionIdentifier key;
							if (!_guess_expression_type(c, dn->elements[i].key, key)) {
								continue;
							}
							if (key.value == String(subscript->attribute->name)) {
								r_type.assigned_expression = dn->elements[i].value;
								found = _guess_expression_type(c, dn->elements[i].value, r_type);
								break;
							}
						}
					}

					if (!found) {
						found = _guess_identifier_type_from_base(c, base, subscript->attribute->name, r_type);
					}
				} else {
					if (subscript->index == nullptr) {
						found = false;
						break;
					}

					RuztaParser::CompletionContext c = p_context;
					c.current_line = subscript->start_line;

					RuztaCompletionIdentifier base;
					if (!_guess_expression_type(c, subscript->base, base)) {
						found = false;
						break;
					}

					RuztaCompletionIdentifier index;
					if (!_guess_expression_type(c, subscript->index, index)) {
						found = false;
						break;
					}

					if (base.type.is_constant && index.type.is_constant) {
						if (base.value.get_type() == Variant::DICTIONARY) {
							Dictionary base_dict = base.value.operator Dictionary();
							// TODO: Dictionary::get_key_validator not available in godot-cpp
							if (base_dict.has(index.value)) {
								r_type = _type_from_variant(base_dict[index.value], p_context);
								found = true;
								break;
							}
						} else {
							bool valid;
							Variant value = base.value.get(index.value, &valid);
							if (valid) {
								r_type = _type_from_variant(value, p_context);
								found = true;
								break;
							}
						}
					}

					// Look if it is a dictionary node.
					const RuztaParser::DictionaryNode* dn = nullptr;
					if (subscript->base->type == RuztaParser::Node::DICTIONARY) {
						dn = static_cast<const RuztaParser::DictionaryNode*>(subscript->base);
					} else if (base.assigned_expression && base.assigned_expression->type == RuztaParser::Node::DICTIONARY) {
						dn = static_cast<const RuztaParser::DictionaryNode*>(base.assigned_expression);
					}

					if (dn) {
						for (int i = 0; i < dn->elements.size(); i++) {
							RuztaCompletionIdentifier key;
							if (!_guess_expression_type(c, dn->elements[i].key, key)) {
								continue;
							}
							if (key.value == index.value) {
								r_type.assigned_expression = dn->elements[i].value;
								found = _guess_expression_type(p_context, dn->elements[i].value, r_type);
								break;
							}
						}
					}

					// Look if it is an array node.
					if (!found && variant_is_num(index.value)) {
						int idx = index.value;
						const RuztaParser::ArrayNode* an = nullptr;
						if (subscript->base->type == RuztaParser::Node::ARRAY) {
							an = static_cast<const RuztaParser::ArrayNode*>(subscript->base);
						} else if (base.assigned_expression && base.assigned_expression->type == RuztaParser::Node::ARRAY) {
							an = static_cast<const RuztaParser::ArrayNode*>(base.assigned_expression);
						}

						if (an && idx >= 0 && an->elements.size() > idx) {
							r_type.assigned_expression = an->elements[idx];
							found = _guess_expression_type(c, an->elements[idx], r_type);
							break;
						}
					}

					// Look for valid indexing in other types
					if (!found && (index.value.get_type() == Variant::STRING || index.value.get_type() == Variant::NODE_PATH)) {
						StringName id = index.value;
						found = _guess_identifier_type_from_base(c, base, id, r_type);
					} else if (!found && index.type.kind == RuztaParser::DataType::BUILTIN) {
						GDExtensionCallError err;
						Variant base_val;
						RuztaVariantExtension::construct(base.type.builtin_type, base_val, nullptr, 0, err);
						bool valid = false;
						Variant res = base_val.get(index.value, &valid);
						if (valid) {
							r_type = _type_from_variant(res, p_context);
							r_type.value = Variant();
							r_type.type.is_constant = false;
							found = true;
						}
					}
				}
			} break;
			case RuztaParser::Node::BINARY_OPERATOR: {
				const RuztaParser::BinaryOpNode* op = static_cast<const RuztaParser::BinaryOpNode*>(p_expression);

				if (op->variant_op == Variant::OP_MAX) {
					break;
				}

				RuztaParser::CompletionContext context = p_context;
				context.current_line = op->start_line;

				RuztaCompletionIdentifier p1;
				RuztaCompletionIdentifier p2;

				if (!_guess_expression_type(context, op->left_operand, p1)) {
					found = false;
					break;
				}

				if (!_guess_expression_type(context, op->right_operand, p2)) {
					found = false;
					break;
				}

				GDExtensionCallError ce;
				bool v1_use_value = p1.value.get_type() != Variant::NIL && p1.value.get_type() != Variant::OBJECT;
				Variant d1;
				RuztaVariantExtension::construct(p1.type.builtin_type, d1, nullptr, 0, ce);
				Variant d2;
				RuztaVariantExtension::construct(p2.type.builtin_type, d2, nullptr, 0, ce);

				Variant v1 = (v1_use_value) ? p1.value : d1;
				bool v2_use_value = p2.value.get_type() != Variant::NIL && p2.value.get_type() != Variant::OBJECT;
				Variant v2 = (v2_use_value) ? p2.value : d2;
				// avoid potential invalid ops
				if ((op->variant_op == Variant::OP_DIVIDE || op->variant_op == Variant::OP_MODULE) && v2.get_type() == Variant::INT) {
					v2 = 1;
					v2_use_value = false;
				}
				if (op->variant_op == Variant::OP_DIVIDE && v2.get_type() == Variant::FLOAT) {
					v2 = 1.0;
					v2_use_value = false;
				}

				Variant res;
				bool valid;
				RuztaVariantExtension::evaluate(op->variant_op, v1, v2, res, valid);
				if (!valid) {
					found = false;
					break;
				}
				r_type = _type_from_variant(res, p_context);
				if (!v1_use_value || !v2_use_value) {
					r_type.value = Variant();
					r_type.type.is_constant = false;
				}

				found = true;
			} break;
			default:
				break;
		}
	}

	// It may have found a null, but that's never useful
	if (found && r_type.type.kind == RuztaParser::DataType::BUILTIN && r_type.type.builtin_type == Variant::NIL) {
		found = false;
	}

	// If the found type was not fully analyzed we analyze it now.
	if (found && r_type.type.kind == RuztaParser::DataType::CLASS && !r_type.type.class_type->resolved_body) {
		Error err;
		Ref<RuztaParserRef> r = RuztaCache::get_parser(r_type.type.script_path, RuztaParserRef::FULLY_SOLVED, err);
	}

	// Check type hint last. For collections we want chance to get the actual value first
	// This way we can detect types from the content of dictionaries and arrays
	if (!found && p_expression->get_datatype().is_hard_type()) {
		r_type.type = p_expression->get_datatype();
		if (!r_type.assigned_expression) {
			r_type.assigned_expression = p_expression;
		}
		found = true;
	}

	return found;
}

static bool _guess_identifier_type(RuztaParser::CompletionContext& p_context, const RuztaParser::IdentifierNode* p_identifier, RuztaCompletionIdentifier& r_type) {
	static int recursion_depth = 0;
	RecursionCheck recursion(&recursion_depth);
	if (unlikely(recursion.check())) {
		ERR_FAIL_V_MSG(false, "Reached recursion limit while trying to guess type.");
	}

	// Look in blocks first.
	int last_assign_line = -1;
	const RuztaParser::ExpressionNode* last_assigned_expression = nullptr;
	RuztaCompletionIdentifier id_type;
	RuztaParser::SuiteNode* suite = p_context.current_suite;
	bool is_function_parameter = false;

	bool can_be_local = true;
	switch (p_identifier->source) {
		case RuztaParser::IdentifierNode::MEMBER_VARIABLE:
		case RuztaParser::IdentifierNode::MEMBER_CONSTANT:
		case RuztaParser::IdentifierNode::MEMBER_FUNCTION:
		case RuztaParser::IdentifierNode::MEMBER_SIGNAL:
		case RuztaParser::IdentifierNode::MEMBER_CLASS:
		case RuztaParser::IdentifierNode::INHERITED_VARIABLE:
		case RuztaParser::IdentifierNode::STATIC_VARIABLE:
		case RuztaParser::IdentifierNode::NATIVE_CLASS:
			can_be_local = false;
			break;
		default:
			break;
	}

	if (can_be_local && suite && suite->has_local(p_identifier->name)) {
		const RuztaParser::SuiteNode::Local& local = suite->get_local(p_identifier->name);

		id_type.type = local.get_datatype();

		// Check initializer as the first assignment.
		switch (local.type) {
			case RuztaParser::SuiteNode::Local::VARIABLE:
				if (local.variable->initializer) {
					last_assign_line = local.variable->initializer->end_line;
					last_assigned_expression = local.variable->initializer;
				}
				break;
			case RuztaParser::SuiteNode::Local::CONSTANT:
				if (local.constant->initializer) {
					last_assign_line = local.constant->initializer->end_line;
					last_assigned_expression = local.constant->initializer;
				}
				break;
			case RuztaParser::SuiteNode::Local::PARAMETER:
				if (local.parameter->initializer) {
					last_assign_line = local.parameter->initializer->end_line;
					last_assigned_expression = local.parameter->initializer;
				}
				is_function_parameter = true;
				break;
			default:
				break;
		}
	} else {
		if (p_context.current_class) {
			RuztaCompletionIdentifier base_identifier;

			RuztaCompletionIdentifier base;
			base.value = p_context.base;
			base.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
			base.type.kind = RuztaParser::DataType::CLASS;
			base.type.class_type = p_context.current_class;
			base.type.is_meta_type = p_context.current_function && p_context.current_function->is_static;

			if (_guess_identifier_type_from_base(p_context, base, p_identifier->name, base_identifier)) {
				id_type = base_identifier;
			}
		}
	}

	while (suite) {
		for (int i = 0; i < suite->statements.size(); i++) {
			if (suite->statements[i]->end_line >= p_context.current_line) {
				break;
			}

			switch (suite->statements[i]->type) {
				case RuztaParser::Node::ASSIGNMENT: {
					const RuztaParser::AssignmentNode* assign = static_cast<const RuztaParser::AssignmentNode*>(suite->statements[i]);
					if (assign->end_line > last_assign_line && assign->assignee && assign->assigned_value && assign->assignee->type == RuztaParser::Node::IDENTIFIER) {
						const RuztaParser::IdentifierNode* id = static_cast<const RuztaParser::IdentifierNode*>(assign->assignee);
						if (id->name == p_identifier->name && id->source == p_identifier->source) {
							last_assign_line = assign->assigned_value->end_line;
							last_assigned_expression = assign->assigned_value;
						}
					}
				} break;
				default:
					// TODO: Check sub blocks (control flow statements) as they might also reassign stuff.
					break;
			}
		}

		if (suite->parent_if && suite->parent_if->condition && suite->parent_if->condition->type == RuztaParser::Node::TYPE_TEST) {
			// Operator `is` used, check if identifier is in there! this helps resolve in blocks that are (if (identifier is value)): which are very common..
			// Super dirty hack, but very useful.
			// Credit: Zylann.
			// TODO: this could be hacked to detect AND-ed conditions too...
			const RuztaParser::TypeTestNode* type_test = static_cast<const RuztaParser::TypeTestNode*>(suite->parent_if->condition);
			if (type_test->operand && type_test->test_type && type_test->operand->type == RuztaParser::Node::IDENTIFIER && static_cast<const RuztaParser::IdentifierNode*>(type_test->operand)->name == p_identifier->name && static_cast<const RuztaParser::IdentifierNode*>(type_test->operand)->source == p_identifier->source) {
				// Bingo.
				RuztaParser::CompletionContext c = p_context;
				c.current_line = type_test->operand->start_line;
				c.current_suite = suite;
				if (type_test->test_datatype.is_hard_type()) {
					id_type.type = type_test->test_datatype;
					if (last_assign_line < c.current_line) {
						// Override last assignment.
						last_assign_line = c.current_line;
						last_assigned_expression = nullptr;
					}
				}
			}
		}

		suite = suite->parent_block;
	}

	if (last_assigned_expression && last_assign_line < p_context.current_line) {
		RuztaParser::CompletionContext c = p_context;
		c.current_line = last_assign_line;
		RuztaCompletionIdentifier assigned_type;
		if (_guess_expression_type(c, last_assigned_expression, assigned_type)) {
			if (id_type.type.is_set() && (assigned_type.type.kind == RuztaParser::DataType::VARIANT || (assigned_type.type.is_set() && !RuztaAnalyzer::check_type_compatibility(id_type.type, assigned_type.type)))) {
				// The assigned type is incompatible. The annotated type takes priority.
				r_type = id_type;
				r_type.assigned_expression = last_assigned_expression;
			} else {
				r_type = assigned_type;
			}
			return true;
		}
	}

	if (is_function_parameter && p_context.current_function && p_context.current_function->source_lambda == nullptr && p_context.current_class) {
		// Check if it's override of native function, then we can assume the type from the signature.
		RuztaParser::DataType base_type = p_context.current_class->base_type;
		while (base_type.is_set()) {
			switch (base_type.kind) {
				case RuztaParser::DataType::CLASS:
					if (base_type.class_type->has_function(p_context.current_function->identifier->name)) {
						RuztaParser::FunctionNode* parent_function = base_type.class_type->get_member(p_context.current_function->identifier->name).function;
						if (parent_function->parameters_indices.has(p_identifier->name)) {
							const RuztaParser::ParameterNode* parameter = parent_function->parameters[parent_function->parameters_indices[p_identifier->name]];
							if ((!id_type.type.is_set() || id_type.type.is_variant()) && parameter->get_datatype().is_hard_type()) {
								id_type.type = parameter->get_datatype();
							}
							if (parameter->initializer) {
								RuztaParser::CompletionContext c = p_context;
								c.current_function = parent_function;
								c.current_class = base_type.class_type;
								c.base = nullptr;
								if (_guess_expression_type(c, parameter->initializer, r_type)) {
									return true;
								}
							}
						}
					}
					base_type = base_type.class_type->base_type;
					break;
				case RuztaParser::DataType::NATIVE: {
					if (id_type.type.is_set() && !id_type.type.is_variant()) {
						base_type = RuztaParser::DataType();
						break;
					}
					// TODO: ClassDB::get_method_info not available in godot-cpp
#if 0
						MethodInfo info;
						if (ClassDB::get_method_info(base_type.native_type, p_context.current_function->identifier->name, &info)) {
							for (const PropertyInfo& E : info.arguments) {
								if (E.name == p_identifier->name) {
									r_type = _type_from_property(E);
									return true;
								}
							}
						}
#endif
					base_type = RuztaParser::DataType();
				} break;
				default:
					break;
			}
		}
	}

	if (id_type.type.is_set() && !id_type.type.is_variant()) {
		r_type = id_type;
		return true;
	}

	// Check global scripts.
	if (RuztaScriptServer::is_global_class(p_identifier->name)) {
		String script = RuztaScriptServer::get_global_class_path(p_identifier->name);
		if (script.to_lower().ends_with(".rz")) {
			Ref<RuztaParserRef> parser = p_context.parser->get_depended_parser_for(script);
			if (parser.is_valid() && parser->raise_status(RuztaParserRef::INTERFACE_SOLVED) == OK) {
				r_type.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
				r_type.type.script_path = script;
				r_type.type.class_type = parser->get_parser()->get_tree();
				r_type.type.is_meta_type = true;
				r_type.type.is_constant = false;
				r_type.type.kind = RuztaParser::DataType::CLASS;
				r_type.value = Variant();
				return true;
			}
		} else {
			Ref<Script> scr = ResourceLoader::get_singleton()->load(RuztaScriptServer::get_global_class_path(p_identifier->name));
			if (scr.is_valid()) {
				r_type = _type_from_variant(scr, p_context);
				r_type.type.is_meta_type = true;
				return true;
			}
		}
		return false;
	}

	// Check global variables (including autoloads).
	if (RuztaLanguage::get_singleton()->get_named_globals_map().has(p_identifier->name)) {
		r_type = _type_from_variant(RuztaLanguage::get_singleton()->get_named_globals_map()[p_identifier->name], p_context);
		return true;
	}

	// Check ClassDB.
	if (ClassDB::class_exists(p_identifier->name) /* && ClassDB::is_class_exposed(p_identifier->name) */) {
		r_type.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
		r_type.type.kind = RuztaParser::DataType::NATIVE;
		r_type.type.builtin_type = Variant::OBJECT;
		r_type.type.native_type = p_identifier->name;
		r_type.type.is_constant = true;
		if (Engine::get_singleton()->has_singleton(p_identifier->name)) {
			r_type.type.is_meta_type = false;
			// TODO: Engine::get_singleton_object not available in godot-cpp
			r_type.value = Variant();
		} else {
			r_type.type.is_meta_type = true;
			r_type.value = Variant();
		}
		return true;
	}

	return false;
}

static bool _guess_identifier_type_from_base(RuztaParser::CompletionContext& p_context, const RuztaCompletionIdentifier& p_base, const StringName& p_identifier, RuztaCompletionIdentifier& r_type) {
	static int recursion_depth = 0;
	RecursionCheck recursion(&recursion_depth);
	if (unlikely(recursion.check())) {
		ERR_FAIL_V_MSG(false, "Reached recursion limit while trying to guess type.");
	}

	RuztaParser::DataType base_type = p_base.type;
	bool is_static = base_type.is_meta_type;
	while (base_type.is_set()) {
		switch (base_type.kind) {
			case RuztaParser::DataType::CLASS:
				if (base_type.class_type->has_member(p_identifier)) {
					const RuztaParser::ClassNode::Member& member = base_type.class_type->get_member(p_identifier);
					switch (member.type) {
						case RuztaParser::ClassNode::Member::CONSTANT:
							r_type.type = member.constant->get_datatype();
							if (member.constant->initializer && member.constant->initializer->is_constant) {
								r_type.value = member.constant->initializer->reduced_value;
							}
							return true;
						case RuztaParser::ClassNode::Member::VARIABLE:
							if (!is_static || member.variable->is_static) {
								if (member.variable->get_datatype().is_set() && !member.variable->get_datatype().is_variant()) {
									r_type.type = member.variable->get_datatype();
									return true;
								} else if (member.variable->initializer) {
									const RuztaParser::ExpressionNode* init = member.variable->initializer;
									if (init->is_constant) {
										r_type.value = init->reduced_value;
										r_type = _type_from_variant(init->reduced_value, p_context);
										return true;
									} else if (init->start_line == p_context.current_line) {
										return false;
										// Detects if variable is assigned to itself
									} else if (_is_expression_named_identifier(init, member.variable->identifier->name)) {
										if (member.variable->initializer->get_datatype().is_set()) {
											r_type.type = member.variable->initializer->get_datatype();
										} else if (member.variable->get_datatype().is_set() && !member.variable->get_datatype().is_variant()) {
											r_type.type = member.variable->get_datatype();
										}
										return true;
									} else if (_guess_expression_type(p_context, init, r_type)) {
										return true;
									} else if (init->get_datatype().is_set() && !init->get_datatype().is_variant()) {
										r_type.type = init->get_datatype();
										return true;
									}
								}
							}
							// TODO: Check assignments in constructor.
							return false;
						case RuztaParser::ClassNode::Member::ENUM:
							r_type.type = member.m_enum->get_datatype();
							r_type.enumeration = member.m_enum->identifier->name;
							return true;
						case RuztaParser::ClassNode::Member::ENUM_VALUE:
							r_type = _type_from_variant(member.enum_value.value, p_context);
							return true;
						case RuztaParser::ClassNode::Member::SIGNAL:
							r_type.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
							r_type.type.kind = RuztaParser::DataType::BUILTIN;
							r_type.type.builtin_type = Variant::SIGNAL;
							r_type.type.method_info = member.signal->method_info;
							return true;
						case RuztaParser::ClassNode::Member::FUNCTION:
							if (is_static && !member.function->is_static) {
								return false;
							}
							r_type = _callable_type_from_method_info(member.function->info);
							return true;
						case RuztaParser::ClassNode::Member::CLASS:
							r_type.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
							r_type.type.kind = RuztaParser::DataType::CLASS;
							r_type.type.class_type = member.m_class;
							r_type.type.is_meta_type = true;
							return true;
						case RuztaParser::ClassNode::Member::GROUP:
							return false;  // No-op, but silences warnings.
						case RuztaParser::ClassNode::Member::UNDEFINED:
							return false;  // Unreachable.
					}
					return false;
				}
				base_type = base_type.class_type->base_type;
				break;
			case RuztaParser::DataType::SCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					// TODO: Script::get_constants not available in godot-cpp
#if 0
						HashMap<StringName, Variant> constants;
						scr->get_constants(&constants);
						if (constants.has(p_identifier)) {
							r_type = _type_from_variant(constants[p_identifier], p_context);
							return true;
						}
#endif

					// godot-cpp: get_property_list and get_script_property_list return TypedArray<Dictionary>
					TypedArray<Dictionary> members_array;
					if (is_static) {
						members_array = scr->get_property_list();
					} else {
						members_array = scr->get_script_property_list();
					}
					for (int i = 0; i < members_array.size(); i++) {
						PropertyInfo prop = PropertyInfo::from_dict(members_array[i]);
						if (prop.name == p_identifier) {
// TODO: Need to handle property getters/setters in godot-cpp
#if 0
								if (scr->is_property_valid_base(prop.name)) {
									MethodBind* g = ClassDB::get_method(scr->get_instance_base_type(), scr->get_property_getter(prop.name));
									if (g) {
										r_type = _type_from_property(g->get_return_info());
										return true;
									}
								}
#endif
							r_type = _type_from_property(prop);
							return true;
						}
					}

					// TODO: Script::get_method_info not available in godot-cpp
#if 0
						if (scr->has_method(p_identifier)) {
							MethodInfo mi = scr->get_method_info(p_identifier);
							r_type = _callable_type_from_method_info(mi);
							return true;
						}
#endif

					Ref<Script> parent = scr->get_base_script();
					if (parent.is_valid()) {
						base_type.script_type = parent;
					} else {
						base_type.kind = RuztaParser::DataType::NATIVE;
						base_type.builtin_type = Variant::OBJECT;
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					return false;
				}
			} break;
			case RuztaParser::DataType::NATIVE: {
				StringName class_name = base_type.native_type;
				if (!ClassDB::class_exists(class_name)) {
					return false;
				}

				// Skip constants since they're all integers. Type does not matter because int has no members.

				PropertyInfo prop;
				bool found_prop = false;
				for (Dictionary prop_dict : ClassDB::class_get_property_list(class_name, true)) {
					if (prop_dict.has("name") && prop_dict["name"] == p_identifier) {
						prop = PropertyInfo::from_dict(prop_dict);
						found_prop = true;
					}
				}
				if (found_prop) {
					// TODO: ClassDB::get_property_getter and MethodBind::get_return_info not available in godot-cpp
#if 0
						StringName getter = ClassDB::get_property_getter(class_name, p_identifier);
						if (getter != StringName()) {
							MethodBind* g = ClassDB::get_method(class_name, getter);
							if (g) {
								r_type = _type_from_property(g->get_return_info());
								return true;
							}
						} else {
#endif
					r_type = _type_from_property(prop);
					return true;
#if 0
						}
#endif
				}

				// TODO: ClassDB::get_method_info is not available in godot-cpp
#if 0
					MethodInfo method;
					if (ClassDB::get_method_info(class_name, p_identifier, &method)) {
						r_type = _callable_type_from_method_info(method);
						return true;
					}
#endif

				if (ClassDB::class_has_enum(class_name, p_identifier)) {
					r_type.type.type_source = RuztaParser::DataType::ANNOTATED_EXPLICIT;
					r_type.type.kind = RuztaParser::DataType::ENUM;
					r_type.type.enum_type = p_identifier;
					r_type.type.is_constant = true;
					r_type.type.is_meta_type = true;
					r_type.type.native_type = String(class_name) + "." + p_identifier;
					return true;
				}

				return false;
			} break;
			case RuztaParser::DataType::BUILTIN: {
				if (RuztaVariantExtension::has_builtin_method(base_type.builtin_type, p_identifier)) {
					r_type = _callable_type_from_method_info(RuztaVariantExtension::get_builtin_method_info(base_type.builtin_type, p_identifier));
					return true;
				} else {
					GDExtensionCallError err;
					Variant tmp;
					RuztaVariantExtension::construct(base_type.builtin_type, tmp, nullptr, 0, err);

					if (err.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
						return false;
					}
					bool valid = false;
					Variant res = tmp.get(p_identifier, &valid);
					if (valid) {
						r_type = _type_from_variant(res, p_context);
						r_type.value = Variant();
						r_type.type.is_constant = false;
						return true;
					}
				}
				return false;
			} break;
			default: {
				return false;
			} break;
		}
	}
	return false;
}

static void _find_last_return_in_block(RuztaParser::CompletionContext& p_context, int& r_last_return_line, const RuztaParser::ExpressionNode** r_last_returned_value) {
	if (!p_context.current_suite) {
		return;
	}

	for (int i = 0; i < p_context.current_suite->statements.size(); i++) {
		if (p_context.current_suite->statements[i]->start_line < r_last_return_line) {
			break;
		}

		RuztaParser::CompletionContext c = p_context;
		switch (p_context.current_suite->statements[i]->type) {
			case RuztaParser::Node::FOR:
				c.current_suite = static_cast<const RuztaParser::ForNode*>(p_context.current_suite->statements[i])->loop;
				_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
				break;
			case RuztaParser::Node::WHILE:
				c.current_suite = static_cast<const RuztaParser::WhileNode*>(p_context.current_suite->statements[i])->loop;
				_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
				break;
			case RuztaParser::Node::IF: {
				const RuztaParser::IfNode* _if = static_cast<const RuztaParser::IfNode*>(p_context.current_suite->statements[i]);
				c.current_suite = _if->true_block;
				_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
				if (_if->false_block) {
					c.current_suite = _if->false_block;
					_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
				}
			} break;
			case RuztaParser::Node::MATCH: {
				const RuztaParser::MatchNode* match = static_cast<const RuztaParser::MatchNode*>(p_context.current_suite->statements[i]);
				for (int j = 0; j < match->branches.size(); j++) {
					c.current_suite = match->branches[j]->block;
					_find_last_return_in_block(c, r_last_return_line, r_last_returned_value);
				}
			} break;
			case RuztaParser::Node::RETURN: {
				const RuztaParser::ReturnNode* ret = static_cast<const RuztaParser::ReturnNode*>(p_context.current_suite->statements[i]);
				if (ret->return_value) {
					if (ret->start_line > r_last_return_line) {
						r_last_return_line = ret->start_line;
						*r_last_returned_value = ret->return_value;
					}
				}
			} break;
			default:
				break;
		}
	}
}

static bool _guess_method_return_type_from_base(RuztaParser::CompletionContext& p_context, const RuztaCompletionIdentifier& p_base, const StringName& p_method, RuztaCompletionIdentifier& r_type) {
	static int recursion_depth = 0;
	RecursionCheck recursion(&recursion_depth);
	if (unlikely(recursion.check())) {
		ERR_FAIL_V_MSG(false, "Reached recursion limit while trying to guess type.");
	}

	RuztaParser::DataType base_type = p_base.type;
	bool is_static = base_type.is_meta_type;

	if (is_static && p_method == StringName("new")) {
		r_type.type = base_type;
		r_type.type.is_meta_type = false;
		r_type.type.is_constant = false;
		return true;
	}

	while (base_type.is_set() && !base_type.is_variant()) {
		switch (base_type.kind) {
			case RuztaParser::DataType::CLASS:
				if (base_type.class_type->has_function(p_method)) {
					RuztaParser::FunctionNode* method = base_type.class_type->get_member(p_method).function;
					if (!is_static || method->is_static) {
						if (method->get_datatype().is_set() && !method->get_datatype().is_variant()) {
							r_type.type = method->get_datatype();
							return true;
						}

						int last_return_line = -1;
						const RuztaParser::ExpressionNode* last_returned_value = nullptr;
						RuztaParser::CompletionContext c = p_context;
						c.current_class = base_type.class_type;
						c.current_function = method;
						c.current_suite = method->body;

						_find_last_return_in_block(c, last_return_line, &last_returned_value);
						if (last_returned_value) {
							c.current_line = c.current_suite->end_line;
							if (_guess_expression_type(c, last_returned_value, r_type)) {
								return true;
							}
						}
					}
				}
				base_type = base_type.class_type->base_type;
				break;
			case RuztaParser::DataType::SCRIPT: {
				Ref<Script> scr = base_type.script_type;
				if (scr.is_valid()) {
					godot::TypedArray<Dictionary> methods = scr->get_script_method_list();
					for (const Dictionary& info : methods) {
						MethodInfo mi = MethodInfo::from_dict(info);
						if (mi.name == p_method) {
							r_type = _type_from_property(mi.return_val);
							return true;
						}
					}
					Ref<Script> base_script = scr->get_base_script();
					if (base_script.is_valid()) {
						base_type.script_type = base_script;
					} else {
						base_type.kind = RuztaParser::DataType::NATIVE;
						base_type.builtin_type = Variant::OBJECT;
						base_type.native_type = scr->get_instance_base_type();
					}
				} else {
					return false;
				}
			} break;
			case RuztaParser::DataType::NATIVE: {
				if (!ClassDB::class_exists(base_type.native_type)) {
					return false;
				}
				// TODO: MethodBind::get_return_info is not available in godot-cpp
#if 0
					MethodBind* mb = ClassDB::get_method(base_type.native_type, p_method);
					if (mb) {
						r_type = _type_from_property(mb->get_return_info());
						return true;
					}
#endif
				return false;
			} break;
			case RuztaParser::DataType::BUILTIN: {
				GDExtensionCallError err;
				Variant tmp;
				RuztaVariantExtension::construct(base_type.builtin_type, tmp, nullptr, 0, err);
				if (err.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
					return false;
				}

				List<MethodInfo> methods;
				RuztaVariantExtension::get_method_list(&tmp, &methods);

				for (const MethodInfo& mi : methods) {
					if (mi.name == p_method) {
						r_type = _type_from_property(mi.return_val);
						return true;
					}
				}
				return false;
			} break;
			default: {
				return false;
			}
		}
	}

	return false;
}

static bool _guess_expecting_callable(RuztaParser::CompletionContext& p_context) {
	if (p_context.call.call != nullptr && p_context.call.call->type == RuztaParser::Node::CALL) {
		RuztaParser::CallNode* call_node = static_cast<RuztaParser::CallNode*>(p_context.call.call);
		RuztaCompletionIdentifier ci;
		if (_guess_expression_type(p_context, call_node->callee, ci)) {
			if (ci.type.kind == RuztaParser::DataType::BUILTIN && ci.type.builtin_type == Variant::CALLABLE) {
				if (p_context.call.argument >= 0 && p_context.call.argument < ci.type.method_info.arguments.size()) {
					return ci.type.method_info.arguments[p_context.call.argument].type == Variant::CALLABLE;
				}
			}
		}
	}

	return false;
}

static void _find_enumeration_candidates(RuztaParser::CompletionContext& p_context, const String& p_enum_hint, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result) {
	if (!string_contains_char(p_enum_hint, '.')) {
		// Global constant or in the current class.
		StringName current_enum = p_enum_hint;
		if (p_context.current_class && p_context.current_class->has_member(current_enum) && p_context.current_class->get_member(current_enum).type == RuztaParser::ClassNode::Member::ENUM) {
			const RuztaParser::EnumNode* _enum = p_context.current_class->get_member(current_enum).m_enum;
			for (int i = 0; i < _enum->values.size(); i++) {
				RuztaLanguage::CodeCompletionOption option(_enum->values[i].identifier->name, RuztaLanguage::CODE_COMPLETION_KIND_ENUM);
				r_result.insert(option.display, option);
			}
		} else {
			for (int i = 0; i < CoreConstants::get_global_constant_count(); i++) {
				if (CoreConstants::get_global_constant_enum(i) == current_enum) {
					RuztaLanguage::CodeCompletionOption option(CoreConstants::get_global_constant_name(i), RuztaLanguage::CODE_COMPLETION_KIND_ENUM);
					r_result.insert(option.display, option);
				}
			}
		}
	} else {
		String class_name = p_enum_hint.get_slicec('.', 0);
		String enum_name = p_enum_hint.get_slicec('.', 1);

		if (!ClassDB::class_exists(class_name)) {
			return;
		}

		List<StringName> enum_constants;
		ClassDB::class_get_enum_constants(class_name, enum_name, &enum_constants);
		for (const StringName& E : enum_constants) {
			String candidate = class_name + "." + E;
			int location = _get_enum_constant_location(class_name, E);
			RuztaLanguage::CodeCompletionOption option(candidate, RuztaLanguage::CODE_COMPLETION_KIND_ENUM, location);
			r_result.insert(option.display, option);
		}
	}
}

static void _list_call_arguments(RuztaParser::CompletionContext& p_context, const RuztaCompletionIdentifier& p_base, const RuztaParser::CallNode* p_call, int p_argidx, bool p_static, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result, String& r_arghint) {
	Variant base = p_base.value;
	RuztaParser::DataType base_type = p_base.type;
	const StringName& method = p_call->function_name;

	const String quote_style = RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/completion/use_single_quotes") ? "'" : "\"";
	const bool use_string_names = RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/completion/add_string_name_literals");
	const bool use_node_paths = RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/completion/add_node_path_literals");

	while (base_type.is_set() && !base_type.is_variant()) {
		switch (base_type.kind) {
			case RuztaParser::DataType::CLASS: {
				if (base_type.is_meta_type && method == StringName("new")) {
					const RuztaParser::ClassNode* current = base_type.class_type;

					do {
						if (current->has_member("_init")) {
							const RuztaParser::ClassNode::Member& member = current->get_member("_init");

							if (member.type == RuztaParser::ClassNode::Member::FUNCTION) {
								r_arghint = base_type.class_type->get_datatype().to_string() + " new" + _make_arguments_hint(member.function, p_argidx, true);
								return;
							}
						}
						current = current->base_type.class_type;
					} while (current != nullptr);

					r_arghint = base_type.class_type->get_datatype().to_string() + " new()";
					return;
				}

				if (base_type.class_type->has_member(method)) {
					const RuztaParser::ClassNode::Member& member = base_type.class_type->get_member(method);

					if (member.type == RuztaParser::ClassNode::Member::FUNCTION) {
						r_arghint = _make_arguments_hint(member.function, p_argidx);
						return;
					}
				}

				base_type = base_type.class_type->base_type;
			} break;
			case RuztaParser::DataType::SCRIPT: {
				// TODO: Script::is_valid and Script::get_method_info are not available in godot-cpp
#if 0
					if (base_type.script_type->is_valid() && base_type.script_type->has_method(method)) {
						r_arghint = _make_arguments_hint(base_type.script_type->get_method_info(method), p_argidx);
						return;
					}
#endif
				Ref<Script> base_script = base_type.script_type->get_base_script();
				if (base_script.is_valid()) {
					base_type.script_type = base_script;
				} else {
					base_type.kind = RuztaParser::DataType::NATIVE;
					base_type.builtin_type = Variant::OBJECT;
					base_type.native_type = base_type.script_type->get_instance_base_type();
				}
			} break;
			case RuztaParser::DataType::NATIVE: {
				StringName class_name = base_type.native_type;
				if (!ClassDB::class_exists(class_name)) {
					base_type.kind = RuztaParser::DataType::UNRESOLVED;
					break;
				}

				MethodInfo info;
				int method_args = 0;

				// TODO: ClassDB::get_method_info and Object::get_argument_options are not available in godot-cpp
#if 0
					if (ClassDB::get_method_info(class_name, method, &info)) {
						method_args = info.arguments.size();
						if (base.get_type() == Variant::OBJECT) {
							Object* obj = base.operator Object*();
							if (obj) {
								List<String> options;
								obj->get_argument_options(method, p_argidx, &options);
								for (String& opt : options) {
									// Handle user preference.
									if (is_string_quoted(opt)) {
										opt = quote_string(unquote_string(opt), quote_style);
									} else {
										opt = quote_string(opt, quote_style);
									}
									RuztaLanguage::CodeCompletionOption option(opt, RuztaLanguage::CODE_COMPLETION_KIND_FUNCTION);
									r_result.insert(option.display, option);
								}
							}
						}
						r_arghint = _make_arguments_hint(info, p_argidx);
						return;
					}
#endif
				if (p_argidx < method_args) {
					const PropertyInfo& arg_info = info.arguments[p_argidx];
					if (arg_info.usage & (PROPERTY_USAGE_CLASS_IS_ENUM | PROPERTY_USAGE_CLASS_IS_BITFIELD)) {
						_find_enumeration_candidates(p_context, arg_info.class_name, r_result);
					}
				}

				if (p_argidx == 1 && p_call && ClassDB::is_parent_class(class_name, StringName("Tween")) && method == StringName("tween_property")) {
					// Get tweened objects properties.
					if (p_call->arguments.is_empty()) {
						base_type.kind = RuztaParser::DataType::UNRESOLVED;
						break;
					}
					RuztaParser::ExpressionNode* tweened_object = p_call->arguments[0];
					if (!tweened_object) {
						base_type.kind = RuztaParser::DataType::UNRESOLVED;
						break;
					}
					StringName native_type = tweened_object->datatype.native_type;
					switch (tweened_object->datatype.kind) {
						case RuztaParser::DataType::SCRIPT: {
							Ref<Script> script = tweened_object->datatype.script_type;
							native_type = script->get_instance_base_type();
							int n = 0;
							while (script.is_valid()) {
								// godot-cpp: get_script_property_list returns TypedArray<Dictionary>
								TypedArray<Dictionary> properties_array = script->get_script_property_list();
								for (int i = 0; i < properties_array.size(); i++) {
									PropertyInfo E = PropertyInfo::from_dict(properties_array[i]);
									if (E.usage & (PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_INTERNAL)) {
										continue;
									}
									String name = quote_string(E.name, quote_style);
									if (use_node_paths) {
										if (p_call->arguments.size() > p_argidx && p_call->arguments[p_argidx] && p_call->arguments[p_argidx]->type == RuztaParser::Node::LITERAL) {
											RuztaParser::LiteralNode* literal = static_cast<RuztaParser::LiteralNode*>(p_call->arguments[p_argidx]);
											if (literal->value.get_type() == Variant::STRING) {
												name = "^" + name;
											}
										} else {
											name = "^" + name;
										}
									}
									RuztaLanguage::CodeCompletionOption option(name, RuztaLanguage::CODE_COMPLETION_KIND_MEMBER, RuztaLanguage::CodeCompletionLocation::LOCATION_LOCAL + n);
									r_result.insert(option.display, option);
								}
								script = script->get_base_script();
								n++;
							}
						} break;
						case RuztaParser::DataType::CLASS: {
							RuztaParser::ClassNode* clss = tweened_object->datatype.class_type;
							native_type = clss->base_type.native_type;
							int n = 0;
							while (clss) {
								for (RuztaParser::ClassNode::Member member : clss->members) {
									if (member.type == RuztaParser::ClassNode::Member::VARIABLE) {
										String name = quote_string(member.get_name(), quote_style);
										if (use_node_paths) {
											if (p_call->arguments.size() > p_argidx && p_call->arguments[p_argidx] && p_call->arguments[p_argidx]->type == RuztaParser::Node::LITERAL) {
												RuztaParser::LiteralNode* literal = static_cast<RuztaParser::LiteralNode*>(p_call->arguments[p_argidx]);
												if (literal->value.get_type() == Variant::STRING) {
													name = "^" + name;
												}
											} else {
												name = "^" + name;
											}
										}
										RuztaLanguage::CodeCompletionOption option(name, RuztaLanguage::CODE_COMPLETION_KIND_MEMBER, RuztaLanguage::CodeCompletionLocation::LOCATION_LOCAL + n);
										r_result.insert(option.display, option);
									}
								}
								if (clss->base_type.kind == RuztaParser::DataType::Kind::CLASS) {
									clss = clss->base_type.class_type;
									n++;
								} else {
									native_type = clss->base_type.native_type;
									clss = nullptr;
								}
							}
						} break;
						default:
							break;
					}

				// TODO: ClassDB::get_property_list is not available in godot-cpp
#if 0
					List<PropertyInfo> properties;
					ClassDB::get_property_list(native_type, &properties);
					for (const PropertyInfo& E : properties) {
						if (E.usage & (PROPERTY_USAGE_SUBGROUP | PROPERTY_USAGE_GROUP | PROPERTY_USAGE_CATEGORY | PROPERTY_USAGE_INTERNAL)) {
							continue;
						}
						String name = quote_string(E.name, quote_style);
						if (use_node_paths) {
							if (p_call->arguments.size() > p_argidx && p_call->arguments[p_argidx] && p_call->arguments[p_argidx]->type == RuztaParser::Node::LITERAL) {
								RuztaParser::LiteralNode* literal = static_cast<RuztaParser::LiteralNode*>(p_call->arguments[p_argidx]);
								if (literal->value.get_type() == Variant::STRING) {
									name = "^" + name;
								}
							} else {
								name = "^" + name;
							}
						}
						RuztaLanguage::CodeCompletionOption option(name, RuztaLanguage::CODE_COMPLETION_KIND_MEMBER);
						r_result.insert(option.display, option);
					}
#endif
				}

				// TODO: ProjectSettings::get_property_list is not available in godot-cpp
#if 0
				if (p_argidx == 0 && ClassDB::is_parent_class(class_name, StringName("Node")) && (method == StringName("get_node") || method == StringName("has_node"))) {
					// Get autoloads
					List<PropertyInfo> props;
					ProjectSettings::get_singleton()->get_property_list(&props);

					for (const PropertyInfo& E : props) {
						String s = E.name;
						if (!s.begins_with("autoload/")) {
							continue;
						}
						String name = s.get_slicec('/', 1);
						String path = quote_string("/root/" + name, quote_style);
						if (use_node_paths) {
							if (p_call->arguments.size() > p_argidx && p_call->arguments[p_argidx] && p_call->arguments[p_argidx]->type == RuztaParser::Node::LITERAL) {
								RuztaParser::LiteralNode* literal = static_cast<RuztaParser::LiteralNode*>(p_call->arguments[p_argidx]);
								if (literal->value.get_type() == Variant::STRING) {
									path = "^" + path;
								}
							} else {
								path = "^" + path;
							}
						}
						RuztaLanguage::CodeCompletionOption option(path, RuztaLanguage::CODE_COMPLETION_KIND_NODE_PATH);
						r_result.insert(option.display, option);
					}
				}
#endif

				// TODO: ProjectSettings::get_property_list is not available in godot-cpp
#if 0
				if (p_argidx == 0 && method_args > 0 && ClassDB::is_parent_class(class_name, StringName("InputEvent")) && String(method).find("action") != -1) {
					// Get input actions
					List<PropertyInfo> props;
					ProjectSettings::get_singleton()->get_property_list(&props);
					for (const PropertyInfo& E : props) {
						String s = E.name;
						if (!s.begins_with("input/")) {
							continue;
						}
						String name = quote_string(s.get_slicec('/', 1), quote_style);
						if (use_string_names) {
							if (p_call->arguments.size() > p_argidx && p_call->arguments[p_argidx] && p_call->arguments[p_argidx]->type == RuztaParser::Node::LITERAL) {
								RuztaParser::LiteralNode* literal = static_cast<RuztaParser::LiteralNode*>(p_call->arguments[p_argidx]);
								if (literal->value.get_type() == Variant::STRING) {
									name = "&" + name;
								}
							} else {
								name = "&" + name;
							}
						}
						RuztaLanguage::CodeCompletionOption option(name, RuztaLanguage::CODE_COMPLETION_KIND_CONSTANT);
						r_result.insert(option.display, option);
					}
				}
#endif
				if (RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/completion/complete_file_paths")) {
					if (p_argidx == 0 && method == StringName("change_scene_to_file") && ClassDB::is_parent_class(class_name, StringName("SceneTree"))) {
						HashMap<String, RuztaLanguage::CodeCompletionOption> list;
#if 0  // TODO: Fix EditorFileSystem for GDExtension
						_get_directory_contents(EditorFileSystem::get_singleton()->get_filesystem(), list, StringName("PackedScene"));
#endif
						for (const KeyValue<String, RuztaLanguage::CodeCompletionOption>& key_value_pair : list) {
							RuztaLanguage::CodeCompletionOption option = key_value_pair.value;
							r_result.insert(option.display, option);
						}
					}
				}

				base_type.kind = RuztaParser::DataType::UNRESOLVED;
			} break;
			case RuztaParser::DataType::BUILTIN: {
				if (base.get_type() == Variant::NIL) {
					GDExtensionCallError err;
					RuztaVariantExtension::construct(base_type.builtin_type, base, nullptr, 0, err);
					if (err.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
						return;
					}
				}

				List<MethodInfo> methods;
				RuztaVariantExtension::get_method_list(&base, &methods);
				for (const MethodInfo& E : methods) {
					if (E.name == method) {
						r_arghint = _make_arguments_hint(E, p_argidx);
						return;
					}
				}

				base_type.kind = RuztaParser::DataType::UNRESOLVED;
			} break;
			default: {
				base_type.kind = RuztaParser::DataType::UNRESOLVED;
			} break;
		}
	}
}

static bool _get_subscript_type(RuztaParser::CompletionContext& p_context, const RuztaParser::SubscriptNode* p_subscript, RuztaParser::DataType& r_base_type, Variant* r_base = nullptr) {
	if (p_context.base == nullptr) {
		return false;
	}

	const RuztaParser::GetNodeNode* get_node = nullptr;

	switch (p_subscript->base->type) {
		case RuztaParser::Node::GET_NODE: {
			get_node = static_cast<RuztaParser::GetNodeNode*>(p_subscript->base);
		} break;

		case RuztaParser::Node::IDENTIFIER: {
			const RuztaParser::IdentifierNode* identifier_node = static_cast<RuztaParser::IdentifierNode*>(p_subscript->base);

			switch (identifier_node->source) {
				case RuztaParser::IdentifierNode::Source::MEMBER_VARIABLE: {
					if (p_context.current_class != nullptr) {
						const StringName& member_name = identifier_node->name;
						const RuztaParser::ClassNode* current_class = p_context.current_class;

						if (current_class->has_member(member_name)) {
							const RuztaParser::ClassNode::Member& member = current_class->get_member(member_name);

							if (member.type == RuztaParser::ClassNode::Member::VARIABLE) {
								const RuztaParser::VariableNode* variable = static_cast<RuztaParser::VariableNode*>(member.variable);

								if (variable->initializer && variable->initializer->type == RuztaParser::Node::GET_NODE) {
									get_node = static_cast<RuztaParser::GetNodeNode*>(variable->initializer);
								}
							}
						}
					}
				} break;
				case RuztaParser::IdentifierNode::Source::LOCAL_VARIABLE: {
					// TODO: Do basic assignment flow analysis like in `_guess_expression_type`.
					const RuztaParser::SuiteNode::Local local = identifier_node->suite->get_local(identifier_node->name);
					switch (local.type) {
						case RuztaParser::SuiteNode::Local::CONSTANT: {
							if (local.constant->initializer && local.constant->initializer->type == RuztaParser::Node::GET_NODE) {
								get_node = static_cast<RuztaParser::GetNodeNode*>(local.constant->initializer);
							}
						} break;
						case RuztaParser::SuiteNode::Local::VARIABLE: {
							if (local.variable->initializer && local.variable->initializer->type == RuztaParser::Node::GET_NODE) {
								get_node = static_cast<RuztaParser::GetNodeNode*>(local.variable->initializer);
							}
						} break;
						default: {
						} break;
					}
				} break;
				default: {
				} break;
			}
		} break;
		default: {
		} break;
	}

	if (get_node != nullptr) {
		const Object* node = p_context.base->call("get_node_or_null", NodePath(get_node->full_path));
		if (node != nullptr) {
			RuztaParser::DataType assigned_type = _type_from_variant(node, p_context).type;
			RuztaParser::DataType base_type = p_subscript->base->datatype;

			if (p_subscript->base->type == RuztaParser::Node::IDENTIFIER && base_type.type_source == RuztaParser::DataType::ANNOTATED_EXPLICIT && (assigned_type.kind != base_type.kind || assigned_type.script_path != base_type.script_path || assigned_type.native_type != base_type.native_type)) {
				// Annotated type takes precedence.
				return false;
			}

			if (r_base != nullptr) {
				*r_base = node;
			}

			r_base_type.type_source = RuztaParser::DataType::INFERRED;
			r_base_type.builtin_type = Variant::OBJECT;
			r_base_type.native_type = node->get_class();

			Ref<Script> scr = node->get_script();
			if (scr.is_null()) {
				r_base_type.kind = RuztaParser::DataType::NATIVE;
			} else {
				r_base_type.kind = RuztaParser::DataType::SCRIPT;
				r_base_type.script_type = scr;
			}

			return true;
		}
	}

	return false;
}

static void _find_call_arguments(RuztaParser::CompletionContext& p_context, const RuztaParser::Node* p_call, int p_argidx, HashMap<String, RuztaLanguage::CodeCompletionOption>& r_result, bool& r_forced, String& r_arghint) {
	if (p_call->type == RuztaParser::Node::PRELOAD) {
		// TODO: EditorFileSystem::get_singleton is not available in godot-cpp
#if 0
			if (p_argidx == 0 && bool(RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/completion/complete_file_paths"))) {
				_get_directory_contents(EditorFileSystem::get_singleton()->get_filesystem(), r_result);
			}
#endif

		MethodInfo mi(PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "Resource"), "preload", PropertyInfo(Variant::STRING, "path"));
		r_arghint = _make_arguments_hint(mi, p_argidx);
		return;
	} else if (p_call->type != RuztaParser::Node::CALL) {
		return;
	}

	Variant base;
	RuztaParser::DataType base_type;
	bool _static = false;
	const RuztaParser::CallNode* call = static_cast<const RuztaParser::CallNode*>(p_call);
	RuztaParser::Node::Type callee_type = call->get_callee_type();

	if (callee_type == RuztaParser::Node::SUBSCRIPT) {
		const RuztaParser::SubscriptNode* subscript = static_cast<const RuztaParser::SubscriptNode*>(call->callee);

		if (subscript->base != nullptr && subscript->base->type == RuztaParser::Node::IDENTIFIER) {
			const RuztaParser::IdentifierNode* base_identifier = static_cast<const RuztaParser::IdentifierNode*>(subscript->base);

			Variant::Type method_type = RuztaParser::get_builtin_type(base_identifier->name);
			if (method_type < Variant::VARIANT_MAX) {
				Variant v;
				GDExtensionCallError err;
				RuztaVariantExtension::construct(method_type, v, nullptr, 0, err);
				if (err.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
					return;
				}
				List<MethodInfo> methods;
				RuztaVariantExtension::get_method_list(&v, &methods);

				for (MethodInfo& E : methods) {
					if (p_argidx >= E.arguments.size()) {
						continue;
					}
					if (E.name == call->function_name) {
						r_arghint += _make_arguments_hint(E, p_argidx);
						return;
					}
				}
			}
		}

		if (subscript->is_attribute) {
			bool found_type = _get_subscript_type(p_context, subscript, base_type, &base);

			if (!found_type) {
				RuztaCompletionIdentifier ci;
				if (_guess_expression_type(p_context, subscript->base, ci)) {
					base_type = ci.type;
					base = ci.value;
				} else {
					return;
				}
			}

			_static = base_type.is_meta_type;
		}
	} else if (RuztaVariantExtension::has_utility_function(call->function_name)) {
		MethodInfo info = RuztaVariantExtension::get_utility_function_info(call->function_name);
		r_arghint = _make_arguments_hint(info, p_argidx);
		return;
	} else if (RuztaUtilityFunctions::function_exists(call->function_name)) {
		MethodInfo info = RuztaUtilityFunctions::get_function_info(call->function_name);
		r_arghint = _make_arguments_hint(info, p_argidx);
		return;
	} else if (RuztaParser::get_builtin_type(call->function_name) < Variant::VARIANT_MAX) {
		// Complete constructor.
		List<MethodInfo> constructors;
		RuztaVariantExtension::get_constructor_list(RuztaParser::get_builtin_type(call->function_name), &constructors);

		int i = 0;
		for (const MethodInfo& E : constructors) {
			if (p_argidx >= E.arguments.size()) {
				continue;
			}
			if (i > 0) {
				r_arghint += "\n";
			}
			r_arghint += _make_arguments_hint(E, p_argidx);
			i++;
		}
		return;
	} else if (call->is_super || callee_type == RuztaParser::Node::IDENTIFIER) {
		base = p_context.base;

		if (p_context.current_class) {
			base_type = p_context.current_class->get_datatype();
			_static = !p_context.current_function || p_context.current_function->is_static;
		}
	} else {
		return;
	}

	RuztaCompletionIdentifier ci;
	ci.type = base_type;
	ci.value = base;
	_list_call_arguments(p_context, ci, call, p_argidx, _static, r_result, r_arghint);

	r_forced = r_result.size() > 0;
}

#else  // !TOOLS_ENABLED

Dictionary RuztaLanguage::_complete_code(const String& p_code, const String& p_path, Object* p_owner) const {
	// TODO: Implement code completion for godot-cpp
	// The Dictionary should contain:
	// - "result": Array of completion options (each as a Dictionary with keys like "display", "insert_text", "kind", etc.)
	// - "forced": bool indicating if completion should be forced
	// - "call_hint": String with function call hint
	// 
	// This requires adapting the TOOLS_ENABLED implementation above to work with godot-cpp's completion API
	return Dictionary();
}


#endif	// TOOLS_ENABLED

//////// END COMPLETION //////////

String RuztaLanguage::_get_indentation() const {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		bool use_space_indentation = RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/behavior/indent/type");

		if (use_space_indentation) {
			int indent_size = RuztaEditorPlugin::get_editor_settings()->get_setting("text_editor/behavior/indent/size");
			return String(" ").repeat(indent_size);
		}
	}
#endif
	return "\t";
}

String RuztaLanguage::_auto_indent_code(const String& p_code, int32_t p_from_line, int32_t p_to_line) const {
	String indent = _get_indentation();

	PackedStringArray lines = p_code.split("\n");
	List<int> indent_stack;

	for (int i = 0; i < lines.size(); i++) {
		String l = lines[i];
		int tc = 0;
		for (int j = 0; j < l.length(); j++) {
			if (l[j] == ' ' || l[j] == '\t') {
				tc++;
			} else {
				break;
			}
		}

		String st = l.substr(tc).strip_edges();
		if (st.is_empty() || st.begins_with("#")) {
			continue;  // ignore!
		}

		int ilevel = 0;
		if (indent_stack.size()) {
			ilevel = indent_stack.back()->get();
		}

		if (tc > ilevel) {
			indent_stack.push_back(tc);
		} else if (tc < ilevel) {
			while (indent_stack.size() && indent_stack.back()->get() > tc) {
				indent_stack.pop_back();
			}

			if (indent_stack.size() && indent_stack.back()->get() != tc) {
				indent_stack.push_back(tc);	 // this is not right but gets the job done
			}
		}

		if (i >= p_from_line) {
			l = indent.repeat(indent_stack.size()) + st;
		} else if (i > p_to_line) {
			break;
		}

		lines[i] = l;
	}

	String new_code = "";
	for (int i = 0; i < lines.size(); i++) {
		if (i > 0) {
			new_code += "\n";
		}
		new_code += lines[i];
	}
	return new_code;
}

#ifdef TOOLS_ENABLED

static Error _lookup_symbol_from_base(const RuztaParser::DataType& p_base, const String& p_symbol, Dictionary& r_result) {
	RuztaParser::DataType base_type = p_base;

	while (true) {
		switch (base_type.kind) {
			case RuztaParser::DataType::CLASS: {
				ERR_FAIL_NULL_V(base_type.class_type, ERR_BUG);

				String name = p_symbol;
				if (name == "new") {
					name = "_init";
				}

				if (!base_type.class_type->has_member(name)) {
					base_type = base_type.class_type->base_type;
					break;
				}

				const RuztaParser::ClassNode::Member& member = base_type.class_type->get_member(name);

				switch (member.type) {
					case RuztaParser::ClassNode::Member::UNDEFINED:
					case RuztaParser::ClassNode::Member::GROUP:
						return ERR_BUG;
					case RuztaParser::ClassNode::Member::CLASS: {
						String doc_type_name;
						String doc_enum_name;
						RuztaDocGen::doctype_from_gdtype(RuztaAnalyzer::type_from_metatype(member.get_datatype()), doc_type_name, doc_enum_name);

						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS;
						r_result["class_name"] = doc_type_name;
					} break;
					case RuztaParser::ClassNode::Member::CONSTANT:
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
						break;
					case RuztaParser::ClassNode::Member::FUNCTION:
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_METHOD;
						break;
					case RuztaParser::ClassNode::Member::SIGNAL:
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_SIGNAL;
						break;
					case RuztaParser::ClassNode::Member::VARIABLE:
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_PROPERTY;
						break;
					case RuztaParser::ClassNode::Member::ENUM:
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_ENUM;
						break;
					case RuztaParser::ClassNode::Member::ENUM_VALUE:
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
						break;
				}

				if (member.type != RuztaParser::ClassNode::Member::CLASS) {
					String doc_type_name;
					String doc_enum_name;
					RuztaDocGen::doctype_from_gdtype(RuztaAnalyzer::type_from_metatype(base_type), doc_type_name, doc_enum_name);

					r_result["class_name"] = doc_type_name;
					r_result["class_member"] = name;
				}

				Error err = OK;
				r_result["script"] = RuztaCache::get_shallow_script(base_type.script_path, err);
				r_result["script_path"] = base_type.script_path;
				r_result["location"] = member.get_line();
				return err;
			} break;
			// case RuztaParser::DataType::SCRIPT: {
			// 	const Ref<Script> scr = base_type.script_type;

			// 	if (scr.is_null()) {
			// 		return ERR_CANT_RESOLVE;
			// 	}

			// 	String name = p_symbol;
			// 	if (name == "new") {
			// 		name = "_init";
			// 	}

			// 	const int line = scr->get_member_line(name);
			// 	if (line >= 0) {
			// 		bool found_type = false;
			// 		r_result["type"] = RuztaLanguage::LOOKUP_RESULT_SCRIPT_LOCATION;
			// 		{
			// 			TypedArray<Dictionary> properties = scr->get_script_property_list();
			// 			for (const Dictionary& property_dict : properties) {
			// 				PropertyInfo property = PropertyInfo::from_dict(property_dict);
			// 				if (property.name == name && (property.usage & PROPERTY_USAGE_SCRIPT_VARIABLE)) {
			// 					found_type = true;
			// 					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_PROPERTY;
			// 					r_result["class_name"] = scr->_get_doc_class_name();
			// 					r_result["class_member"] = name;
			// 					break;
			// 				}
			// 			}
			// 		}
			// 		if (!found_type) {
			// 			godot::TypedArray<Dictionary> methods = scr->get_script_method_list();
			// 			for (const Dictionary& info : methods) {
			// 				MethodInfo method = MethodInfo::from_dict(info);
			// 				if (method.name == name) {
			// 					found_type = true;
			// 					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_METHOD;
			// 					r_result["class_name"] = scr->_get_doc_class_name();
			// 					r_result["class_member"] = name;
			// 					break;
			// 				}
			// 			}
			// 		}
			// 		if (!found_type) {
			// 			godot::TypedArray<Dictionary> methods = scr->get_script_method_list();
			// 			for (const Dictionary& info : methods) {
			// 				MethodInfo signal = MethodInfo::from_dict(info);
			// 				if (signal.name == name) {
			// 					found_type = true;
			// 					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_SIGNAL;
			// 					r_result["class_name"] = scr->_get_doc_class_name();
			// 					r_result["class_member"] = name;
			// 					break;
			// 				}
			// 			}
			// 		}
			// 		if (!found_type) {
			// 			const Ref<Ruzta> gds = scr;
			// 			if (gds.is_valid()) {
			// 				const Ref<Ruzta>* subclass = gds->get_subclasses().getptr(name);
			// 				if (subclass != nullptr) {
			// 					found_type = true;
			// 					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS;
			// 					r_result["class_name"] = subclass->ptr()->_get_doc_class_name();
			// 				}
			// 				// TODO: enums.
			// 			}
			// 		}
			// 		if (!found_type) {
			// 			HashMap<StringName, Variant> constants;
			// 			scr->get_constants(&constants);
			// 			if (constants.has(name)) {
			// 				found_type = true;
			// 				r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
			// 				r_result["class_name"] = scr->_get_doc_class_name();
			// 				r_result["class_member"] = name;
			// 			}
			// 		}

			// 		r_result["script"] = scr;
			// 		r_result["script_path"] = base_type.script_path;
			// 		r_result["location"] = line;
			// 		return OK;
			// 	}

			// 	const Ref<Script> base_script = scr->get_base_script();
			// 	if (base_script.is_valid()) {
			// 		base_type.script_type = base_script;
			// 	} else {
			// 		base_type.kind = RuztaParser::DataType::NATIVE;
			// 		base_type.builtin_type = Variant::OBJECT;
			// 		base_type.native_type = scr->get_instance_base_type();
			// 	}
			// } break;
			case RuztaParser::DataType::NATIVE: {
				const StringName& class_name = base_type.native_type;

				ERR_FAIL_COND_V(!ClassDB::class_exists(class_name), ERR_BUG);

				if (ClassDB::class_has_method(class_name, p_symbol, true)) {
					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_METHOD;
					r_result["class_name"] = class_name;
					r_result["class_member"] = p_symbol;
					return OK;
				}

				// TODO: ClassDB::get_virtual_methods is not available in godot-cpp
#if 0
				List<MethodInfo> virtual_methods;
				ClassDB::get_virtual_methods(class_name, &virtual_methods, true);
				for (const MethodInfo& E : virtual_methods) {
					if (E.name == p_symbol) {
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_METHOD;
						r_result["class_name"] = class_name;
						r_result["class_member"] = p_symbol;
						return OK;
					}
				}
#endif

				if (ClassDB::class_has_signal(class_name, p_symbol)) {
					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_SIGNAL;
					r_result["class_name"] = class_name;
					r_result["class_member"] = p_symbol;
					return OK;
				}

				List<StringName> enums;
				ClassDB::class_get_enum_list(class_name, &enums);
				for (const StringName& E : enums) {
					if (E == p_symbol) {
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_ENUM;
						r_result["class_name"] = class_name;
						r_result["class_member"] = p_symbol;
						return OK;
					}
				}

				if (!String(ClassDB::class_get_integer_constant_enum(class_name, p_symbol, true)).is_empty()) {
					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
					r_result["class_name"] = class_name;
					r_result["class_member"] = p_symbol;
					return OK;
				}

				PackedStringArray constants = ClassDB::class_get_integer_constant_list(class_name, true);
				for (const String& E : constants) {
					if (E == p_symbol) {
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
						r_result["class_name"] = class_name;
						r_result["class_member"] = p_symbol;
						return OK;
					}
				}

				if (ClassDB_has_property(class_name, p_symbol, true)) {
					PropertyInfo prop_info;
					bool found_prop = false;
					for (Dictionary prop_dict : ClassDB::class_get_property_list(class_name, true)) {
						if (prop_dict.has("name") && prop_dict["name"] == p_symbol) {
							prop_info = PropertyInfo::from_dict(prop_dict);
							found_prop = true;
						}
					}
					if (prop_info.usage & PROPERTY_USAGE_INTERNAL) {
						return ERR_CANT_RESOLVE;
					}

					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_PROPERTY;
					r_result["class_name"] = class_name;
					r_result["class_member"] = p_symbol;
					return OK;
				}

				const StringName parent_class = ClassDB::get_parent_class(class_name);
				if (parent_class != StringName()) {
					base_type.native_type = parent_class;
				} else {
					return ERR_CANT_RESOLVE;
				}
			} break;
			case RuztaParser::DataType::BUILTIN: {
				if (base_type.is_meta_type) {
					if (RuztaVariantExtension::has_enum(base_type.builtin_type, p_symbol)) {
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_ENUM;
						r_result["class_name"] = Variant::get_type_name(base_type.builtin_type);
						r_result["class_member"] = p_symbol;
						return OK;
					}

					if (RuztaVariantExtension::has_constant(base_type.builtin_type, p_symbol)) {
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
						r_result["class_name"] = Variant::get_type_name(base_type.builtin_type);
						r_result["class_member"] = p_symbol;
						return OK;
					}
				} else {
					if (RuztaVariantExtension::has_member(base_type.builtin_type, p_symbol)) {
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_PROPERTY;
						r_result["class_name"] = Variant::get_type_name(base_type.builtin_type);
						r_result["class_member"] = p_symbol;
						return OK;
					}
				}

				if (RuztaVariantExtension::has_builtin_method(base_type.builtin_type, p_symbol)) {
					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_METHOD;
					r_result["class_name"] = Variant::get_type_name(base_type.builtin_type);
					r_result["class_member"] = p_symbol;
					return OK;
				}

				return ERR_CANT_RESOLVE;
			} break;
			case RuztaParser::DataType::ENUM: {
				if (base_type.is_meta_type) {
					if (base_type.enum_values.has(p_symbol)) {
						String doc_type_name;
						String doc_enum_name;
						RuztaDocGen::doctype_from_gdtype(RuztaAnalyzer::type_from_metatype(base_type), doc_type_name, doc_enum_name);

						if (ClassDB::class_has_integer_constant("@GlobalScope", doc_enum_name)) {  // Replaced CoreConstants::is_global_enum
							r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
							r_result["class_name"] = "@GlobalScope";
							r_result["class_member"] = p_symbol;
							return OK;
						} else {
							const int dot_pos = doc_enum_name.rfind(String("."));
							if (dot_pos >= 0) {
								Error err = OK;
								r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
								if (base_type.class_type != nullptr) {
									// For script enums the value isn't accessible as class constant so we need the full enum name.
									r_result["class_name"] = doc_enum_name;
									r_result["class_member"] = p_symbol;
									r_result["script"] = RuztaCache::get_shallow_script(base_type.script_path, err);
									r_result["script_path"] = base_type.script_path;
									const String enum_name = doc_enum_name.substr(dot_pos + 1);
									if (base_type.class_type->has_member(enum_name)) {
										const RuztaParser::ClassNode::Member member = base_type.class_type->get_member(enum_name);
										if (member.type == RuztaParser::ClassNode::Member::ENUM) {
											for (const RuztaParser::EnumNode::Value& value : member.m_enum->values) {
												if (value.identifier->name == p_symbol) {
													r_result["location"] = value.line;
													break;
												}
											}
										}
									}
								} else if (base_type.script_type.is_valid()) {
									// For script enums the value isn't accessible as class constant so we need the full enum name.
									r_result["class_name"] = doc_enum_name;
									r_result["class_member"] = p_symbol;
									r_result["script"] = base_type.script_type;
									r_result["script_path"] = base_type.script_path;
									// TODO: Script::get_member_line is not available in godot-cpp
					r_result["location"] = -1;  // Fallback: location unavailable
					// r_result["location"] = base_type.script_type->get_member_line(doc_enum_name.substr(dot_pos + 1));
								} else {
									r_result["class_name"] = doc_enum_name.left(dot_pos);
									r_result["class_member"] = p_symbol;
								}
								return err;
							}
						}
					} else if (RuztaVariantExtension::has_builtin_method(Variant::DICTIONARY, p_symbol)) {
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_METHOD;
						r_result["class_name"] = "Dictionary";
						r_result["class_member"] = p_symbol;
						return OK;
					}
				}

				return ERR_CANT_RESOLVE;
			} break;
			case RuztaParser::DataType::VARIANT: {
				if (base_type.is_meta_type) {
					const String enum_name = "Variant." + p_symbol;
					if (CoreConstants::is_global_enum(enum_name)) {
						r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_ENUM;
						r_result["class_name"] = "@GlobalScope";
						r_result["class_member"] = enum_name;
						return OK;
					}
				}

				return ERR_CANT_RESOLVE;
			} break;
			case RuztaParser::DataType::RESOLVING:
			case RuztaParser::DataType::UNRESOLVED: {
				return ERR_CANT_RESOLVE;
			} break;
		}
	}

	return ERR_CANT_RESOLVE;
}

Dictionary RuztaLanguage::_lookup_code(const String& p_code, const String& p_symbol, const String& p_path, Object* p_owner) const {
	Dictionary r_result;

	// Before parsing, try the usual stuff.
	if (ClassDB::class_exists(p_symbol)) {
		r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS;
		r_result["class_name"] = p_symbol;
		return r_result;
	}

	if (RuztaVariantExtension::get_type_by_name(p_symbol) < Variant::VARIANT_MAX) {
		r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS;
		r_result["class_name"] = p_symbol;
		return r_result;
	}

	if (p_symbol == "Variant") {
		r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS;
		r_result["class_name"] = "Variant";
		return r_result;
	}

	if (p_symbol == "PI" || p_symbol == "TAU" || p_symbol == "INF" || p_symbol == "NAN") {
		r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
		r_result["class_name"] = "@Ruzta";
		r_result["class_member"] = p_symbol;
		return r_result;
	}

	RuztaParser parser;
	parser.parse(p_code, p_path, true);

	RuztaParser::CompletionContext context = parser.get_completion_context();
	context.base = p_owner;

	// Allows class functions with the names like built-ins to be handled properly.
	if (context.type != RuztaParser::COMPLETION_ATTRIBUTE) {
		// Need special checks for `assert` and `preload` as they are technically
		// keywords, so are not registered in `RuztaUtilityFunctions`.
		if (RuztaUtilityFunctions::function_exists(p_symbol) || p_symbol == "assert" || p_symbol == "preload") {
			r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_METHOD;
			r_result["class_name"] = "@Ruzta";
			r_result["class_member"] = p_symbol;
			return r_result;
		}
	}

	RuztaAnalyzer analyzer(&parser);
	analyzer.analyze();

	if (context.current_class && context.current_class->extends.size() > 0) {
		StringName class_name = context.current_class->extends[0]->name;

		bool success = ClassDB::class_has_integer_constant(class_name, p_symbol);
		if (success) {
			r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
			r_result["class_name"] = class_name;
			r_result["class_member"] = p_symbol;
			return r_result;
		}
		do {
			PackedStringArray enums = ClassDB::class_get_enum_list(class_name, true);
			for (const StringName& enum_name : enums) {
				if (enum_name == p_symbol) {
					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_ENUM;
					r_result["class_name"] = class_name;
					r_result["class_member"] = p_symbol;
					return r_result;
				}
			}
			class_name = ClassDB::get_parent_class(class_name);
		} while (class_name != StringName());
	}

	const RuztaParser::TypeNode* type_node = dynamic_cast<const RuztaParser::TypeNode*>(context.node);
	if (type_node != nullptr && !type_node->type_chain.is_empty()) {
		StringName class_name = type_node->type_chain[0]->name;
		if (RuztaScriptServer::is_global_class(class_name)) {
			class_name = RuztaScriptServer::get_global_class_native_base(class_name);
		}
		do {
			PackedStringArray enums = ClassDB::class_get_enum_list(class_name, true);
			for (const StringName& enum_name : enums) {
				if (enum_name == p_symbol) {
					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_ENUM;
					r_result["class_name"] = class_name;
					r_result["class_member"] = p_symbol;
					return r_result;
				}
			}
			class_name = ClassDB::get_parent_class(class_name);
		} while (class_name != StringName());
	}

	bool is_function = false;

	switch (context.type) {
		case RuztaParser::COMPLETION_BUILT_IN_TYPE_CONSTANT_OR_STATIC_METHOD: {
			RuztaParser::DataType base_type;
			base_type.kind = RuztaParser::DataType::BUILTIN;
			base_type.builtin_type = context.builtin_type;
			base_type.is_meta_type = true;
			if (_lookup_symbol_from_base(base_type, p_symbol, r_result) == OK) {
				return r_result;
			}
		} break;
		case RuztaParser::COMPLETION_SUPER: {
			if (context.current_class && context.current_function) {
				if (_lookup_symbol_from_base(context.current_class->base_type, context.current_function->info.name, r_result) == OK) {
					return r_result;
				}
			}
		} break;
		case RuztaParser::COMPLETION_SUPER_METHOD:
		case RuztaParser::COMPLETION_METHOD:
		case RuztaParser::COMPLETION_ASSIGN:
		case RuztaParser::COMPLETION_CALL_ARGUMENTS:
		case RuztaParser::COMPLETION_IDENTIFIER:
		case RuztaParser::COMPLETION_PROPERTY_METHOD:
		case RuztaParser::COMPLETION_SUBSCRIPT: {
			RuztaParser::DataType base_type;
			if (context.current_class) {
				if (context.type != RuztaParser::COMPLETION_SUPER_METHOD) {
					base_type = context.current_class->get_datatype();
				} else {
					base_type = context.current_class->base_type;
				}
			} else {
				break;
			}

			if (!is_function && context.current_suite) {
				// Lookup local variables.
				const RuztaParser::SuiteNode* suite = context.current_suite;
				while (suite) {
					if (suite->has_local(p_symbol)) {
						const RuztaParser::SuiteNode::Local& local = suite->get_local(p_symbol);

						switch (local.type) {
							case RuztaParser::SuiteNode::Local::UNDEFINED:
								return Dictionary();
							case RuztaParser::SuiteNode::Local::CONSTANT:
								r_result["type"] = RuztaLanguage::LOOKUP_RESULT_LOCAL_CONSTANT;
								r_result["description"] = local.constant->doc_data.description;
								r_result["is_deprecated"] = local.constant->doc_data.is_deprecated;
								r_result["deprecated_message"] = local.constant->doc_data.deprecated_message;
								r_result["is_experimental"] = local.constant->doc_data.is_experimental;
								r_result["experimental_message"] = local.constant->doc_data.experimental_message;
								if (local.constant->initializer != nullptr) {
									r_result["value"] = RuztaDocGen::docvalue_from_expression(local.constant->initializer);
								}
								break;
							case RuztaParser::SuiteNode::Local::VARIABLE:
								r_result["type"] = RuztaLanguage::LOOKUP_RESULT_LOCAL_VARIABLE;
								r_result["description"] = local.variable->doc_data.description;
								r_result["is_deprecated"] = local.variable->doc_data.is_deprecated;
								r_result["deprecated_message"] = local.variable->doc_data.deprecated_message;
								r_result["is_experimental"] = local.variable->doc_data.is_experimental;
								r_result["experimental_message"] = local.variable->doc_data.experimental_message;
								if (local.variable->initializer != nullptr) {
									r_result["value"] = RuztaDocGen::docvalue_from_expression(local.variable->initializer);
								}
								break;
							case RuztaParser::SuiteNode::Local::PARAMETER:
							case RuztaParser::SuiteNode::Local::FOR_VARIABLE:
							case RuztaParser::SuiteNode::Local::PATTERN_BIND:
								r_result["type"] = RuztaLanguage::LOOKUP_RESULT_LOCAL_VARIABLE;
								break;
						}

						String doc_type, enumeration;
						RuztaDocGen::doctype_from_gdtype(local.get_datatype(), doc_type, enumeration);
						r_result["doc_type"] = doc_type;
						r_result["enumeration"] = enumeration;

						Error err = OK;
						r_result["script"] = RuztaCache::get_shallow_script(base_type.script_path, err);
						r_result["script_path"] = base_type.script_path;
						r_result["location"] = local.start_line;
						return r_result;
					}
					suite = suite->parent_block;
				}
			}

			if (_lookup_symbol_from_base(base_type, p_symbol, r_result) == OK) {
				return r_result;
			}

			if (!is_function) {
				// TODO: ProjectSettings autoload methods are not available in godot-cpp
#if 0
				if (ProjectSettings::get_singleton()->has_autoload(p_symbol)) {
					const ProjectSettings::AutoloadInfo& autoload = ProjectSettings::get_singleton()->get_autoload(p_symbol);
					if (autoload.is_singleton) {
						String scr_path = autoload.path;
						if (!scr_path.ends_with(".rz")) {
							// Not a script, try find the script anyway, may have some success.
							scr_path = scr_path.get_basename() + ".rz";
						}

						if (FileAccess::file_exists(scr_path)) {
							r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS;
							r_result["class_name"] = p_symbol;
							r_result["script"] = ResourceLoader::get_singleton()->load(scr_path);
							r_result["script_path"] = scr_path;
							r_result["location"] = 0;
							return r_result;
						}
					}
				}
#endif

				if (RuztaScriptServer::is_global_class(p_symbol)) {
					const String scr_path = RuztaScriptServer::get_global_class_path(p_symbol);
					const Ref<Script> scr = ResourceLoader::get_singleton()->load(scr_path);
					if (scr.is_null()) {
						return Dictionary();
					}
					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS;
					r_result["class_name"] = scr->get_global_name();
					r_result["script"] = scr;
					r_result["script_path"] = scr_path;
					r_result["location"] = 0;
					return r_result;
				}

				const HashMap<StringName, int>& global_map = RuztaLanguage::get_singleton()->get_global_map();
				if (global_map.has(p_symbol)) {
					Variant value = RuztaLanguage::get_singleton()->get_global_array()[global_map[p_symbol]];
					if (value.get_type() == Variant::OBJECT) {
						const Object* obj = value;
						if (obj) {
							if (Object::cast_to<RuztaNativeClass>(obj)) {
								r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS;
								r_result["class_name"] = Object::cast_to<RuztaNativeClass>(obj)->get_name();
							} else {
								r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS;
								r_result["class_name"] = obj->get_class();
							}
							return r_result;
						}
					}
				}

				if (CoreConstants::is_global_enum(p_symbol)) {
					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_ENUM;
					r_result["class_name"] = "@GlobalScope";
					r_result["class_member"] = p_symbol;
					return r_result;
				}

				if (CoreConstants::is_global_constant(p_symbol)) {
					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_CONSTANT;
					r_result["class_name"] = "@GlobalScope";
					r_result["class_member"] = p_symbol;
					return r_result;
				}

				if (RuztaVariantExtension::has_utility_function(p_symbol)) {
					r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_METHOD;
					r_result["class_name"] = "@GlobalScope";
					r_result["class_member"] = p_symbol;
					return r_result;
				}
			}
		} break;
		case RuztaParser::COMPLETION_ATTRIBUTE_METHOD:
		case RuztaParser::COMPLETION_ATTRIBUTE: {
			if (context.node->type != RuztaParser::Node::SUBSCRIPT) {
				break;
			}
			const RuztaParser::SubscriptNode* subscript = static_cast<const RuztaParser::SubscriptNode*>(context.node);
			if (!subscript->is_attribute) {
				break;
			}
			RuztaCompletionIdentifier base;

			bool found_type = _get_subscript_type(context, subscript, base.type);
			if (!found_type && !_guess_expression_type(context, subscript->base, base)) {
				break;
			}

			if (_lookup_symbol_from_base(base.type, p_symbol, r_result) == OK) {
				return r_result;
			}
		} break;
		case RuztaParser::COMPLETION_TYPE_ATTRIBUTE: {
			if (context.node == nullptr || context.node->type != RuztaParser::Node::TYPE) {
				break;
			}
			const RuztaParser::TypeNode* type = static_cast<const RuztaParser::TypeNode*>(context.node);

			RuztaParser::DataType base_type;
			const RuztaParser::IdentifierNode* prev = nullptr;
			for (const RuztaParser::IdentifierNode* E : type->type_chain) {
				if (E->name == p_symbol && prev != nullptr) {
					base_type = prev->get_datatype();
					break;
				}
				prev = E;
			}
			if (base_type.kind != RuztaParser::DataType::CLASS) {
				RuztaCompletionIdentifier base;
				if (!_guess_expression_type(context, prev, base)) {
					break;
				}
				base_type = base.type;
			}

			if (_lookup_symbol_from_base(base_type, p_symbol, r_result) == OK) {
				return r_result;
			}
		} break;
		case RuztaParser::COMPLETION_OVERRIDE_METHOD: {
			RuztaParser::DataType base_type = context.current_class->base_type;

			if (_lookup_symbol_from_base(base_type, p_symbol, r_result) == OK) {
				return r_result;
			}
		} break;
		case RuztaParser::COMPLETION_PROPERTY_DECLARATION_OR_TYPE:
		case RuztaParser::COMPLETION_TYPE_NAME_OR_VOID:
		case RuztaParser::COMPLETION_TYPE_NAME: {
			RuztaParser::DataType base_type = context.current_class->get_datatype();

			if (_lookup_symbol_from_base(base_type, p_symbol, r_result) == OK) {
				return r_result;
			}
		} break;
		case RuztaParser::COMPLETION_ANNOTATION: {
			const String annotation_symbol = "@" + p_symbol;
			if (parser.annotation_exists(annotation_symbol)) {
				r_result["type"] = RuztaLanguage::LOOKUP_RESULT_CLASS_ANNOTATION;
				r_result["class_name"] = "@Ruzta";
				r_result["class_member"] = annotation_symbol;
				return r_result;
			}
		} break;
		default: {
		}
	}

	return Dictionary();
}

#endif	// TOOLS_ENABLED
