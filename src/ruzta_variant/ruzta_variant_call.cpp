/**************************************************************************/
/*  ruzta_variant_call.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                RUZTA                                   */
/*                    https://seremtitus.co.ke/ruzta                      */
/**************************************************************************/
/* Copyright (c) 2025-present Ruzta contributors (see AUTHORS.md).  */
/* Copyright (c) 2014-present Godot Engine contributors                   */
/*                                             (see OG_AUTHORS.md).       */
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
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/ref.hpp>

#include "ruzta_variant_extension.h"
#include "span.h" // original: span.h
#include "span_to_string.h" // original: span_to_string.h

// TODO: #include "core/crypto/crypto_core.h" // original: core/crypto/crypto_core.h
#include <godot_cpp/classes/engine_debugger.hpp>  // original: core/debugger/engine_debugger.h
#include <godot_cpp/classes/marshalls.hpp>		  // original: core/io/marshalls.h
#include <godot_cpp/classes/os.hpp>				  // original: core/os/os.h
#include <godot_cpp/core/class_db.hpp>			  // original: core/object/class_db.h
#include <godot_cpp/templates/a_hash_map.hpp>	  // original: core/templates/a_hash_map.h
#include <godot_cpp/templates/local_vector.hpp>	  // original: core/templates/local_vector.h
#include <godot_cpp/variant/variant_internal.hpp>

typedef void (*VariantFunc)(Variant& r_ret, Variant& p_self, const Variant** p_args);
typedef void (*VariantConstructFunc)(Variant& r_ret, const Variant** p_args);

template <typename R, typename... P>
static _FORCE_INLINE_ void vc_static_method_call(R (*method)(P...), const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	call_with_variant_args_static_ret_dv(method, p_args, p_argcount, r_ret, r_error, p_defvals);
}

template <typename... P>
static _FORCE_INLINE_ void vc_static_method_call(void (*method)(P...), const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	call_with_variant_args_static_dv(method, p_args, p_argcount, r_error, p_defvals);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call(R (T::*method)(P...), Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	call_with_variant_args_ret_dv(&VariantInternalAccessor<T>::get(base), method, p_args, p_argcount, r_ret, r_error, p_defvals);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call(R (T::*method)(P...) const, Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	call_with_variant_args_retc_dv(&VariantInternalAccessor<T>::get(base), method, p_args, p_argcount, r_ret, r_error, p_defvals);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call(void (T::*method)(P...), Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	VariantInternal::clear(&r_ret);
	call_with_variant_args_dv(&VariantInternalAccessor<T>::get(base), method, p_args, p_argcount, r_error, p_defvals);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call(void (T::*method)(P...) const, Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	VariantInternal::clear(&r_ret);
	call_with_variant_argsc_dv(&VariantInternalAccessor<T>::get(base), method, p_args, p_argcount, r_error, p_defvals);
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_method_call(R (T::*method)(P...), Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	T converted(static_cast<T>(VariantInternalAccessor<From>::get(base)));
	call_with_variant_args_ret_dv(&converted, method, p_args, p_argcount, r_ret, r_error, p_defvals);
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_method_call(R (T::*method)(P...) const, Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	T converted(static_cast<T>(VariantInternalAccessor<From>::get(base)));
	call_with_variant_args_retc_dv(&converted, method, p_args, p_argcount, r_ret, r_error, p_defvals);
}

template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_method_call(void (T::*method)(P...), Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	T converted(static_cast<T>(VariantInternalAccessor<From>::get(base)));
	call_with_variant_args_dv(&converted, method, p_args, p_argcount, r_error, p_defvals);
}

template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_method_call(void (T::*method)(P...) const, Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	T converted(static_cast<T>(VariantInternalAccessor<From>::get(base)));
	call_with_variant_argsc_dv(&converted, method, p_args, p_argcount, r_error, p_defvals);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call_static(R (*method)(T*, P...), Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	call_with_variant_args_retc_static_helper_dv(&VariantInternalAccessor<T>::get(base), method, p_args, p_argcount, r_ret, p_defvals, r_error);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_method_call_static(void (*method)(T*, P...), Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) {
	call_with_variant_args_static_helper_dv(&VariantInternalAccessor<T>::get(base), method, p_args, p_argcount, p_defvals, r_error);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call(R (T::*method)(P...), Variant* base, const Variant** p_args, Variant* r_ret) {
	call_with_validated_variant_args_ret(base, method, p_args, r_ret);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call(R (T::*method)(P...) const, Variant* base, const Variant** p_args, Variant* r_ret) {
	call_with_validated_variant_args_retc(base, method, p_args, r_ret);
}
template <typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call(void (T::*method)(P...), Variant* base, const Variant** p_args, Variant* r_ret) {
	call_with_validated_variant_args(base, method, p_args);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call(void (T::*method)(P...) const, Variant* base, const Variant** p_args, Variant* r_ret) {
	call_with_validated_variant_argsc(base, method, p_args);
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_validated_call(R (T::*method)(P...), Variant* base, const Variant** p_args, Variant* r_ret) {
	T converted(static_cast<T>(VariantInternalAccessor<From>::get(base)));
	call_with_validated_variant_args_ret_helper<T, R, P...>(&converted, method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_validated_call(R (T::*method)(P...) const, Variant* base, const Variant** p_args, Variant* r_ret) {
	T converted(static_cast<T>(VariantInternalAccessor<From>::get(base)));
	call_with_validated_variant_args_retc_helper<T, R, P...>(&converted, method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}
template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_validated_call(void (T::*method)(P...), Variant* base, const Variant** p_args, Variant* r_ret) {
	T converted(static_cast<T>(VariantInternalAccessor<From>::get(base)));
	call_with_validated_variant_args_helper<T, P...>(&converted, method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_validated_call(void (T::*method)(P...) const, Variant* base, const Variant** p_args, Variant* r_ret) {
	T converted(static_cast<T>(VariantInternalAccessor<From>::get(base)));
	call_with_validated_variant_argsc_helper<T, P...>(&converted, method, p_args, r_ret, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call_static(R (*method)(T*, P...), Variant* base, const Variant** p_args, Variant* r_ret) {
	call_with_validated_variant_args_static_retc(base, method, p_args, r_ret);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_validated_call_static(void (*method)(T*, P...), Variant* base, const Variant** p_args, Variant* r_ret) {
	call_with_validated_variant_args_static(base, method, p_args);
}

template <typename R, typename... P>
static _FORCE_INLINE_ void vc_validated_static_call(R (*method)(P...), const Variant** p_args, Variant* r_ret) {
	call_with_validated_variant_args_static_method_ret(method, p_args, r_ret);
}

template <typename... P>
static _FORCE_INLINE_ void vc_validated_static_call(void (*method)(P...), const Variant** p_args, Variant* r_ret) {
	call_with_validated_variant_args_static_method(method, p_args);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(R (T::*method)(P...), void* p_base, const void** p_args, void* r_ret) {
	call_with_ptr_args_ret(reinterpret_cast<T*>(p_base), method, p_args, r_ret);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(R (T::*method)(P...) const, void* p_base, const void** p_args, void* r_ret) {
	call_with_ptr_args_retc(reinterpret_cast<T*>(p_base), method, p_args, r_ret);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(void (T::*method)(P...), void* p_base, const void** p_args, void* r_ret) {
	call_with_ptr_args(reinterpret_cast<T*>(p_base), method, p_args);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(void (T::*method)(P...) const, void* p_base, const void** p_args, void* r_ret) {
	call_with_ptr_argsc(reinterpret_cast<T*>(p_base), method, p_args);
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_ptrcall(R (T::*method)(P...), void* p_base, const void** p_args, void* r_ret) {
	T converted(*reinterpret_cast<From*>(p_base));
	call_with_ptr_args_ret(&converted, method, p_args, r_ret);
}

template <typename From, typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_ptrcall(R (T::*method)(P...) const, void* p_base, const void** p_args, void* r_ret) {
	T converted(*reinterpret_cast<From*>(p_base));
	call_with_ptr_args_retc(&converted, method, p_args, r_ret);
}

template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_ptrcall(void (T::*method)(P...), void* p_base, const void** p_args, void* r_ret) {
	T converted(*reinterpret_cast<From*>(p_base));
	call_with_ptr_args(&converted, method, p_args);
}

template <typename From, typename T, typename... P>
static _FORCE_INLINE_ void vc_convert_ptrcall(void (T::*method)(P...) const, void* p_base, const void** p_args, void* r_ret) {
	T converted(*reinterpret_cast<From*>(p_base));
	call_with_ptr_argsc(&converted, method, p_args);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count(R (T::*method)(P...)) {
	return sizeof...(P);
}
template <typename R, typename T, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count(R (T::*method)(P...) const) {
	return sizeof...(P);
}

template <typename T, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count(void (T::*method)(P...)) {
	return sizeof...(P);
}

template <typename T, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count(void (T::*method)(P...) const) {
	return sizeof...(P);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count(R (*method)(T*, P...)) {
	return sizeof...(P);
}

template <typename R, typename... P>
static _FORCE_INLINE_ int vc_get_argument_count_static(R (*method)(P...)) {
	return sizeof...(P);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type(R (T::*method)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}
template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type(R (T::*method)(P...) const, int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type(void (T::*method)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type(void (T::*method)(P...) const, int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type(R (*method)(T*, P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_argument_type_static(R (*method)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(R (T::*method)(P...)) {
	return (Variant::Type)GetTypeInfo<R>::VARIANT_TYPE;
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(R (T::*method)(P...) const) {
	return (Variant::Type)GetTypeInfo<R>::VARIANT_TYPE;
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(void (T::*method)(P...)) {
	return Variant::NIL;
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(void (T::*method)(P...) const) {
	return Variant::NIL;
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(R (*method)(P...)) {
	return (Variant::Type)GetTypeInfo<R>::VARIANT_TYPE;
}

template <typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_return_type(void (*method)(P...)) {
	return Variant::NIL;
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ bool vc_has_return_type(R (T::*method)(P...)) {
	return true;
}
template <typename R, typename T, typename... P>
static _FORCE_INLINE_ bool vc_has_return_type(R (T::*method)(P...) const) {
	return true;
}

template <typename T, typename... P>
static _FORCE_INLINE_ bool vc_has_return_type(void (T::*method)(P...)) {
	return false;
}

template <typename T, typename... P>
static _FORCE_INLINE_ bool vc_has_return_type(void (T::*method)(P...) const) {
	return false;
}

template <typename... P>
static _FORCE_INLINE_ bool vc_has_return_type_static(void (*method)(P...)) {
	return false;
}

template <typename R, typename... P>
static _FORCE_INLINE_ bool vc_has_return_type_static(R (*method)(P...)) {
	return true;
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ bool vc_is_const(R (T::*method)(P...)) {
	return false;
}
template <typename R, typename T, typename... P>
static _FORCE_INLINE_ bool vc_is_const(R (T::*method)(P...) const) {
	return true;
}

template <typename T, typename... P>
static _FORCE_INLINE_ bool vc_is_const(void (T::*method)(P...)) {
	return false;
}

template <typename T, typename... P>
static _FORCE_INLINE_ bool vc_is_const(void (T::*method)(P...) const) {
	return true;
}

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_base_type(R (T::*method)(P...)) {
	return (Variant::Type)GetTypeInfo<T>::VARIANT_TYPE;
}
template <typename R, typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_base_type(R (T::*method)(P...) const) {
	return (Variant::Type)GetTypeInfo<T>::VARIANT_TYPE;
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_base_type(void (T::*method)(P...)) {
	return (Variant::Type)GetTypeInfo<T>::VARIANT_TYPE;
}

template <typename T, typename... P>
static _FORCE_INLINE_ Variant::Type vc_get_base_type(void (T::*method)(P...) const) {
	return (Variant::Type)GetTypeInfo<T>::VARIANT_TYPE;
}

#define METHOD_CLASS(m_class, m_exposed_name, m_method_name, m_method_ptr)                                                                                         \
	struct Method_##m_class##_##m_method_name {                                                                                                                    \
		static void call(Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) { \
			vc_method_call(m_method_ptr, base, p_args, p_argcount, r_ret, p_defvals, r_error);                                                                     \
		}                                                                                                                                                          \
		static void validated_call(Variant* base, const Variant** p_args, int p_argcount, Variant* r_ret) {                                                        \
			vc_validated_call(m_method_ptr, base, p_args, r_ret);                                                                                                  \
		}                                                                                                                                                          \
		static void ptrcall(void* p_base, const void** p_args, void* r_ret, int p_argcount) {                                                                      \
			vc_ptrcall(m_method_ptr, p_base, p_args, r_ret);                                                                                                       \
		}                                                                                                                                                          \
		static int get_argument_count() {                                                                                                                          \
			return vc_get_argument_count(m_method_ptr);                                                                                                            \
		}                                                                                                                                                          \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                        \
			return vc_get_argument_type(m_method_ptr, p_arg);                                                                                                      \
		}                                                                                                                                                          \
		static Variant::Type get_return_type() {                                                                                                                   \
			return vc_get_return_type(m_method_ptr);                                                                                                               \
		}                                                                                                                                                          \
		static bool has_return_type() {                                                                                                                            \
			return vc_has_return_type(m_method_ptr);                                                                                                               \
		}                                                                                                                                                          \
		static bool is_const() {                                                                                                                                   \
			return vc_is_const(m_method_ptr);                                                                                                                      \
		}                                                                                                                                                          \
		static bool is_static() {                                                                                                                                  \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static bool is_vararg() {                                                                                                                                  \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static Variant::Type get_base_type() {                                                                                                                     \
			return vc_get_base_type(m_method_ptr);                                                                                                                 \
		}                                                                                                                                                          \
		static StringName get_name() {                                                                                                                             \
			return StringName(#m_exposed_name);                                                                                                                    \
		}                                                                                                                                                          \
	};

#define CONVERT_METHOD_CLASS(m_class, m_exposed_name, m_method_name, m_method_ptr)                                                                                 \
	struct Method_##m_class##_##m_method_name {                                                                                                                    \
		static void call(Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) { \
			vc_convert_method_call<m_class>(m_method_ptr, base, p_args, p_argcount, r_ret, p_defvals, r_error);                                                    \
		}                                                                                                                                                          \
		static void validated_call(Variant* base, const Variant** p_args, int p_argcount, Variant* r_ret) {                                                        \
			vc_convert_validated_call<m_class>(m_method_ptr, base, p_args, r_ret);                                                                                 \
		}                                                                                                                                                          \
		static void ptrcall(void* p_base, const void** p_args, void* r_ret, int p_argcount) {                                                                      \
			vc_convert_ptrcall<m_class>(m_method_ptr, p_base, p_args, r_ret);                                                                                      \
		}                                                                                                                                                          \
		static int get_argument_count() {                                                                                                                          \
			return vc_get_argument_count(m_method_ptr);                                                                                                            \
		}                                                                                                                                                          \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                        \
			return vc_get_argument_type(m_method_ptr, p_arg);                                                                                                      \
		}                                                                                                                                                          \
		static Variant::Type get_return_type() {                                                                                                                   \
			return vc_get_return_type(m_method_ptr);                                                                                                               \
		}                                                                                                                                                          \
		static bool has_return_type() {                                                                                                                            \
			return vc_has_return_type(m_method_ptr);                                                                                                               \
		}                                                                                                                                                          \
		static bool is_const() {                                                                                                                                   \
			return vc_is_const(m_method_ptr);                                                                                                                      \
		}                                                                                                                                                          \
		static bool is_static() {                                                                                                                                  \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static bool is_vararg() {                                                                                                                                  \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static Variant::Type get_base_type() {                                                                                                                     \
			return (Variant::Type)GetTypeInfo<m_class>::VARIANT_TYPE;                                                                                              \
		}                                                                                                                                                          \
		static StringName get_name() {                                                                                                                             \
			return StringName(#m_exposed_name);                                                                                                                    \
		}                                                                                                                                                          \
	};

template <typename R, typename... P>
static _FORCE_INLINE_ void vc_static_ptrcall(R (*method)(P...), const void** p_args, void* r_ret) {
	call_with_ptr_args_static_method_ret<R, P...>(method, p_args, r_ret);
}

template <typename... P>
static _FORCE_INLINE_ void vc_static_ptrcall(void (*method)(P...), const void** p_args, void* r_ret) {
	call_with_ptr_args_static_method<P...>(method, p_args);
}

#define STATIC_METHOD_CLASS(m_class, m_exposed_name, m_method_name, m_method_ptr)                                                                                  \
	struct Method_##m_class##_##m_method_name {                                                                                                                    \
		static void call(Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) { \
			vc_static_method_call(m_method_ptr, p_args, p_argcount, r_ret, p_defvals, r_error);                                                                    \
		}                                                                                                                                                          \
		static void validated_call(Variant* base, const Variant** p_args, int p_argcount, Variant* r_ret) {                                                        \
			vc_validated_static_call(m_method_ptr, p_args, r_ret);                                                                                                 \
		}                                                                                                                                                          \
		static void ptrcall(void* p_base, const void** p_args, void* r_ret, int p_argcount) {                                                                      \
			vc_static_ptrcall(m_method_ptr, p_args, r_ret);                                                                                                        \
		}                                                                                                                                                          \
		static int get_argument_count() {                                                                                                                          \
			return vc_get_argument_count_static(m_method_ptr);                                                                                                     \
		}                                                                                                                                                          \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                        \
			return vc_get_argument_type_static(m_method_ptr, p_arg);                                                                                               \
		}                                                                                                                                                          \
		static Variant::Type get_return_type() {                                                                                                                   \
			return vc_get_return_type(m_method_ptr);                                                                                                               \
		}                                                                                                                                                          \
		static bool has_return_type() {                                                                                                                            \
			return vc_has_return_type_static(m_method_ptr);                                                                                                        \
		}                                                                                                                                                          \
		static bool is_const() {                                                                                                                                   \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static bool is_static() {                                                                                                                                  \
			return true;                                                                                                                                           \
		}                                                                                                                                                          \
		static bool is_vararg() {                                                                                                                                  \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static Variant::Type get_base_type() {                                                                                                                     \
			return (Variant::Type)GetTypeInfo<m_class>::VARIANT_TYPE;                                                                                              \
		}                                                                                                                                                          \
		static StringName get_name() {                                                                                                                             \
			return StringName(#m_exposed_name);                                                                                                                    \
		}                                                                                                                                                          \
	};

template <typename R, typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(R (*method)(T*, P...), void* p_base, const void** p_args, void* r_ret) {
	call_with_ptr_args_static_retc<T, R, P...>(reinterpret_cast<T*>(p_base), method, p_args, r_ret);
}

template <typename T, typename... P>
static _FORCE_INLINE_ void vc_ptrcall(void (*method)(T*, P...), void* p_base, const void** p_args, void* r_ret) {
	call_with_ptr_args_static<T, P...>(reinterpret_cast<T*>(p_base), method, p_args);
}

#define FUNCTION_CLASS(m_class, m_exposed_name, m_method_name, m_method_ptr, m_const)                                                                              \
	struct Method_##m_class##_##m_method_name {                                                                                                                    \
		static void call(Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) { \
			vc_method_call_static(m_method_ptr, base, p_args, p_argcount, r_ret, p_defvals, r_error);                                                              \
		}                                                                                                                                                          \
		static void validated_call(Variant* base, const Variant** p_args, int p_argcount, Variant* r_ret) {                                                        \
			vc_validated_call_static(m_method_ptr, base, p_args, r_ret);                                                                                           \
		}                                                                                                                                                          \
		static void ptrcall(void* p_base, const void** p_args, void* r_ret, int p_argcount) {                                                                      \
			vc_ptrcall(m_method_ptr, p_base, p_args, r_ret);                                                                                                       \
		}                                                                                                                                                          \
		static int get_argument_count() {                                                                                                                          \
			return vc_get_argument_count(m_method_ptr);                                                                                                            \
		}                                                                                                                                                          \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                        \
			return vc_get_argument_type(m_method_ptr, p_arg);                                                                                                      \
		}                                                                                                                                                          \
		static Variant::Type get_return_type() {                                                                                                                   \
			return vc_get_return_type(m_method_ptr);                                                                                                               \
		}                                                                                                                                                          \
		static bool has_return_type() {                                                                                                                            \
			return vc_has_return_type_static(m_method_ptr);                                                                                                        \
		}                                                                                                                                                          \
		static bool is_const() {                                                                                                                                   \
			return m_const;                                                                                                                                        \
		}                                                                                                                                                          \
		static bool is_static() {                                                                                                                                  \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static bool is_vararg() {                                                                                                                                  \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static Variant::Type get_base_type() {                                                                                                                     \
			return (Variant::Type)GetTypeInfo<m_class>::VARIANT_TYPE;                                                                                              \
		}                                                                                                                                                          \
		static StringName get_name() {                                                                                                                             \
			return StringName(#m_exposed_name);                                                                                                                    \
		}                                                                                                                                                          \
	};

#define VARARG_CLASS(m_class, m_exposed_name, m_method_name, m_method_ptr, m_has_return, m_return_type)                                                            \
	struct Method_##m_class##_##m_method_name {                                                                                                                    \
		static void call(Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) { \
			m_method_ptr(base, p_args, p_argcount, r_ret, r_error);                                                                                                \
		}                                                                                                                                                          \
		static void validated_call(Variant* base, const Variant** p_args, int p_argcount, Variant* r_ret) {                                                        \
			GDExtensionCallError ce;                                                                                                                               \
			m_method_ptr(base, p_args, p_argcount, *r_ret, ce);                                                                                                    \
		}                                                                                                                                                          \
		static void ptrcall(void* p_base, const void** p_args, void* r_ret, int p_argcount) {                                                                      \
			LocalVector<Variant> vars;                                                                                                                             \
			LocalVector<const Variant*> vars_ptrs;                                                                                                                 \
			vars.resize(p_argcount);                                                                                                                               \
			vars_ptrs.resize(p_argcount);                                                                                                                          \
			for (int i = 0; i < p_argcount; i++) {                                                                                                                 \
				vars[i] = PtrToArg<Variant>::convert(p_args[i]);                                                                                                   \
				vars_ptrs[i] = &vars[i];                                                                                                                           \
			}                                                                                                                                                      \
			Variant base = PtrToArg<m_class>::convert(p_base);                                                                                                     \
			Variant ret;                                                                                                                                           \
			GDExtensionCallError ce;                                                                                                                               \
			m_method_ptr(&base, vars_ptrs.ptr(), p_argcount, ret, ce);                                                                                             \
			if (m_has_return) {                                                                                                                                    \
				m_return_type r = ret;                                                                                                                             \
				PtrToArg<m_return_type>::encode(ret, r_ret);                                                                                                       \
			}                                                                                                                                                      \
		}                                                                                                                                                          \
		static int get_argument_count() {                                                                                                                          \
			return 0;                                                                                                                                              \
		}                                                                                                                                                          \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                        \
			return Variant::NIL;                                                                                                                                   \
		}                                                                                                                                                          \
		static Variant::Type get_return_type() {                                                                                                                   \
			return (Variant::Type)GetTypeInfo<m_return_type>::VARIANT_TYPE;                                                                                        \
		}                                                                                                                                                          \
		static bool has_return_type() {                                                                                                                            \
			return m_has_return;                                                                                                                                   \
		}                                                                                                                                                          \
		static bool is_const() {                                                                                                                                   \
			return true;                                                                                                                                           \
		}                                                                                                                                                          \
		static bool is_static() {                                                                                                                                  \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static bool is_vararg() {                                                                                                                                  \
			return true;                                                                                                                                           \
		}                                                                                                                                                          \
		static Variant::Type get_base_type() {                                                                                                                     \
			return (Variant::Type)GetTypeInfo<m_class>::VARIANT_TYPE;                                                                                              \
		}                                                                                                                                                          \
		static StringName get_name() {                                                                                                                             \
			return StringName(#m_exposed_name);                                                                                                                    \
		}                                                                                                                                                          \
	};

#define VARARG_CLASS1(m_class, m_exposed_name, m_method_name, m_method_ptr, m_arg_type)                                                                            \
	struct Method_##m_class##_##m_method_name {                                                                                                                    \
		static void call(Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) { \
			m_method_ptr(base, p_args, p_argcount, r_ret, r_error);                                                                                                \
		}                                                                                                                                                          \
		static void validated_call(Variant* base, const Variant** p_args, int p_argcount, Variant* r_ret) {                                                        \
			GDExtensionCallError ce;                                                                                                                               \
			m_method_ptr(base, p_args, p_argcount, *r_ret, ce);                                                                                                    \
		}                                                                                                                                                          \
		static void ptrcall(void* p_base, const void** p_args, void* r_ret, int p_argcount) {                                                                      \
			LocalVector<Variant> vars;                                                                                                                             \
			LocalVector<const Variant*> vars_ptrs;                                                                                                                 \
			vars.resize(p_argcount);                                                                                                                               \
			vars_ptrs.resize(p_argcount);                                                                                                                          \
			for (int i = 0; i < p_argcount; i++) {                                                                                                                 \
				vars[i] = PtrToArg<Variant>::convert(p_args[i]);                                                                                                   \
				vars_ptrs[i] = &vars[i];                                                                                                                           \
			}                                                                                                                                                      \
			Variant base = PtrToArg<m_class>::convert(p_base);                                                                                                     \
			Variant ret;                                                                                                                                           \
			GDExtensionCallError ce;                                                                                                                               \
			m_method_ptr(&base, vars_ptrs.ptr(), p_argcount, ret, ce);                                                                                             \
		}                                                                                                                                                          \
		static int get_argument_count() {                                                                                                                          \
			return 1;                                                                                                                                              \
		}                                                                                                                                                          \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                        \
			return m_arg_type;                                                                                                                                     \
		}                                                                                                                                                          \
		static Variant::Type get_return_type() {                                                                                                                   \
			return Variant::NIL;                                                                                                                                   \
		}                                                                                                                                                          \
		static bool has_return_type() {                                                                                                                            \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static bool is_const() {                                                                                                                                   \
			return true;                                                                                                                                           \
		}                                                                                                                                                          \
		static bool is_static() {                                                                                                                                  \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static bool is_vararg() {                                                                                                                                  \
			return true;                                                                                                                                           \
		}                                                                                                                                                          \
		static Variant::Type get_base_type() {                                                                                                                     \
			return (Variant::Type)GetTypeInfo<m_class>::VARIANT_TYPE;                                                                                              \
		}                                                                                                                                                          \
		static StringName get_name() {                                                                                                                             \
			return StringName(#m_exposed_name);                                                                                                                    \
		}                                                                                                                                                          \
	};

#define VARCALL_ARRAY_GETTER_SETTER(m_packed_type, m_type)                                                      \
	static m_type func_##m_packed_type##_get(m_packed_type* p_instance, int64_t p_index) {                      \
		ERR_FAIL_INDEX_V(p_index, p_instance->size(), m_type());                                                \
		return p_instance->get(p_index);                                                                        \
	}                                                                                                           \
	static void func_##m_packed_type##_set(m_packed_type* p_instance, int64_t p_index, const m_type& p_value) { \
		ERR_FAIL_INDEX(p_index, p_instance->size());                                                            \
		p_instance->set(p_index, p_value);                                                                      \
	}

struct ConstantData {
	HashMap<StringName, int64_t> value;
#ifdef DEBUG_ENABLED
	List<StringName> value_ordered;
#endif	// DEBUG_ENABLED
	HashMap<StringName, Variant> variant_value;
#ifdef DEBUG_ENABLED
	List<StringName> variant_value_ordered;
#endif	// DEBUG_ENABLED
};
static ConstantData* constant_data;

struct EnumData {
	HashMap<StringName, HashMap<StringName, int>> value;
	HashMap<StringName, StringName> value_to_enum;
};

static EnumData* enum_data;

void add_constant(int p_type, const StringName& p_constant_name, int64_t p_constant_value) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(constant_data[p_type].value.has(p_constant_name));
	ERR_FAIL_COND(enum_data[p_type].value.has(p_constant_name));
	ERR_FAIL_COND(enum_data[p_type].value_to_enum.has(p_constant_name));
#endif	// DEBUG_ENABLED
	constant_data[p_type].value[p_constant_name] = p_constant_value;
#ifdef DEBUG_ENABLED
	constant_data[p_type].value_ordered.push_back(p_constant_name);
#endif	// DEBUG_ENABLED
}
void add_variant_constant(int p_type, const StringName& p_constant_name, const Variant& p_constant_value) {
	constant_data[p_type].variant_value[p_constant_name] = p_constant_value;
#ifdef DEBUG_ENABLED
	constant_data[p_type].variant_value_ordered.push_back(p_constant_name);
#endif	// DEBUG_ENABLED
}

void add_enum_constant(int p_type, const StringName& p_enum_type_name, const StringName& p_enumeration_name, int p_enum_value) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(constant_data[p_type].value.has(p_enumeration_name));
	ERR_FAIL_COND(enum_data[p_type].value.has(p_enumeration_name));
	ERR_FAIL_COND(enum_data[p_type].value_to_enum.has(p_enumeration_name));
#endif	// DEBUG_ENABLED
	enum_data[p_type].value[p_enum_type_name][p_enumeration_name] = p_enum_value;
	enum_data[p_type].value_to_enum[p_enumeration_name] = p_enum_type_name;
}

struct VariantBuiltInMethodInfo {
	void (*call)(Variant* base, const Variant** p_args, int p_argcount, Variant& r_ret, const Vector<Variant>& p_defvals, GDExtensionCallError& r_error) = nullptr;
	RuztaVariantExtension::ValidatedBuiltInMethod validated_call = nullptr;
	RuztaVariantExtension::PTRBuiltInMethod ptrcall = nullptr;

	Vector<Variant> default_arguments;
	Vector<String> argument_names;

	bool is_const = false;
	bool is_static = false;
	bool has_return_type = false;
	bool is_vararg = false;
	Variant::Type return_type;
	int argument_count = 0;
	Variant::Type (*get_argument_type)(int p_arg) = nullptr;

	MethodInfo get_method_info(const StringName& p_name) const {
		MethodInfo mi;
		mi.name = p_name;

		if (has_return_type) {
			mi.return_val.type = return_type;
			if (mi.return_val.type == Variant::NIL) {
				mi.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
			}
		}

		if (is_const) {
			mi.flags |= METHOD_FLAG_CONST;
		}
		if (is_vararg) {
			mi.flags |= METHOD_FLAG_VARARG;
		}
		if (is_static) {
			mi.flags |= METHOD_FLAG_STATIC;
		}

		for (int i = 0; i < argument_count; i++) {
			PropertyInfo pi;
#ifdef DEBUG_ENABLED
			pi.name = argument_names[i];
#else
			pi.name = "arg" + itos(i + 1);
#endif	// DEBUG_ENABLED
			pi.type = (*get_argument_type)(i);
			if (pi.type == Variant::NIL) {
				pi.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
			}
			mi.arguments.push_back(pi);
		}

		mi.default_arguments = default_arguments;

		return mi;
	}

	uint32_t get_hash() const {
		uint32_t hash = hash_murmur3_one_32(is_const);
		hash = hash_murmur3_one_32(is_static, hash);
		hash = hash_murmur3_one_32(is_vararg, hash);
		hash = hash_murmur3_one_32(has_return_type, hash);
		if (has_return_type) {
			hash = hash_murmur3_one_32(return_type, hash);
		}
		hash = hash_murmur3_one_32(argument_count, hash);
		for (int i = 0; i < argument_count; i++) {
			hash = hash_murmur3_one_32(get_argument_type(i), hash);
		}

		return hash_fmix32(hash);
	}
};

typedef AHashMap<StringName, VariantBuiltInMethodInfo> BuiltinMethodMap;
static BuiltinMethodMap* builtin_method_info;
static List<StringName>* builtin_method_names;

#ifndef DISABLE_DEPRECATED
typedef AHashMap<StringName, LocalVector<VariantBuiltInMethodInfo>> BuiltinCompatMethodMap;
static BuiltinCompatMethodMap* builtin_compat_method_info;
#endif

template <typename T>
static void _populate_variant_builtin_method_info(VariantBuiltInMethodInfo& r_imi, const Vector<String>& p_argnames, const Vector<Variant>& p_def_args) {
	r_imi.call = T::call;
	r_imi.validated_call = T::validated_call;
	r_imi.ptrcall = T::ptrcall;

	r_imi.default_arguments = p_def_args;
	r_imi.argument_names = p_argnames;

	r_imi.is_const = T::is_const();
	r_imi.is_static = T::is_static();
	r_imi.is_vararg = T::is_vararg();
	r_imi.has_return_type = T::has_return_type();
	r_imi.return_type = T::get_return_type();
	r_imi.argument_count = T::get_argument_count();
	r_imi.get_argument_type = T::get_argument_type;
}

template <typename T>
static void register_builtin_method(const Vector<String>& p_argnames, const Vector<Variant>& p_def_args) {
	StringName name = T::get_name();

	ERR_FAIL_COND(builtin_method_info[T::get_base_type()].has(name));

	VariantBuiltInMethodInfo imi;
	_populate_variant_builtin_method_info<T>(imi, p_argnames, p_def_args);

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(!imi.is_vararg && imi.argument_count != imi.argument_names.size());
#endif	// DEBUG_ENABLED

	builtin_method_info[T::get_base_type()].insert(name, imi);
	builtin_method_names[T::get_base_type()].push_back(name);
}

#ifndef DISABLE_DEPRECATED
template <typename T>
static void register_builtin_compat_method(const Vector<String>& p_argnames, const Vector<Variant>& p_def_args) {
	StringName name = T::get_name();

	ERR_FAIL_COND(!builtin_method_info[T::get_base_type()].has(name));

	VariantBuiltInMethodInfo imi;
	_populate_variant_builtin_method_info<T>(imi, p_argnames, p_def_args);

#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(!imi.is_vararg && imi.argument_count != imi.argument_names.size());
#endif	// DEBUG_ENABLED

	if (!builtin_compat_method_info[T::get_base_type()].has(name)) {
		builtin_compat_method_info[T::get_base_type()].insert(name, LocalVector<VariantBuiltInMethodInfo>());
	}
	builtin_compat_method_info[T::get_base_type()][name].push_back(imi);
}
#endif

bool RuztaVariantExtension::has_builtin_method(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	return builtin_method_info[p_type].has(p_method);
}

RuztaVariantExtension::ValidatedBuiltInMethod RuztaVariantExtension::get_validated_builtin_method(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, nullptr);
	return method->validated_call;
}

RuztaVariantExtension::PTRBuiltInMethod RuztaVariantExtension::get_ptr_builtin_method(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, nullptr);
	return method->ptrcall;
}

RuztaVariantExtension::PTRBuiltInMethod RuztaVariantExtension::get_ptr_builtin_method_with_compatibility(Variant::Type p_type, const StringName& p_method, uint32_t p_hash) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);

	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	if (method && method->get_hash() == p_hash) {
		return method->ptrcall;
	}

#ifndef DISABLE_DEPRECATED
	const LocalVector<VariantBuiltInMethodInfo>* compat_methods = builtin_compat_method_info[p_type].getptr(p_method);
	if (compat_methods) {
		for (const VariantBuiltInMethodInfo& imi : *compat_methods) {
			if (imi.get_hash() == p_hash) {
				return imi.ptrcall;
			}
		}
	}
#endif

	return nullptr;
}

MethodInfo RuztaVariantExtension::get_builtin_method_info(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, MethodInfo());
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, MethodInfo());
	return method->get_method_info(p_method);
}

int RuztaVariantExtension::get_builtin_method_argument_count(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, 0);
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, 0);
	return method->argument_count;
}

Variant::Type RuztaVariantExtension::get_builtin_method_argument_type(Variant::Type p_type, const StringName& p_method, int p_argument) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Variant::NIL);
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, Variant::NIL);
	ERR_FAIL_INDEX_V(p_argument, method->argument_count, Variant::NIL);
	return method->get_argument_type(p_argument);
}

String RuztaVariantExtension::get_builtin_method_argument_name(Variant::Type p_type, const StringName& p_method, int p_argument) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, String());
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, String());
#ifdef DEBUG_ENABLED
	ERR_FAIL_INDEX_V(p_argument, method->argument_count, String());
	return method->argument_names[p_argument];
#else
	return "arg" + itos(p_argument + 1);
#endif	// DEBUG_ENABLED
}

Vector<Variant> RuztaVariantExtension::get_builtin_method_default_arguments(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Vector<Variant>());
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, Vector<Variant>());
	return method->default_arguments;
}

bool RuztaVariantExtension::has_builtin_method_return_value(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, false);
	return method->has_return_type;
}

void RuztaVariantExtension::get_builtin_method_list(Variant::Type p_type, List<StringName>* p_list) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);
	for (const StringName& E : builtin_method_names[p_type]) {
		p_list->push_back(E);
	}
}

int RuztaVariantExtension::get_builtin_method_count(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, -1);
	return builtin_method_names[p_type].size();
}

Variant::Type RuztaVariantExtension::get_builtin_method_return_type(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Variant::NIL);
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, Variant::NIL);
	return method->return_type;
}

bool RuztaVariantExtension::is_builtin_method_const(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, false);
	return method->is_const;
}

bool RuztaVariantExtension::is_builtin_method_static(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, false);
	return method->is_static;
}

bool RuztaVariantExtension::is_builtin_method_vararg(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, false);
	return method->is_vararg;
}

uint32_t RuztaVariantExtension::get_builtin_method_hash(Variant::Type p_type, const StringName& p_method) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, 0);
	const VariantBuiltInMethodInfo* method = builtin_method_info[p_type].getptr(p_method);
	ERR_FAIL_NULL_V(method, 0);
	return method->get_hash();
}

Vector<uint32_t> RuztaVariantExtension::get_builtin_method_compatibility_hashes(Variant::Type p_type, const StringName& p_method) {
	Vector<uint32_t> method_hashes;
#ifndef DISABLE_DEPRECATED
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, method_hashes);
	const LocalVector<VariantBuiltInMethodInfo>* compat_methods = builtin_compat_method_info[p_type].getptr(p_method);
	if (compat_methods) {
		for (const VariantBuiltInMethodInfo& imi : *compat_methods) {
			method_hashes.push_back(imi.get_hash());
		}
	}
#endif
	return method_hashes;
}

void RuztaVariantExtension::get_constants_for_type(Variant::Type p_type, List<StringName>* p_constants) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);

	const ConstantData& cd = constant_data[p_type];

#ifdef DEBUG_ENABLED
	for (const List<StringName>::Element* E = cd.value_ordered.front(); E; E = E->next()) {
		p_constants->push_back(E->get());
#else
	for (const KeyValue<StringName, int64_t>& E : cd.value) {
		p_constants->push_back(E.key);
#endif	// DEBUG_ENABLED
	}

#ifdef DEBUG_ENABLED
	for (const List<StringName>::Element* E = cd.variant_value_ordered.front(); E; E = E->next()) {
		p_constants->push_back(E->get());
#else
	for (const KeyValue<StringName, Variant>& E : cd.variant_value) {
		p_constants->push_back(E.key);
#endif	// DEBUG_ENABLED
	}
}

int RuztaVariantExtension::get_constants_count_for_type(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, -1);
	ConstantData& cd = constant_data[p_type];

	return cd.value.size() + cd.variant_value.size();
}

bool RuztaVariantExtension::has_constant(Variant::Type p_type, const StringName& p_value) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	ConstantData& cd = constant_data[p_type];
	return cd.value.has(p_value) || cd.variant_value.has(p_value);
}

Variant RuztaVariantExtension::get_constant_value(Variant::Type p_type, const StringName& p_value, bool* r_valid) {
	if (r_valid) {
		*r_valid = false;
	}

	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, 0);
	ConstantData& cd = constant_data[p_type];

	HashMap<StringName, int64_t>::Iterator E = cd.value.find(p_value);
	if (!E) {
		HashMap<StringName, Variant>::Iterator F = cd.variant_value.find(p_value);
		if (F) {
			if (r_valid) {
				*r_valid = true;
			}
			return F->value;
		} else {
			return -1;
		}
	}
	if (r_valid) {
		*r_valid = true;
	}

	return E->value;
}

void RuztaVariantExtension::get_enums_for_type(Variant::Type p_type, List<StringName>* p_enums) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);

	EnumData& r_enum_data = enum_data[p_type];

	for (const KeyValue<StringName, HashMap<StringName, int>>& E : r_enum_data.value) {
		p_enums->push_back(E.key);
	}
}

void RuztaVariantExtension::get_enumerations_for_enum(Variant::Type p_type, const StringName& p_enum_name, List<StringName>* p_enumerations) {
	ERR_FAIL_INDEX(p_type, Variant::VARIANT_MAX);

	EnumData& r_enum_data = enum_data[p_type];

	for (const KeyValue<StringName, HashMap<StringName, int>>& E : r_enum_data.value) {
		for (const KeyValue<StringName, int>& V : E.value) {
			p_enumerations->push_back(V.key);
		}
	}
}

int RuztaVariantExtension::get_enum_value(Variant::Type p_type, const StringName& p_enum_name, const StringName& p_enumeration, bool* r_valid) {
	if (r_valid) {
		*r_valid = false;
	}

	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, -1);

	EnumData& r_enum_data = enum_data[p_type];

	HashMap<StringName, HashMap<StringName, int>>::Iterator E = r_enum_data.value.find(p_enum_name);
	if (!E) {
		return -1;
	}

	HashMap<StringName, int>::Iterator V = E->value.find(p_enumeration);
	if (!V) {
		return -1;
	}

	if (r_valid) {
		*r_valid = true;
	}

	return V->value;
}

bool RuztaVariantExtension::has_enum(Variant::Type p_type, const StringName& p_enum_name) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);

	EnumData& r_enum_data = enum_data[p_type];

	return r_enum_data.value.has(p_enum_name);
}

StringName RuztaVariantExtension::get_enum_for_enumeration(Variant::Type p_type, const StringName& p_enumeration) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, StringName());

	EnumData& r_enum_data = enum_data[p_type];

	const StringName* enum_name = r_enum_data.value_to_enum.getptr(p_enumeration);
	return (enum_name == nullptr) ? StringName() : *enum_name;
}

#ifdef DEBUG_ENABLED
#define bind_method(m_type, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_method, m_method, &m_type::m_method);   \
	register_builtin_method<Method_##m_type##_##m_method>(m_arg_names, m_default_args);
#else
#define bind_method(m_type, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_method, m_method, &m_type ::m_method);  \
	register_builtin_method<Method_##m_type##_##m_method>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_compat_method(m_type, m_exposed_method, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_exposed_method, m_method, &m_type::m_method);                    \
	register_builtin_compat_method<Method_##m_type##_##m_method>(m_arg_names, m_default_args);
#else
#define bind_compat_method(m_type, m_exposed_method, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_exposed_method, m_method, &m_type ::m_method);                   \
	register_builtin_compat_method<Method_##m_type##_##m_method>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_convert_method(m_type_from, m_type_to, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_method, m_method, &m_type_to::m_method);           \
	register_builtin_method<Method_##m_type_from##_##m_method>(m_arg_names, m_default_args);
#else
#define bind_convert_method(m_type_from, m_type_to, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_method, m_method, &m_type_to ::m_method);          \
	register_builtin_method<Method_##m_type_from##_##m_method>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_convert_compat_method(m_type_from, m_type_to, m_exposed_method, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_exposed_method, m_method, &m_type_to::m_method);                            \
	register_builtin_compat_method<Method_##m_type_from##_##m_method>(m_arg_names, m_default_args);
#else
#define bind_convert_compat_method(m_type_from, m_type_to, m_exposed_method, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_exposed_method, m_method, &m_type_to ::m_method);                           \
	register_builtin_compat_method<Method_##m_type_from##_##m_method>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_static_method(m_type, m_method, m_arg_names, m_default_args) \
	STATIC_METHOD_CLASS(m_type, m_method, m_method, m_type::m_method);    \
	register_builtin_method<Method_##m_type##_##m_method>(m_arg_names, m_default_args);
#else
#define bind_static_method(m_type, m_method, m_arg_names, m_default_args) \
	STATIC_METHOD_CLASS(m_type, m_method, m_method, m_type ::m_method);   \
	register_builtin_method<Method_##m_type##_##m_method>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_static_compat_method(m_type, m_exposed_method, m_method, m_arg_names, m_default_args) \
	STATIC_METHOD_CLASS(m_type, m_exposed_method, m_method, m_type::m_method);                     \
	register_builtin_compat_method<Method_##m_type##_##m_method>(m_arg_names, m_default_args);
#else
#define bind_static_compat_method(m_type, m_exposed_method, m_method, m_arg_names, m_default_args) \
	STATIC_METHOD_CLASS(m_type, m_exposed_method, m_method, m_type ::m_method);                    \
	register_builtin_compat_method<Method_##m_type##_##m_method>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_static_methodv(m_type, m_name, m_method, m_arg_names, m_default_args) \
	STATIC_METHOD_CLASS(m_type, m_name, m_name, m_method);                         \
	register_builtin_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_static_methodv(m_type, m_name, m_method, m_arg_names, m_default_args) \
	STATIC_METHOD_CLASS(m_type, m_name, m_name, m_method);                         \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif

#ifdef DEBUG_ENABLED
#define bind_static_compat_methodv(m_type, m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	STATIC_METHOD_CLASS(m_type, m_exposed_name, m_name, m_method);                                        \
	register_builtin_compat_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_static_compat_methodv(m_type, m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	STATIC_METHOD_CLASS(m_type, m_exposed_name, m_name, m_method);                                        \
	register_builtin_compat_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif

#ifdef DEBUG_ENABLED
#define bind_methodv(m_type, m_name, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_name, m_name, m_method);                         \
	register_builtin_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_methodv(m_type, m_name, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_name, m_name, m_method);                         \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_compat_methodv(m_type, m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_exposed_name, m_name, m_method);                                        \
	register_builtin_compat_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_compat_methodv(m_type, m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	METHOD_CLASS(m_type, m_exposed_name, m_name, m_method);                                        \
	register_builtin_compat_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_convert_methodv(m_type_from, m_type_to, m_name, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_name, m_name, m_method);                                    \
	register_builtin_method<Method_##m_type_from##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_convert_methodv(m_type_from, m_type_to, m_name, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_name, m_name, m_method);                                    \
	register_builtin_method<Method_##m_type_from##_##m_name>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_convert_compat_methodv(m_type_from, m_type_to, m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_exposed_name, m_name, m_method);                                                   \
	register_builtin_compat_method<Method_##m_type_from##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_convert_compat_methodv(m_type_from, m_type_to, m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	CONVERT_METHOD_CLASS(m_type_from, m_exposed_name, m_name, m_method);                                                   \
	register_builtin_compat_method<Method_##m_type_from##_##m_name>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_function(m_type, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_name, m_name, m_method, true);                  \
	register_builtin_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_function(m_type, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_name, m_name, m_method, true);                  \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_compat_function(m_type, m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_exposed_name, m_name, m_method, true);                                 \
	register_builtin_compat_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_compat_function(m_type, m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_exposed_name, m_name, m_method, true);                                 \
	register_builtin_compat_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_functionnc(m_type, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_name, m_name, m_method, false);                   \
	register_builtin_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_functionnc(m_type, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_name, m_name, m_method, false);                   \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#ifdef DEBUG_ENABLED
#define bind_compat_functionnc(m_type, m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_exposed_name, m_name, m_method, false);                                  \
	register_builtin_compat_method<Method_##m_type##_##m_name>(m_arg_names, m_default_args);
#else
#define bind_compat_functionnc(m_type, m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	FUNCTION_CLASS(m_type, m_exposed_name, m_name, m_method, false);                                  \
	register_builtin_compat_method<Method_##m_type##_##m_name>(sarray(), m_default_args);
#endif	// DEBUG_ENABLED

#define bind_string_method(m_method, m_arg_names, m_default_args) \
	bind_method(String, m_method, m_arg_names, m_default_args);   \
	bind_convert_method(StringName, String, m_method, m_arg_names, m_default_args);

#define bind_string_methodv(m_name, m_method, m_arg_names, m_default_args) \
	bind_methodv(String, m_name, m_method, m_arg_names, m_default_args);   \
	bind_convert_methodv(StringName, String, m_name, m_method, m_arg_names, m_default_args);

#define bind_string_compat_method(m_exposed_name, m_method, m_arg_names, m_default_args) \
	bind_compat_method(String, m_exposed_name, m_method, m_arg_names, m_default_args);   \
	bind_convert_compat_method(StringName, String, m_exposed_name, m_method, m_arg_names, m_default_args);

#define bind_string_compat_methodv(m_exposed_name, m_name, m_method, m_arg_names, m_default_args) \
	bind_compat_methodv(String, m_exposed_name, m_name, m_method, m_arg_names, m_default_args);   \
	bind_convert_compat_methodv(StringName, String, m_exposed_name, m_name, m_method, m_arg_names, m_default_args);

#define bind_custom(m_type, m_name, m_method, m_has_return, m_ret_type)      \
	VARARG_CLASS(m_type, m_name, m_name, m_method, m_has_return, m_ret_type) \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(), Vector<Variant>());

#define bind_custom1(m_type, m_name, m_method, m_arg_type, m_arg_name) \
	VARARG_CLASS1(m_type, m_name, m_name, m_method, m_arg_type)        \
	register_builtin_method<Method_##m_type##_##m_name>(sarray(m_arg_name), Vector<Variant>());

#define bind_compat_custom(m_type, m_exposed_name, m_name, m_method, m_has_return, m_ret_type) \
	VARARG_CLASS(m_type, m_exposed_name, m_name, m_method, m_has_return, m_ret_type)           \
	register_builtin_compat_method<Method_##m_type##_##m_name>(sarray(), Vector<Variant>());

#define bind_compat_custom1(m_type, m_exposed_name, m_name, m_method, m_arg_type, m_arg_name) \
	VARARG_CLASS1(m_type, m_exposed_name, m_name, m_method, m_arg_type)                       \
	register_builtin_compat_method<Method_##m_type##_##m_name>(sarray(m_arg_name), Vector<Variant>());

static void _register_variant_builtin_methods_string() {
	constant_data = memnew_arr(ConstantData, Variant::VARIANT_MAX);
	enum_data = memnew_arr(EnumData, Variant::VARIANT_MAX);
	builtin_method_info = memnew_arr(BuiltinMethodMap, Variant::VARIANT_MAX);
	builtin_method_names = memnew_arr(List<StringName>, Variant::VARIANT_MAX);

	/* String */
	bind_string_method(casecmp_to, sarray("to"), varray());
	bind_string_method(nocasecmp_to, sarray("to"), varray());
	bind_string_method(naturalcasecmp_to, sarray("to"), varray());
	bind_string_method(naturalnocasecmp_to, sarray("to"), varray());
	bind_string_method(filecasecmp_to, sarray("to"), varray());
	bind_string_method(filenocasecmp_to, sarray("to"), varray());
	bind_string_method(length, sarray(), varray());
	bind_string_method(substr, sarray("from", "len"), varray(-1));

	bind_string_methodv(get_slice, &String::get_slice, sarray("delimiter", "slice"), varray());
	bind_string_method(get_slicec, sarray("delimiter", "slice"), varray());
	bind_string_methodv(get_slice_count, &String::get_slice_count, sarray("delimiter"), varray());
	bind_string_methodv(find, &String::find, sarray("what", "from"), varray(0));
	bind_string_methodv(findn, &String::findn, sarray("what", "from"), varray(0));
	bind_string_methodv(count, &String::count, sarray("what", "from", "to"), varray(0, 0));
	bind_string_methodv(countn, &String::countn, sarray("what", "from", "to"), varray(0, 0));
	bind_string_methodv(rfind, &String::rfind, sarray("what", "from"), varray(-1));
	bind_string_methodv(rfindn, &String::rfindn, sarray("what", "from"), varray(-1));
	bind_string_method(match, sarray("expr"), varray());
	bind_string_method(matchn, sarray("expr"), varray());
	bind_string_methodv(begins_with, &String::begins_with, sarray("text"), varray());
	bind_string_methodv(ends_with, &String::ends_with, sarray("text"), varray());
	bind_string_method(is_subsequence_of, sarray("text"), varray());
	bind_string_method(is_subsequence_ofn, sarray("text"), varray());
	bind_string_method(bigrams, sarray(), varray());
	bind_string_method(similarity, sarray("text"), varray());

	bind_string_method(format, sarray("values", "placeholder"), varray("{_}"));
	bind_string_methodv(replace, &String::replace, sarray("what", "forwhat"), varray());
	bind_string_methodv(replacen, &String::replacen, sarray("what", "forwhat"), varray());
	bind_string_method(replace_char, sarray("key", "with"), varray());
	bind_string_methodv(replace_chars, &String::replace_chars, sarray("keys", "with"), varray());
	bind_string_method(remove_char, sarray("what"), varray());
	bind_string_methodv(remove_chars, &String::remove_chars, sarray("chars"), varray());
	bind_string_method(repeat, sarray("count"), varray());
	bind_string_method(reverse, sarray(), varray());
	bind_string_method(insert, sarray("position", "what"), varray());
	bind_string_method(erase, sarray("position", "chars"), varray(1));
	bind_string_method(capitalize, sarray(), varray());
	bind_string_method(to_camel_case, sarray(), varray());
	bind_string_method(to_pascal_case, sarray(), varray());
	bind_string_method(to_snake_case, sarray(), varray());
	bind_string_method(to_kebab_case, sarray(), varray());
	bind_string_methodv(split, &String::split, sarray("delimiter", "allow_empty", "maxsplit"), varray("", true, 0));
	bind_string_methodv(rsplit, &String::rsplit, sarray("delimiter", "allow_empty", "maxsplit"), varray("", true, 0));
	bind_string_method(split_floats, sarray("delimiter", "allow_empty"), varray(true));
	bind_string_method(join, sarray("parts"), varray());

	bind_string_method(to_upper, sarray(), varray());
	bind_string_method(to_lower, sarray(), varray());

	bind_string_method(left, sarray("length"), varray());
	bind_string_method(right, sarray("length"), varray());

	bind_string_method(strip_edges, sarray("left", "right"), varray(true, true));
	bind_string_method(strip_escapes, sarray(), varray());
	bind_string_method(lstrip, sarray("chars"), varray());
	bind_string_method(rstrip, sarray("chars"), varray());
	bind_string_method(get_extension, sarray(), varray());
	bind_string_method(get_basename, sarray(), varray());
	bind_string_method(path_join, sarray("path"), varray());
	bind_string_method(unicode_at, sarray("at"), varray());
	bind_string_method(indent, sarray("prefix"), varray());
	bind_string_method(dedent, sarray(), varray());
	bind_method(String, hash, sarray(), varray());
	bind_string_method(md5_text, sarray(), varray());
	bind_string_method(sha1_text, sarray(), varray());
	bind_string_method(sha256_text, sarray(), varray());
	bind_string_method(md5_buffer, sarray(), varray());
	bind_string_method(sha1_buffer, sarray(), varray());
	bind_string_method(sha256_buffer, sarray(), varray());
	bind_string_method(is_empty, sarray(), varray());
	bind_string_methodv(contains, &String::contains, sarray("what"), varray());
	bind_string_methodv(containsn, &String::containsn, sarray("what"), varray());

	bind_string_method(is_absolute_path, sarray(), varray());
	bind_string_method(is_relative_path, sarray(), varray());
	bind_string_method(simplify_path, sarray(), varray());
	bind_string_method(get_base_dir, sarray(), varray());
	bind_string_method(get_file, sarray(), varray());
	bind_string_method(xml_escape, sarray("escape_quotes"), varray(false));
	bind_string_method(xml_unescape, sarray(), varray());
	bind_string_method(uri_encode, sarray(), varray());
	bind_string_method(uri_decode, sarray(), varray());
	bind_string_method(uri_file_decode, sarray(), varray());
	bind_string_method(c_escape, sarray(), varray());
	bind_string_method(c_unescape, sarray(), varray());
	bind_string_method(json_escape, sarray(), varray());

	bind_string_method(validate_node_name, sarray(), varray());
	bind_string_method(validate_filename, sarray(), varray());

	bind_string_method(is_valid_ascii_identifier, sarray(), varray());
	bind_string_method(is_valid_unicode_identifier, sarray(), varray());
	bind_string_method(is_valid_identifier, sarray(), varray());
	bind_string_method(is_valid_int, sarray(), varray());
	bind_string_method(is_valid_float, sarray(), varray());
	bind_string_method(is_valid_hex_number, sarray("with_prefix"), varray(false));
	bind_string_method(is_valid_html_color, sarray(), varray());
	bind_string_method(is_valid_ip_address, sarray(), varray());
	bind_string_method(is_valid_filename, sarray(), varray());

	bind_string_method(to_int, sarray(), varray());
	bind_string_method(to_float, sarray(), varray());
	bind_string_method(hex_to_int, sarray(), varray());
	bind_string_method(bin_to_int, sarray(), varray());

	bind_string_method(lpad, sarray("min_length", "character"), varray(" "));
	bind_string_method(rpad, sarray("min_length", "character"), varray(" "));
	bind_string_method(pad_decimals, sarray("digits"), varray());
	bind_string_method(pad_zeros, sarray("digits"), varray());
	bind_string_methodv(trim_prefix, static_cast<String (String::*)(const String&) const>(&String::trim_prefix), sarray("prefix"), varray());
	bind_string_methodv(trim_suffix, static_cast<String (String::*)(const String&) const>(&String::trim_suffix), sarray("suffix"), varray());

	bind_string_method(to_ascii_buffer, sarray(), varray());
	bind_string_method(to_utf8_buffer, sarray(), varray());
	bind_string_method(to_utf16_buffer, sarray(), varray());
	bind_string_method(to_utf32_buffer, sarray(), varray());
	bind_string_method(to_wchar_buffer, sarray(), varray());
	bind_string_method(to_multibyte_char_buffer, sarray("encoding"), varray(String()));
	bind_string_method(hex_decode, sarray(), varray());

	bind_static_methodv(String, num_scientific, static_cast<String (*)(double)>(&String::num_scientific), sarray("number"), varray());
	bind_static_method(String, num, sarray("number", "decimals"), varray(-1));
	bind_static_method(String, num_int64, sarray("number", "base", "capitalize_hex"), varray(10, false));
	bind_static_method(String, num_uint64, sarray("number", "base", "capitalize_hex"), varray(10, false));
	bind_static_method(String, chr, sarray("code"), varray());
	bind_static_method(String, humanize_size, sarray("size"), varray());

	/* StringName */

	bind_method(StringName, hash, sarray(), varray());
}

static void _register_variant_builtin_methods_math() {
	/* Vector2 */

	bind_method(Vector2, angle, sarray(), varray());
	bind_method(Vector2, angle_to, sarray("to"), varray());
	bind_method(Vector2, angle_to_point, sarray("to"), varray());
	bind_method(Vector2, direction_to, sarray("to"), varray());
	bind_method(Vector2, distance_to, sarray("to"), varray());
	bind_method(Vector2, distance_squared_to, sarray("to"), varray());
	bind_method(Vector2, length, sarray(), varray());
	bind_method(Vector2, length_squared, sarray(), varray());
	bind_method(Vector2, limit_length, sarray("length"), varray(1.0));
	bind_method(Vector2, normalized, sarray(), varray());
	bind_method(Vector2, is_normalized, sarray(), varray());
	bind_method(Vector2, is_equal_approx, sarray("to"), varray());
	bind_method(Vector2, is_zero_approx, sarray(), varray());
	bind_method(Vector2, is_finite, sarray(), varray());
	bind_method(Vector2, posmod, sarray("mod"), varray());
	bind_method(Vector2, posmodv, sarray("modv"), varray());
	bind_method(Vector2, project, sarray("b"), varray());
	bind_method(Vector2, lerp, sarray("to", "weight"), varray());
	bind_method(Vector2, slerp, sarray("to", "weight"), varray());
	bind_method(Vector2, cubic_interpolate, sarray("b", "pre_a", "post_b", "weight"), varray());
	bind_method(Vector2, cubic_interpolate_in_time, sarray("b", "pre_a", "post_b", "weight", "b_t", "pre_a_t", "post_b_t"), varray());
	bind_method(Vector2, bezier_interpolate, sarray("control_1", "control_2", "end", "t"), varray());
	bind_method(Vector2, bezier_derivative, sarray("control_1", "control_2", "end", "t"), varray());
	bind_method(Vector2, max_axis_index, sarray(), varray());
	bind_method(Vector2, min_axis_index, sarray(), varray());
	bind_method(Vector2, move_toward, sarray("to", "delta"), varray());
	bind_method(Vector2, rotated, sarray("angle"), varray());
	bind_method(Vector2, orthogonal, sarray(), varray());
	bind_method(Vector2, floor, sarray(), varray());
	bind_method(Vector2, ceil, sarray(), varray());
	bind_method(Vector2, round, sarray(), varray());
	bind_method(Vector2, aspect, sarray(), varray());
	bind_method(Vector2, dot, sarray("with"), varray());
	bind_method(Vector2, slide, sarray("n"), varray());
	bind_method(Vector2, bounce, sarray("n"), varray());
	bind_method(Vector2, reflect, sarray("line"), varray());
	bind_method(Vector2, cross, sarray("with"), varray());
	bind_method(Vector2, abs, sarray(), varray());
	bind_method(Vector2, sign, sarray(), varray());
	bind_method(Vector2, clamp, sarray("min", "max"), varray());
	bind_method(Vector2, clampf, sarray("min", "max"), varray());
	bind_method(Vector2, snapped, sarray("step"), varray());
	bind_method(Vector2, snappedf, sarray("step"), varray());
	bind_method(Vector2, min, sarray("with"), varray());
	bind_method(Vector2, minf, sarray("with"), varray());
	bind_method(Vector2, max, sarray("with"), varray());
	bind_method(Vector2, maxf, sarray("with"), varray());

	bind_static_method(Vector2, from_angle, sarray("angle"), varray());

	/* Vector2i */

	bind_method(Vector2i, aspect, sarray(), varray());
	bind_method(Vector2i, max_axis_index, sarray(), varray());
	bind_method(Vector2i, min_axis_index, sarray(), varray());
	bind_method(Vector2i, distance_to, sarray("to"), varray());
	bind_method(Vector2i, distance_squared_to, sarray("to"), varray());
	bind_method(Vector2i, length, sarray(), varray());
	bind_method(Vector2i, length_squared, sarray(), varray());
	bind_method(Vector2i, sign, sarray(), varray());
	bind_method(Vector2i, abs, sarray(), varray());
	bind_method(Vector2i, clamp, sarray("min", "max"), varray());
	bind_method(Vector2i, clampi, sarray("min", "max"), varray());
	bind_method(Vector2i, snapped, sarray("step"), varray());
	bind_method(Vector2i, snappedi, sarray("step"), varray());
	bind_method(Vector2i, min, sarray("with"), varray());
	bind_method(Vector2i, mini, sarray("with"), varray());
	bind_method(Vector2i, max, sarray("with"), varray());
	bind_method(Vector2i, maxi, sarray("with"), varray());

	/* Rect2 */

	bind_method(Rect2, get_center, sarray(), varray());
	bind_method(Rect2, get_area, sarray(), varray());
	bind_method(Rect2, has_area, sarray(), varray());
	bind_method(Rect2, has_point, sarray("point"), varray());
	bind_method(Rect2, is_equal_approx, sarray("rect"), varray());
	bind_method(Rect2, is_finite, sarray(), varray());
	bind_method(Rect2, intersects, sarray("b", "include_borders"), varray(false));
	bind_method(Rect2, encloses, sarray("b"), varray());
	bind_method(Rect2, intersection, sarray("b"), varray());
	bind_method(Rect2, merge, sarray("b"), varray());
	bind_method(Rect2, expand, sarray("to"), varray());
	bind_method(Rect2, get_support, sarray("direction"), varray());
	bind_method(Rect2, grow, sarray("amount"), varray());
	bind_methodv(Rect2, grow_side, &Rect2::grow_side_bind, sarray("side", "amount"), varray());
	bind_method(Rect2, grow_individual, sarray("left", "top", "right", "bottom"), varray());
	bind_method(Rect2, abs, sarray(), varray());

	/* Rect2i */

	bind_method(Rect2i, get_center, sarray(), varray());
	bind_method(Rect2i, get_area, sarray(), varray());
	bind_method(Rect2i, has_area, sarray(), varray());
	bind_method(Rect2i, has_point, sarray("point"), varray());
	bind_method(Rect2i, intersects, sarray("b"), varray());
	bind_method(Rect2i, encloses, sarray("b"), varray());
	bind_method(Rect2i, intersection, sarray("b"), varray());
	bind_method(Rect2i, merge, sarray("b"), varray());
	bind_method(Rect2i, expand, sarray("to"), varray());
	bind_method(Rect2i, grow, sarray("amount"), varray());
	bind_methodv(Rect2i, grow_side, &Rect2i::grow_side_bind, sarray("side", "amount"), varray());
	bind_method(Rect2i, grow_individual, sarray("left", "top", "right", "bottom"), varray());
	bind_method(Rect2i, abs, sarray(), varray());

	/* Vector3 */

	bind_method(Vector3, min_axis_index, sarray(), varray());
	bind_method(Vector3, max_axis_index, sarray(), varray());
	bind_method(Vector3, angle_to, sarray("to"), varray());
	bind_method(Vector3, signed_angle_to, sarray("to", "axis"), varray());
	bind_method(Vector3, direction_to, sarray("to"), varray());
	bind_method(Vector3, distance_to, sarray("to"), varray());
	bind_method(Vector3, distance_squared_to, sarray("to"), varray());
	bind_method(Vector3, length, sarray(), varray());
	bind_method(Vector3, length_squared, sarray(), varray());
	bind_method(Vector3, limit_length, sarray("length"), varray(1.0));
	bind_method(Vector3, normalized, sarray(), varray());
	bind_method(Vector3, is_normalized, sarray(), varray());
	bind_method(Vector3, is_equal_approx, sarray("to"), varray());
	bind_method(Vector3, is_zero_approx, sarray(), varray());
	bind_method(Vector3, is_finite, sarray(), varray());
	bind_method(Vector3, inverse, sarray(), varray());
	bind_method(Vector3, clamp, sarray("min", "max"), varray());
	bind_method(Vector3, clampf, sarray("min", "max"), varray());
	bind_method(Vector3, snapped, sarray("step"), varray());
	bind_method(Vector3, snappedf, sarray("step"), varray());
	bind_method(Vector3, rotated, sarray("axis", "angle"), varray());
	bind_method(Vector3, lerp, sarray("to", "weight"), varray());
	bind_method(Vector3, slerp, sarray("to", "weight"), varray());
	bind_method(Vector3, cubic_interpolate, sarray("b", "pre_a", "post_b", "weight"), varray());
	bind_method(Vector3, cubic_interpolate_in_time, sarray("b", "pre_a", "post_b", "weight", "b_t", "pre_a_t", "post_b_t"), varray());
	bind_method(Vector3, bezier_interpolate, sarray("control_1", "control_2", "end", "t"), varray());
	bind_method(Vector3, bezier_derivative, sarray("control_1", "control_2", "end", "t"), varray());
	bind_method(Vector3, move_toward, sarray("to", "delta"), varray());
	bind_method(Vector3, dot, sarray("with"), varray());
	bind_method(Vector3, cross, sarray("with"), varray());
	bind_method(Vector3, outer, sarray("with"), varray());
	bind_method(Vector3, abs, sarray(), varray());
	bind_method(Vector3, floor, sarray(), varray());
	bind_method(Vector3, ceil, sarray(), varray());
	bind_method(Vector3, round, sarray(), varray());
	bind_method(Vector3, posmod, sarray("mod"), varray());
	bind_method(Vector3, posmodv, sarray("modv"), varray());
	bind_method(Vector3, project, sarray("b"), varray());
	bind_method(Vector3, slide, sarray("n"), varray());
	bind_method(Vector3, bounce, sarray("n"), varray());
	bind_method(Vector3, reflect, sarray("n"), varray());
	bind_method(Vector3, sign, sarray(), varray());
	bind_method(Vector3, octahedron_encode, sarray(), varray());
	bind_method(Vector3, min, sarray("with"), varray());
	bind_method(Vector3, minf, sarray("with"), varray());
	bind_method(Vector3, max, sarray("with"), varray());
	bind_method(Vector3, maxf, sarray("with"), varray());
	bind_static_method(Vector3, octahedron_decode, sarray("uv"), varray());

	/* Vector3i */

	bind_method(Vector3i, min_axis_index, sarray(), varray());
	bind_method(Vector3i, max_axis_index, sarray(), varray());
	bind_method(Vector3i, distance_to, sarray("to"), varray());
	bind_method(Vector3i, distance_squared_to, sarray("to"), varray());
	bind_method(Vector3i, length, sarray(), varray());
	bind_method(Vector3i, length_squared, sarray(), varray());
	bind_method(Vector3i, sign, sarray(), varray());
	bind_method(Vector3i, abs, sarray(), varray());
	bind_method(Vector3i, clamp, sarray("min", "max"), varray());
	bind_method(Vector3i, clampi, sarray("min", "max"), varray());
	bind_method(Vector3i, snapped, sarray("step"), varray());
	bind_method(Vector3i, snappedi, sarray("step"), varray());
	bind_method(Vector3i, min, sarray("with"), varray());
	bind_method(Vector3i, mini, sarray("with"), varray());
	bind_method(Vector3i, max, sarray("with"), varray());
	bind_method(Vector3i, maxi, sarray("with"), varray());

	/* Vector4 */

	bind_method(Vector4, min_axis_index, sarray(), varray());
	bind_method(Vector4, max_axis_index, sarray(), varray());
	bind_method(Vector4, length, sarray(), varray());
	bind_method(Vector4, length_squared, sarray(), varray());
	bind_method(Vector4, abs, sarray(), varray());
	bind_method(Vector4, sign, sarray(), varray());
	bind_method(Vector4, floor, sarray(), varray());
	bind_method(Vector4, ceil, sarray(), varray());
	bind_method(Vector4, round, sarray(), varray());
	bind_method(Vector4, lerp, sarray("to", "weight"), varray());
	bind_method(Vector4, cubic_interpolate, sarray("b", "pre_a", "post_b", "weight"), varray());
	bind_method(Vector4, cubic_interpolate_in_time, sarray("b", "pre_a", "post_b", "weight", "b_t", "pre_a_t", "post_b_t"), varray());
	bind_method(Vector4, posmod, sarray("mod"), varray());
	bind_method(Vector4, posmodv, sarray("modv"), varray());
	bind_method(Vector4, snapped, sarray("step"), varray());
	bind_method(Vector4, snappedf, sarray("step"), varray());
	bind_method(Vector4, clamp, sarray("min", "max"), varray());
	bind_method(Vector4, clampf, sarray("min", "max"), varray());
	bind_method(Vector4, normalized, sarray(), varray());
	bind_method(Vector4, is_normalized, sarray(), varray());
	bind_method(Vector4, direction_to, sarray("to"), varray());
	bind_method(Vector4, distance_to, sarray("to"), varray());
	bind_method(Vector4, distance_squared_to, sarray("to"), varray());
	bind_method(Vector4, dot, sarray("with"), varray());
	bind_method(Vector4, inverse, sarray(), varray());
	bind_method(Vector4, is_equal_approx, sarray("to"), varray());
	bind_method(Vector4, is_zero_approx, sarray(), varray());
	bind_method(Vector4, is_finite, sarray(), varray());
	bind_method(Vector4, min, sarray("with"), varray());
	bind_method(Vector4, minf, sarray("with"), varray());
	bind_method(Vector4, max, sarray("with"), varray());
	bind_method(Vector4, maxf, sarray("with"), varray());

	/* Vector4i */

	bind_method(Vector4i, min_axis_index, sarray(), varray());
	bind_method(Vector4i, max_axis_index, sarray(), varray());
	bind_method(Vector4i, length, sarray(), varray());
	bind_method(Vector4i, length_squared, sarray(), varray());
	bind_method(Vector4i, sign, sarray(), varray());
	bind_method(Vector4i, abs, sarray(), varray());
	bind_method(Vector4i, clamp, sarray("min", "max"), varray());
	bind_method(Vector4i, clampi, sarray("min", "max"), varray());
	bind_method(Vector4i, snapped, sarray("step"), varray());
	bind_method(Vector4i, snappedi, sarray("step"), varray());
	bind_method(Vector4i, min, sarray("with"), varray());
	bind_method(Vector4i, mini, sarray("with"), varray());
	bind_method(Vector4i, max, sarray("with"), varray());
	bind_method(Vector4i, maxi, sarray("with"), varray());
	bind_method(Vector4i, distance_to, sarray("to"), varray());
	bind_method(Vector4i, distance_squared_to, sarray("to"), varray());

	/* Plane */

	bind_method(Plane, normalized, sarray(), varray());
	bind_method(Plane, get_center, sarray(), varray());
	bind_method(Plane, is_equal_approx, sarray("to_plane"), varray());
	bind_method(Plane, is_finite, sarray(), varray());
	bind_method(Plane, is_point_over, sarray("point"), varray());
	bind_method(Plane, distance_to, sarray("point"), varray());
	bind_method(Plane, has_point, sarray("point", "tolerance"), varray(CMP_EPSILON));
	bind_method(Plane, project, sarray("point"), varray());
	bind_methodv(Plane, intersect_3, &Plane::intersect_3_bind, sarray("b", "c"), varray());
	bind_methodv(Plane, intersects_ray, &Plane::intersects_ray_bind, sarray("from", "dir"), varray());
	bind_methodv(Plane, intersects_segment, &Plane::intersects_segment_bind, sarray("from", "to"), varray());

	/* Quaternion */

	bind_method(Quaternion, length, sarray(), varray());
	bind_method(Quaternion, length_squared, sarray(), varray());
	bind_method(Quaternion, normalized, sarray(), varray());
	bind_method(Quaternion, is_normalized, sarray(), varray());
	bind_method(Quaternion, is_equal_approx, sarray("to"), varray());
	bind_method(Quaternion, is_finite, sarray(), varray());
	bind_method(Quaternion, inverse, sarray(), varray());
	bind_method(Quaternion, log, sarray(), varray());
	bind_method(Quaternion, exp, sarray(), varray());
	bind_method(Quaternion, angle_to, sarray("to"), varray());
	bind_method(Quaternion, dot, sarray("with"), varray());
	bind_method(Quaternion, slerp, sarray("to", "weight"), varray());
	bind_method(Quaternion, slerpni, sarray("to", "weight"), varray());
	bind_method(Quaternion, spherical_cubic_interpolate, sarray("b", "pre_a", "post_b", "weight"), varray());
	bind_method(Quaternion, spherical_cubic_interpolate_in_time, sarray("b", "pre_a", "post_b", "weight", "b_t", "pre_a_t", "post_b_t"), varray());
	bind_method(Quaternion, get_euler, sarray("order"), varray((int64_t)EulerOrder::EULER_ORDER_YXZ));
	bind_static_method(Quaternion, from_euler, sarray("euler"), varray());
	bind_method(Quaternion, get_axis, sarray(), varray());
	bind_method(Quaternion, get_angle, sarray(), varray());

	/* Color */

	bind_method(Color, to_argb32, sarray(), varray());
	bind_method(Color, to_abgr32, sarray(), varray());
	bind_method(Color, to_rgba32, sarray(), varray());
	bind_method(Color, to_argb64, sarray(), varray());
	bind_method(Color, to_abgr64, sarray(), varray());
	bind_method(Color, to_rgba64, sarray(), varray());
	bind_method(Color, to_html, sarray("with_alpha"), varray(true));

	bind_method(Color, clamp, sarray("min", "max"), varray(Color(0, 0, 0, 0), Color(1, 1, 1, 1)));
	bind_method(Color, inverted, sarray(), varray());
	bind_method(Color, lerp, sarray("to", "weight"), varray());
	bind_method(Color, lightened, sarray("amount"), varray());
	bind_method(Color, darkened, sarray("amount"), varray());
	bind_method(Color, blend, sarray("over"), varray());
	bind_method(Color, get_luminance, sarray(), varray());
	bind_method(Color, srgb_to_linear, sarray(), varray());
	bind_method(Color, linear_to_srgb, sarray(), varray());

	bind_method(Color, is_equal_approx, sarray("to"), varray());

	bind_static_method(Color, hex, sarray("hex"), varray());
	bind_static_method(Color, hex64, sarray("hex"), varray());
	bind_static_method(Color, html, sarray("rgba"), varray());
	bind_static_method(Color, html_is_valid, sarray("color"), varray());

	bind_static_method(Color, from_string, sarray("str", "default"), varray());
	bind_static_method(Color, from_hsv, sarray("h", "s", "v", "alpha"), varray(1.0));
	// TODO:
	// bind_static_method(Color, from_ok_hsl, sarray("h", "s", "l", "alpha"), varray(1.0));
	bind_static_method(Color, from_rgbe9995, sarray("rgbe"), varray());
	bind_static_method(Color, from_rgba8, sarray("r8", "g8", "b8", "a8"), varray(255));
}

// VARIANT_ENUM_CAST(ResourceDeepDuplicateMode);

Vector3 Vector3_LEFT = Vector3{-1, 0, 0};
Vector3 Vector3_RIGHT = Vector3{1, 0, 0};
Vector3 Vector3_UP = Vector3{0, 1, 0};
Vector3 Vector3_DOWN = Vector3{0, -1, 0};
Vector3 Vector3_FORWARD = Vector3{0, 0, -1};
Vector3 Vector3_BACK = Vector3{0, 0, 1};
Vector3 Vector3_MODEL_LEFT = Vector3{1, 0, 0};
Vector3 Vector3_MODEL_RIGHT = Vector3{-1, 0, 0};
Vector3 Vector3_MODEL_TOP = Vector3{0, 1, 0};
Vector3 Vector3_MODEL_BOTTOM = Vector3{0, -1, 0};
Vector3 Vector3_MODEL_FRONT = Vector3{0, 0, 1};
Vector3 Vector3_MODEL_REAR = Vector3{0, 0, -1};

Vector3i Vector3i_LEFT = Vector3i{-1, 0, 0};
Vector3i Vector3i_RIGHT = Vector3i{1, 0, 0};
Vector3i Vector3i_UP = Vector3i{0, 1, 0};
Vector3i Vector3i_DOWN = Vector3i{0, -1, 0};
Vector3i Vector3i_FORWARD = Vector3i{0, 0, -1};
Vector3i Vector3i_BACK = Vector3i{0, 0, 1};

Vector2 Vector2_LEFT = Vector2{-1, 0};
Vector2 Vector2_RIGHT = Vector2{1, 0};
Vector2 Vector2_UP = Vector2{0, -1};
Vector2 Vector2_DOWN = Vector2{0, 1};

Vector2i Vector2i_LEFT = Vector2i{-1, 0};
Vector2i Vector2i_RIGHT = Vector2i{1, 0};
Vector2i Vector2i_UP = Vector2i{0, -1};
Vector2i Vector2i_DOWN = Vector2i{0, 1};

Transform2D Transform2D_FLIP_X = Transform2D{{-1, 0}, {0, 1}, {0, 0}};
Transform2D Transform2D_FLIP_Y = Transform2D{{1, 0}, {0, -1}, {0, 0}};

Basis Basis_FLIP_X = Basis{{-1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
Basis Basis_FLIP_Y = Basis{{1, 0, 0}, {0, -1, 0}, {0, 0, 1}};
Basis Basis_FLIP_Z = Basis{{1, 0, 0}, {0, 1, 0}, {0, 0, -1}};

Transform3D Transform3D_FLIP_X = Transform3D{Basis_FLIP_X};
Transform3D Transform3D_FLIP_Y = Transform3D{Basis_FLIP_Y};
Transform3D Transform3D_FLIP_Z = Transform3D{Basis_FLIP_Z};

Plane Plane_PLANE_YZ = Plane{1, 0, 0, 0};
Plane Plane_PLANE_XZ = Plane{0, 1, 0, 0};
Plane Plane_PLANE_XY = Plane{0, 0, 1, 0};

void func_Callable_call(Variant* v, const Variant** p_args, int p_argcount, Variant& r_ret, GDExtensionCallError& r_error) {
	r_ret = Callable(*v).call(p_args);
};

void func_Callable_call_deferred(Variant* v, const Variant** p_args, int p_argcount, Variant& r_ret, GDExtensionCallError& r_error) {
	Callable(*v).call_deferred(p_args);
}

void func_Callable_rpc(Variant* v, const Variant** p_args, int p_argcount, Variant& r_ret, GDExtensionCallError& r_error) {
	Callable(*v).rpc(p_args);
}

void func_Callable_rpc_id(Variant* v, const Variant** p_args, int p_argcount, Variant& r_ret, GDExtensionCallError& r_error) {
	if (p_argcount == 0) {
		r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
	} else if (p_args[0]->get_type() != Variant::INT) {
		r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::INT;
	} else {
		Callable(*v).rpc_id(*p_args[0], &p_args[1]);
	}
}

void func_Callable_bind(Variant* v, const Variant** p_args, int p_argcount, Variant& r_ret, GDExtensionCallError& r_error) {
	r_ret = Callable(*v).bind(p_args);
}

void func_Signal_emit(Variant* v, const Variant** p_args, int p_argcount, Variant& r_ret, GDExtensionCallError& r_error) {
	Signal(*v).emit(p_args);
}

static void _register_variant_builtin_methods_misc() {
	/* RID */

	bind_method(RID, is_valid, sarray(), varray());
	bind_method(RID, get_id, sarray(), varray());

	/* NodePath */

	bind_method(NodePath, is_absolute, sarray(), varray());
	bind_method(NodePath, get_name_count, sarray(), varray());
	bind_method(NodePath, get_name, sarray("idx"), varray());
	bind_method(NodePath, get_subname_count, sarray(), varray());
	bind_method(NodePath, hash, sarray(), varray());
	bind_method(NodePath, get_subname, sarray("idx"), varray());
	bind_method(NodePath, get_concatenated_names, sarray(), varray());
	bind_method(NodePath, get_concatenated_subnames, sarray(), varray());
	bind_method(NodePath, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(NodePath, get_as_property_path, sarray(), varray());
	bind_method(NodePath, is_empty, sarray(), varray());

	/* Callable */

	bind_static_method(Callable, create, sarray("variant", "method"), varray());
	bind_method(Callable, callv, sarray("arguments"), varray());
	bind_method(Callable, is_null, sarray(), varray());
	bind_method(Callable, is_custom, sarray(), varray());
	bind_method(Callable, is_standard, sarray(), varray());
	bind_method(Callable, is_valid, sarray(), varray());
	bind_method(Callable, get_object, sarray(), varray());
	bind_method(Callable, get_object_id, sarray(), varray());
	bind_method(Callable, get_method, sarray(), varray());
	bind_method(Callable, get_argument_count, sarray(), varray());
	bind_method(Callable, get_bound_arguments_count, sarray(), varray());
	bind_method(Callable, get_bound_arguments, sarray(), varray());
	bind_method(Callable, get_unbound_arguments_count, sarray(), varray());
	bind_method(Callable, hash, sarray(), varray());
	bind_method(Callable, bindv, sarray("arguments"), varray());
	bind_method(Callable, unbind, sarray("argcount"), varray());

	bind_custom(Callable, call, func_Callable_call, true, Variant);
	bind_custom(Callable, call_deferred, func_Callable_call_deferred, false, Variant);
	bind_custom(Callable, rpc, func_Callable_rpc, false, Variant);
	bind_custom1(Callable, rpc_id, func_Callable_rpc_id, Variant::INT, "peer_id");
	bind_custom(Callable, bind, func_Callable_bind, true, Callable);

	/* Signal */

	bind_method(Signal, is_null, sarray(), varray());
	bind_method(Signal, get_object, sarray(), varray());
	bind_method(Signal, get_object_id, sarray(), varray());
	bind_method(Signal, get_name, sarray(), varray());

	bind_method(Signal, connect, sarray("callable", "flags"), varray(0));
	bind_method(Signal, disconnect, sarray("callable"), varray());
	bind_method(Signal, is_connected, sarray("callable"), varray());
	bind_method(Signal, get_connections, sarray(), varray());
	bind_method(Signal, has_connections, sarray(), varray());

	bind_custom(Signal, emit, func_Signal_emit, false, Variant);

	/* Transform2D */

	bind_method(Transform2D, inverse, sarray(), varray());
	bind_method(Transform2D, affine_inverse, sarray(), varray());
	bind_method(Transform2D, get_rotation, sarray(), varray());
	bind_method(Transform2D, get_origin, sarray(), varray());
	bind_method(Transform2D, get_scale, sarray(), varray());
	bind_method(Transform2D, get_skew, sarray(), varray());
	bind_method(Transform2D, orthonormalized, sarray(), varray());
	bind_method(Transform2D, rotated, sarray("angle"), varray());
	bind_method(Transform2D, rotated_local, sarray("angle"), varray());
	bind_method(Transform2D, scaled, sarray("scale"), varray());
	bind_method(Transform2D, scaled_local, sarray("scale"), varray());
	bind_method(Transform2D, translated, sarray("offset"), varray());
	bind_method(Transform2D, translated_local, sarray("offset"), varray());
	bind_method(Transform2D, determinant, sarray(), varray());
	bind_method(Transform2D, basis_xform, sarray("v"), varray());
	bind_method(Transform2D, basis_xform_inv, sarray("v"), varray());
	bind_method(Transform2D, interpolate_with, sarray("xform", "weight"), varray());
	bind_method(Transform2D, is_conformal, sarray(), varray());
	bind_method(Transform2D, is_equal_approx, sarray("xform"), varray());
	bind_method(Transform2D, is_finite, sarray(), varray());
	// Do not bind functions like set_rotation, set_scale, set_skew, etc because this type is immutable and can't be modified.
	bind_method(Transform2D, looking_at, sarray("target"), varray(Vector2()));

	/* Basis */

	bind_method(Basis, inverse, sarray(), varray());
	bind_method(Basis, transposed, sarray(), varray());
	bind_method(Basis, orthonormalized, sarray(), varray());
	bind_method(Basis, determinant, sarray(), varray());
	bind_methodv(Basis, rotated, static_cast<Basis (Basis::*)(const Vector3&, real_t) const>(&Basis::rotated), sarray("axis", "angle"), varray());
	bind_method(Basis, scaled, sarray("scale"), varray());
	bind_method(Basis, scaled_local, sarray("scale"), varray());
	bind_method(Basis, get_scale, sarray(), varray());
	bind_method(Basis, get_euler, sarray("order"), varray((int64_t)EulerOrder::EULER_ORDER_YXZ));
	bind_method(Basis, tdotx, sarray("with"), varray());
	bind_method(Basis, tdoty, sarray("with"), varray());
	bind_method(Basis, tdotz, sarray("with"), varray());
	bind_method(Basis, slerp, sarray("to", "weight"), varray());
	bind_method(Basis, is_conformal, sarray(), varray());
	bind_method(Basis, is_equal_approx, sarray("b"), varray());
	bind_method(Basis, is_finite, sarray(), varray());
	bind_method(Basis, get_rotation_quaternion, sarray(), varray());
	bind_static_method(Basis, looking_at, sarray("target", "up", "use_model_front"), varray(Vector3_UP, false));
	bind_static_method(Basis, from_scale, sarray("scale"), varray());
	bind_static_method(Basis, from_euler, sarray("euler", "order"), varray((int64_t)EulerOrder::EULER_ORDER_YXZ));

	/* AABB */

	bind_method(AABB, abs, sarray(), varray());
	bind_method(AABB, get_center, sarray(), varray());
	bind_method(AABB, get_volume, sarray(), varray());
	bind_method(AABB, has_volume, sarray(), varray());
	bind_method(AABB, has_surface, sarray(), varray());
	bind_method(AABB, has_point, sarray("point"), varray());
	bind_method(AABB, is_equal_approx, sarray("aabb"), varray());
	bind_method(AABB, is_finite, sarray(), varray());
	bind_method(AABB, intersects, sarray("with"), varray());
	bind_method(AABB, encloses, sarray("with"), varray());
	bind_method(AABB, intersects_plane, sarray("plane"), varray());
	bind_method(AABB, intersection, sarray("with"), varray());
	bind_method(AABB, merge, sarray("with"), varray());
	bind_method(AABB, expand, sarray("to_point"), varray());
	bind_method(AABB, grow, sarray("by"), varray());
	bind_method(AABB, get_support, sarray("direction"), varray());
	bind_method(AABB, get_longest_axis, sarray(), varray());
	bind_method(AABB, get_longest_axis_index, sarray(), varray());
	bind_method(AABB, get_longest_axis_size, sarray(), varray());
	bind_method(AABB, get_shortest_axis, sarray(), varray());
	bind_method(AABB, get_shortest_axis_index, sarray(), varray());
	bind_method(AABB, get_shortest_axis_size, sarray(), varray());
	bind_method(AABB, get_endpoint, sarray("idx"), varray());
	bind_methodv(AABB, intersects_segment, &AABB::intersects_segment_bind, sarray("from", "to"), varray());
	bind_methodv(AABB, intersects_ray, &AABB::intersects_ray_bind, sarray("from", "dir"), varray());

	/* Transform3D */

	bind_method(Transform3D, inverse, sarray(), varray());
	bind_method(Transform3D, affine_inverse, sarray(), varray());
	bind_method(Transform3D, orthonormalized, sarray(), varray());
	bind_method(Transform3D, rotated, sarray("axis", "angle"), varray());
	bind_method(Transform3D, rotated_local, sarray("axis", "angle"), varray());
	bind_method(Transform3D, scaled, sarray("scale"), varray());
	bind_method(Transform3D, scaled_local, sarray("scale"), varray());
	bind_method(Transform3D, translated, sarray("offset"), varray());
	bind_method(Transform3D, translated_local, sarray("offset"), varray());
	bind_method(Transform3D, looking_at, sarray("target", "up", "use_model_front"), varray(Vector3_UP, false));
	bind_method(Transform3D, interpolate_with, sarray("xform", "weight"), varray());
	bind_method(Transform3D, is_equal_approx, sarray("xform"), varray());
	bind_method(Transform3D, is_finite, sarray(), varray());

	/* Projection */

	bind_static_method(Projection, create_depth_correction, sarray("flip_y"), varray());
	bind_static_method(Projection, create_light_atlas_rect, sarray("rect"), varray());
	bind_static_method(Projection, create_perspective, sarray("fovy", "aspect", "z_near", "z_far", "flip_fov"), varray(false));
	bind_static_method(Projection, create_perspective_hmd, sarray("fovy", "aspect", "z_near", "z_far", "flip_fov", "eye", "intraocular_dist", "convergence_dist"), varray());
	bind_static_method(Projection, create_for_hmd, sarray("eye", "aspect", "intraocular_dist", "display_width", "display_to_lens", "oversample", "z_near", "z_far"), varray());
	bind_static_method(Projection, create_orthogonal, sarray("left", "right", "bottom", "top", "z_near", "z_far"), varray());
	bind_static_method(Projection, create_orthogonal_aspect, sarray("size", "aspect", "z_near", "z_far", "flip_fov"), varray(false));
	bind_static_method(Projection, create_frustum, sarray("left", "right", "bottom", "top", "z_near", "z_far"), varray());
	bind_static_method(Projection, create_frustum_aspect, sarray("size", "aspect", "offset", "z_near", "z_far", "flip_fov"), varray(false));
	bind_static_method(Projection, create_fit_aabb, sarray("aabb"), varray());

	bind_method(Projection, determinant, sarray(), varray());
	bind_method(Projection, perspective_znear_adjusted, sarray("new_znear"), varray());
	bind_method(Projection, get_projection_plane, sarray("plane"), varray());
	bind_method(Projection, flipped_y, sarray(), varray());
	bind_method(Projection, jitter_offseted, sarray("offset"), varray());

	bind_static_method(Projection, get_fovy, sarray("fovx", "aspect"), varray());

	bind_method(Projection, get_z_far, sarray(), varray());
	bind_method(Projection, get_z_near, sarray(), varray());
	bind_method(Projection, get_aspect, sarray(), varray());
	bind_method(Projection, get_fov, sarray(), varray());
	bind_method(Projection, is_orthogonal, sarray(), varray());

	bind_method(Projection, get_viewport_half_extents, sarray(), varray());
	bind_method(Projection, get_far_plane_half_extents, sarray(), varray());

	bind_method(Projection, inverse, sarray(), varray());
	bind_method(Projection, get_pixels_per_meter, sarray("for_pixel_width"), varray());
	bind_method(Projection, get_lod_multiplier, sarray(), varray());

	/* Dictionary */

	bind_method(Dictionary, size, sarray(), varray());
	bind_method(Dictionary, is_empty, sarray(), varray());
	bind_method(Dictionary, clear, sarray(), varray());
	bind_method(Dictionary, assign, sarray("dictionary"), varray());
	bind_method(Dictionary, sort, sarray(), varray());
	bind_method(Dictionary, merge, sarray("dictionary", "overwrite"), varray(false));
	bind_method(Dictionary, merged, sarray("dictionary", "overwrite"), varray(false));
	bind_method(Dictionary, has, sarray("key"), varray());
	bind_method(Dictionary, has_all, sarray("keys"), varray());
	bind_method(Dictionary, find_key, sarray("value"), varray());
	bind_method(Dictionary, erase, sarray("key"), varray());
	bind_method(Dictionary, hash, sarray(), varray());
	bind_method(Dictionary, keys, sarray(), varray());
	bind_method(Dictionary, values, sarray(), varray());
	bind_method(Dictionary, duplicate, sarray("deep"), varray(false));
	bind_method(Dictionary, duplicate_deep, sarray("deep_subresources_mode"), varray(0));
	bind_method(Dictionary, get, sarray("key", "default"), varray(Variant()));
	bind_method(Dictionary, get_or_add, sarray("key", "default"), varray(Variant()));
	bind_method(Dictionary, set, sarray("key", "value"), varray());
	bind_method(Dictionary, is_typed, sarray(), varray());
	bind_method(Dictionary, is_typed_key, sarray(), varray());
	bind_method(Dictionary, is_typed_value, sarray(), varray());
	bind_method(Dictionary, is_same_typed, sarray("dictionary"), varray());
	bind_method(Dictionary, is_same_typed_key, sarray("dictionary"), varray());
	bind_method(Dictionary, is_same_typed_value, sarray("dictionary"), varray());
	bind_method(Dictionary, get_typed_key_builtin, sarray(), varray());
	bind_method(Dictionary, get_typed_value_builtin, sarray(), varray());
	bind_method(Dictionary, get_typed_key_class_name, sarray(), varray());
	bind_method(Dictionary, get_typed_value_class_name, sarray(), varray());
	bind_method(Dictionary, get_typed_key_script, sarray(), varray());
	bind_method(Dictionary, get_typed_value_script, sarray(), varray());
	bind_method(Dictionary, make_read_only, sarray(), varray());
	bind_method(Dictionary, is_read_only, sarray(), varray());
	bind_method(Dictionary, recursive_equal, sarray("dictionary", "recursion_count"), varray());
}

static void _register_variant_builtin_methods_array() {
	/* Array */

	bind_method(Array, size, sarray(), varray());
	bind_method(Array, is_empty, sarray(), varray());
	bind_method(Array, clear, sarray(), varray());
	bind_method(Array, hash, sarray(), varray());
	bind_method(Array, assign, sarray("array"), varray());
	bind_method(Array, get, sarray("index"), varray());
	bind_method(Array, set, sarray("index", "value"), varray());
	bind_method(Array, push_back, sarray("value"), varray());
	bind_method(Array, push_front, sarray("value"), varray());
	bind_method(Array, append, sarray("value"), varray());
	bind_method(Array, append_array, sarray("array"), varray());
	bind_method(Array, resize, sarray("size"), varray());
	bind_method(Array, insert, sarray("position", "value"), varray());
	bind_method(Array, remove_at, sarray("position"), varray());
	bind_method(Array, fill, sarray("value"), varray());
	bind_method(Array, erase, sarray("value"), varray());
	bind_method(Array, front, sarray(), varray());
	bind_method(Array, back, sarray(), varray());
	bind_method(Array, pick_random, sarray(), varray());
	bind_method(Array, find, sarray("what", "from"), varray(0));
	bind_method(Array, find_custom, sarray("method", "from"), varray(0));
	bind_method(Array, rfind, sarray("what", "from"), varray(-1));
	bind_method(Array, rfind_custom, sarray("method", "from"), varray(-1));
	bind_method(Array, count, sarray("value"), varray());
	bind_method(Array, has, sarray("value"), varray());
	bind_method(Array, pop_back, sarray(), varray());
	bind_method(Array, pop_front, sarray(), varray());
	bind_method(Array, pop_at, sarray("position"), varray());
	bind_method(Array, sort, sarray(), varray());
	bind_method(Array, sort_custom, sarray("func"), varray());
	bind_method(Array, shuffle, sarray(), varray());
	bind_method(Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(Array, bsearch_custom, sarray("value", "func", "before"), varray(true));
	bind_method(Array, reverse, sarray(), varray());
	bind_method(Array, duplicate, sarray("deep"), varray(false));
	bind_method(Array, duplicate_deep, sarray("deep_subresources_mode"), varray(1));
	bind_method(Array, slice, sarray("begin", "end", "step", "deep"), varray(INT_MAX, 1, false));
	bind_method(Array, filter, sarray("method"), varray());
	bind_method(Array, map, sarray("method"), varray());
	bind_method(Array, reduce, sarray("method", "accum"), varray(Variant()));
	bind_method(Array, any, sarray("method"), varray());
	bind_method(Array, all, sarray("method"), varray());
	bind_method(Array, max, sarray(), varray());
	bind_method(Array, min, sarray(), varray());
	bind_method(Array, is_typed, sarray(), varray());
	bind_method(Array, is_same_typed, sarray("array"), varray());
	bind_method(Array, get_typed_builtin, sarray(), varray());
	bind_method(Array, get_typed_class_name, sarray(), varray());
	bind_method(Array, get_typed_script, sarray(), varray());
	bind_method(Array, make_read_only, sarray(), varray());
	bind_method(Array, is_read_only, sarray(), varray());

	/* Packed*Array get/set (see VARCALL_ARRAY_GETTER_SETTER macro) */
	bind_method(PackedByteArray, get, sarray("index"), varray());
	bind_method(PackedColorArray, get, sarray("index"), varray());
	bind_method(PackedFloat32Array, get, sarray("index"), varray());
	bind_method(PackedFloat64Array, get, sarray("index"), varray());
	bind_method(PackedInt32Array, get, sarray("index"), varray());
	bind_method(PackedInt64Array, get, sarray("index"), varray());
	bind_method(PackedStringArray, get, sarray("index"), varray());
	bind_method(PackedVector2Array, get, sarray("index"), varray());
	bind_method(PackedVector3Array, get, sarray("index"), varray());
	bind_method(PackedVector4Array, get, sarray("index"), varray());

	bind_method(PackedByteArray, set, sarray("index", "value"), varray());
	bind_method(PackedColorArray, set, sarray("index", "value"), varray());
	bind_method(PackedFloat32Array, set, sarray("index", "value"), varray());
	bind_method(PackedFloat64Array, set, sarray("index", "value"), varray());
	bind_method(PackedInt32Array, set, sarray("index", "value"), varray());
	bind_method(PackedInt64Array, set, sarray("index", "value"), varray());
	bind_method(PackedStringArray, set, sarray("index", "value"), varray());
	bind_method(PackedVector2Array, set, sarray("index", "value"), varray());
	bind_method(PackedVector3Array, set, sarray("index", "value"), varray());
	bind_method(PackedVector4Array, set, sarray("index", "value"), varray());

	/* Byte Array */
	bind_method(PackedByteArray, size, sarray(), varray());
	bind_method(PackedByteArray, is_empty, sarray(), varray());
	bind_method(PackedByteArray, push_back, sarray("value"), varray());
	bind_method(PackedByteArray, append, sarray("value"), varray());
	bind_method(PackedByteArray, append_array, sarray("array"), varray());
	bind_method(PackedByteArray, remove_at, sarray("index"), varray());
	bind_method(PackedByteArray, insert, sarray("at_index", "value"), varray());
	bind_method(PackedByteArray, fill, sarray("value"), varray());
	bind_method(PackedByteArray, resize, sarray("new_size"), varray());
	bind_method(PackedByteArray, clear, sarray(), varray());
	bind_method(PackedByteArray, has, sarray("value"), varray());
	bind_method(PackedByteArray, reverse, sarray(), varray());
	bind_method(PackedByteArray, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedByteArray, sort, sarray(), varray());
	bind_method(PackedByteArray, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedByteArray, duplicate, sarray(), varray());
	bind_method(PackedByteArray, find, sarray("value", "from"), varray(0));
	bind_method(PackedByteArray, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedByteArray, count, sarray("value"), varray());
	bind_method(PackedByteArray, erase, sarray("value"), varray());

	bind_method(PackedByteArray, get_string_from_ascii, sarray(), varray());
	bind_method(PackedByteArray, get_string_from_utf8, sarray(), varray());
	bind_method(PackedByteArray, get_string_from_utf16, sarray(), varray());
	bind_method(PackedByteArray, get_string_from_utf32, sarray(), varray());
	bind_method(PackedByteArray, get_string_from_wchar, sarray(), varray());
	bind_method(PackedByteArray, get_string_from_multibyte_char, sarray("encoding"), varray(String()));
	bind_method(PackedByteArray, hex_encode, sarray(), varray());
	bind_method(PackedByteArray, compress, sarray("compression_mode"), varray(0));
	bind_method(PackedByteArray, decompress, sarray("buffer_size", "compression_mode"), varray(0));
	bind_method(PackedByteArray, decompress_dynamic, sarray("max_output_size", "compression_mode"), varray(0));

	bind_method(PackedByteArray, decode_u8, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, decode_s8, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, decode_u16, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, decode_s16, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, decode_u32, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, decode_s32, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, decode_u64, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, decode_s64, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, decode_half, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, decode_float, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, decode_double, sarray("byte_offset"), varray());
	bind_method(PackedByteArray, has_encoded_var, sarray("byte_offset", "allow_objects"), varray(false));
	bind_method(PackedByteArray, decode_var, sarray("byte_offset", "allow_objects"), varray(false));
	bind_method(PackedByteArray, decode_var_size, sarray("byte_offset", "allow_objects"), varray(false));

	bind_method(PackedByteArray, to_int32_array, sarray(), varray());
	bind_method(PackedByteArray, to_int64_array, sarray(), varray());
	bind_method(PackedByteArray, to_float32_array, sarray(), varray());
	bind_method(PackedByteArray, to_float64_array, sarray(), varray());
	bind_method(PackedByteArray, to_vector2_array, sarray(), varray());
	bind_method(PackedByteArray, to_vector3_array, sarray(), varray());
	bind_method(PackedByteArray, to_vector4_array, sarray(), varray());
	bind_method(PackedByteArray, to_color_array, sarray(), varray());

	bind_method(PackedByteArray, bswap16, sarray("offset", "count"), varray(0, -1));
	bind_method(PackedByteArray, bswap32, sarray("offset", "count"), varray(0, -1));
	bind_method(PackedByteArray, bswap64, sarray("offset", "count"), varray(0, -1));

	bind_method(PackedByteArray, encode_u8, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_s8, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_u16, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_s16, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_u32, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_s32, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_u64, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_s64, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_half, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_float, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_double, sarray("byte_offset", "value"), varray());
	bind_method(PackedByteArray, encode_var, sarray("byte_offset", "value", "allow_objects"), varray(false));

	/* Int32 Array */

	bind_method(PackedInt32Array, size, sarray(), varray());
	bind_method(PackedInt32Array, is_empty, sarray(), varray());
	bind_method(PackedInt32Array, push_back, sarray("value"), varray());
	bind_method(PackedInt32Array, append, sarray("value"), varray());
	bind_method(PackedInt32Array, append_array, sarray("array"), varray());
	bind_method(PackedInt32Array, remove_at, sarray("index"), varray());
	bind_method(PackedInt32Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedInt32Array, fill, sarray("value"), varray());
	bind_method(PackedInt32Array, resize, sarray("new_size"), varray());
	bind_method(PackedInt32Array, clear, sarray(), varray());
	bind_method(PackedInt32Array, has, sarray("value"), varray());
	bind_method(PackedInt32Array, reverse, sarray(), varray());
	bind_method(PackedInt32Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedInt32Array, to_byte_array, sarray(), varray());
	bind_method(PackedInt32Array, sort, sarray(), varray());
	bind_method(PackedInt32Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedInt32Array, duplicate, sarray(), varray());
	bind_method(PackedInt32Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedInt32Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedInt32Array, count, sarray("value"), varray());
	bind_method(PackedInt32Array, erase, sarray("value"), varray());

	/* Int64 Array */

	bind_method(PackedInt64Array, size, sarray(), varray());
	bind_method(PackedInt64Array, is_empty, sarray(), varray());
	bind_method(PackedInt64Array, push_back, sarray("value"), varray());
	bind_method(PackedInt64Array, append, sarray("value"), varray());
	bind_method(PackedInt64Array, append_array, sarray("array"), varray());
	bind_method(PackedInt64Array, remove_at, sarray("index"), varray());
	bind_method(PackedInt64Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedInt64Array, fill, sarray("value"), varray());
	bind_method(PackedInt64Array, resize, sarray("new_size"), varray());
	bind_method(PackedInt64Array, clear, sarray(), varray());
	bind_method(PackedInt64Array, has, sarray("value"), varray());
	bind_method(PackedInt64Array, reverse, sarray(), varray());
	bind_method(PackedInt64Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedInt64Array, to_byte_array, sarray(), varray());
	bind_method(PackedInt64Array, sort, sarray(), varray());
	bind_method(PackedInt64Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedInt64Array, duplicate, sarray(), varray());
	bind_method(PackedInt64Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedInt64Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedInt64Array, count, sarray("value"), varray());
	bind_method(PackedInt64Array, erase, sarray("value"), varray());

	/* Float32 Array */

	bind_method(PackedFloat32Array, size, sarray(), varray());
	bind_method(PackedFloat32Array, is_empty, sarray(), varray());
	bind_method(PackedFloat32Array, push_back, sarray("value"), varray());
	bind_method(PackedFloat32Array, append, sarray("value"), varray());
	bind_method(PackedFloat32Array, append_array, sarray("array"), varray());
	bind_method(PackedFloat32Array, remove_at, sarray("index"), varray());
	bind_method(PackedFloat32Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedFloat32Array, fill, sarray("value"), varray());
	bind_method(PackedFloat32Array, resize, sarray("new_size"), varray());
	bind_method(PackedFloat32Array, clear, sarray(), varray());
	bind_method(PackedFloat32Array, has, sarray("value"), varray());
	bind_method(PackedFloat32Array, reverse, sarray(), varray());
	bind_method(PackedFloat32Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedFloat32Array, to_byte_array, sarray(), varray());
	bind_method(PackedFloat32Array, sort, sarray(), varray());
	bind_method(PackedFloat32Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedFloat32Array, duplicate, sarray(), varray());
	bind_method(PackedFloat32Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedFloat32Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedFloat32Array, count, sarray("value"), varray());
	bind_method(PackedFloat32Array, erase, sarray("value"), varray());

	/* Float64 Array */

	bind_method(PackedFloat64Array, size, sarray(), varray());
	bind_method(PackedFloat64Array, is_empty, sarray(), varray());
	bind_method(PackedFloat64Array, push_back, sarray("value"), varray());
	bind_method(PackedFloat64Array, append, sarray("value"), varray());
	bind_method(PackedFloat64Array, append_array, sarray("array"), varray());
	bind_method(PackedFloat64Array, remove_at, sarray("index"), varray());
	bind_method(PackedFloat64Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedFloat64Array, fill, sarray("value"), varray());
	bind_method(PackedFloat64Array, resize, sarray("new_size"), varray());
	bind_method(PackedFloat64Array, clear, sarray(), varray());
	bind_method(PackedFloat64Array, has, sarray("value"), varray());
	bind_method(PackedFloat64Array, reverse, sarray(), varray());
	bind_method(PackedFloat64Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedFloat64Array, to_byte_array, sarray(), varray());
	bind_method(PackedFloat64Array, sort, sarray(), varray());
	bind_method(PackedFloat64Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedFloat64Array, duplicate, sarray(), varray());
	bind_method(PackedFloat64Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedFloat64Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedFloat64Array, count, sarray("value"), varray());
	bind_method(PackedFloat64Array, erase, sarray("value"), varray());

	/* String Array */

	bind_method(PackedStringArray, size, sarray(), varray());
	bind_method(PackedStringArray, is_empty, sarray(), varray());
	bind_method(PackedStringArray, push_back, sarray("value"), varray());
	bind_method(PackedStringArray, append, sarray("value"), varray());
	bind_method(PackedStringArray, append_array, sarray("array"), varray());
	bind_method(PackedStringArray, remove_at, sarray("index"), varray());
	bind_method(PackedStringArray, insert, sarray("at_index", "value"), varray());
	bind_method(PackedStringArray, fill, sarray("value"), varray());
	bind_method(PackedStringArray, resize, sarray("new_size"), varray());
	bind_method(PackedStringArray, clear, sarray(), varray());
	bind_method(PackedStringArray, has, sarray("value"), varray());
	bind_method(PackedStringArray, reverse, sarray(), varray());
	bind_method(PackedStringArray, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedStringArray, to_byte_array, sarray(), varray());
	bind_method(PackedStringArray, sort, sarray(), varray());
	bind_method(PackedStringArray, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedStringArray, duplicate, sarray(), varray());
	bind_method(PackedStringArray, find, sarray("value", "from"), varray(0));
	bind_method(PackedStringArray, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedStringArray, count, sarray("value"), varray());
	bind_method(PackedStringArray, erase, sarray("value"), varray());

	/* Vector2 Array */

	bind_method(PackedVector2Array, size, sarray(), varray());
	bind_method(PackedVector2Array, is_empty, sarray(), varray());
	bind_method(PackedVector2Array, push_back, sarray("value"), varray());
	bind_method(PackedVector2Array, append, sarray("value"), varray());
	bind_method(PackedVector2Array, append_array, sarray("array"), varray());
	bind_method(PackedVector2Array, remove_at, sarray("index"), varray());
	bind_method(PackedVector2Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedVector2Array, fill, sarray("value"), varray());
	bind_method(PackedVector2Array, resize, sarray("new_size"), varray());
	bind_method(PackedVector2Array, clear, sarray(), varray());
	bind_method(PackedVector2Array, has, sarray("value"), varray());
	bind_method(PackedVector2Array, reverse, sarray(), varray());
	bind_method(PackedVector2Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedVector2Array, to_byte_array, sarray(), varray());
	bind_method(PackedVector2Array, sort, sarray(), varray());
	bind_method(PackedVector2Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedVector2Array, duplicate, sarray(), varray());
	bind_method(PackedVector2Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedVector2Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedVector2Array, count, sarray("value"), varray());
	bind_method(PackedVector2Array, erase, sarray("value"), varray());

	/* Vector3 Array */

	bind_method(PackedVector3Array, size, sarray(), varray());
	bind_method(PackedVector3Array, is_empty, sarray(), varray());
	bind_method(PackedVector3Array, push_back, sarray("value"), varray());
	bind_method(PackedVector3Array, append, sarray("value"), varray());
	bind_method(PackedVector3Array, append_array, sarray("array"), varray());
	bind_method(PackedVector3Array, remove_at, sarray("index"), varray());
	bind_method(PackedVector3Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedVector3Array, fill, sarray("value"), varray());
	bind_method(PackedVector3Array, resize, sarray("new_size"), varray());
	bind_method(PackedVector3Array, clear, sarray(), varray());
	bind_method(PackedVector3Array, has, sarray("value"), varray());
	bind_method(PackedVector3Array, reverse, sarray(), varray());
	bind_method(PackedVector3Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedVector3Array, to_byte_array, sarray(), varray());
	bind_method(PackedVector3Array, sort, sarray(), varray());
	bind_method(PackedVector3Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedVector3Array, duplicate, sarray(), varray());
	bind_method(PackedVector3Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedVector3Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedVector3Array, count, sarray("value"), varray());
	bind_method(PackedVector3Array, erase, sarray("value"), varray());

	/* Color Array */

	bind_method(PackedColorArray, size, sarray(), varray());
	bind_method(PackedColorArray, is_empty, sarray(), varray());
	bind_method(PackedColorArray, push_back, sarray("value"), varray());
	bind_method(PackedColorArray, append, sarray("value"), varray());
	bind_method(PackedColorArray, append_array, sarray("array"), varray());
	bind_method(PackedColorArray, remove_at, sarray("index"), varray());
	bind_method(PackedColorArray, insert, sarray("at_index", "value"), varray());
	bind_method(PackedColorArray, fill, sarray("value"), varray());
	bind_method(PackedColorArray, resize, sarray("new_size"), varray());
	bind_method(PackedColorArray, clear, sarray(), varray());
	bind_method(PackedColorArray, has, sarray("value"), varray());
	bind_method(PackedColorArray, reverse, sarray(), varray());
	bind_method(PackedColorArray, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedColorArray, to_byte_array, sarray(), varray());
	bind_method(PackedColorArray, sort, sarray(), varray());
	bind_method(PackedColorArray, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedColorArray, duplicate, sarray(), varray());
	bind_method(PackedColorArray, find, sarray("value", "from"), varray(0));
	bind_method(PackedColorArray, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedColorArray, count, sarray("value"), varray());
	bind_method(PackedColorArray, erase, sarray("value"), varray());

	/* Vector4 Array */

	bind_method(PackedVector4Array, size, sarray(), varray());
	bind_method(PackedVector4Array, is_empty, sarray(), varray());
	bind_method(PackedVector4Array, push_back, sarray("value"), varray());
	bind_method(PackedVector4Array, append, sarray("value"), varray());
	bind_method(PackedVector4Array, append_array, sarray("array"), varray());
	bind_method(PackedVector4Array, remove_at, sarray("index"), varray());
	bind_method(PackedVector4Array, insert, sarray("at_index", "value"), varray());
	bind_method(PackedVector4Array, fill, sarray("value"), varray());
	bind_method(PackedVector4Array, resize, sarray("new_size"), varray());
	bind_method(PackedVector4Array, clear, sarray(), varray());
	bind_method(PackedVector4Array, has, sarray("value"), varray());
	bind_method(PackedVector4Array, reverse, sarray(), varray());
	bind_method(PackedVector4Array, slice, sarray("begin", "end"), varray(INT_MAX));
	bind_method(PackedVector4Array, to_byte_array, sarray(), varray());
	bind_method(PackedVector4Array, sort, sarray(), varray());
	bind_method(PackedVector4Array, bsearch, sarray("value", "before"), varray(true));
	bind_method(PackedVector4Array, duplicate, sarray(), varray());
	bind_method(PackedVector4Array, find, sarray("value", "from"), varray(0));
	bind_method(PackedVector4Array, rfind, sarray("value", "from"), varray(-1));
	bind_method(PackedVector4Array, count, sarray("value"), varray());
	bind_method(PackedVector4Array, erase, sarray("value"), varray());
}

static void _register_variant_builtin_constants() {
	/* Register constants */
	int ncc = Color::get_named_color_count();
	for (int i = 0; i < ncc; i++) {
		add_variant_constant(Variant::COLOR, Color::get_named_color_name(i), Color::get_named_color(i));
	}

	add_enum_constant(Variant::VECTOR3, "Axis", "AXIS_X", Vector3::AXIS_X);
	add_enum_constant(Variant::VECTOR3, "Axis", "AXIS_Y", Vector3::AXIS_Y);
	add_enum_constant(Variant::VECTOR3, "Axis", "AXIS_Z", Vector3::AXIS_Z);

	add_variant_constant(Variant::VECTOR3, "ZERO", Vector3(0, 0, 0));
	add_variant_constant(Variant::VECTOR3, "ONE", Vector3(1, 1, 1));
	add_variant_constant(Variant::VECTOR3, "INF", Vector3(Math_INF, Math_INF, Math_INF));
	add_variant_constant(Variant::VECTOR3, "LEFT", Vector3_LEFT);
	add_variant_constant(Variant::VECTOR3, "RIGHT", Vector3_RIGHT);
	add_variant_constant(Variant::VECTOR3, "UP", Vector3_UP);
	add_variant_constant(Variant::VECTOR3, "DOWN", Vector3_DOWN);
	add_variant_constant(Variant::VECTOR3, "FORWARD", Vector3_FORWARD);
	add_variant_constant(Variant::VECTOR3, "BACK", Vector3_BACK);

	add_variant_constant(Variant::VECTOR3, "MODEL_LEFT", Vector3_MODEL_LEFT);
	add_variant_constant(Variant::VECTOR3, "MODEL_RIGHT", Vector3_MODEL_RIGHT);
	add_variant_constant(Variant::VECTOR3, "MODEL_TOP", Vector3_MODEL_TOP);
	add_variant_constant(Variant::VECTOR3, "MODEL_BOTTOM", Vector3_MODEL_BOTTOM);
	add_variant_constant(Variant::VECTOR3, "MODEL_FRONT", Vector3_MODEL_FRONT);
	add_variant_constant(Variant::VECTOR3, "MODEL_REAR", Vector3_MODEL_REAR);

	add_enum_constant(Variant::VECTOR4, "Axis", "AXIS_X", Vector4::AXIS_X);
	add_enum_constant(Variant::VECTOR4, "Axis", "AXIS_Y", Vector4::AXIS_Y);
	add_enum_constant(Variant::VECTOR4, "Axis", "AXIS_Z", Vector4::AXIS_Z);
	add_enum_constant(Variant::VECTOR4, "Axis", "AXIS_W", Vector4::AXIS_W);

	add_variant_constant(Variant::VECTOR4, "ZERO", Vector4(0, 0, 0, 0));
	add_variant_constant(Variant::VECTOR4, "ONE", Vector4(1, 1, 1, 1));
	add_variant_constant(Variant::VECTOR4, "INF", Vector4(Math_INF, Math_INF, Math_INF, Math_INF));

	add_enum_constant(Variant::VECTOR3I, "Axis", "AXIS_X", Vector3i::AXIS_X);
	add_enum_constant(Variant::VECTOR3I, "Axis", "AXIS_Y", Vector3i::AXIS_Y);
	add_enum_constant(Variant::VECTOR3I, "Axis", "AXIS_Z", Vector3i::AXIS_Z);

	add_enum_constant(Variant::VECTOR4I, "Axis", "AXIS_X", Vector4i::AXIS_X);
	add_enum_constant(Variant::VECTOR4I, "Axis", "AXIS_Y", Vector4i::AXIS_Y);
	add_enum_constant(Variant::VECTOR4I, "Axis", "AXIS_Z", Vector4i::AXIS_Z);
	add_enum_constant(Variant::VECTOR4I, "Axis", "AXIS_W", Vector4i::AXIS_W);

	add_variant_constant(Variant::VECTOR4I, "ZERO", Vector4i(0, 0, 0, 0));
	add_variant_constant(Variant::VECTOR4I, "ONE", Vector4i(1, 1, 1, 1));
	add_variant_constant(Variant::VECTOR4I, "MIN", Vector4i(INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN));
	add_variant_constant(Variant::VECTOR4I, "MAX", Vector4i(INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX));

	add_variant_constant(Variant::VECTOR3I, "ZERO", Vector3i(0, 0, 0));
	add_variant_constant(Variant::VECTOR3I, "ONE", Vector3i(1, 1, 1));
	add_variant_constant(Variant::VECTOR3I, "MIN", Vector3i(INT32_MIN, INT32_MIN, INT32_MIN));
	add_variant_constant(Variant::VECTOR3I, "MAX", Vector3i(INT32_MAX, INT32_MAX, INT32_MAX));
	add_variant_constant(Variant::VECTOR3I, "LEFT", Vector3i_LEFT);
	add_variant_constant(Variant::VECTOR3I, "RIGHT", Vector3i_RIGHT);
	add_variant_constant(Variant::VECTOR3I, "UP", Vector3i_UP);
	add_variant_constant(Variant::VECTOR3I, "DOWN", Vector3i_DOWN);
	add_variant_constant(Variant::VECTOR3I, "FORWARD", Vector3i_FORWARD);
	add_variant_constant(Variant::VECTOR3I, "BACK", Vector3i_BACK);

	add_enum_constant(Variant::VECTOR2, "Axis", "AXIS_X", Vector2::AXIS_X);
	add_enum_constant(Variant::VECTOR2, "Axis", "AXIS_Y", Vector2::AXIS_Y);

	add_enum_constant(Variant::VECTOR2I, "Axis", "AXIS_X", Vector2i::AXIS_X);
	add_enum_constant(Variant::VECTOR2I, "Axis", "AXIS_Y", Vector2i::AXIS_Y);

	add_variant_constant(Variant::VECTOR2, "ZERO", Vector2(0, 0));
	add_variant_constant(Variant::VECTOR2, "ONE", Vector2(1, 1));
	add_variant_constant(Variant::VECTOR2, "INF", Vector2(Math_INF, Math_INF));
	add_variant_constant(Variant::VECTOR2, "LEFT", Vector2_LEFT);
	add_variant_constant(Variant::VECTOR2, "RIGHT", Vector2_RIGHT);
	add_variant_constant(Variant::VECTOR2, "UP", Vector2_UP);
	add_variant_constant(Variant::VECTOR2, "DOWN", Vector2_DOWN);

	add_variant_constant(Variant::VECTOR2I, "ZERO", Vector2i(0, 0));
	add_variant_constant(Variant::VECTOR2I, "ONE", Vector2i(1, 1));
	add_variant_constant(Variant::VECTOR2I, "MIN", Vector2i(INT32_MIN, INT32_MIN));
	add_variant_constant(Variant::VECTOR2I, "MAX", Vector2i(INT32_MAX, INT32_MAX));
	add_variant_constant(Variant::VECTOR2I, "LEFT", Vector2i_LEFT);
	add_variant_constant(Variant::VECTOR2I, "RIGHT", Vector2i_RIGHT);
	add_variant_constant(Variant::VECTOR2I, "UP", Vector2i_UP);
	add_variant_constant(Variant::VECTOR2I, "DOWN", Vector2i_DOWN);

	add_variant_constant(Variant::TRANSFORM2D, "IDENTITY", Transform2D());
	add_variant_constant(Variant::TRANSFORM2D, "FLIP_X", Transform2D_FLIP_X);
	add_variant_constant(Variant::TRANSFORM2D, "FLIP_Y", Transform2D_FLIP_Y);

	add_variant_constant(Variant::TRANSFORM3D, "IDENTITY", Transform3D());
	add_variant_constant(Variant::TRANSFORM3D, "FLIP_X", Transform3D_FLIP_X);
	add_variant_constant(Variant::TRANSFORM3D, "FLIP_Y", Transform3D_FLIP_Y);
	add_variant_constant(Variant::TRANSFORM3D, "FLIP_Z", Transform3D_FLIP_Z);

	add_variant_constant(Variant::BASIS, "IDENTITY", Basis());
	add_variant_constant(Variant::BASIS, "FLIP_X", Basis_FLIP_X);
	add_variant_constant(Variant::BASIS, "FLIP_Y", Basis_FLIP_Y);
	add_variant_constant(Variant::BASIS, "FLIP_Z", Basis_FLIP_Z);

	add_variant_constant(Variant::PLANE, "PLANE_YZ", Plane_PLANE_YZ);
	add_variant_constant(Variant::PLANE, "PLANE_XZ", Plane_PLANE_XZ);
	add_variant_constant(Variant::PLANE, "PLANE_XY", Plane_PLANE_XY);

	add_variant_constant(Variant::QUATERNION, "IDENTITY", Quaternion());

	add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_NEAR", Projection::PLANE_NEAR);
	add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_FAR", Projection::PLANE_FAR);
	add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_LEFT", Projection::PLANE_LEFT);
	add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_TOP", Projection::PLANE_TOP);
	add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_RIGHT", Projection::PLANE_RIGHT);
	add_enum_constant(Variant::PROJECTION, "Planes", "PLANE_BOTTOM", Projection::PLANE_BOTTOM);

	Projection p;
	add_variant_constant(Variant::PROJECTION, "IDENTITY", p);
	p.set_zero();
	add_variant_constant(Variant::PROJECTION, "ZERO", p);
}

void RuztaVariantExtension::_register_variant_methods() {
	_register_variant_builtin_methods_string();
	_register_variant_builtin_methods_math();
	_register_variant_builtin_methods_misc();
	_register_variant_builtin_methods_array();
	_register_variant_builtin_constants();
}

void RuztaVariantExtension::_unregister_variant_methods() {
	// clear methods
	memdelete_arr(builtin_method_names);
	memdelete_arr(builtin_method_info);
#ifndef DISABLE_DEPRECATED
	memdelete_arr(builtin_compat_method_info);
#endif
	memdelete_arr(constant_data);
	memdelete_arr(enum_data);
}

void RuztaVariantExtension::get_property_list(Variant* base, List<PropertyInfo>* p_list) {
	if (base->get_type() == Variant::DICTIONARY) {
		const Dictionary* dic = reinterpret_cast<const Dictionary*>(base);
		for (const Variant& key : dic->keys()) {
			if (key.get_type() == Variant::STRING) {
				p_list->push_back(PropertyInfo(key.get_type(), key));
			}
		}
	} else if (base->get_type() == Variant::OBJECT) {
		Object* obj = base->get_validated_object();
		ERR_FAIL_NULL(obj);
		for (Dictionary prop : obj->get_property_list()) {
			p_list->push_back(PropertyInfo::from_dict(prop));
		}
	} else {
		List<StringName> members;
		get_member_list(base->get_type(), &members);
		for (const StringName& E : members) {
			PropertyInfo pi;
			pi.name = E;
			pi.type = RuztaVariantExtension::get_member_type(base->get_type(), E);
			p_list->push_back(pi);
		}
	}
}

void RuztaVariantExtension::get_method_list(Variant* base, List<MethodInfo>* p_list) {
	if (base->get_type() == Variant::OBJECT) {
		Object* obj = base->get_validated_object();
		if (obj) {
			for (Dictionary method : obj->get_method_list()) {
				p_list->push_back(MethodInfo::from_dict(method));
			}
		}
	} else {
		for (const StringName& E : builtin_method_names[base->get_type()]) {
			const VariantBuiltInMethodInfo* method = builtin_method_info[base->get_type()].getptr(E);
			ERR_CONTINUE(!method);
			p_list->push_back(method->get_method_info(E));
		}
	}
}
