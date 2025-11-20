/**************************************************************************/
/*  ruzta_rpc_callable.cpp                                             */
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

#include "ruzta_rpc_callable.h"

#include <godot_cpp/classes/script_language.hpp> // original: core/object/script_language.h
#include <godot_cpp/templates/hashfuncs.hpp> // original: core/templates/hashfuncs.h
#include <godot_cpp/classes/node.hpp> // original: scene/main/node.h

bool RuztaRPCCallable::compare_equal(const CallableCustom *p_a, const CallableCustom *p_b) {
	return p_a->hash() == p_b->hash();
}

bool RuztaRPCCallable::compare_less(const CallableCustom *p_a, const CallableCustom *p_b) {
	return p_a->hash() < p_b->hash();
}

uint32_t RuztaRPCCallable::hash() const {
	return h;
}

String RuztaRPCCallable::get_as_text() const {
	String class_name = object->get_class();
	Ref<Script> script = object->get_script();
	if (script.is_valid()) {
		if (!script->get_global_name().is_empty()) {
			class_name += "(" + script->get_global_name() + ")";
		} else if (script->get_path().is_resource_file()) {
			class_name += "(" + script->get_path().get_file() + ")";
		}
	}
	return class_name + "::" + String(method) + " (rpc)";
}

CallableCustom::CompareEqualFunc RuztaRPCCallable::get_compare_equal_func() const {
	return compare_equal;
}

CallableCustom::CompareLessFunc RuztaRPCCallable::get_compare_less_func() const {
	return compare_less;
}

ObjectID RuztaRPCCallable::get_object() const {
	return object->get_instance_id();
}

StringName RuztaRPCCallable::get_method() const {
	return method;
}

int RuztaRPCCallable::get_argument_count(bool &r_is_valid) const {
	return object->get_method_argument_count(method, &r_is_valid);
}

void RuztaRPCCallable::call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, GDExtensionCallError &r_call_error) const {
	r_return_value = object->callp(method, p_arguments, p_argcount, r_call_error);
}

RuztaRPCCallable::RuztaRPCCallable(Object *p_object, const StringName &p_method) {
	ERR_FAIL_NULL(p_object);
	object = p_object;
	method = p_method;
	h = method.hash();
	h = hash_murmur3_one_64(object->get_instance_id(), h);
	node = Object::cast_to<Node>(object);
	ERR_FAIL_NULL_MSG(node, "RPC can only be defined on class that extends Node.");
}

Error RuztaRPCCallable::rpc(int p_peer_id, const Variant **p_arguments, int p_argcount, GDExtensionCallError &r_call_error) const {
	if (unlikely(!node)) {
		r_call_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL;
		return ERR_UNCONFIGURED;
	}
	r_call_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_OK;
	return node->rpcp(p_peer_id, method, p_arguments, p_argcount);
}
