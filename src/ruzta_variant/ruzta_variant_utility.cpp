/**************************************************************************/
/*  ruzta_variant_utility.cpp                                             */
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

#include "ruzta_variant_extension.h"

#include "method_ptrcall.h" // original: method_ptrcall.h

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

struct VariantUtilityFunctionInfo {
	void (*call_utility)(Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) = nullptr;
	RuztaVariantExtension::ValidatedUtilityFunction validated_call_utility = nullptr;
	RuztaVariantExtension::PTRUtilityFunction ptr_call_utility = nullptr;
	Vector<String> argnames;
	bool is_vararg = false;
	bool returns_value = false;
	int argcount = 0;
	Variant::Type (*get_arg_type)(int) = nullptr;
	Variant::Type return_type;
	RuztaVariantExtension::UtilityFunctionType type;
};

static AHashMap<StringName, VariantUtilityFunctionInfo> utility_function_table;
static List<StringName> utility_function_name_table;

#ifdef DEBUG_ENABLED
#define VCALLR *ret = p_func(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...)
#define VCALL p_func(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...)
#else
#define VCALLR *ret = p_func(VariantCaster<P>::cast(*p_args[Is])...)
#define VCALL p_func(VariantCaster<P>::cast(*p_args[Is])...)
#endif	// DEBUG_ENABLED

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void call_helperpr(R (*p_func)(P...), Variant* ret, const Variant** p_args, GDExtensionCallError& r_error, IndexSequence<Is...>) {
	r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_OK;
	VCALLR;
	(void)p_args;  // avoid gcc warning
	(void)r_error;
}

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void validated_call_helperpr(R (*p_func)(P...), Variant* ret, const Variant** p_args, IndexSequence<Is...>) {
	*ret = p_func(VariantCaster<P>::cast(*p_args[Is])...);
	(void)p_args;
}

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void ptr_call_helperpr(R (*p_func)(P...), void* ret, const void** p_args, IndexSequence<Is...>) {
	PtrToArg<R>::encode(p_func(PtrToArg<P>::convert(p_args[Is])...), ret);
	(void)p_args;
}

template <typename R, typename... P>
static _FORCE_INLINE_ void call_helperr(R (*p_func)(P...), Variant* ret, const Variant** p_args, GDExtensionCallError& r_error) {
	call_helperpr(p_func, ret, p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ void validated_call_helperr(R (*p_func)(P...), Variant* ret, const Variant** p_args) {
	validated_call_helperpr(p_func, ret, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ void ptr_call_helperr(R (*p_func)(P...), void* ret, const void** p_args) {
	ptr_call_helperpr(p_func, ret, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ int get_arg_count_helperr(R (*p_func)(P...)) {
	return sizeof...(P);
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type get_arg_type_helperr(R (*p_func)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type get_ret_type_helperr(R (*p_func)(P...)) {
	return GetTypeInfo<R>::VARIANT_TYPE;
}

// WITHOUT RET

template <typename... P, size_t... Is>
static _FORCE_INLINE_ void call_helperp(void (*p_func)(P...), const Variant** p_args, GDExtensionCallError& r_error, IndexSequence<Is...>) {
	r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_OK;
	VCALL;
	(void)p_args;
	(void)r_error;
}

template <typename... P, size_t... Is>
static _FORCE_INLINE_ void validated_call_helperp(void (*p_func)(P...), const Variant** p_args, IndexSequence<Is...>) {
	p_func(VariantCaster<P>::cast(*p_args[Is])...);
	(void)p_args;
}

template <typename... P, size_t... Is>
static _FORCE_INLINE_ void ptr_call_helperp(void (*p_func)(P...), const void** p_args, IndexSequence<Is...>) {
	p_func(PtrToArg<P>::convert(p_args[Is])...);
	(void)p_args;
}

template <typename... P>
static _FORCE_INLINE_ void call_helper(void (*p_func)(P...), const Variant** p_args, GDExtensionCallError& r_error) {
	call_helperp(p_func, p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
static _FORCE_INLINE_ void validated_call_helper(void (*p_func)(P...), const Variant** p_args) {
	validated_call_helperp(p_func, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
static _FORCE_INLINE_ void ptr_call_helper(void (*p_func)(P...), const void** p_args) {
	ptr_call_helperp(p_func, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
static _FORCE_INLINE_ int get_arg_count_helper(void (*p_func)(P...)) {
	return sizeof...(P);
}

template <typename... P>
static _FORCE_INLINE_ Variant::Type get_arg_type_helper(void (*p_func)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename... P>
static _FORCE_INLINE_ Variant::Type get_ret_type_helper(void (*p_func)(P...)) {
	return Variant::NIL;
}

#define FUNCBINDR(m_func, m_args, m_category)                                                                     \
	class Func_##m_func {                                                                                         \
	   public:                                                                                                    \
		static void call(Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) { \
			call_helperr(UtilityFunctions::m_func, r_ret, p_args, r_error);                                       \
		}                                                                                                         \
		static void validated_call(Variant* r_ret, const Variant** p_args, int p_argcount) {                      \
			validated_call_helperr(UtilityFunctions::m_func, r_ret, p_args);                                      \
		}                                                                                                         \
		static void ptrcall(void* ret, const void** p_args, int p_argcount) {                                     \
			ptr_call_helperr(UtilityFunctions::m_func, ret, p_args);                                              \
		}                                                                                                         \
		static int get_argument_count() {                                                                         \
			return get_arg_count_helperr(UtilityFunctions::m_func);                                               \
		}                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                       \
			return get_arg_type_helperr(UtilityFunctions::m_func, p_arg);                                         \
		}                                                                                                         \
		static Variant::Type get_return_type() {                                                                  \
			return get_ret_type_helperr(UtilityFunctions::m_func);                                                \
		}                                                                                                         \
		static bool has_return_type() {                                                                           \
			return true;                                                                                          \
		}                                                                                                         \
		static bool is_vararg() {                                                                                 \
			return false;                                                                                         \
		}                                                                                                         \
		static RuztaVariantExtension::UtilityFunctionType get_type() {                                            \
			return m_category;                                                                                    \
		}                                                                                                         \
	};                                                                                                            \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVR(m_func, m_args, m_category)                                                                    \
	class Func_##m_func {                                                                                         \
	   public:                                                                                                    \
		static void call(Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) { \
			r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_OK;                                        \
			*r_ret = UtilityFunctions::m_func(*p_args[0], r_error);                                               \
		}                                                                                                         \
		static void validated_call(Variant* r_ret, const Variant** p_args, int p_argcount) {                      \
			GDExtensionCallError ce;                                                                              \
			*r_ret = UtilityFunctions::m_func(*p_args[0], ce);                                                    \
		}                                                                                                         \
		static void ptrcall(void* ret, const void** p_args, int p_argcount) {                                     \
			GDExtensionCallError ce;                                                                              \
			PtrToArg<Variant>::encode(UtilityFunctions::m_func(PtrToArg<Variant>::convert(p_args[0]), ce), ret);  \
		}                                                                                                         \
		static int get_argument_count() {                                                                         \
			return 1;                                                                                             \
		}                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                       \
			return Variant::NIL;                                                                                  \
		}                                                                                                         \
		static Variant::Type get_return_type() {                                                                  \
			return Variant::NIL;                                                                                  \
		}                                                                                                         \
		static bool has_return_type() {                                                                           \
			return true;                                                                                          \
		}                                                                                                         \
		static bool is_vararg() {                                                                                 \
			return false;                                                                                         \
		}                                                                                                         \
		static RuztaVariantExtension::UtilityFunctionType get_type() {                                            \
			return m_category;                                                                                    \
		}                                                                                                         \
	};                                                                                                            \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVR2(m_func, m_args, m_category)                                                                             \
	class Func_##m_func {                                                                                                   \
	   public:                                                                                                              \
		static void call(Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) {           \
			r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_OK;                                                  \
			*r_ret = UtilityFunctions::m_func(*p_args[0], *p_args[1], r_error);                                             \
		}                                                                                                                   \
		static void validated_call(Variant* r_ret, const Variant** p_args, int p_argcount) {                                \
			GDExtensionCallError ce;                                                                                        \
			*r_ret = UtilityFunctions::m_func(*p_args[0], *p_args[1], ce);                                                  \
		}                                                                                                                   \
		static void ptrcall(void* ret, const void** p_args, int p_argcount) {                                               \
			GDExtensionCallError ce;                                                                                        \
			Variant r;                                                                                                      \
			r = UtilityFunctions::m_func(PtrToArg<Variant>::convert(p_args[0]), PtrToArg<Variant>::convert(p_args[1]), ce); \
			PtrToArg<Variant>::encode(r, ret);                                                                              \
		}                                                                                                                   \
		static int get_argument_count() {                                                                                   \
			return 2;                                                                                                       \
		}                                                                                                                   \
		static Variant::Type get_argument_type(int p_arg) {                                                                 \
			return Variant::NIL;                                                                                            \
		}                                                                                                                   \
		static Variant::Type get_return_type() {                                                                            \
			return Variant::NIL;                                                                                            \
		}                                                                                                                   \
		static bool has_return_type() {                                                                                     \
			return true;                                                                                                    \
		}                                                                                                                   \
		static bool is_vararg() {                                                                                           \
			return false;                                                                                                   \
		}                                                                                                                   \
		static RuztaVariantExtension::UtilityFunctionType get_type() {                                                      \
			return m_category;                                                                                              \
		}                                                                                                                   \
	};                                                                                                                      \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVR3(m_func, m_args, m_category)                                                                                                                    \
	class Func_##m_func {                                                                                                                                          \
	   public:                                                                                                                                                     \
		static void call(Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) {                                                  \
			r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_OK;                                                                                         \
			*r_ret = UtilityFunctions::m_func(*p_args[0], *p_args[1], *p_args[2], r_error);                                                                        \
		}                                                                                                                                                          \
		static void validated_call(Variant* r_ret, const Variant** p_args, int p_argcount) {                                                                       \
			GDExtensionCallError ce;                                                                                                                               \
			*r_ret = UtilityFunctions::m_func(*p_args[0], *p_args[1], *p_args[2], ce);                                                                             \
		}                                                                                                                                                          \
		static void ptrcall(void* ret, const void** p_args, int p_argcount) {                                                                                      \
			GDExtensionCallError ce;                                                                                                                               \
			Variant r;                                                                                                                                             \
			r = UtilityFunctions::m_func(PtrToArg<Variant>::convert(p_args[0]), PtrToArg<Variant>::convert(p_args[1]), PtrToArg<Variant>::convert(p_args[2]), ce); \
			PtrToArg<Variant>::encode(r, ret);                                                                                                                     \
		}                                                                                                                                                          \
		static int get_argument_count() {                                                                                                                          \
			return 3;                                                                                                                                              \
		}                                                                                                                                                          \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                        \
			return Variant::NIL;                                                                                                                                   \
		}                                                                                                                                                          \
		static Variant::Type get_return_type() {                                                                                                                   \
			return Variant::NIL;                                                                                                                                   \
		}                                                                                                                                                          \
		static bool has_return_type() {                                                                                                                            \
			return true;                                                                                                                                           \
		}                                                                                                                                                          \
		static bool is_vararg() {                                                                                                                                  \
			return false;                                                                                                                                          \
		}                                                                                                                                                          \
		static RuztaVariantExtension::UtilityFunctionType get_type() {                                                                                             \
			return m_category;                                                                                                                                     \
		}                                                                                                                                                          \
	};                                                                                                                                                             \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVARARG(m_func, m_args, m_category)                                                                \
	class Func_##m_func {                                                                                         \
	   public:                                                                                                    \
		static void call(Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) { \
			r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_OK;                                        \
			*r_ret = UtilityFunctions::m_func(p_args, p_argcount, r_error);                                       \
		}                                                                                                         \
		static void validated_call(Variant* r_ret, const Variant** p_args, int p_argcount) {                      \
			GDExtensionCallError c;                                                                               \
			*r_ret = UtilityFunctions::m_func(p_args, p_argcount, c);                                             \
		}                                                                                                         \
		static void ptrcall(void* ret, const void** p_args, int p_argcount) {                                     \
			Vector<Variant> args;                                                                                 \
			for (int i = 0; i < p_argcount; i++) {                                                                \
				args.push_back(PtrToArg<Variant>::convert(p_args[i]));                                            \
			}                                                                                                     \
			Vector<const Variant*> argsp;                                                                         \
			for (int i = 0; i < p_argcount; i++) {                                                                \
				argsp.push_back(&args[i]);                                                                        \
			}                                                                                                     \
			Variant r;                                                                                            \
			validated_call(&r, (const Variant**)argsp.ptr(), p_argcount);                                         \
			PtrToArg<Variant>::encode(r, ret);                                                                    \
		}                                                                                                         \
		static int get_argument_count() {                                                                         \
			return 2;                                                                                             \
		}                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                       \
			return Variant::NIL;                                                                                  \
		}                                                                                                         \
		static Variant::Type get_return_type() {                                                                  \
			return Variant::NIL;                                                                                  \
		}                                                                                                         \
		static bool has_return_type() {                                                                           \
			return true;                                                                                          \
		}                                                                                                         \
		static bool is_vararg() {                                                                                 \
			return true;                                                                                          \
		}                                                                                                         \
		static RuztaVariantExtension::UtilityFunctionType get_type() {                                            \
			return m_category;                                                                                    \
		}                                                                                                         \
	};                                                                                                            \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVARARGS(m_func, m_args, m_category)                                                               \
	class Func_##m_func {                                                                                         \
	   public:                                                                                                    \
		static void call(Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) { \
			r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_OK;                                        \
			*r_ret = UtilityFunctions::m_func(p_args, p_argcount, r_error);                                       \
		}                                                                                                         \
		static void validated_call(Variant* r_ret, const Variant** p_args, int p_argcount) {                      \
			GDExtensionCallError c;                                                                               \
			*r_ret = UtilityFunctions::m_func(p_args, p_argcount, c);                                             \
		}                                                                                                         \
		static void ptrcall(void* ret, const void** p_args, int p_argcount) {                                     \
			Vector<Variant> args;                                                                                 \
			for (int i = 0; i < p_argcount; i++) {                                                                \
				args.push_back(PtrToArg<Variant>::convert(p_args[i]));                                            \
			}                                                                                                     \
			Vector<const Variant*> argsp;                                                                         \
			for (int i = 0; i < p_argcount; i++) {                                                                \
				argsp.push_back(&args[i]);                                                                        \
			}                                                                                                     \
			Variant r;                                                                                            \
			validated_call(&r, (const Variant**)argsp.ptr(), p_argcount);                                         \
			PtrToArg<String>::encode(r.operator String(), ret);                                                   \
		}                                                                                                         \
		static int get_argument_count() {                                                                         \
			return 1;                                                                                             \
		}                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                       \
			return Variant::NIL;                                                                                  \
		}                                                                                                         \
		static Variant::Type get_return_type() {                                                                  \
			return Variant::STRING;                                                                               \
		}                                                                                                         \
		static bool has_return_type() {                                                                           \
			return true;                                                                                          \
		}                                                                                                         \
		static bool is_vararg() {                                                                                 \
			return true;                                                                                          \
		}                                                                                                         \
		static RuztaVariantExtension::UtilityFunctionType get_type() {                                            \
			return m_category;                                                                                    \
		}                                                                                                         \
	};                                                                                                            \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVARARGV_CNAME(m_func, m_func_cname, m_args, m_category)                                           \
	class Func_##m_func {                                                                                         \
	   public:                                                                                                    \
		static void call(Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) { \
			r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_OK;                                        \
			UtilityFunctions::m_func_cname(p_args, p_argcount, r_error);                                          \
		}                                                                                                         \
		static void validated_call(Variant* r_ret, const Variant** p_args, int p_argcount) {                      \
			GDExtensionCallError c;                                                                               \
			UtilityFunctions::m_func_cname(p_args, p_argcount, c);                                                \
		}                                                                                                         \
		static void ptrcall(void* ret, const void** p_args, int p_argcount) {                                     \
			Vector<Variant> args;                                                                                 \
			for (int i = 0; i < p_argcount; i++) {                                                                \
				args.push_back(PtrToArg<Variant>::convert(p_args[i]));                                            \
			}                                                                                                     \
			Vector<const Variant*> argsp;                                                                         \
			for (int i = 0; i < p_argcount; i++) {                                                                \
				argsp.push_back(&args[i]);                                                                        \
			}                                                                                                     \
			Variant r;                                                                                            \
			validated_call(&r, (const Variant**)argsp.ptr(), p_argcount);                                         \
		}                                                                                                         \
		static int get_argument_count() {                                                                         \
			return 1;                                                                                             \
		}                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                       \
			return Variant::NIL;                                                                                  \
		}                                                                                                         \
		static Variant::Type get_return_type() {                                                                  \
			return Variant::NIL;                                                                                  \
		}                                                                                                         \
		static bool has_return_type() {                                                                           \
			return false;                                                                                         \
		}                                                                                                         \
		static bool is_vararg() {                                                                                 \
			return true;                                                                                          \
		}                                                                                                         \
		static RuztaVariantExtension::UtilityFunctionType get_type() {                                            \
			return m_category;                                                                                    \
		}                                                                                                         \
	};                                                                                                            \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVARARGV(m_func, m_args, m_category) FUNCBINDVARARGV_CNAME(m_func, m_func, m_args, m_category)

#define FUNCBIND(m_func, m_args, m_category)                                                                      \
	class Func_##m_func {                                                                                         \
	   public:                                                                                                    \
		static void call(Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) { \
			call_helper(UtilityFunctions::m_func, p_args, r_error);                                               \
		}                                                                                                         \
		static void validated_call(Variant* r_ret, const Variant** p_args, int p_argcount) {                      \
			validated_call_helper(UtilityFunctions::m_func, p_args);                                              \
		}                                                                                                         \
		static void ptrcall(void* ret, const void** p_args, int p_argcount) {                                     \
			ptr_call_helper(UtilityFunctions::m_func, p_args);                                                    \
		}                                                                                                         \
		static int get_argument_count() {                                                                         \
			return get_arg_count_helper(UtilityFunctions::m_func);                                                \
		}                                                                                                         \
		static Variant::Type get_argument_type(int p_arg) {                                                       \
			return get_arg_type_helper(UtilityFunctions::m_func, p_arg);                                          \
		}                                                                                                         \
		static Variant::Type get_return_type() {                                                                  \
			return get_ret_type_helper(UtilityFunctions::m_func);                                                 \
		}                                                                                                         \
		static bool has_return_type() {                                                                           \
			return false;                                                                                         \
		}                                                                                                         \
		static bool is_vararg() {                                                                                 \
			return false;                                                                                         \
		}                                                                                                         \
		static RuztaVariantExtension::UtilityFunctionType get_type() {                                            \
			return m_category;                                                                                    \
		}                                                                                                         \
	};                                                                                                            \
	register_utility_function<Func_##m_func>(#m_func, m_args)

template <typename T>
void register_utility_function(const String& p_name, const Vector<String>& argnames) {
	String name = p_name;
	if (name.begins_with("_")) {
		name = name.substr(1);
	}
	StringName sname = name;
	ERR_FAIL_COND(utility_function_table.has(sname));

	VariantUtilityFunctionInfo bfi;
	bfi.call_utility = T::call;
	bfi.validated_call_utility = T::validated_call;
	bfi.ptr_call_utility = T::ptrcall;
	bfi.is_vararg = T::is_vararg();
	bfi.argnames = argnames;
	bfi.argcount = T::get_argument_count();
	if (!bfi.is_vararg) {
		ERR_FAIL_COND_MSG(argnames.size() != bfi.argcount, vformat("Wrong number of arguments binding utility function: '%s'.", name));
	}
	bfi.get_arg_type = T::get_argument_type;
	bfi.return_type = T::get_return_type();
	bfi.type = T::get_type();
	bfi.returns_value = T::has_return_type();

	utility_function_table.insert(sname, bfi);
	utility_function_name_table.push_back(sname);
}

void RuztaVariantExtension::_register_variant_utility_functions() {
	// Math

	FUNCBINDR(sin, sarray("angle_rad"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cos, sarray("angle_rad"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(tan, sarray("angle_rad"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(sinh, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cosh, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(tanh, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(asin, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(acos, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(atan, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(atan2, sarray("y", "x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(asinh, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(acosh, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(atanh, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(sqrt, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(fmod, sarray("x", "y"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(fposmod, sarray("x", "y"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(posmod, sarray("x", "y"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(floor, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(floorf, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(floori, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(ceil, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(ceilf, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(ceili, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(round, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(roundf, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(roundi, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(abs, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(absf, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(absi, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(sign, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(signf, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(signi, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(snapped, sarray("x", "step"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(snappedf, sarray("x", "step"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(snappedi, sarray("x", "step"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(pow, sarray("base", "exp"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(log, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(exp, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(is_nan, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(is_inf, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(is_equal_approx, sarray("a", "b"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(is_zero_approx, sarray("x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(is_finite, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(ease, sarray("x", "curve"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(step_decimals, sarray("x"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(lerp, sarray("from", "to", "weight"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(lerpf, sarray("from", "to", "weight"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cubic_interpolate, sarray("from", "to", "pre", "post", "weight"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cubic_interpolate_angle, sarray("from", "to", "pre", "post", "weight"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cubic_interpolate_in_time, sarray("from", "to", "pre", "post", "weight", "to_t", "pre_t", "post_t"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cubic_interpolate_angle_in_time, sarray("from", "to", "pre", "post", "weight", "to_t", "pre_t", "post_t"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(bezier_interpolate, sarray("start", "control_1", "control_2", "end", "t"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(bezier_derivative, sarray("start", "control_1", "control_2", "end", "t"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(angle_difference, sarray("from", "to"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(lerp_angle, sarray("from", "to", "weight"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(inverse_lerp, sarray("from", "to", "weight"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(remap, sarray("value", "istart", "istop", "ostart", "ostop"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(smoothstep, sarray("from", "to", "x"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(move_toward, sarray("from", "to", "delta"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(rotate_toward, sarray("from", "to", "delta"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(deg_to_rad, sarray("deg"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(rad_to_deg, sarray("rad"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(linear_to_db, sarray("lin"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(db_to_linear, sarray("db"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(wrap, sarray("value", "min", "max"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(wrapi, sarray("value", "min", "max"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(wrapf, sarray("value", "min", "max"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVARARG(max, sarray(), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(maxi, sarray("a", "b"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(maxf, sarray("a", "b"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVARARG(min, sarray(), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(mini, sarray("a", "b"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(minf, sarray("a", "b"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(clamp, sarray("value", "min", "max"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(clampi, sarray("value", "min", "max"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(clampf, sarray("value", "min", "max"), UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(nearest_po2, sarray("value"), UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(pingpong, sarray("value", "length"), UTILITY_FUNC_TYPE_MATH);

	// Random

	FUNCBIND(randomize, sarray(), UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(randi, sarray(), UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(randf, sarray(), UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(randi_range, sarray("from", "to"), UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(randf_range, sarray("from", "to"), UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(randfn, sarray("mean", "deviation"), UTILITY_FUNC_TYPE_RANDOM);
	FUNCBIND(seed, sarray("base"), UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(rand_from_seed, sarray("seed"), UTILITY_FUNC_TYPE_RANDOM);

	// Utility

	FUNCBINDR(weakref, sarray("obj"), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(type_of, sarray("variable"), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(type_convert, sarray("variant", "type"), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGS(str, sarray(), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(error_string, sarray("error"), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(type_string, sarray("type"), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(print, sarray(), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(print_rich, sarray(), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(printerr, sarray(), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(printt, sarray(), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(prints, sarray(), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(printraw, sarray(), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV_CNAME(print_verbose, print_verbose, sarray(), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(push_error, sarray(), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(push_warning, sarray(), UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(var_to_str, sarray("variable"), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(str_to_var, sarray("string"), UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(var_to_bytes, sarray("variable"), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(bytes_to_var, sarray("bytes"), UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(var_to_bytes_with_objects, sarray("variable"), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(bytes_to_var_with_objects, sarray("bytes"), UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(hash, sarray("variable"), UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(instance_from_id, sarray("instance_id"), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(is_instance_id_valid, sarray("id"), UTILITY_FUNC_TYPE_GENERAL);

	// UtilityFunction::is_instance_valid - does not exist
	class Func_is_instance_valid {
	   public:
		static bool is_instance_valid(const Variant& p_instance) {
			if (p_instance.get_type() != Variant::OBJECT) {
				return false;
			}
			return UtilityFunctions::is_instance_id_valid(((ObjectID)p_instance).operator int64_t());
		}
		static void call(Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) {
			call_helperr(is_instance_valid, r_ret, p_args, r_error);
		}
		static void validated_call(Variant* r_ret, const Variant** p_args, int p_argcount) {
			validated_call_helperr(is_instance_valid, r_ret, p_args);
		}
		static void ptrcall(void* ret, const void** p_args, int p_argcount) {
			ptr_call_helperr(is_instance_valid, ret, p_args);
		}
		static int get_argument_count() {
			return get_arg_count_helperr(is_instance_valid);
		}
		static Variant::Type get_argument_type(int p_arg) {
			return get_arg_type_helperr(is_instance_valid, p_arg);
		}
		static Variant::Type get_return_type() {
			return get_ret_type_helperr(is_instance_valid);
		}
		static bool has_return_type() { return true; }
		static bool is_vararg() { return false; }
		static RuztaVariantExtension::UtilityFunctionType get_type() { return UTILITY_FUNC_TYPE_GENERAL; }
	};
	register_utility_function<Func_is_instance_valid>("is_instance_valid", sarray("instance"));

	FUNCBINDR(rid_allocate_id, Vector<String>(), UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(rid_from_int64, sarray("base"), UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(is_same, sarray("a", "b"), UTILITY_FUNC_TYPE_GENERAL);
}

void RuztaVariantExtension::_unregister_variant_utility_functions() {
	utility_function_table.clear();
	utility_function_name_table.clear();
}

void RuztaVariantExtension::call_utility_function(const StringName& p_name, Variant* r_ret, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_INVALID_METHOD;
		r_error.argument = 0;
		r_error.expected = 0;
		return;
	}

	if (unlikely(!bfi->is_vararg && p_argcount < bfi->argcount)) {
		r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = bfi->argcount;
		return;
	}

	if (unlikely(!bfi->is_vararg && p_argcount > bfi->argcount)) {
		r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = bfi->argcount;
		return;
	}

	bfi->call_utility(r_ret, p_args, p_argcount, r_error);
}

bool RuztaVariantExtension::has_utility_function(const StringName& p_name) {
	return utility_function_table.has(p_name);
}

RuztaVariantExtension::ValidatedUtilityFunction RuztaVariantExtension::get_validated_utility_function(const StringName& p_name) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return nullptr;
	}

	return bfi->validated_call_utility;
}

RuztaVariantExtension::PTRUtilityFunction RuztaVariantExtension::get_ptr_utility_function(const StringName& p_name) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return nullptr;
	}

	return bfi->ptr_call_utility;
}

RuztaVariantExtension::UtilityFunctionType RuztaVariantExtension::get_utility_function_type(const StringName& p_name) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return UTILITY_FUNC_TYPE_MATH;
	}

	return bfi->type;
}

MethodInfo RuztaVariantExtension::get_utility_function_info(const StringName& p_name) {
	MethodInfo info;
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (bfi) {
		info.name = p_name;
		if (bfi->returns_value && bfi->return_type == Variant::NIL) {
			info.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
		}
		info.return_val.type = bfi->return_type;
		if (bfi->is_vararg) {
			info.flags |= METHOD_FLAG_VARARG;
		}
		for (int i = 0; i < bfi->argnames.size(); ++i) {
			PropertyInfo arg;
			arg.type = bfi->get_arg_type(i);
			arg.name = bfi->argnames[i];
			info.arguments.push_back(arg);
		}
	}
	return info;
}

int RuztaVariantExtension::get_utility_function_argument_count(const StringName& p_name) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return 0;
	}

	return bfi->argcount;
}

Variant::Type RuztaVariantExtension::get_utility_function_argument_type(const StringName& p_name, int p_arg) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return Variant::NIL;
	}

	return bfi->get_arg_type(p_arg);
}

String RuztaVariantExtension::get_utility_function_argument_name(const StringName& p_name, int p_arg) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return String();
	}
	ERR_FAIL_INDEX_V(p_arg, bfi->argnames.size(), String());
	ERR_FAIL_COND_V(bfi->is_vararg, String());
	return bfi->argnames[p_arg];
}

bool RuztaVariantExtension::has_utility_function_return_value(const StringName& p_name) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return false;
	}
	return bfi->returns_value;
}

Variant::Type RuztaVariantExtension::get_utility_function_return_type(const StringName& p_name) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return Variant::NIL;
	}

	return bfi->return_type;
}

bool RuztaVariantExtension::is_utility_function_vararg(const StringName& p_name) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	if (!bfi) {
		return false;
	}

	return bfi->is_vararg;
}

uint32_t RuztaVariantExtension::get_utility_function_hash(const StringName& p_name) {
	const VariantUtilityFunctionInfo* bfi = utility_function_table.getptr(p_name);
	ERR_FAIL_NULL_V(bfi, 0);

	uint32_t hash = hash_murmur3_one_32(bfi->is_vararg);
	hash = hash_murmur3_one_32(bfi->returns_value, hash);
	if (bfi->returns_value) {
		hash = hash_murmur3_one_32(bfi->return_type, hash);
	}
	hash = hash_murmur3_one_32(bfi->argcount, hash);
	for (int i = 0; i < bfi->argcount; i++) {
		hash = hash_murmur3_one_32(bfi->get_arg_type(i), hash);
	}

	return hash_fmix32(hash);
}

void RuztaVariantExtension::get_utility_function_list(List<StringName>* r_functions) {
	for (const StringName& E : utility_function_name_table) {
		r_functions->push_back(E);
	}
}

int RuztaVariantExtension::get_utility_function_count() {
	return utility_function_name_table.size();
}
