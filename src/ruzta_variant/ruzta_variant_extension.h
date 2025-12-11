/**************************************************************************/
/*  ruzta_variant_extension.h                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                RUZTA                                   */
/*                    https://seremtitus.co.ke/ruzta                      */
/**************************************************************************/
/* Copyright (c) 2025-present Ruzta contributors (see AUTHORS.md).  */
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

#include <godot_cpp/core/object.hpp>		   // original:
#include <godot_cpp/templates/a_hash_map.hpp>  // original:
#include <godot_cpp/templates/list.hpp>		   // original:
#include <godot_cpp/variant/variant.hpp>	   // original:

using namespace godot;

enum ErrorHandlerType {
	ERR_HANDLER_ERROR,
	ERR_HANDLER_WARNING,
	ERR_HANDLER_SCRIPT,
	ERR_HANDLER_SHADER,
};

class RuztaVariantExtension {
	static void _register_variant_operators();

	static void _register_variant_utility_functions();
	static void _unregister_variant_utility_functions();

	static void _register_variant_constructors();
	static void _unregister_variant_constructors();

	static void _register_variant_methods();
	static void _unregister_variant_methods();

	static void _register_variant_setters_getters();
	static void _unregister_variant_setters_getters();

   public:
	static void register_types() {
		_register_variant_operators();
		_register_variant_utility_functions();
		_register_variant_constructors();
		_register_variant_methods();
		_register_variant_setters_getters();
	}

	static void unregister_types() {
		_unregister_variant_utility_functions();
		_unregister_variant_constructors();
		_unregister_variant_methods();
		_unregister_variant_setters_getters();
	}
	
	// Ops
	typedef void (*ValidatedOperatorEvaluator)(const Variant* left, const Variant* right, Variant* r_ret);
	typedef void (*PTROperatorEvaluator)(const void* left, const void* right, void* r_ret);

	static String get_operator_name(Variant::Operator p_op);
	static void evaluate(const Variant::Operator& p_op, const Variant& p_a, const Variant& p_b, Variant& r_ret, bool& r_valid);
	static _FORCE_INLINE_ Variant evaluate(const Variant::Operator& p_op, const Variant& p_a, const Variant& p_b) {
		bool valid = true;
		Variant res;
		evaluate(p_op, p_a, p_b, res, valid);
		return res;
	}
	static Variant::Type get_operator_return_type(Variant::Operator p_operator, Variant::Type p_type_a, Variant::Type p_type_b);
	static ValidatedOperatorEvaluator get_validated_operator_evaluator(Variant::Operator p_operator, Variant::Type p_type_a, Variant::Type p_type_b);
	static PTROperatorEvaluator get_ptr_operator_evaluator(Variant::Operator p_operator, Variant::Type p_type_a, Variant::Type p_type_b);

	// Built-In Methods
	typedef void (*ValidatedBuiltInMethod)(Variant* base, const Variant** p_args, int p_argcount, Variant* r_ret);
	typedef void (*PTRBuiltInMethod)(void* p_base, const void** p_args, void* r_ret, int p_argcount);

	static bool has_builtin_method(Variant::Type p_type, const StringName& p_method);

	static ValidatedBuiltInMethod get_validated_builtin_method(Variant::Type p_type, const StringName& p_method);
	static PTRBuiltInMethod get_ptr_builtin_method(Variant::Type p_type, const StringName& p_method);
	static PTRBuiltInMethod get_ptr_builtin_method_with_compatibility(Variant::Type p_type, const StringName& p_method, uint32_t p_hash);

	static MethodInfo get_builtin_method_info(Variant::Type p_type, const StringName& p_method);
	static int get_builtin_method_argument_count(Variant::Type p_type, const StringName& p_method);
	static Variant::Type get_builtin_method_argument_type(Variant::Type p_type, const StringName& p_method, int p_argument);
	static String get_builtin_method_argument_name(Variant::Type p_type, const StringName& p_method, int p_argument);
	static Vector<Variant> get_builtin_method_default_arguments(Variant::Type p_type, const StringName& p_method);
	static bool has_builtin_method_return_value(Variant::Type p_type, const StringName& p_method);
	static Variant::Type get_builtin_method_return_type(Variant::Type p_type, const StringName& p_method);
	static bool is_builtin_method_const(Variant::Type p_type, const StringName& p_method);
	static bool is_builtin_method_static(Variant::Type p_type, const StringName& p_method);
	static bool is_builtin_method_vararg(Variant::Type p_type, const StringName& p_method);
	static void get_builtin_method_list(Variant::Type p_type, List<StringName>* p_list);
	static int get_builtin_method_count(Variant::Type p_type);
	static uint32_t get_builtin_method_hash(Variant::Type p_type, const StringName& p_method);
	static Vector<uint32_t> get_builtin_method_compatibility_hashes(Variant::Type p_type, const StringName& p_method);

	static String get_call_error_text(const StringName& p_method, const Variant** p_argptrs, int p_argcount, const GDExtensionCallError& ce);
	static String get_call_error_text(Object* p_base, const StringName& p_method, const Variant** p_argptrs, int p_argcount, const GDExtensionCallError& ce);
	static String get_callable_error_text(const Callable& p_callable, const Variant** p_argptrs, int p_argcount, const GDExtensionCallError& ce);

	// Utility Functions
	typedef void (*ValidatedUtilityFunction)(Variant* r_ret, const Variant** p_args, int p_argcount);
	typedef void (*PTRUtilityFunction)(void* r_ret, const void** p_args, int p_argcount);

	enum UtilityFunctionType {
		UTILITY_FUNC_TYPE_MATH,
		UTILITY_FUNC_TYPE_RANDOM,
		UTILITY_FUNC_TYPE_GENERAL,
	};

	static void call_utility_function(const StringName& p_name, Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error);
	static bool has_utility_function(const StringName& p_name);
	static ValidatedUtilityFunction get_validated_utility_function(const StringName& p_name);
	static PTRUtilityFunction get_ptr_utility_function(const StringName& p_name);
	static UtilityFunctionType get_utility_function_type(const StringName& p_name);
	static MethodInfo get_utility_function_info(const StringName& p_name);
	static int get_utility_function_argument_count(const StringName& p_name);
	static Variant::Type get_utility_function_argument_type(const StringName& p_name, int p_arg);
	static String get_utility_function_argument_name(const StringName& p_name, int p_arg);
	static bool has_utility_function_return_value(const StringName& p_name);
	static Variant::Type get_utility_function_return_type(const StringName& p_name);
	static bool is_utility_function_vararg(const StringName& p_name);
	static uint32_t get_utility_function_hash(const StringName& p_name);
	static void get_utility_function_list(List<StringName>* r_functions);
	static int get_utility_function_count();

	// Variant Constructors
	typedef void (*ValidatedConstructor)(Variant* r_base, const Variant** p_args);
	typedef void (*PTRConstructor)(void* base, const void** p_args);

	static void construct(Variant::Type, Variant& base, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error);
	static int get_constructor_count(Variant::Type p_type);
	static ValidatedConstructor get_validated_constructor(Variant::Type p_type, int p_constructor);
	static PTRConstructor get_ptr_constructor(Variant::Type p_type, int p_constructor);
	static int get_constructor_argument_count(Variant::Type p_type, int p_constructor);
	static Variant::Type get_constructor_argument_type(Variant::Type p_type, int p_constructor, int p_argument);
	static String get_constructor_argument_name(Variant::Type p_type, int p_constructor, int p_argument);
	static void get_constructor_list(Variant::Type p_type, List<MethodInfo>* r_list);

	// Calls
	static void get_method_list(Variant* base, List<MethodInfo>* p_list);
	static void get_property_list(Variant* base, List<PropertyInfo>* p_list);

	static Variant::Type get_type_by_name(const String& p_type_name) {
		static HashMap<String, Variant::Type> type_names;
		if (unlikely(type_names.is_empty())) {
			for (int i = 0; i < Variant::VARIANT_MAX; i++) {
				type_names[Variant::get_type_name((Variant::Type)i)] = (Variant::Type)i;
			}
		}

		const Variant::Type* ptr = type_names.getptr(p_type_name);
		return (ptr == nullptr) ? Variant::VARIANT_MAX : *ptr;
	}

	static bool is_type_shared(Variant::Type p_type) {
		switch (p_type) {
			case Variant::OBJECT:
			case Variant::ARRAY:
			case Variant::DICTIONARY:
				return true;
			default:
				break;
		}
		return false;
	}

	static void get_constants_for_type(Variant::Type p_type, List<StringName>* p_constants);
	static int get_constants_count_for_type(Variant::Type p_type);
	static bool has_constant(Variant::Type p_type, const StringName& p_value);
	static Variant get_constant_value(Variant::Type p_type, const StringName& p_value, bool* r_valid = nullptr);

	static void get_enums_for_type(Variant::Type p_type, List<StringName>* p_enums);
	static void get_enumerations_for_enum(Variant::Type p_type, const StringName& p_enum_name, List<StringName>* p_enumerations);
	static int get_enum_value(Variant::Type p_type, const StringName& p_enum_name, const StringName& p_enumeration, bool* r_valid = nullptr);
	static bool has_enum(Variant::Type p_type, const StringName& p_enum_name);
	static StringName get_enum_for_enumeration(Variant::Type p_type, const StringName& p_enumeration);

	// Properties
	typedef void (*ValidatedSetter)(Variant* base, const Variant* value);
	typedef void (*ValidatedGetter)(const Variant* base, Variant* value);

	static bool has_member(Variant::Type p_type, const StringName& p_member);
	static Variant::Type get_member_type(Variant::Type p_type, const StringName& p_member);
	static void get_member_list(Variant::Type p_type, List<StringName>* r_members);
	static int get_member_count(Variant::Type p_type);

	static ValidatedSetter get_member_validated_setter(Variant::Type p_type, const StringName& p_member);
	static ValidatedGetter get_member_validated_getter(Variant::Type p_type, const StringName& p_member);

	typedef void (*PTRSetter)(void* base, const void* value);
	typedef void (*PTRGetter)(const void* base, void* value);

	static PTRSetter get_member_ptr_setter(Variant::Type p_type, const StringName& p_member);
	static PTRGetter get_member_ptr_getter(Variant::Type p_type, const StringName& p_member);

	// Indexing
	static bool has_indexing(Variant::Type p_type);
	static Variant::Type get_indexed_element_type(Variant::Type p_type);
	static uint32_t get_indexed_element_usage(Variant::Type p_type);

	typedef void (*ValidatedIndexedSetter)(Variant* base, int64_t index, const Variant* value, bool* oob);
	typedef void (*ValidatedIndexedGetter)(const Variant* base, int64_t index, Variant* value, bool* oob);

	static ValidatedIndexedSetter get_member_validated_indexed_setter(Variant::Type p_type);
	static ValidatedIndexedGetter get_member_validated_indexed_getter(Variant::Type p_type);

	typedef void (*PTRIndexedSetter)(void* base, int64_t index, const void* value);
	typedef void (*PTRIndexedGetter)(const void* base, int64_t index, void* value);

	static PTRIndexedSetter get_member_ptr_indexed_setter(Variant::Type p_type);
	static PTRIndexedGetter get_member_ptr_indexed_getter(Variant::Type p_type);

	// Keying
	static bool is_keyed(Variant::Type p_type);

	typedef void (*ValidatedKeyedSetter)(Variant* base, const Variant* key, const Variant* value, bool* valid);
	typedef void (*ValidatedKeyedGetter)(const Variant* base, const Variant* key, Variant* value, bool* valid);
	typedef bool (*ValidatedKeyedChecker)(const Variant* base, const Variant* key, bool* valid);

	static ValidatedKeyedSetter get_member_validated_keyed_setter(Variant::Type p_type);
	static ValidatedKeyedGetter get_member_validated_keyed_getter(Variant::Type p_type);
	static ValidatedKeyedChecker get_member_validated_keyed_checker(Variant::Type p_type);

	typedef void (*PTRKeyedSetter)(void* base, const void* key, const void* value);
	typedef void (*PTRKeyedGetter)(const void* base, const void* key, void* value);
	typedef uint32_t (*PTRKeyedChecker)(const void* base, const void* key);

	static PTRKeyedSetter get_member_ptr_keyed_setter(Variant::Type p_type);
	static PTRKeyedGetter get_member_ptr_keyed_getter(Variant::Type p_type);
	static PTRKeyedChecker get_member_ptr_keyed_checker(Variant::Type p_type);
};

template <typename... VarArgs>
Vector<Variant> varray(VarArgs... p_args) {
	return Vector<Variant>{p_args...};
}

template <typename... P>
Vector<String> sarray(P... p_args) {
	return Vector<String>({String(p_args)...});
}

template <typename T, size_t SIZE>
constexpr size_t std_size(const T (&)[SIZE]) {
	return SIZE;
}