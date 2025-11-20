/**************************************************************************/
/*  ruzta_analyzer.h                                                   */
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

#pragma once

#include "ruzta_cache.h"
#include "ruzta_parser.h"

#include <godot_cpp/classes/object.hpp> // original: core/object/object.h
#include <godot_cpp/classes/ref_counted.hpp> // original: core/object/ref_counted.h

class RuztaAnalyzer {
	RuztaParser *parser = nullptr;

	template <typename Fn>
	class Finally {
		Fn fn;

	public:
		Finally(Fn p_fn) :
				fn(p_fn) {}
		~Finally() {
			fn();
		}
	};

	const RuztaParser::EnumNode *current_enum = nullptr;
	RuztaParser::LambdaNode *current_lambda = nullptr;
	List<RuztaParser::LambdaNode *> pending_body_resolution_lambdas;
	HashMap<const RuztaParser::ClassNode *, Ref<RuztaParserRef>> external_class_parser_cache;
	bool static_context = false;

	// Tests for detecting invalid overloading of script members
	static _FORCE_INLINE_ bool has_member_name_conflict_in_script_class(const StringName &p_name, const RuztaParser::ClassNode *p_current_class_node, const RuztaParser::Node *p_member);
	static _FORCE_INLINE_ bool has_member_name_conflict_in_native_type(const StringName &p_name, const StringName &p_native_type_string);
	Error check_native_member_name_conflict(const StringName &p_member_name, const RuztaParser::Node *p_member_node, const StringName &p_native_type_string);
	Error check_class_member_name_conflict(const RuztaParser::ClassNode *p_class_node, const StringName &p_member_name, const RuztaParser::Node *p_member_node);

	void get_class_node_current_scope_classes(RuztaParser::ClassNode *p_node, List<RuztaParser::ClassNode *> *p_list, RuztaParser::Node *p_source);

	Error resolve_class_inheritance(RuztaParser::ClassNode *p_class, const RuztaParser::Node *p_source = nullptr);
	Error resolve_class_inheritance(RuztaParser::ClassNode *p_class, bool p_recursive);
	RuztaParser::DataType resolve_datatype(RuztaParser::TypeNode *p_type);

	void decide_suite_type(RuztaParser::Node *p_suite, RuztaParser::Node *p_statement);

	void resolve_annotation(RuztaParser::AnnotationNode *p_annotation);
	void resolve_class_member(RuztaParser::ClassNode *p_class, const StringName &p_name, const RuztaParser::Node *p_source = nullptr);
	void resolve_class_member(RuztaParser::ClassNode *p_class, int p_index, const RuztaParser::Node *p_source = nullptr);
	void resolve_class_interface(RuztaParser::ClassNode *p_class, const RuztaParser::Node *p_source = nullptr);
	void resolve_class_interface(RuztaParser::ClassNode *p_class, bool p_recursive);
	void resolve_class_body(RuztaParser::ClassNode *p_class, const RuztaParser::Node *p_source = nullptr);
	void resolve_class_body(RuztaParser::ClassNode *p_class, bool p_recursive);
	void resolve_function_signature(RuztaParser::FunctionNode *p_function, const RuztaParser::Node *p_source = nullptr, bool p_is_lambda = false);
	void resolve_function_body(RuztaParser::FunctionNode *p_function, bool p_is_lambda = false);
	void resolve_node(RuztaParser::Node *p_node, bool p_is_root = true);
	void resolve_suite(RuztaParser::SuiteNode *p_suite);
	void resolve_assignable(RuztaParser::AssignableNode *p_assignable, const char *p_kind);
	void resolve_variable(RuztaParser::VariableNode *p_variable, bool p_is_local);
	void resolve_constant(RuztaParser::ConstantNode *p_constant, bool p_is_local);
	void resolve_parameter(RuztaParser::ParameterNode *p_parameter);
	void resolve_if(RuztaParser::IfNode *p_if);
	void resolve_for(RuztaParser::ForNode *p_for);
	void resolve_while(RuztaParser::WhileNode *p_while);
	void resolve_assert(RuztaParser::AssertNode *p_assert);
	void resolve_match(RuztaParser::MatchNode *p_match);
	void resolve_match_branch(RuztaParser::MatchBranchNode *p_match_branch, RuztaParser::ExpressionNode *p_match_test);
	void resolve_match_pattern(RuztaParser::PatternNode *p_match_pattern, RuztaParser::ExpressionNode *p_match_test);
	void resolve_return(RuztaParser::ReturnNode *p_return);

	// Reduction functions.
	void reduce_expression(RuztaParser::ExpressionNode *p_expression, bool p_is_root = false);
	void reduce_array(RuztaParser::ArrayNode *p_array);
	void reduce_assignment(RuztaParser::AssignmentNode *p_assignment);
	void reduce_await(RuztaParser::AwaitNode *p_await);
	void reduce_binary_op(RuztaParser::BinaryOpNode *p_binary_op);
	void reduce_call(RuztaParser::CallNode *p_call, bool p_is_await = false, bool p_is_root = false);
	void reduce_cast(RuztaParser::CastNode *p_cast);
	void reduce_dictionary(RuztaParser::DictionaryNode *p_dictionary);
	void reduce_get_node(RuztaParser::GetNodeNode *p_get_node);
	void reduce_identifier(RuztaParser::IdentifierNode *p_identifier, bool can_be_builtin = false);
	void reduce_identifier_from_base(RuztaParser::IdentifierNode *p_identifier, RuztaParser::DataType *p_base = nullptr);
	void reduce_lambda(RuztaParser::LambdaNode *p_lambda);
	void reduce_literal(RuztaParser::LiteralNode *p_literal);
	void reduce_preload(RuztaParser::PreloadNode *p_preload);
	void reduce_self(RuztaParser::SelfNode *p_self);
	void reduce_subscript(RuztaParser::SubscriptNode *p_subscript, bool p_can_be_pseudo_type = false);
	void reduce_ternary_op(RuztaParser::TernaryOpNode *p_ternary_op, bool p_is_root = false);
	void reduce_type_test(RuztaParser::TypeTestNode *p_type_test);
	void reduce_unary_op(RuztaParser::UnaryOpNode *p_unary_op);

	Variant make_expression_reduced_value(RuztaParser::ExpressionNode *p_expression, bool &is_reduced);
	Variant make_array_reduced_value(RuztaParser::ArrayNode *p_array, bool &is_reduced);
	Variant make_dictionary_reduced_value(RuztaParser::DictionaryNode *p_dictionary, bool &is_reduced);
	Variant make_subscript_reduced_value(RuztaParser::SubscriptNode *p_subscript, bool &is_reduced);
	Variant make_call_reduced_value(RuztaParser::CallNode *p_call, bool &is_reduced);

	// Helpers.
	Array make_array_from_element_datatype(const RuztaParser::DataType &p_element_datatype, const RuztaParser::Node *p_source_node = nullptr);
	Dictionary make_dictionary_from_element_datatype(const RuztaParser::DataType &p_key_element_datatype, const RuztaParser::DataType &p_value_element_datatype, const RuztaParser::Node *p_source_node = nullptr);
	RuztaParser::DataType type_from_variant(const Variant &p_value, const RuztaParser::Node *p_source);
	RuztaParser::DataType type_from_property(const PropertyInfo &p_property, bool p_is_arg = false, bool p_is_readonly = false) const;
	RuztaParser::DataType make_global_class_meta_type(const StringName &p_class_name, const RuztaParser::Node *p_source);
	bool get_function_signature(RuztaParser::Node *p_source, bool p_is_constructor, RuztaParser::DataType base_type, const StringName &p_function, RuztaParser::DataType &r_return_type, List<RuztaParser::DataType> &r_par_types, int &r_default_arg_count, BitField<MethodFlags> &r_method_flags, StringName *r_native_class = nullptr);
	bool function_signature_from_info(const MethodInfo &p_info, RuztaParser::DataType &r_return_type, List<RuztaParser::DataType> &r_par_types, int &r_default_arg_count, BitField<MethodFlags> &r_method_flags);
	void validate_call_arg(const List<RuztaParser::DataType> &p_par_types, int p_default_args_count, bool p_is_vararg, const RuztaParser::CallNode *p_call);
	void validate_call_arg(const MethodInfo &p_method, const RuztaParser::CallNode *p_call);
	RuztaParser::DataType get_operation_type(Variant::Operator p_operation, const RuztaParser::DataType &p_a, const RuztaParser::DataType &p_b, bool &r_valid, const RuztaParser::Node *p_source);
	RuztaParser::DataType get_operation_type(Variant::Operator p_operation, const RuztaParser::DataType &p_a, bool &r_valid, const RuztaParser::Node *p_source);
	void update_const_expression_builtin_type(RuztaParser::ExpressionNode *p_expression, const RuztaParser::DataType &p_type, const char *p_usage, bool p_is_cast = false);
	void update_array_literal_element_type(RuztaParser::ArrayNode *p_array, const RuztaParser::DataType &p_element_type);
	void update_dictionary_literal_element_type(RuztaParser::DictionaryNode *p_dictionary, const RuztaParser::DataType &p_key_element_type, const RuztaParser::DataType &p_value_element_type);
	bool is_type_compatible(const RuztaParser::DataType &p_target, const RuztaParser::DataType &p_source, bool p_allow_implicit_conversion = false, const RuztaParser::Node *p_source_node = nullptr);
	void push_error(const String &p_message, const RuztaParser::Node *p_origin = nullptr);
	void mark_node_unsafe(const RuztaParser::Node *p_node);
	void downgrade_node_type_source(RuztaParser::Node *p_node);
	void mark_lambda_use_self();
	void resolve_pending_lambda_bodies();
	bool class_exists(const StringName &p_class) const;
	void reduce_identifier_from_base_set_class(RuztaParser::IdentifierNode *p_identifier, RuztaParser::DataType p_identifier_datatype);
	Ref<RuztaParserRef> ensure_cached_external_parser_for_class(const RuztaParser::ClassNode *p_class, const RuztaParser::ClassNode *p_from_class, const char *p_context, const RuztaParser::Node *p_source);
	Ref<RuztaParserRef> find_cached_external_parser_for_class(const RuztaParser::ClassNode *p_class, const Ref<RuztaParserRef> &p_dependant_parser);
	Ref<RuztaParserRef> find_cached_external_parser_for_class(const RuztaParser::ClassNode *p_class, RuztaParser *p_dependant_parser);
	Ref<Ruzta> get_depended_shallow_script(const String &p_path, Error &r_error);
#ifdef DEBUG_ENABLED
	void is_shadowing(RuztaParser::IdentifierNode *p_identifier, const String &p_context, const bool p_in_local_scope);
#endif

public:
	Error resolve_inheritance();
	Error resolve_interface();
	Error resolve_body();
	Error resolve_dependencies();
	Error analyze();

	Variant make_variable_default_value(RuztaParser::VariableNode *p_variable);

	static bool check_type_compatibility(const RuztaParser::DataType &p_target, const RuztaParser::DataType &p_source, bool p_allow_implicit_conversion = false, const RuztaParser::Node *p_source_node = nullptr);
	static RuztaParser::DataType type_from_metatype(const RuztaParser::DataType &p_meta_type);

	RuztaAnalyzer(RuztaParser *p_parser);
};
