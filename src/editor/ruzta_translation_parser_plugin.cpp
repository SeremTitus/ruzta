/**************************************************************************/
/*  ruzta_translation_parser_plugin.cpp                                */
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

#include "ruzta_translation_parser_plugin.h"

#include "../ruzta.h"
#include "../ruzta_analyzer.h"

#include <godot_cpp/classes/resource_loader.hpp> // original: core/io/resource_loader.h

void RuztaEditorTranslationParserPlugin::get_recognized_extensions(List<String> *r_extensions) const {
	RuztaLanguage::get_singleton()->get_recognized_extensions(r_extensions);
}

Error RuztaEditorTranslationParserPlugin::parse_file(const String &p_path, Vector<Vector<String>> *r_translations) {
	// Extract all translatable strings using the parsed tree from RuztaParser.
	// The strategy is to find all ExpressionNode and AssignmentNode from the tree and extract strings if relevant, i.e
	// Search strings in ExpressionNode -> CallNode -> tr(), set_text(), set_placeholder() etc.
	// Search strings in AssignmentNode -> text = "__", tooltip_text = "__" etc.

	Error err;
	Ref<Resource> loaded_res = ResourceLoader::load(p_path, "", ResourceFormatLoader::CACHE_MODE_REUSE, &err);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to load " + p_path);

	translations = r_translations;

	Ref<Ruzta> ruzta = loaded_res;
	String source_code = ruzta->get_source_code();

	RuztaParser parser;
	err = parser.parse(source_code, p_path, false);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to parse Ruzta with RuztaParser.");

	RuztaAnalyzer analyzer(&parser);
	err = analyzer.analyze();
	ERR_FAIL_COND_V_MSG(err, err, "Failed to analyze Ruzta with RuztaAnalyzer.");

	comment_data = &parser.comment_data;

	// Traverse through the parsed tree from RuztaParser.
	RuztaParser::ClassNode *c = parser.get_tree();
	_traverse_class(c);

	comment_data = nullptr;

	return OK;
}

bool RuztaEditorTranslationParserPlugin::_is_constant_string(const RuztaParser::ExpressionNode *p_expression) {
	ERR_FAIL_NULL_V(p_expression, false);
	return p_expression->is_constant && p_expression->reduced_value.is_string();
}

String RuztaEditorTranslationParserPlugin::_parse_comment(int p_line, bool &r_skip) const {
	// Parse inline comment.
	if (comment_data->has(p_line)) {
		const String stripped_comment = comment_data->get(p_line).comment.trim_prefix("#").strip_edges();

		if (stripped_comment.begins_with("TRANSLATORS:")) {
			return stripped_comment.trim_prefix("TRANSLATORS:").strip_edges(true, false);
		}
		if (stripped_comment == "NO_TRANSLATE" || stripped_comment.begins_with("NO_TRANSLATE:")) {
			r_skip = true;
			return String();
		}
	}

	// Parse multiline comment.
	String multiline_comment;
	for (int line = p_line - 1; comment_data->has(line) && comment_data->get(line).new_line; line--) {
		const String stripped_comment = comment_data->get(line).comment.trim_prefix("#").strip_edges();

		if (stripped_comment.is_empty()) {
			continue;
		}

		if (multiline_comment.is_empty()) {
			multiline_comment = stripped_comment;
		} else {
			multiline_comment = stripped_comment + "\n" + multiline_comment;
		}

		if (stripped_comment.begins_with("TRANSLATORS:")) {
			return multiline_comment.trim_prefix("TRANSLATORS:").strip_edges(true, false);
		}
		if (stripped_comment == "NO_TRANSLATE" || stripped_comment.begins_with("NO_TRANSLATE:")) {
			r_skip = true;
			return String();
		}
	}

	return String();
}

void RuztaEditorTranslationParserPlugin::_add_id(const String &p_id, int p_line) {
	bool skip = false;
	const String comment = _parse_comment(p_line, skip);
	if (skip) {
		return;
	}

	translations->push_back({ p_id, String(), String(), comment, itos(p_line) });
}

void RuztaEditorTranslationParserPlugin::_add_id_ctx_plural(const Vector<String> &p_id_ctx_plural, int p_line) {
	bool skip = false;
	const String comment = _parse_comment(p_line, skip);
	if (skip) {
		return;
	}

	translations->push_back({ p_id_ctx_plural[0], p_id_ctx_plural[1], p_id_ctx_plural[2], comment, itos(p_line) });
}

void RuztaEditorTranslationParserPlugin::_traverse_class(const RuztaParser::ClassNode *p_class) {
	for (int i = 0; i < p_class->members.size(); i++) {
		const RuztaParser::ClassNode::Member &m = p_class->members[i];
		// Other member types can't contain translatable strings.
		switch (m.type) {
			case RuztaParser::ClassNode::Member::CLASS:
				_traverse_class(m.m_class);
				break;
			case RuztaParser::ClassNode::Member::FUNCTION:
				_traverse_function(m.function);
				break;
			case RuztaParser::ClassNode::Member::VARIABLE:
				_assess_expression(m.variable->initializer);
				if (m.variable->property == RuztaParser::VariableNode::PROP_INLINE) {
					_traverse_function(m.variable->setter);
					_traverse_function(m.variable->getter);
				}
				break;
			default:
				break;
		}
	}
}

void RuztaEditorTranslationParserPlugin::_traverse_function(const RuztaParser::FunctionNode *p_func) {
	if (!p_func) {
		return;
	}

	for (int i = 0; i < p_func->parameters.size(); i++) {
		_assess_expression(p_func->parameters[i]->initializer);
	}
	_traverse_block(p_func->body);
}

void RuztaEditorTranslationParserPlugin::_traverse_block(const RuztaParser::SuiteNode *p_suite) {
	if (!p_suite) {
		return;
	}

	const Vector<RuztaParser::Node *> &statements = p_suite->statements;
	for (int i = 0; i < statements.size(); i++) {
		const RuztaParser::Node *statement = statements[i];

		// BREAK, BREAKPOINT, CONSTANT, CONTINUE, and PASS are skipped because they can't contain translatable strings.
		switch (statement->type) {
			case RuztaParser::Node::ASSERT: {
				const RuztaParser::AssertNode *assert_node = static_cast<const RuztaParser::AssertNode *>(statement);
				_assess_expression(assert_node->condition);
				_assess_expression(assert_node->message);
			} break;
			case RuztaParser::Node::ASSIGNMENT: {
				_assess_assignment(static_cast<const RuztaParser::AssignmentNode *>(statement));
			} break;
			case RuztaParser::Node::FOR: {
				const RuztaParser::ForNode *for_node = static_cast<const RuztaParser::ForNode *>(statement);
				_assess_expression(for_node->list);
				_traverse_block(for_node->loop);
			} break;
			case RuztaParser::Node::IF: {
				const RuztaParser::IfNode *if_node = static_cast<const RuztaParser::IfNode *>(statement);
				_assess_expression(if_node->condition);
				_traverse_block(if_node->true_block);
				_traverse_block(if_node->false_block);
			} break;
			case RuztaParser::Node::MATCH: {
				const RuztaParser::MatchNode *match_node = static_cast<const RuztaParser::MatchNode *>(statement);
				_assess_expression(match_node->test);
				for (int j = 0; j < match_node->branches.size(); j++) {
					_traverse_block(match_node->branches[j]->guard_body);
					_traverse_block(match_node->branches[j]->block);
				}
			} break;
			case RuztaParser::Node::RETURN: {
				_assess_expression(static_cast<const RuztaParser::ReturnNode *>(statement)->return_value);
			} break;
			case RuztaParser::Node::VARIABLE: {
				_assess_expression(static_cast<const RuztaParser::VariableNode *>(statement)->initializer);
			} break;
			case RuztaParser::Node::WHILE: {
				const RuztaParser::WhileNode *while_node = static_cast<const RuztaParser::WhileNode *>(statement);
				_assess_expression(while_node->condition);
				_traverse_block(while_node->loop);
			} break;
			default: {
				if (statement->is_expression()) {
					_assess_expression(static_cast<const RuztaParser::ExpressionNode *>(statement));
				}
			} break;
		}
	}
}

void RuztaEditorTranslationParserPlugin::_assess_expression(const RuztaParser::ExpressionNode *p_expression) {
	// Explore all ExpressionNodes to find CallNodes which contain translation strings, such as tr(), set_text() etc.
	// tr() can be embedded quite deep within multiple ExpressionNodes so need to dig down to search through all ExpressionNodes.
	if (!p_expression) {
		return;
	}

	// GET_NODE, IDENTIFIER, LITERAL, PRELOAD, SELF, and TYPE are skipped because they can't contain translatable strings.
	switch (p_expression->type) {
		case RuztaParser::Node::ARRAY: {
			const RuztaParser::ArrayNode *array_node = static_cast<const RuztaParser::ArrayNode *>(p_expression);
			for (int i = 0; i < array_node->elements.size(); i++) {
				_assess_expression(array_node->elements[i]);
			}
		} break;
		case RuztaParser::Node::ASSIGNMENT: {
			_assess_assignment(static_cast<const RuztaParser::AssignmentNode *>(p_expression));
		} break;
		case RuztaParser::Node::AWAIT: {
			_assess_expression(static_cast<const RuztaParser::AwaitNode *>(p_expression)->to_await);
		} break;
		case RuztaParser::Node::BINARY_OPERATOR: {
			const RuztaParser::BinaryOpNode *binary_op_node = static_cast<const RuztaParser::BinaryOpNode *>(p_expression);
			_assess_expression(binary_op_node->left_operand);
			_assess_expression(binary_op_node->right_operand);
		} break;
		case RuztaParser::Node::CALL: {
			_assess_call(static_cast<const RuztaParser::CallNode *>(p_expression));
		} break;
		case RuztaParser::Node::CAST: {
			_assess_expression(static_cast<const RuztaParser::CastNode *>(p_expression)->operand);
		} break;
		case RuztaParser::Node::DICTIONARY: {
			const RuztaParser::DictionaryNode *dict_node = static_cast<const RuztaParser::DictionaryNode *>(p_expression);
			for (int i = 0; i < dict_node->elements.size(); i++) {
				_assess_expression(dict_node->elements[i].key);
				_assess_expression(dict_node->elements[i].value);
			}
		} break;
		case RuztaParser::Node::LAMBDA: {
			_traverse_function(static_cast<const RuztaParser::LambdaNode *>(p_expression)->function);
		} break;
		case RuztaParser::Node::SUBSCRIPT: {
			const RuztaParser::SubscriptNode *subscript_node = static_cast<const RuztaParser::SubscriptNode *>(p_expression);
			_assess_expression(subscript_node->base);
			if (!subscript_node->is_attribute) {
				_assess_expression(subscript_node->index);
			}
		} break;
		case RuztaParser::Node::TERNARY_OPERATOR: {
			const RuztaParser::TernaryOpNode *ternary_op_node = static_cast<const RuztaParser::TernaryOpNode *>(p_expression);
			_assess_expression(ternary_op_node->condition);
			_assess_expression(ternary_op_node->true_expr);
			_assess_expression(ternary_op_node->false_expr);
		} break;
		case RuztaParser::Node::TYPE_TEST: {
			_assess_expression(static_cast<const RuztaParser::TypeTestNode *>(p_expression)->operand);
		} break;
		case RuztaParser::Node::UNARY_OPERATOR: {
			_assess_expression(static_cast<const RuztaParser::UnaryOpNode *>(p_expression)->operand);
		} break;
		default: {
		} break;
	}
}

void RuztaEditorTranslationParserPlugin::_assess_assignment(const RuztaParser::AssignmentNode *p_assignment) {
	_assess_expression(p_assignment->assignee);
	_assess_expression(p_assignment->assigned_value);

	// Extract the translatable strings coming from assignments. For example, get_node("Label").text = "____"

	StringName assignee_name;
	if (p_assignment->assignee->type == RuztaParser::Node::IDENTIFIER) {
		assignee_name = static_cast<const RuztaParser::IdentifierNode *>(p_assignment->assignee)->name;
	} else if (p_assignment->assignee->type == RuztaParser::Node::SUBSCRIPT) {
		const RuztaParser::SubscriptNode *subscript = static_cast<const RuztaParser::SubscriptNode *>(p_assignment->assignee);
		if (subscript->is_attribute && subscript->attribute) {
			assignee_name = subscript->attribute->name;
		} else if (subscript->index && _is_constant_string(subscript->index)) {
			assignee_name = subscript->index->reduced_value;
		}
	}

	if (assignee_name != StringName() && assignment_patterns.has(assignee_name) && _is_constant_string(p_assignment->assigned_value)) {
		// If the assignment is towards one of the extract patterns (text, tooltip_text etc.), and the value is a constant string, we collect the string.
		_add_id(p_assignment->assigned_value->reduced_value, p_assignment->assigned_value->start_line);
	} else if (assignee_name == fd_filters) {
		// Extract from `get_node("FileDialog").filters = <filter array>`.
		_extract_fd_filter_array(p_assignment->assigned_value);
	}
}

void RuztaEditorTranslationParserPlugin::_assess_call(const RuztaParser::CallNode *p_call) {
	_assess_expression(p_call->callee);
	for (int i = 0; i < p_call->arguments.size(); i++) {
		_assess_expression(p_call->arguments[i]);
	}

	// Extract the translatable strings coming from function calls. For example:
	// tr("___"), get_node("Label").set_text("____"), get_node("LineEdit").set_placeholder("____").

	StringName function_name = p_call->function_name;

	// Variables for extracting tr() and tr_n().
	Vector<String> id_ctx_plural;
	id_ctx_plural.resize(3);
	bool extract_id_ctx_plural = true;

	if (function_name == tr_func || function_name == atr_func) {
		// Extract from `tr(id, ctx)` or `atr(id, ctx)`.
		for (int i = 0; i < p_call->arguments.size(); i++) {
			if (_is_constant_string(p_call->arguments[i])) {
				id_ctx_plural.write[i] = p_call->arguments[i]->reduced_value;
			} else {
				// Avoid adding something like tr("Flying dragon", var_context_level_1). We want to extract both id and context together.
				extract_id_ctx_plural = false;
			}
		}
		if (extract_id_ctx_plural) {
			_add_id_ctx_plural(id_ctx_plural, p_call->start_line);
		}
	} else if (function_name == trn_func || function_name == atrn_func) {
		// Extract from `tr_n(id, plural, n, ctx)` or `atr_n(id, plural, n, ctx)`.
		Vector<int> indices;
		indices.push_back(0);
		indices.push_back(3);
		indices.push_back(1);
		for (int i = 0; i < indices.size(); i++) {
			if (indices[i] >= p_call->arguments.size()) {
				continue;
			}

			if (_is_constant_string(p_call->arguments[indices[i]])) {
				id_ctx_plural.write[i] = p_call->arguments[indices[i]]->reduced_value;
			} else {
				extract_id_ctx_plural = false;
			}
		}
		if (extract_id_ctx_plural) {
			_add_id_ctx_plural(id_ctx_plural, p_call->start_line);
		}
	} else if (first_arg_patterns.has(function_name)) {
		if (!p_call->arguments.is_empty() && _is_constant_string(p_call->arguments[0])) {
			_add_id(p_call->arguments[0]->reduced_value, p_call->arguments[0]->start_line);
		}
	} else if (second_arg_patterns.has(function_name)) {
		if (p_call->arguments.size() > 1 && _is_constant_string(p_call->arguments[1])) {
			_add_id(p_call->arguments[1]->reduced_value, p_call->arguments[1]->start_line);
		}
	} else if (function_name == fd_add_filter) {
		if (p_call->arguments.size() == 1) {
			// The first parameter may contain a description, like `"*.jpg; JPEG Images"`.
			_extract_fd_filter_string(p_call->arguments[0], p_call->arguments[0]->start_line);
		} else if (p_call->arguments.size() >= 2) {
			// The second optional parameter can be a description.
			if (_is_constant_string(p_call->arguments[1])) {
				_add_id(p_call->arguments[1]->reduced_value, p_call->arguments[1]->start_line);
			}
		}
	} else if (function_name == fd_set_filter) {
		// Extract from `get_node("FileDialog").set_filters(<filter array>)`.
		if (!p_call->arguments.is_empty()) {
			_extract_fd_filter_array(p_call->arguments[0]);
		}
	}
}

void RuztaEditorTranslationParserPlugin::_extract_fd_filter_string(const RuztaParser::ExpressionNode *p_expression, int p_line) {
	// Extract the description from `"filter; Description"` format.
	// The description part is optional, so we skip if it's missing or empty.
	if (_is_constant_string(p_expression)) {
		const PackedStringArray arr = p_expression->reduced_value.operator String().split(";", true, 1);
		if (arr.size() >= 2) {
			const String description = arr[1].strip_edges();
			if (!description.is_empty()) {
				_add_id(description, p_line);
			}
		}
	}
}

void RuztaEditorTranslationParserPlugin::_extract_fd_filter_array(const RuztaParser::ExpressionNode *p_expression) {
	const RuztaParser::ArrayNode *array_node = nullptr;

	if (p_expression->type == RuztaParser::Node::ARRAY) {
		// Extract from `["*.png ; PNG Images","*.rz ; Ruzta Files"]` (implicit cast to `PackedStringArray`).
		array_node = static_cast<const RuztaParser::ArrayNode *>(p_expression);
	} else if (p_expression->type == RuztaParser::Node::CALL) {
		// Extract from `PackedStringArray(["*.png ; PNG Images","*.rz ; Ruzta Files"])`.
		const RuztaParser::CallNode *call_node = static_cast<const RuztaParser::CallNode *>(p_expression);
		if (call_node->get_callee_type() == RuztaParser::Node::IDENTIFIER && call_node->function_name == StringName("PackedStringArray") && !call_node->arguments.is_empty() && call_node->arguments[0]->type == RuztaParser::Node::ARRAY) {
			array_node = static_cast<const RuztaParser::ArrayNode *>(call_node->arguments[0]);
		}
	}

	if (array_node) {
		for (int i = 0; i < array_node->elements.size(); i++) {
			_extract_fd_filter_string(array_node->elements[i], array_node->elements[i]->start_line);
		}
	}
}

RuztaEditorTranslationParserPlugin::RuztaEditorTranslationParserPlugin() {
	assignment_patterns.insert("text");
	assignment_patterns.insert("placeholder_text");
	assignment_patterns.insert("tooltip_text");

	first_arg_patterns.insert("set_text");
	first_arg_patterns.insert("set_tooltip_text");
	first_arg_patterns.insert("set_placeholder");
	first_arg_patterns.insert("add_tab");
	first_arg_patterns.insert("add_check_item");
	first_arg_patterns.insert("add_item");
	first_arg_patterns.insert("add_multistate_item");
	first_arg_patterns.insert("add_radio_check_item");
	first_arg_patterns.insert("add_separator");
	first_arg_patterns.insert("add_submenu_item");

	second_arg_patterns.insert("set_tab_title");
	second_arg_patterns.insert("add_icon_check_item");
	second_arg_patterns.insert("add_icon_item");
	second_arg_patterns.insert("add_icon_radio_check_item");
	second_arg_patterns.insert("set_item_text");
}
