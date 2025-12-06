/**************************************************************************/
/*  ruzta_lambda_callable.h                                            */
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

#include <godot_cpp/classes/ref_counted.hpp> // original: core/object/ref_counted.h
#include <godot_cpp/templates/vector.hpp> // original: core/templates/vector.h
#include <godot_cpp/variant/callable.hpp> // original: core/variant/callable.h
#include <godot_cpp/variant/variant.hpp> // original: core/variant/variant.h

class RuztaFunction;
class RuztaInstance;

class RuztaLambdaCallable : public CallableCustom {
	Ruzta::UpdatableFuncPtr function;
	Ref<Ruzta> script;
	uint32_t h;

	Vector<Variant> captures;

	static bool compare_equal(const CallableCustom *p_a, const CallableCustom *p_b);
	static bool compare_less(const CallableCustom *p_a, const CallableCustom *p_b);

public:
	bool is_valid() const override;
	uint32_t hash() const override;
	String get_as_text() const override;
	CompareEqualFunc get_compare_equal_func() const override;
	CompareLessFunc get_compare_less_func() const override;
	ObjectID get_object() const override;
	StringName get_method() const;
	int get_argument_count(bool &r_is_valid) const override;
	void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, GDExtensionCallError &r_call_error) const override;

	RuztaLambdaCallable(RuztaLambdaCallable &) = delete;
	RuztaLambdaCallable(const RuztaLambdaCallable &) = delete;
	RuztaLambdaCallable(Ref<Ruzta> p_script, RuztaFunction *p_function, const Vector<Variant> &p_captures);
	virtual ~RuztaLambdaCallable() = default;
};

// Lambda callable that references a particular object, so it can use `self` in the body.
class RuztaLambdaSelfCallable : public CallableCustom {
	Ruzta::UpdatableFuncPtr function;
	Ref<RefCounted> reference; // For objects that are RefCounted, keep a reference.
	Object *object = nullptr; // For non RefCounted objects, use a direct pointer.
	uint32_t h;

	Vector<Variant> captures;

	static bool compare_equal(const CallableCustom *p_a, const CallableCustom *p_b);
	static bool compare_less(const CallableCustom *p_a, const CallableCustom *p_b);

public:
	bool is_valid() const override;
	uint32_t hash() const override;
	String get_as_text() const override;
	CompareEqualFunc get_compare_equal_func() const override;
	CompareLessFunc get_compare_less_func() const override;
	ObjectID get_object() const override;
	StringName get_method() const;
	int get_argument_count(bool &r_is_valid) const override;
	void call(const Variant **p_arguments, int p_argcount, Variant &r_return_value, GDExtensionCallError &r_call_error) const override;

	RuztaLambdaSelfCallable(RuztaLambdaSelfCallable &) = delete;
	RuztaLambdaSelfCallable(const RuztaLambdaSelfCallable &) = delete;
	RuztaLambdaSelfCallable(Ref<RefCounted> p_self, RuztaFunction *p_function, const Vector<Variant> &p_captures);
	RuztaLambdaSelfCallable(Object *p_self, RuztaFunction *p_function, const Vector<Variant> &p_captures);
	virtual ~RuztaLambdaSelfCallable() = default;
};
