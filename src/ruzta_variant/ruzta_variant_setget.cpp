/**************************************************************************/
/*  variant_setget.cpp                                                    */
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

#include <godot_cpp/variant/variant_internal.hpp>

#include "method_ptrcall.h" // original: method_ptrcall.h
#include "ruzta_variant_extension.h"

#define SETGET_STRUCT(m_base_type, m_member_type, m_member)                                          \
	struct VariantSetGet_##m_base_type##_##m_member {                                                \
		static void get(const Variant* base, Variant* member) {                                      \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_member;                      \
		}                                                                                            \
		static inline void validated_get(const Variant* base, Variant* member) {                     \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_member;                      \
		}                                                                                            \
		static void ptr_get(const void* base, void* member) {                                        \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_member, member);  \
		}                                                                                            \
		static void set(Variant* base, const Variant* value, bool& valid) {                          \
			if (value->get_type() == GetTypeInfo<m_member_type>::VARIANT_TYPE) {                     \
				((m_base_type*)base)->m_member = VariantInternalAccessor<m_member_type>::get(value); \
				valid = true;                                                                        \
			} else {                                                                                 \
				valid = false;                                                                       \
			}                                                                                        \
		}                                                                                            \
		static inline void validated_set(Variant* base, const Variant* value) {                      \
			((m_base_type*)base)->m_member = VariantInternalAccessor<m_member_type>::get(value);     \
		}                                                                                            \
		static void ptr_set(void* base, const void* member) {                                        \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                    \
			b.m_member = PtrToArg<m_member_type>::convert(member);                                   \
			PtrToArg<m_base_type>::encode(b, base);                                                  \
		}                                                                                            \
		static Variant::Type get_type() {                                                            \
			return (Variant::Type)GetTypeInfo<m_member_type>::VARIANT_TYPE;                          \
		}                                                                                            \
	};

#define SETGET_NUMBER_STRUCT(m_base_type, m_member_type, m_member)                                  \
	struct VariantSetGet_##m_base_type##_##m_member {                                               \
		static void get(const Variant* base, Variant* member) {                                     \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_member;                     \
		}                                                                                           \
		static inline void validated_get(const Variant* base, Variant* member) {                    \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_member;                     \
		}                                                                                           \
		static void ptr_get(const void* base, void* member) {                                       \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_member, member); \
		}                                                                                           \
		static void set(Variant* base, const Variant* value, bool& valid) {                         \
			if (value->get_type() == Variant::FLOAT) {                                              \
				((m_base_type*)base)->m_member = VariantInternalAccessor<double>::get(value);       \
				valid = true;                                                                       \
			} else if (value->get_type() == Variant::INT) {                                         \
				((m_base_type*)base)->m_member = VariantInternalAccessor<int64_t>::get(value);      \
				valid = true;                                                                       \
			} else {                                                                                \
				valid = false;                                                                      \
			}                                                                                       \
		}                                                                                           \
		static inline void validated_set(Variant* base, const Variant* value) {                     \
			((m_base_type*)base)->m_member = VariantInternalAccessor<m_member_type>::get(value);    \
		}                                                                                           \
		static void ptr_set(void* base, const void* member) {                                       \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                   \
			b.m_member = PtrToArg<m_member_type>::convert(member);                                  \
			PtrToArg<m_base_type>::encode(b, base);                                                 \
		}                                                                                           \
		static Variant::Type get_type() {                                                           \
			return (Variant::Type)GetTypeInfo<m_member_type>::VARIANT_TYPE;                         \
		}                                                                                           \
	};

#define SETGET_STRUCT_CUSTOM(m_base_type, m_member_type, m_member, m_custom)                         \
	struct VariantSetGet_##m_base_type##_##m_member {                                                \
		static void get(const Variant* base, Variant* member) {                                      \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_custom;                      \
		}                                                                                            \
		static inline void validated_get(const Variant* base, Variant* member) {                     \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_custom;                      \
		}                                                                                            \
		static void ptr_get(const void* base, void* member) {                                        \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_custom, member);  \
		}                                                                                            \
		static void set(Variant* base, const Variant* value, bool& valid) {                          \
			if (value->get_type() == GetTypeInfo<m_member_type>::VARIANT_TYPE) {                     \
				((m_base_type*)base)->m_custom = VariantInternalAccessor<m_member_type>::get(value); \
				valid = true;                                                                        \
			} else {                                                                                 \
				valid = false;                                                                       \
			}                                                                                        \
		}                                                                                            \
		static inline void validated_set(Variant* base, const Variant* value) {                      \
			((m_base_type*)base)->m_custom = VariantInternalAccessor<m_member_type>::get(value);     \
		}                                                                                            \
		static void ptr_set(void* base, const void* member) {                                        \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                    \
			b.m_custom = PtrToArg<m_member_type>::convert(member);                                   \
			PtrToArg<m_base_type>::encode(b, base);                                                  \
		}                                                                                            \
		static Variant::Type get_type() {                                                            \
			return (Variant::Type)GetTypeInfo<m_member_type>::VARIANT_TYPE;                          \
		}                                                                                            \
	};

#define SETGET_NUMBER_STRUCT_CUSTOM(m_base_type, m_member_type, m_member, m_custom)                 \
	struct VariantSetGet_##m_base_type##_##m_member {                                               \
		static void get(const Variant* base, Variant* member) {                                     \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_custom;                     \
		}                                                                                           \
		static inline void validated_get(const Variant* base, Variant* member) {                    \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_custom;                     \
		}                                                                                           \
		static void ptr_get(const void* base, void* member) {                                       \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_custom, member); \
		}                                                                                           \
		static void set(Variant* base, const Variant* value, bool& valid) {                         \
			if (value->get_type() == Variant::FLOAT) {                                              \
				((m_base_type*)base)->m_custom = VariantInternalAccessor<double>::get(value);       \
				valid = true;                                                                       \
			} else if (value->get_type() == Variant::INT) {                                         \
				((m_base_type*)base)->m_custom = VariantInternalAccessor<int64_t>::get(value);      \
				valid = true;                                                                       \
			} else {                                                                                \
				valid = false;                                                                      \
			}                                                                                       \
		}                                                                                           \
		static inline void validated_set(Variant* base, const Variant* value) {                     \
			((m_base_type*)base)->m_custom = VariantInternalAccessor<m_member_type>::get(value);    \
		}                                                                                           \
		static void ptr_set(void* base, const void* member) {                                       \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                   \
			b.m_custom = PtrToArg<m_member_type>::convert(member);                                  \
			PtrToArg<m_base_type>::encode(b, base);                                                 \
		}                                                                                           \
		static Variant::Type get_type() {                                                           \
			return (Variant::Type)GetTypeInfo<m_member_type>::VARIANT_TYPE;                         \
		}                                                                                           \
	};

#define SETGET_STRUCT_FUNC(m_base_type, m_member_type, m_member, m_setter, m_getter)                  \
	struct VariantSetGet_##m_base_type##_##m_member {                                                 \
		static void get(const Variant* base, Variant* member) {                                       \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_getter();                     \
		}                                                                                             \
		static inline void validated_get(const Variant* base, Variant* member) {                      \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_getter();                     \
		}                                                                                             \
		static void ptr_get(const void* base, void* member) {                                         \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_getter(), member); \
		}                                                                                             \
		static void set(Variant* base, const Variant* value, bool& valid) {                           \
			if (value->get_type() == GetTypeInfo<m_member_type>::VARIANT_TYPE) {                      \
				((m_base_type*)base)->m_setter(VariantInternalAccessor<m_member_type>::get(value));   \
				valid = true;                                                                         \
			} else {                                                                                  \
				valid = false;                                                                        \
			}                                                                                         \
		}                                                                                             \
		static inline void validated_set(Variant* base, const Variant* value) {                       \
			((m_base_type*)base)->m_setter(VariantInternalAccessor<m_member_type>::get(value));       \
		}                                                                                             \
		static void ptr_set(void* base, const void* member) {                                         \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                     \
			b.m_setter(PtrToArg<m_member_type>::convert(member));                                     \
			PtrToArg<m_base_type>::encode(b, base);                                                   \
		}                                                                                             \
		static Variant::Type get_type() {                                                             \
			return (Variant::Type)GetTypeInfo<m_member_type>::VARIANT_TYPE;                           \
		}                                                                                             \
	};

#define SETGET_NUMBER_STRUCT_FUNC(m_base_type, m_member_type, m_member, m_setter, m_getter)           \
	struct VariantSetGet_##m_base_type##_##m_member {                                                 \
		static void get(const Variant* base, Variant* member) {                                       \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_getter();                     \
		}                                                                                             \
		static inline void validated_get(const Variant* base, Variant* member) {                      \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_getter();                     \
		}                                                                                             \
		static void ptr_get(const void* base, void* member) {                                         \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_getter(), member); \
		}                                                                                             \
		static void set(Variant* base, const Variant* value, bool& valid) {                           \
			if (value->get_type() == Variant::FLOAT) {                                                \
				((m_base_type*)base)->m_setter(VariantInternalAccessor<double>::get(value));          \
				valid = true;                                                                         \
			} else if (value->get_type() == Variant::INT) {                                           \
				((m_base_type*)base)->m_setter(VariantInternalAccessor<int64_t>::get(value));         \
				valid = true;                                                                         \
			} else {                                                                                  \
				valid = false;                                                                        \
			}                                                                                         \
		}                                                                                             \
		static inline void validated_set(Variant* base, const Variant* value) {                       \
			((m_base_type*)base)->m_setter(VariantInternalAccessor<m_member_type>::get(value));       \
		}                                                                                             \
		static void ptr_set(void* base, const void* member) {                                         \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                     \
			b.m_setter(PtrToArg<m_member_type>::convert(member));                                     \
			PtrToArg<m_base_type>::encode(b, base);                                                   \
		}                                                                                             \
		static Variant::Type get_type() {                                                             \
			return (Variant::Type)GetTypeInfo<m_member_type>::VARIANT_TYPE;                           \
		}                                                                                             \
	};

#define SETGET_STRUCT_FUNC_INDEX(m_base_type, m_member_type, m_member, m_setter, m_getter, m_index)          \
	struct VariantSetGet_##m_base_type##_##m_member {                                                        \
		static void get(const Variant* base, Variant* member) {                                              \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_getter(m_index);                     \
		}                                                                                                    \
		static inline void validated_get(const Variant* base, Variant* member) {                             \
			*member = VariantInternalAccessor<m_base_type>::get(base).m_getter(m_index);                     \
		}                                                                                                    \
		static void ptr_get(const void* base, void* member) {                                                \
			PtrToArg<m_member_type>::encode(PtrToArg<m_base_type>::convert(base).m_getter(m_index), member); \
		}                                                                                                    \
		static void set(Variant* base, const Variant* value, bool& valid) {                                  \
			if (value->get_type() == GetTypeInfo<m_member_type>::VARIANT_TYPE) {                             \
				((m_base_type*)base)->m_setter(m_index, VariantInternalAccessor<m_member_type>::get(value)); \
				valid = true;                                                                                \
			} else {                                                                                         \
				valid = false;                                                                               \
			}                                                                                                \
		}                                                                                                    \
		static inline void validated_set(Variant* base, const Variant* value) {                              \
			((m_base_type*)base)->m_setter(m_index, VariantInternalAccessor<m_member_type>::get(value));     \
		}                                                                                                    \
		static void ptr_set(void* base, const void* member) {                                                \
			m_base_type b = PtrToArg<m_base_type>::convert(base);                                            \
			b.m_setter(m_index, PtrToArg<m_member_type>::convert(member));                                   \
			PtrToArg<m_base_type>::encode(b, base);                                                          \
		}                                                                                                    \
		static Variant::Type get_type() {                                                                    \
			return (Variant::Type)GetTypeInfo<m_member_type>::VARIANT_TYPE;                                  \
		}                                                                                                    \
	};

SETGET_NUMBER_STRUCT(Vector2, double, x)
SETGET_NUMBER_STRUCT(Vector2, double, y)

SETGET_NUMBER_STRUCT(Vector2i, int64_t, x)
SETGET_NUMBER_STRUCT(Vector2i, int64_t, y)

SETGET_NUMBER_STRUCT(Vector3, double, x)
SETGET_NUMBER_STRUCT(Vector3, double, y)
SETGET_NUMBER_STRUCT(Vector3, double, z)

SETGET_NUMBER_STRUCT(Vector3i, int64_t, x)
SETGET_NUMBER_STRUCT(Vector3i, int64_t, y)
SETGET_NUMBER_STRUCT(Vector3i, int64_t, z)

SETGET_NUMBER_STRUCT(Vector4, double, x)
SETGET_NUMBER_STRUCT(Vector4, double, y)
SETGET_NUMBER_STRUCT(Vector4, double, z)
SETGET_NUMBER_STRUCT(Vector4, double, w)

SETGET_NUMBER_STRUCT(Vector4i, int64_t, x)
SETGET_NUMBER_STRUCT(Vector4i, int64_t, y)
SETGET_NUMBER_STRUCT(Vector4i, int64_t, z)
SETGET_NUMBER_STRUCT(Vector4i, int64_t, w)

SETGET_STRUCT(Rect2, Vector2, position)
SETGET_STRUCT(Rect2, Vector2, size)

SETGET_STRUCT_FUNC(Rect2, Vector2, end, set_end, get_end)

SETGET_STRUCT(Rect2i, Vector2i, position)
SETGET_STRUCT(Rect2i, Vector2i, size)
SETGET_STRUCT_FUNC(Rect2i, Vector2i, end, set_end, get_end)

SETGET_STRUCT(AABB, Vector3, position)
SETGET_STRUCT(AABB, Vector3, size)
SETGET_STRUCT_FUNC(AABB, Vector3, end, set_end, get_end)

SETGET_STRUCT_CUSTOM(Transform2D, Vector2, x, columns[0])
SETGET_STRUCT_CUSTOM(Transform2D, Vector2, y, columns[1])
SETGET_STRUCT_CUSTOM(Transform2D, Vector2, origin, columns[2])

SETGET_NUMBER_STRUCT_CUSTOM(Plane, double, x, normal.x)
SETGET_NUMBER_STRUCT_CUSTOM(Plane, double, y, normal.y)
SETGET_NUMBER_STRUCT_CUSTOM(Plane, double, z, normal.z)
SETGET_STRUCT(Plane, Vector3, normal)
SETGET_NUMBER_STRUCT(Plane, double, d)

SETGET_NUMBER_STRUCT(Quaternion, double, x)
SETGET_NUMBER_STRUCT(Quaternion, double, y)
SETGET_NUMBER_STRUCT(Quaternion, double, z)
SETGET_NUMBER_STRUCT(Quaternion, double, w)

SETGET_STRUCT_FUNC_INDEX(Basis, Vector3, x, set_column, get_column, 0)
SETGET_STRUCT_FUNC_INDEX(Basis, Vector3, y, set_column, get_column, 1)
SETGET_STRUCT_FUNC_INDEX(Basis, Vector3, z, set_column, get_column, 2)

SETGET_STRUCT(Transform3D, Basis, basis)
SETGET_STRUCT(Transform3D, Vector3, origin)

SETGET_STRUCT_CUSTOM(Projection, Vector4, x, columns[0])
SETGET_STRUCT_CUSTOM(Projection, Vector4, y, columns[1])
SETGET_STRUCT_CUSTOM(Projection, Vector4, z, columns[2])
SETGET_STRUCT_CUSTOM(Projection, Vector4, w, columns[3])

SETGET_NUMBER_STRUCT(Color, double, r)
SETGET_NUMBER_STRUCT(Color, double, g)
SETGET_NUMBER_STRUCT(Color, double, b)
SETGET_NUMBER_STRUCT(Color, double, a)

SETGET_NUMBER_STRUCT_FUNC(Color, int64_t, r8, set_r8, get_r8)
SETGET_NUMBER_STRUCT_FUNC(Color, int64_t, g8, set_g8, get_g8)
SETGET_NUMBER_STRUCT_FUNC(Color, int64_t, b8, set_b8, get_b8)
SETGET_NUMBER_STRUCT_FUNC(Color, int64_t, a8, set_a8, get_a8)

SETGET_NUMBER_STRUCT_FUNC(Color, double, h, set_h, get_h)
SETGET_NUMBER_STRUCT_FUNC(Color, double, s, set_s, get_s)
SETGET_NUMBER_STRUCT_FUNC(Color, double, v, set_v, get_v)

// TODO: When API Catches up
// SETGET_NUMBER_STRUCT_FUNC(Color, double, ok_hsl_h, set_ok_hsl_h, get_ok_hsl_h)
// SETGET_NUMBER_STRUCT_FUNC(Color, double, ok_hsl_s, set_ok_hsl_s, get_ok_hsl_s)
// SETGET_NUMBER_STRUCT_FUNC(Color, double, ok_hsl_l, set_ok_hsl_l, get_ok_hsl_l)

struct VariantSetterGetterInfo {
	void (*setter)(Variant* base, const Variant* value, bool& valid);
	void (*getter)(const Variant* base, Variant* value);
	RuztaVariantExtension::ValidatedSetter validated_setter;
	RuztaVariantExtension::ValidatedGetter validated_getter;
	RuztaVariantExtension::PTRSetter ptr_setter;
	RuztaVariantExtension::PTRGetter ptr_getter;
	Variant::Type member_type;
};

static LocalVector<VariantSetterGetterInfo> variant_setters_getters[Variant::VARIANT_MAX];
static LocalVector<StringName> variant_setters_getters_names[Variant::VARIANT_MAX];	 // one next to another to make it cache friendly

template <typename T>
static void register_member(Variant::Type p_type, const StringName& p_member) {
	VariantSetterGetterInfo sgi;
	sgi.setter = T::set;
	sgi.validated_setter = T::validated_set;
	sgi.ptr_setter = T::ptr_set;

	sgi.getter = T::get;
	sgi.validated_getter = T::validated_get;
	sgi.ptr_getter = T::ptr_get;

	sgi.member_type = T::get_type();

	variant_setters_getters[p_type].push_back(sgi);
	variant_setters_getters_names[p_type].push_back(p_member);
}

void register_named_setters_getters() {
#define REGISTER_MEMBER(m_base_type, m_member) register_member<VariantSetGet_##m_base_type##_##m_member>((Variant::Type)GetTypeInfo<m_base_type>::VARIANT_TYPE, #m_member)

	REGISTER_MEMBER(Vector2, x);
	REGISTER_MEMBER(Vector2, y);

	REGISTER_MEMBER(Vector2i, x);
	REGISTER_MEMBER(Vector2i, y);

	REGISTER_MEMBER(Vector3, x);
	REGISTER_MEMBER(Vector3, y);
	REGISTER_MEMBER(Vector3, z);

	REGISTER_MEMBER(Vector3i, x);
	REGISTER_MEMBER(Vector3i, y);
	REGISTER_MEMBER(Vector3i, z);

	REGISTER_MEMBER(Vector4, x);
	REGISTER_MEMBER(Vector4, y);
	REGISTER_MEMBER(Vector4, z);
	REGISTER_MEMBER(Vector4, w);

	REGISTER_MEMBER(Vector4i, x);
	REGISTER_MEMBER(Vector4i, y);
	REGISTER_MEMBER(Vector4i, z);
	REGISTER_MEMBER(Vector4i, w);

	REGISTER_MEMBER(Rect2, position);
	REGISTER_MEMBER(Rect2, size);
	REGISTER_MEMBER(Rect2, end);

	REGISTER_MEMBER(Rect2i, position);
	REGISTER_MEMBER(Rect2i, size);
	REGISTER_MEMBER(Rect2i, end);

	REGISTER_MEMBER(AABB, position);
	REGISTER_MEMBER(AABB, size);
	REGISTER_MEMBER(AABB, end);

	REGISTER_MEMBER(Transform2D, x);
	REGISTER_MEMBER(Transform2D, y);
	REGISTER_MEMBER(Transform2D, origin);

	REGISTER_MEMBER(Plane, x);
	REGISTER_MEMBER(Plane, y);
	REGISTER_MEMBER(Plane, z);
	REGISTER_MEMBER(Plane, d);
	REGISTER_MEMBER(Plane, normal);

	REGISTER_MEMBER(Quaternion, x);
	REGISTER_MEMBER(Quaternion, y);
	REGISTER_MEMBER(Quaternion, z);
	REGISTER_MEMBER(Quaternion, w);

	REGISTER_MEMBER(Basis, x);
	REGISTER_MEMBER(Basis, y);
	REGISTER_MEMBER(Basis, z);

	REGISTER_MEMBER(Transform3D, basis);
	REGISTER_MEMBER(Transform3D, origin);

	REGISTER_MEMBER(Projection, x);
	REGISTER_MEMBER(Projection, y);
	REGISTER_MEMBER(Projection, z);
	REGISTER_MEMBER(Projection, w);

	REGISTER_MEMBER(Color, r);
	REGISTER_MEMBER(Color, g);
	REGISTER_MEMBER(Color, b);
	REGISTER_MEMBER(Color, a);

	REGISTER_MEMBER(Color, r8);
	REGISTER_MEMBER(Color, g8);
	REGISTER_MEMBER(Color, b8);
	REGISTER_MEMBER(Color, a8);

	REGISTER_MEMBER(Color, h);
	REGISTER_MEMBER(Color, s);
	REGISTER_MEMBER(Color, v);

	// REGISTER_MEMBER(Color, ok_hsl_h);
	// REGISTER_MEMBER(Color, ok_hsl_s);
	// REGISTER_MEMBER(Color, ok_hsl_l);
}

void unregister_named_setters_getters() {
	for (int i = 0; i < Variant::VARIANT_MAX; i++) {
		variant_setters_getters[i].clear();
		variant_setters_getters_names[i].clear();
	}
}

bool RuztaVariantExtension::has_member(Variant::Type p_type, const StringName& p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);

	for (const StringName& member : variant_setters_getters_names[p_type]) {
		if (member == p_member) {
			return true;
		}
	}
	return false;
}

Variant::Type RuztaVariantExtension::get_member_type(Variant::Type p_type, const StringName& p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Variant::VARIANT_MAX);

	for (uint32_t i = 0; i < variant_setters_getters_names[p_type].size(); i++) {
		if (variant_setters_getters_names[p_type][i] == p_member) {
			return variant_setters_getters[p_type][i].member_type;
		}
	}

	return Variant::NIL;
}

void RuztaVariantExtension::get_member_list(Variant::Type p_type, List<StringName>* r_members) {
	for (const StringName& member : variant_setters_getters_names[p_type]) {
		r_members->push_back(member);
	}
}

int RuztaVariantExtension::get_member_count(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, -1);
	return variant_setters_getters_names[p_type].size();
}

RuztaVariantExtension::ValidatedSetter RuztaVariantExtension::get_member_validated_setter(Variant::Type p_type, const StringName& p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);

	for (uint32_t i = 0; i < variant_setters_getters_names[p_type].size(); i++) {
		if (variant_setters_getters_names[p_type][i] == p_member) {
			return variant_setters_getters[p_type][i].validated_setter;
		}
	}

	return nullptr;
}

RuztaVariantExtension::ValidatedGetter RuztaVariantExtension::get_member_validated_getter(Variant::Type p_type, const StringName& p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);

	for (uint32_t i = 0; i < variant_setters_getters_names[p_type].size(); i++) {
		if (variant_setters_getters_names[p_type][i] == p_member) {
			return variant_setters_getters[p_type][i].validated_getter;
		}
	}

	return nullptr;
}

RuztaVariantExtension::PTRSetter RuztaVariantExtension::get_member_ptr_setter(Variant::Type p_type, const StringName& p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);

	for (uint32_t i = 0; i < variant_setters_getters_names[p_type].size(); i++) {
		if (variant_setters_getters_names[p_type][i] == p_member) {
			return variant_setters_getters[p_type][i].ptr_setter;
		}
	}

	return nullptr;
}

RuztaVariantExtension::PTRGetter RuztaVariantExtension::get_member_ptr_getter(Variant::Type p_type, const StringName& p_member) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);

	for (uint32_t i = 0; i < variant_setters_getters_names[p_type].size(); i++) {
		if (variant_setters_getters_names[p_type][i] == p_member) {
			return variant_setters_getters[p_type][i].ptr_getter;
		}
	}

	return nullptr;
}

/**** INDEXED SETTERS AND GETTERS ****/

#ifdef DEBUG_ENABLED

#define OOB_TEST(m_idx, m_v) \
	ERR_FAIL_INDEX(m_idx, m_v)

#else

#define OOB_TEST(m_idx, m_v)

#endif

#ifdef DEBUG_ENABLED

#define NULL_TEST(m_key) \
	ERR_FAIL_NULL(m_key)

#else

#define NULL_TEST(m_key)

#endif

#define INDEXED_SETGET_STRUCT_TYPED(m_base_type, m_elem_type)                                         \
	struct VariantIndexedSetGet_##m_base_type {                                                       \
		static void get(const Variant* base, int64_t index, Variant* value, bool* oob) {              \
			int64_t size = VariantInternalAccessor<m_base_type>::get(base).size();                    \
			if (index < 0) {                                                                          \
				index += size;                                                                        \
			}                                                                                         \
			if (index < 0 || index >= size) {                                                         \
				*oob = true;                                                                          \
				return;                                                                               \
			}                                                                                         \
			*value = (VariantInternalAccessor<m_base_type>::get(base))[index];                        \
			*oob = false;                                                                             \
		}                                                                                             \
		static void ptr_get(const void* base, int64_t index, void* member) {                          \
			/* avoid ptrconvert for performance*/                                                     \
			const m_base_type& v = *reinterpret_cast<const m_base_type*>(base);                       \
			if (index < 0)                                                                            \
				index += v.size();                                                                    \
			OOB_TEST(index, v.size());                                                                \
			PtrToArg<m_elem_type>::encode(v[index], member);                                          \
		}                                                                                             \
		static void set(Variant* base, int64_t index, const Variant* value, bool* valid, bool* oob) { \
			if (value->get_type() != GetTypeInfo<m_elem_type>::VARIANT_TYPE) {                        \
				*oob = false;                                                                         \
				*valid = false;                                                                       \
				return;                                                                               \
			}                                                                                         \
			int64_t size = VariantInternalAccessor<m_base_type>::get(base).size();                    \
			if (index < 0) {                                                                          \
				index += size;                                                                        \
			}                                                                                         \
			if (index < 0 || index >= size) {                                                         \
				*oob = true;                                                                          \
				*valid = false;                                                                       \
				return;                                                                               \
			}                                                                                         \
			m_base_type(*base).set(index, VariantInternalAccessor<m_elem_type>::get(value));          \
			*oob = false;                                                                             \
			*valid = true;                                                                            \
		}                                                                                             \
		static void validated_set(Variant* base, int64_t index, const Variant* value, bool* oob) {    \
			int64_t size = VariantInternalAccessor<m_base_type>::get(base).size();                    \
			if (index < 0) {                                                                          \
				index += size;                                                                        \
			}                                                                                         \
			if (index < 0 || index >= size) {                                                         \
				*oob = true;                                                                          \
				return;                                                                               \
			}                                                                                         \
			m_base_type(*base).set(index, VariantInternalAccessor<m_elem_type>::get(value));          \
			*oob = false;                                                                             \
		}                                                                                             \
		static void ptr_set(void* base, int64_t index, const void* member) {                          \
			/* avoid ptrconvert for performance*/                                                     \
			m_base_type& v = *reinterpret_cast<m_base_type*>(base);                                   \
			if (index < 0)                                                                            \
				index += v.size();                                                                    \
			OOB_TEST(index, v.size());                                                                \
			v.set(index, PtrToArg<m_elem_type>::convert(member));                                     \
		}                                                                                             \
		static Variant::Type get_index_type() {                                                       \
			return (Variant::Type)GetTypeInfo<m_elem_type>::VARIANT_TYPE;                             \
		}                                                                                             \
		static uint32_t get_index_usage() {                                                           \
			return GetTypeInfo<m_elem_type>::get_class_info().usage;                                  \
		}                                                                                             \
		static uint64_t get_indexed_size(const Variant* base) {                                       \
			return VariantInternalAccessor<m_base_type>::get(base).size();                            \
		}                                                                                             \
	};

#define INDEXED_SETGET_STRUCT_TYPED_NUMERIC(m_base_type, m_elem_type, m_assign_type)                  \
	struct VariantIndexedSetGet_##m_base_type {                                                       \
		static void get(const Variant* base, int64_t index, Variant* value, bool* oob) {              \
			int64_t size = VariantInternalAccessor<m_base_type>::get(base).size();                    \
			if (index < 0) {                                                                          \
				index += size;                                                                        \
			}                                                                                         \
			if (index < 0 || index >= size) {                                                         \
				*oob = true;                                                                          \
				return;                                                                               \
			}                                                                                         \
			*value = base[index];                                                                     \
			*oob = false;                                                                             \
		}                                                                                             \
		static void ptr_get(const void* base, int64_t index, void* member) {                          \
			/* avoid ptrconvert for performance*/                                                     \
			const m_base_type& v = *reinterpret_cast<const m_base_type*>(base);                       \
			if (index < 0)                                                                            \
				index += v.size();                                                                    \
			OOB_TEST(index, v.size());                                                                \
			PtrToArg<m_elem_type>::encode(v[index], member);                                          \
		}                                                                                             \
		static void set(Variant* base, int64_t index, const Variant* value, bool* valid, bool* oob) { \
			int64_t size = VariantInternalAccessor<m_base_type>::get(base).size();                    \
			if (index < 0) {                                                                          \
				index += size;                                                                        \
			}                                                                                         \
			if (index < 0 || index >= size) {                                                         \
				*oob = true;                                                                          \
				*valid = false;                                                                       \
				return;                                                                               \
			}                                                                                         \
			m_assign_type num;                                                                        \
			if (value->get_type() == Variant::INT) {                                                  \
				num = (m_assign_type) * &VariantInternalAccessor<int64_t>::get(value);                \
			} else if (value->get_type() == Variant::FLOAT) {                                         \
				num = (m_assign_type) * &VariantInternalAccessor<double>::get(value);                 \
			} else {                                                                                  \
				*oob = false;                                                                         \
				*valid = false;                                                                       \
				return;                                                                               \
			}                                                                                         \
			m_base_type(*base).set(index, num);                                                       \
			*oob = false;                                                                             \
			*valid = true;                                                                            \
		}                                                                                             \
		static void validated_set(Variant* base, int64_t index, const Variant* value, bool* oob) {    \
			int64_t size = VariantInternalAccessor<m_base_type>::get(base).size();                    \
			if (index < 0) {                                                                          \
				index += size;                                                                        \
			}                                                                                         \
			if (index < 0 || index >= size) {                                                         \
				*oob = true;                                                                          \
				return;                                                                               \
			}                                                                                         \
			m_base_type(*base).set(index, VariantInternalAccessor<m_elem_type>::get(value));          \
			*oob = false;                                                                             \
		}                                                                                             \
		static void ptr_set(void* base, int64_t index, const void* member) {                          \
			/* avoid ptrconvert for performance*/                                                     \
			m_base_type& v = *reinterpret_cast<m_base_type*>(base);                                   \
			if (index < 0)                                                                            \
				index += v.size();                                                                    \
			OOB_TEST(index, v.size());                                                                \
			v.set(index, PtrToArg<m_elem_type>::convert(member));                                     \
		}                                                                                             \
		static Variant::Type get_index_type() {                                                       \
			return (Variant::Type)GetTypeInfo<m_elem_type>::VARIANT_TYPE;                             \
		}                                                                                             \
		static uint32_t get_index_usage() {                                                           \
			return GetTypeInfo<m_elem_type>::get_class_info().usage;                                  \
		}                                                                                             \
		static uint64_t get_indexed_size(const Variant* base) {                                       \
			return VariantInternalAccessor<m_base_type>::get(base).size();                            \
		}                                                                                             \
	};

#define INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(m_base_type, m_elem_type, m_assign_type, m_max)         \
	struct VariantIndexedSetGet_##m_base_type {                                                       \
		static void get(const Variant* base, int64_t index, Variant* value, bool* oob) {              \
			if (index < 0 || index >= m_max) {                                                        \
				*oob = true;                                                                          \
				return;                                                                               \
			}                                                                                         \
			*value = (VariantInternalAccessor<m_base_type>::get(base))[index];                        \
			*oob = false;                                                                             \
		}                                                                                             \
		static void ptr_get(const void* base, int64_t index, void* member) {                          \
			/* avoid ptrconvert for performance*/                                                     \
			const m_base_type& v = *reinterpret_cast<const m_base_type*>(base);                       \
			OOB_TEST(index, m_max);                                                                   \
			PtrToArg<m_elem_type>::encode(v[index], member);                                          \
		}                                                                                             \
		static void set(Variant* base, int64_t index, const Variant* value, bool* valid, bool* oob) { \
			if (index < 0 || index >= m_max) {                                                        \
				*oob = true;                                                                          \
				*valid = false;                                                                       \
				return;                                                                               \
			}                                                                                         \
			m_assign_type num;                                                                        \
			if (value->get_type() == Variant::INT) {                                                  \
				num = (m_assign_type) * &VariantInternalAccessor<int64_t>::get(value);                \
			} else if (value->get_type() == Variant::FLOAT) {                                         \
				num = (m_assign_type) * &VariantInternalAccessor<double>::get(value);                 \
			} else {                                                                                  \
				*oob = false;                                                                         \
				*valid = false;                                                                       \
				return;                                                                               \
			}                                                                                         \
			base[index] = num;                                                                        \
			*oob = false;                                                                             \
			*valid = true;                                                                            \
		}                                                                                             \
		static void validated_set(Variant* base, int64_t index, const Variant* value, bool* oob) {    \
			if (index < 0 || index >= m_max) {                                                        \
				*oob = true;                                                                          \
				return;                                                                               \
			}                                                                                         \
			base[index] = VariantInternalAccessor<m_elem_type>::get(value);                           \
			*oob = false;                                                                             \
		}                                                                                             \
		static void ptr_set(void* base, int64_t index, const void* member) {                          \
			/* avoid ptrconvert for performance*/                                                     \
			m_base_type& v = *reinterpret_cast<m_base_type*>(base);                                   \
			OOB_TEST(index, m_max);                                                                   \
			v[index] = PtrToArg<m_elem_type>::convert(member);                                        \
		}                                                                                             \
		static Variant::Type get_index_type() {                                                       \
			return (Variant::Type)GetTypeInfo<m_elem_type>::VARIANT_TYPE;                             \
		}                                                                                             \
		static uint32_t get_index_usage() {                                                           \
			return GetTypeInfo<m_elem_type>::get_class_info().usage;                                  \
		}                                                                                             \
		static uint64_t get_indexed_size(const Variant* base) {                                       \
			return m_max;                                                                             \
		}                                                                                             \
	};

#define INDEXED_SETGET_STRUCT_BUILTIN_ACCESSOR(m_base_type, m_elem_type, m_accessor, m_max)           \
	struct VariantIndexedSetGet_##m_base_type {                                                       \
		static void get(const Variant* base, int64_t index, Variant* value, bool* oob) {              \
			if (index < 0 || index >= m_max) {                                                        \
				*oob = true;                                                                          \
				return;                                                                               \
			}                                                                                         \
			*value = (VariantInternalAccessor<m_base_type>::get(base))m_accessor[index];              \
			*oob = false;                                                                             \
		}                                                                                             \
		static void ptr_get(const void* base, int64_t index, void* member) {                          \
			/* avoid ptrconvert for performance*/                                                     \
			const m_base_type& v = *reinterpret_cast<const m_base_type*>(base);                       \
			OOB_TEST(index, m_max);                                                                   \
			PtrToArg<m_elem_type>::encode(v m_accessor[index], member);                               \
		}                                                                                             \
		static void set(Variant* base, int64_t index, const Variant* value, bool* valid, bool* oob) { \
			if (value->get_type() != GetTypeInfo<m_elem_type>::VARIANT_TYPE) {                        \
				*oob = false;                                                                         \
				*valid = false;                                                                       \
			}                                                                                         \
			if (index < 0 || index >= m_max) {                                                        \
				*oob = true;                                                                          \
				*valid = false;                                                                       \
				return;                                                                               \
			}                                                                                         \
			m_base_type(*base) m_accessor[index] = VariantInternalAccessor<m_elem_type>::get(value);  \
			*oob = false;                                                                             \
			*valid = true;                                                                            \
		}                                                                                             \
		static void validated_set(Variant* base, int64_t index, const Variant* value, bool* oob) {    \
			if (index < 0 || index >= m_max) {                                                        \
				*oob = true;                                                                          \
				return;                                                                               \
			}                                                                                         \
			m_base_type(*base) m_accessor[index] = VariantInternalAccessor<m_elem_type>::get(value);  \
			*oob = false;                                                                             \
		}                                                                                             \
		static void ptr_set(void* base, int64_t index, const void* member) {                          \
			/* avoid ptrconvert for performance*/                                                     \
			m_base_type& v = *reinterpret_cast<m_base_type*>(base);                                   \
			OOB_TEST(index, m_max);                                                                   \
			v m_accessor[index] = PtrToArg<m_elem_type>::convert(member);                             \
		}                                                                                             \
		static Variant::Type get_index_type() {                                                       \
			return (Variant::Type)GetTypeInfo<m_elem_type>::VARIANT_TYPE;                             \
		}                                                                                             \
		static uint32_t get_index_usage() {                                                           \
			return GetTypeInfo<m_elem_type>::get_class_info().usage;                                  \
		}                                                                                             \
		static uint64_t get_indexed_size(const Variant* base) {                                       \
			return m_max;                                                                             \
		}                                                                                             \
	};

#define INDEXED_SETGET_STRUCT_BUILTIN_FUNC(m_base_type, m_elem_type, m_set, m_get, m_max)             \
	struct VariantIndexedSetGet_##m_base_type {                                                       \
		static void get(const Variant* base, int64_t index, Variant* value, bool* oob) {              \
			if (index < 0 || index >= m_max) {                                                        \
				*oob = true;                                                                          \
				return;                                                                               \
			}                                                                                         \
			*value = VariantInternalAccessor<m_base_type>::get(base).m_get(index);                    \
			*oob = false;                                                                             \
		}                                                                                             \
		static void ptr_get(const void* base, int64_t index, void* member) {                          \
			/* avoid ptrconvert for performance*/                                                     \
			const m_base_type& v = *reinterpret_cast<const m_base_type*>(base);                       \
			OOB_TEST(index, m_max);                                                                   \
			PtrToArg<m_elem_type>::encode(v.m_get(index), member);                                    \
		}                                                                                             \
		static void set(Variant* base, int64_t index, const Variant* value, bool* valid, bool* oob) { \
			if (value->get_type() != GetTypeInfo<m_elem_type>::VARIANT_TYPE) {                        \
				*oob = false;                                                                         \
				*valid = false;                                                                       \
			}                                                                                         \
			if (index < 0 || index >= m_max) {                                                        \
				*oob = true;                                                                          \
				*valid = false;                                                                       \
				return;                                                                               \
			}                                                                                         \
			m_base_type(*base).m_set(index, VariantInternalAccessor<m_elem_type>::get(value));        \
			*oob = false;                                                                             \
			*valid = true;                                                                            \
		}                                                                                             \
		static void validated_set(Variant* base, int64_t index, const Variant* value, bool* oob) {    \
			if (index < 0 || index >= m_max) {                                                        \
				*oob = true;                                                                          \
				return;                                                                               \
			}                                                                                         \
			m_base_type(*base).m_set(index, VariantInternalAccessor<m_elem_type>::get(value));        \
			*oob = false;                                                                             \
		}                                                                                             \
		static void ptr_set(void* base, int64_t index, const void* member) {                          \
			/* avoid ptrconvert for performance*/                                                     \
			m_base_type& v = *reinterpret_cast<m_base_type*>(base);                                   \
			OOB_TEST(index, m_max);                                                                   \
			v.m_set(index, PtrToArg<m_elem_type>::convert(member));                                   \
		}                                                                                             \
		static Variant::Type get_index_type() {                                                       \
			return (Variant::Type)GetTypeInfo<m_elem_type>::VARIANT_TYPE;                             \
		}                                                                                             \
		static uint32_t get_index_usage() {                                                           \
			return GetTypeInfo<m_elem_type>::get_class_info().usage;                                  \
		}                                                                                             \
		static uint64_t get_indexed_size(const Variant* base) {                                       \
			return m_max;                                                                             \
		}                                                                                             \
	};

struct VariantIndexedSetGet_Array {
	static void get(const Variant* base, int64_t index, Variant* value, bool* oob) {
		int64_t size = VariantInternalAccessor<Array>::get(base).size();
		if (index < 0) {
			index += size;
		}
		if (index < 0 || index >= size) {
			*oob = true;
			return;
		}
		*value = (VariantInternalAccessor<Array>::get(base))[index];
		*oob = false;
	}
	static void ptr_get(const void* base, int64_t index, void* member) {
		/* avoid ptrconvert for performance*/
		const Array& v = *reinterpret_cast<const Array*>(base);
		if (index < 0) {
			index += v.size();
		}
		OOB_TEST(index, v.size());
		PtrToArg<Variant>::encode(v[index], member);
	}
	static void set(Variant* base, int64_t index, const Variant* value, bool* valid, bool* oob) {
		if (VariantInternalAccessor<Array>::get(base).is_read_only()) {
			*valid = false;
			*oob = true;
			return;
		}
		int64_t size = VariantInternalAccessor<Array>::get(base).size();
		if (index < 0) {
			index += size;
		}
		if (index < 0 || index >= size) {
			*oob = true;
			*valid = false;
			return;
		}
		Array(*base).set(index, *value);
		*oob = false;
		*valid = true;
	}
	static void validated_set(Variant* base, int64_t index, const Variant* value, bool* oob) {
		if (VariantInternalAccessor<Array>::get(base).is_read_only()) {
			*oob = true;
			return;
		}
		int64_t size = VariantInternalAccessor<Array>::get(base).size();
		if (index < 0) {
			index += size;
		}
		if (index < 0 || index >= size) {
			*oob = true;
			return;
		}
		Array(*base).set(index, *value);
		*oob = false;
	}
	static void ptr_set(void* base, int64_t index, const void* member) {
		/* avoid ptrconvert for performance*/
		Array& v = *reinterpret_cast<Array*>(base);
		if (index < 0) {
			index += v.size();
		}
		OOB_TEST(index, v.size());
		v.set(index, PtrToArg<Variant>::convert(member));
	}
	static Variant::Type get_index_type() { return Variant::NIL; }
	static uint32_t get_index_usage() { return PROPERTY_USAGE_NIL_IS_VARIANT; }
	static uint64_t get_indexed_size(const Variant* base) { return 0; }
};

struct VariantIndexedSetGet_Dictionary {
	static void get(const Variant* base, int64_t index, Variant* value, bool* oob) {
		const Variant* ptr = &Dictionary(*base).get(index, Variant());
		if (!ptr) {
			*oob = true;
			return;
		}
		*value = *ptr;
		*oob = false;
	}
	static void ptr_get(const void* base, int64_t index, void* member) {
		// Avoid ptrconvert for performance.
		const Dictionary& v = *reinterpret_cast<const Dictionary*>(base);
		const Variant* ptr = &v.get(index, Variant());
		NULL_TEST(ptr);
		PtrToArg<Variant>::encode(*ptr, member);
	}
	static void set(Variant* base, int64_t index, const Variant* value, bool* valid, bool* oob) {
		*valid = Dictionary(*base).set(index, *value);
		*oob = VariantInternalAccessor<Dictionary>::get(base).is_read_only();
	}
	static void validated_set(Variant* base, int64_t index, const Variant* value, bool* oob) {
		Dictionary(*base).set(index, *value);
		*oob = VariantInternalAccessor<Dictionary>::get(base).is_read_only();
	}
	static void ptr_set(void* base, int64_t index, const void* member) {
		Dictionary& v = *reinterpret_cast<Dictionary*>(base);
		v.set(index, PtrToArg<Variant>::convert(member));
	}
	static Variant::Type get_index_type() { return Variant::NIL; }
	static uint32_t get_index_usage() { return PROPERTY_USAGE_DEFAULT; }
	static uint64_t get_indexed_size(const Variant* base) { return VariantInternalAccessor<Dictionary>::get(base).size(); }
};

struct VariantIndexedSetGet_String {
	static void get(const Variant* base, int64_t index, Variant* value, bool* oob) {
		int64_t length = VariantInternalAccessor<String>::get(base).length();
		if (index < 0) {
			index += length;
		}
		if (index < 0 || index >= length) {
			*oob = true;
			return;
		}
		*value = String::chr((VariantInternalAccessor<String>::get(base))[index]);
		*oob = false;
	}
	static void ptr_get(const void* base, int64_t index, void* member) {
		/* avoid ptrconvert for performance*/
		const String& v = *reinterpret_cast<const String*>(base);
		if (index < 0) {
			index += v.length();
		}
		OOB_TEST(index, v.length());
		PtrToArg<String>::encode(String::chr(v[index]), member);
	}
	static void set(Variant* base, int64_t index, const Variant* value, bool* valid, bool* oob) {
		if (value->get_type() != Variant::STRING) {
			*oob = false;
			*valid = false;
			return;
		}
		int64_t length = VariantInternalAccessor<String>::get(base).length();
		if (index < 0) {
			index += length;
		}
		if (index < 0 || index >= length) {
			*oob = true;
			*valid = false;
			return;
		}
		String* b = &String(*base);
		const String* v = VariantInternal::get_string(value);
		if (v->length() == 0) {
			b->remove_char(index);
		} else {
			b[index] = v[0];
		}
		*oob = false;
		*valid = true;
	}
	static void validated_set(Variant* base, int64_t index, const Variant* value, bool* oob) {
		int64_t length = VariantInternalAccessor<String>::get(base).length();
		if (index < 0) {
			index += length;
		}
		if (index < 0 || index >= length) {
			*oob = true;
			return;
		}
		String* b = &String(*base);
		const String* v = VariantInternal::get_string(value);
		if (v->length() == 0) {
			b->remove_char(index);
		} else {
			b[index] = v[0];
		}
		*oob = false;
	}
	static void ptr_set(void* base, int64_t index, const void* member) {
		/* avoid ptrconvert for performance*/
		String& v = *reinterpret_cast<String*>(base);
		if (index < 0) {
			index += v.length();
		}
		OOB_TEST(index, v.length());
		const String& m = *reinterpret_cast<const String*>(member);
		if (unlikely(m.length() == 0)) {
			v.remove_char(index);
		} else {
			v[index] = m.unicode_at(0);
		}
	}
	static Variant::Type get_index_type() { return Variant::STRING; }
	static uint32_t get_index_usage() { return PROPERTY_USAGE_DEFAULT; }
	static uint64_t get_indexed_size(const Variant* base) { return VariantInternal::get_string(base)->length(); }
};

INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector2, double, real_t, 2)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector2i, int64_t, int32_t, 2)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector3, double, real_t, 3)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector3i, int64_t, int32_t, 3)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector4, double, real_t, 4)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Vector4i, int64_t, int32_t, 4)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Quaternion, double, real_t, 4)
INDEXED_SETGET_STRUCT_BUILTIN_NUMERIC(Color, double, float, 4)

INDEXED_SETGET_STRUCT_BUILTIN_ACCESSOR(Transform2D, Vector2, .columns, 3)
INDEXED_SETGET_STRUCT_BUILTIN_FUNC(Basis, Vector3, set_column, get_column, 3)
INDEXED_SETGET_STRUCT_BUILTIN_ACCESSOR(Projection, Vector4, .columns, 4)

INDEXED_SETGET_STRUCT_TYPED_NUMERIC(PackedByteArray, int64_t, uint8_t)
INDEXED_SETGET_STRUCT_TYPED_NUMERIC(PackedInt32Array, int64_t, int32_t)
INDEXED_SETGET_STRUCT_TYPED_NUMERIC(PackedInt64Array, int64_t, int64_t)
INDEXED_SETGET_STRUCT_TYPED_NUMERIC(PackedFloat32Array, double, float)
INDEXED_SETGET_STRUCT_TYPED_NUMERIC(PackedFloat64Array, double, double)
INDEXED_SETGET_STRUCT_TYPED(PackedVector2Array, Vector2)
INDEXED_SETGET_STRUCT_TYPED(PackedVector3Array, Vector3)
INDEXED_SETGET_STRUCT_TYPED(PackedStringArray, String)
INDEXED_SETGET_STRUCT_TYPED(PackedColorArray, Color)
INDEXED_SETGET_STRUCT_TYPED(PackedVector4Array, Vector4)

struct VariantIndexedSetterGetterInfo {
	void (*setter)(Variant* base, int64_t index, const Variant* value, bool* valid, bool* oob) = nullptr;
	void (*getter)(const Variant* base, int64_t index, Variant* value, bool* oob) = nullptr;

	RuztaVariantExtension::ValidatedIndexedSetter validated_setter = nullptr;
	RuztaVariantExtension::ValidatedIndexedGetter validated_getter = nullptr;

	RuztaVariantExtension::PTRIndexedSetter ptr_setter = nullptr;
	RuztaVariantExtension::PTRIndexedGetter ptr_getter = nullptr;

	uint64_t (*get_indexed_size)(const Variant* base) = nullptr;

	Variant::Type index_type = Variant::NIL;
	uint32_t index_usage = PROPERTY_USAGE_DEFAULT;

	bool valid = false;
};

static VariantIndexedSetterGetterInfo variant_indexed_setters_getters[Variant::VARIANT_MAX];

template <typename T>
static void register_indexed_member(Variant::Type p_type) {
	VariantIndexedSetterGetterInfo& sgi = variant_indexed_setters_getters[p_type];

	sgi.setter = T::set;
	sgi.validated_setter = T::validated_set;
	sgi.ptr_setter = T::ptr_set;

	sgi.getter = T::get;
	sgi.validated_getter = T::get;
	sgi.ptr_getter = T::ptr_get;

	sgi.index_type = T::get_index_type();
	sgi.index_usage = T::get_index_usage();
	sgi.get_indexed_size = T::get_indexed_size;

	sgi.valid = true;
}

void register_indexed_setters_getters() {
#define REGISTER_INDEXED_MEMBER(m_base_type) register_indexed_member<VariantIndexedSetGet_##m_base_type>((Variant::Type)GetTypeInfo<m_base_type>::VARIANT_TYPE)

	REGISTER_INDEXED_MEMBER(String);
	REGISTER_INDEXED_MEMBER(Vector2);
	REGISTER_INDEXED_MEMBER(Vector2i);
	REGISTER_INDEXED_MEMBER(Vector3);
	REGISTER_INDEXED_MEMBER(Vector3i);
	REGISTER_INDEXED_MEMBER(Vector4);
	REGISTER_INDEXED_MEMBER(Vector4i);
	REGISTER_INDEXED_MEMBER(Quaternion);
	REGISTER_INDEXED_MEMBER(Color);
	REGISTER_INDEXED_MEMBER(Transform2D);
	REGISTER_INDEXED_MEMBER(Basis);
	REGISTER_INDEXED_MEMBER(Projection);

	REGISTER_INDEXED_MEMBER(PackedByteArray);
	REGISTER_INDEXED_MEMBER(PackedInt32Array);
	REGISTER_INDEXED_MEMBER(PackedInt64Array);
	REGISTER_INDEXED_MEMBER(PackedFloat32Array);
	REGISTER_INDEXED_MEMBER(PackedFloat64Array);
	REGISTER_INDEXED_MEMBER(PackedVector2Array);
	REGISTER_INDEXED_MEMBER(PackedVector3Array);
	REGISTER_INDEXED_MEMBER(PackedStringArray);
	REGISTER_INDEXED_MEMBER(PackedColorArray);
	REGISTER_INDEXED_MEMBER(PackedVector4Array);

	REGISTER_INDEXED_MEMBER(Array);
	REGISTER_INDEXED_MEMBER(Dictionary);
}

bool RuztaVariantExtension::has_indexing(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, false);
	return variant_indexed_setters_getters[p_type].valid;
}

Variant::Type RuztaVariantExtension::get_indexed_element_type(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, Variant::VARIANT_MAX);
	return variant_indexed_setters_getters[p_type].index_type;
}

uint32_t RuztaVariantExtension::get_indexed_element_usage(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, PROPERTY_USAGE_DEFAULT);
	return variant_indexed_setters_getters[p_type].index_usage;
}

RuztaVariantExtension::ValidatedIndexedSetter RuztaVariantExtension::get_member_validated_indexed_setter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	return variant_indexed_setters_getters[p_type].validated_setter;
}

RuztaVariantExtension::ValidatedIndexedGetter RuztaVariantExtension::get_member_validated_indexed_getter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	return variant_indexed_setters_getters[p_type].validated_getter;
}

RuztaVariantExtension::PTRIndexedSetter RuztaVariantExtension::get_member_ptr_indexed_setter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	return variant_indexed_setters_getters[p_type].ptr_setter;
}

RuztaVariantExtension::PTRIndexedGetter RuztaVariantExtension::get_member_ptr_indexed_getter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::VARIANT_MAX, nullptr);
	return variant_indexed_setters_getters[p_type].ptr_getter;
}

struct VariantKeyedSetGetDictionary {
	static void get(const Variant* base, const Variant* key, Variant* value, bool* r_valid) {
		const Variant* ptr = &Dictionary(*base).get(*key, Variant());
		if (!ptr) {
			*r_valid = false;
			return;
		}
		*value = *ptr;
		*r_valid = true;
	}
	static void ptr_get(const void* base, const void* key, void* value) {
		/* avoid ptrconvert for performance*/
		const Dictionary& v = *reinterpret_cast<const Dictionary*>(base);
		const Variant* ptr = &v.get(PtrToArg<Variant>::convert(key), Variant());
		NULL_TEST(ptr);
		PtrToArg<Variant>::encode(*ptr, value);
	}
	static void set(Variant* base, const Variant* key, const Variant* value, bool* r_valid) {
		*r_valid = Dictionary(*base).set(*key, *value);
	}
	static void ptr_set(void* base, const void* key, const void* value) {
		Dictionary& v = *reinterpret_cast<Dictionary*>(base);
		v.set(PtrToArg<Variant>::convert(key), PtrToArg<Variant>::convert(value));
	}

	static bool has(const Variant* base, const Variant* key, bool* r_valid) {
		*r_valid = true;
		return VariantInternalAccessor<Dictionary>::get(base).has(*key);
	}
	static uint32_t ptr_has(const void* base, const void* key) {
		/* avoid ptrconvert for performance*/
		const Dictionary& v = *reinterpret_cast<const Dictionary*>(base);
		return v.has(PtrToArg<Variant>::convert(key));
	}
};

struct VariantKeyedSetGetObject {
	static void get(const Variant* base, const Variant* key, Variant* value, bool* r_valid) {
		Object* obj = base->get_validated_object();

		if (!obj) {
			*r_valid = false;
			*value = Variant();
			return;
		}
		*value = obj->get(*key);
	}
	static void ptr_get(const void* base, const void* key, void* value) {
		const Object* obj = PtrToArg<Object*>::convert(base);
		NULL_TEST(obj);
		Variant v = obj->get(PtrToArg<Variant>::convert(key));
		PtrToArg<Variant>::encode(v, value);
	}
	static void set(Variant* base, const Variant* key, const Variant* value, bool* r_valid) {
		Object* obj = base->get_validated_object();

		if (!obj) {
			*r_valid = false;
			return;
		}
		obj->set(*key, *value);
	}
	static void ptr_set(void* base, const void* key, const void* value) {
		Object* obj = PtrToArg<Object*>::convert(base);
		NULL_TEST(obj);
		obj->set(PtrToArg<Variant>::convert(key), PtrToArg<Variant>::convert(value));
	}
	static bool has(const Variant* base, const Variant* key, bool* r_valid) {
		Object* obj = base->get_validated_object();
		if (!obj) {
			*r_valid = false;
			return false;
		}
		*r_valid = true;
		bool exists = false;
		for (Dictionary prop : obj->get_property_list()) {
			String prop_name = *key;
			if (prop.has(prop_name)) {
				exists = true;
				break;
			}
		}
		return exists;
	}
	static uint32_t ptr_has(const void* base, const void* key) {
		const Object* obj = PtrToArg<Object*>::convert(base);
		ERR_FAIL_NULL_V(obj, false);
		bool valid = false;
		for (Dictionary prop : obj->get_property_list()) {
			String prop_name = PtrToArg<Variant>::convert(key);
			if (prop.has(prop_name)) {
				valid = true;
				break;
			}
		}
		return valid;
	}
};

struct VariantKeyedSetterGetterInfo {
	RuztaVariantExtension::ValidatedKeyedSetter validated_setter = nullptr;
	RuztaVariantExtension::ValidatedKeyedGetter validated_getter = nullptr;
	RuztaVariantExtension::ValidatedKeyedChecker validated_checker = nullptr;

	RuztaVariantExtension::PTRKeyedSetter ptr_setter = nullptr;
	RuztaVariantExtension::PTRKeyedGetter ptr_getter = nullptr;
	RuztaVariantExtension::PTRKeyedChecker ptr_checker = nullptr;

	bool valid = false;
};

static VariantKeyedSetterGetterInfo variant_keyed_setters_getters[Variant::VARIANT_MAX];

template <typename T>
static void register_keyed_member(Variant::Type p_type) {
	VariantKeyedSetterGetterInfo& sgi = variant_keyed_setters_getters[p_type];

	sgi.validated_setter = T::set;
	sgi.ptr_setter = T::ptr_set;

	sgi.validated_getter = T::get;
	sgi.ptr_getter = T::ptr_get;

	sgi.validated_checker = T::has;
	sgi.ptr_checker = T::ptr_has;

	sgi.valid = true;
}

static void register_keyed_setters_getters() {
	register_keyed_member<VariantKeyedSetGetDictionary>(Variant::DICTIONARY);
	register_keyed_member<VariantKeyedSetGetObject>(Variant::OBJECT);
}

bool RuztaVariantExtension::is_keyed(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::Type::VARIANT_MAX, false);
	return variant_keyed_setters_getters[p_type].valid;
}

RuztaVariantExtension::ValidatedKeyedSetter RuztaVariantExtension::get_member_validated_keyed_setter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::Type::VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].validated_setter;
}

RuztaVariantExtension::ValidatedKeyedGetter RuztaVariantExtension::get_member_validated_keyed_getter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::Type::VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].validated_getter;
}

RuztaVariantExtension::ValidatedKeyedChecker RuztaVariantExtension::get_member_validated_keyed_checker(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::Type::VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].validated_checker;
}

RuztaVariantExtension::PTRKeyedSetter RuztaVariantExtension::get_member_ptr_keyed_setter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::Type::VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].ptr_setter;
}

RuztaVariantExtension::PTRKeyedGetter RuztaVariantExtension::get_member_ptr_keyed_getter(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::Type::VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].ptr_getter;
}

RuztaVariantExtension::PTRKeyedChecker RuztaVariantExtension::get_member_ptr_keyed_checker(Variant::Type p_type) {
	ERR_FAIL_INDEX_V(p_type, Variant::Type::VARIANT_MAX, nullptr);
	return variant_keyed_setters_getters[p_type].ptr_checker;
}

void RuztaVariantExtension::_register_variant_setters_getters() {
	register_named_setters_getters();
	register_indexed_setters_getters();
	register_keyed_setters_getters();
}
void RuztaVariantExtension::_unregister_variant_setters_getters() {
	unregister_named_setters_getters();
}
