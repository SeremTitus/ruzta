/**************************************************************************/
/*  ruzta_compiler.h                                                   */
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

#include "ruzta.h"
#include "ruzta_codegen.h"
#include "ruzta_function.h"
#include "ruzta_parser.h"

#include <godot_cpp/templates/hash_set.hpp> // original: core/templates/hash_set.h

class RuztaCompiler {
	const RuztaParser *parser = nullptr;
	HashSet<Ruzta *> parsed_classes;
	HashSet<Ruzta *> parsing_classes;
	Ruzta *main_script = nullptr;

	struct FunctionLambdaInfo {
		RuztaFunction *function = nullptr;
		RuztaFunction *parent = nullptr;
		Ruzta *script = nullptr;
		StringName name;
		int line = 0;
		int index = 0;
		int depth = 0;
		//uint64_t code_hash;
		//int code_size;
		int capture_count = 0;
		bool use_self = false;
		int arg_count = 0;
		int default_arg_count = 0;
		//Vector<RuztaDataType> argument_types;
		//RuztaDataType return_type;
		Vector<FunctionLambdaInfo> sublambdas;
	};

	struct ScriptLambdaInfo {
		Vector<FunctionLambdaInfo> implicit_initializer_info;
		Vector<FunctionLambdaInfo> implicit_ready_info;
		Vector<FunctionLambdaInfo> static_initializer_info;
		HashMap<StringName, Vector<FunctionLambdaInfo>> member_function_infos;
		Vector<FunctionLambdaInfo> other_function_infos;
		HashMap<StringName, ScriptLambdaInfo> subclass_info;
	};

	struct CodeGen {
		Ruzta *script = nullptr;
		const RuztaParser::ClassNode *class_node = nullptr;
		const RuztaParser::FunctionNode *function_node = nullptr;
		StringName function_name;
		RuztaCodeGenerator *generator = nullptr;
		HashMap<StringName, RuztaCodeGenerator::Address> parameters;
		HashMap<StringName, RuztaCodeGenerator::Address> locals;
		List<HashMap<StringName, RuztaCodeGenerator::Address>> locals_stack;
		bool is_static = false;

		RuztaCodeGenerator::Address add_local(const StringName &p_name, const RuztaDataType &p_type) {
			uint32_t addr = generator->add_local(p_name, p_type);
			locals[p_name] = RuztaCodeGenerator::Address(RuztaCodeGenerator::Address::LOCAL_VARIABLE, addr, p_type);
			return locals[p_name];
		}

		RuztaCodeGenerator::Address add_local_constant(const StringName &p_name, const Variant &p_value) {
			uint32_t addr = generator->add_local_constant(p_name, p_value);
			locals[p_name] = RuztaCodeGenerator::Address(RuztaCodeGenerator::Address::CONSTANT, addr);
			return locals[p_name];
		}

		RuztaCodeGenerator::Address add_temporary(const RuztaDataType &p_type = RuztaDataType()) {
			uint32_t addr = generator->add_temporary(p_type);
			return RuztaCodeGenerator::Address(RuztaCodeGenerator::Address::TEMPORARY, addr, p_type);
		}

		RuztaCodeGenerator::Address add_constant(const Variant &p_constant) {
			RuztaDataType type;
			type.kind = RuztaDataType::BUILTIN;
			type.builtin_type = p_constant.get_type();
			if (type.builtin_type == Variant::OBJECT) {
				Object *obj = p_constant;
				if (obj) {
					type.kind = RuztaDataType::NATIVE;
					type.native_type = obj->get_class();

					Ref<Script> scr = obj->get_script();
					if (scr.is_valid()) {
						type.script_type = scr.ptr();
						Ref<Ruzta> ruzta = scr;
						if (ruzta.is_valid()) {
							type.kind = RuztaDataType::RUZTA;
						} else {
							type.kind = RuztaDataType::SCRIPT;
						}
					}
				} else {
					type.builtin_type = Variant::NIL;
				}
			}

			uint32_t addr = generator->add_or_get_constant(p_constant);
			return RuztaCodeGenerator::Address(RuztaCodeGenerator::Address::CONSTANT, addr, type);
		}

		void start_block() {
			HashMap<StringName, RuztaCodeGenerator::Address> old_locals = locals;
			locals_stack.push_back(old_locals);
			generator->start_block();
		}

		void end_block() {
			locals = locals_stack.back()->get();
			locals_stack.pop_back();
			generator->end_block();
		}
	};

	bool _is_class_member_property(CodeGen &codegen, const StringName &p_name);
	bool _is_class_member_property(Ruzta *owner, const StringName &p_name);
	bool _is_local_or_parameter(CodeGen &codegen, const StringName &p_name);

	void _set_error(const String &p_error, const RuztaParser::Node *p_node);

	RuztaDataType _gdtype_from_datatype(const RuztaParser::DataType &p_datatype, Ruzta *p_owner, bool p_handle_metatype = true);

	RuztaCodeGenerator::Address _parse_expression(CodeGen &codegen, Error &r_error, const RuztaParser::ExpressionNode *p_expression, bool p_root = false, bool p_initializer = false);
	RuztaCodeGenerator::Address _parse_match_pattern(CodeGen &codegen, Error &r_error, const RuztaParser::PatternNode *p_pattern, const RuztaCodeGenerator::Address &p_value_addr, const RuztaCodeGenerator::Address &p_type_addr, const RuztaCodeGenerator::Address &p_previous_test, bool p_is_first, bool p_is_nested);
	List<RuztaCodeGenerator::Address> _add_block_locals(CodeGen &codegen, const RuztaParser::SuiteNode *p_block);
	void _clear_block_locals(CodeGen &codegen, const List<RuztaCodeGenerator::Address> &p_locals);
	Error _parse_block(CodeGen &codegen, const RuztaParser::SuiteNode *p_block, bool p_add_locals = true, bool p_clear_locals = true);
	RuztaFunction *_parse_function(Error &r_error, Ruzta *p_script, const RuztaParser::ClassNode *p_class, const RuztaParser::FunctionNode *p_func, bool p_for_ready = false, bool p_for_lambda = false);
	RuztaFunction *_make_static_initializer(Error &r_error, Ruzta *p_script, const RuztaParser::ClassNode *p_class);
	Error _parse_setter_getter(Ruzta *p_script, const RuztaParser::ClassNode *p_class, const RuztaParser::VariableNode *p_variable, bool p_is_setter);
	Error _prepare_compilation(Ruzta *p_script, const RuztaParser::ClassNode *p_class, bool p_keep_state);
	Error _compile_class(Ruzta *p_script, const RuztaParser::ClassNode *p_class, bool p_keep_state);
	FunctionLambdaInfo _get_function_replacement_info(RuztaFunction *p_func, int p_index = -1, int p_depth = 0, RuztaFunction *p_parent_func = nullptr);
	Vector<FunctionLambdaInfo> _get_function_lambda_replacement_info(RuztaFunction *p_func, int p_depth = 0, RuztaFunction *p_parent_func = nullptr);
	ScriptLambdaInfo _get_script_lambda_replacement_info(Ruzta *p_script);
	bool _do_function_infos_match(const FunctionLambdaInfo &p_old_info, const FunctionLambdaInfo *p_new_info);
	void _get_function_ptr_replacements(HashMap<RuztaFunction *, RuztaFunction *> &r_replacements, const FunctionLambdaInfo &p_old_info, const FunctionLambdaInfo *p_new_info);
	void _get_function_ptr_replacements(HashMap<RuztaFunction *, RuztaFunction *> &r_replacements, const Vector<FunctionLambdaInfo> &p_old_infos, const Vector<FunctionLambdaInfo> *p_new_infos);
	void _get_function_ptr_replacements(HashMap<RuztaFunction *, RuztaFunction *> &r_replacements, const ScriptLambdaInfo &p_old_info, const ScriptLambdaInfo *p_new_info);
	int err_line = 0;
	int err_column = 0;
	StringName source;
	String error;
	RuztaParser::ExpressionNode *awaited_node = nullptr;
	bool has_static_data = false;

public:
	static void convert_to_initializer_type(Variant &p_variant, const RuztaParser::VariableNode *p_node);
	static void make_scripts(Ruzta *p_script, const RuztaParser::ClassNode *p_class, bool p_keep_state);
	Error compile(const RuztaParser *p_parser, Ruzta *p_script, bool p_keep_state = false);

	String get_error() const;
	int get_error_line() const;
	int get_error_column() const;

	RuztaCompiler();
};
