/**************************************************************************/
/*  core_constants.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                RUZTA                                   */
/*                    https://seremtitus.co.ke/ruzta                      */
/**************************************************************************/
/* Copyright (c) 2025-present Ruzta contributors (see AUTHORS.md).        */
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

#include "ruzta_variant/core_constants.h" // original: core_constants.h

#include <godot_cpp/classes/global_constants.hpp>  // original:
#include <godot_cpp/core/class_db.hpp>			   // original: core/object/class_db.h
#include <godot_cpp/core/type_info.hpp>			   // original:
#include <godot_cpp/variant/variant.hpp>		   // original: core/variant/variant.h

struct _CoreConstant {
#ifdef DEBUG_ENABLED
	bool ignore_value_in_docs = false;
	bool is_bitfield = false;
#endif	// DEBUG_ENABLED
	StringName enum_name;
	const char* name = nullptr;
	int64_t value = 0;

	_CoreConstant() {}

#ifdef DEBUG_ENABLED
	_CoreConstant(const StringName& p_enum_name, const char* p_name, int64_t p_value, bool p_ignore_value_in_docs = false, bool p_is_bitfield = false) : ignore_value_in_docs(p_ignore_value_in_docs),
																																						 is_bitfield(p_is_bitfield),
																																						 enum_name(p_enum_name),
																																						 name(p_name),
																																						 value(p_value) {
	}
#else
	_CoreConstant(const StringName& p_enum_name, const char* p_name, int64_t p_value) : enum_name(p_enum_name),
																						name(p_name),
																						value(p_value) {
	}
#endif	// DEBUG_ENABLED
};

static Vector<_CoreConstant> _global_constants;
static HashMap<StringName, int> _global_constants_map;
static HashMap<StringName, Vector<_CoreConstant>> _global_enums;

#ifdef DEBUG_ENABLED

#define BIND_CORE_CONSTANT(m_constant)                                                 \
	_global_constants.push_back(_CoreConstant(StringName(), #m_constant, m_constant)); \
	_global_constants_map[#m_constant] = _global_constants.size() - 1;

#define BIND_CORE_ENUM_CONSTANT(m_constant)                                                          \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_enum_name(m_constant, StringName(#m_constant));     \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant));              \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_BITFIELD_FLAG(m_constant)                                                          \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_bitfield_name(m_constant, StringName(#m_constant)); \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant, false, true)); \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

// This just binds enum classes as if they were regular enum constants.
#define BIND_CORE_ENUM_CLASS_CONSTANT(m_enum, m_prefix, m_member)                                                  \
	{                                                                                                              \
		StringName enum_name = _gde_constant_get_enum_name(m_enum::m_member, StringName(#m_member));               \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                             \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);               \
	}

#define BIND_CORE_BITFIELD_CLASS_FLAG(m_enum, m_prefix, m_member)                                                               \
	{                                                                                                                           \
		StringName enum_name = _gde_constant_get_bitfield_name(m_enum::m_member, StringName(#m_member));                        \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member, false, true)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                                          \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);                            \
	}

#define BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(m_enum, m_name, m_member)                               \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_enum_name(m_enum::m_member, StringName(#m_member)); \
		_global_constants.push_back(_CoreConstant(enum_name, #m_name, (int64_t)m_enum::m_member));   \
		_global_constants_map[#m_name] = _global_constants.size() - 1;                               \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_BITFIELD_CLASS_FLAG_CUSTOM(m_enum, m_name, m_member)                                          \
	{                                                                                                           \
		StringName enum_name = _gde_constant_get_bitfield_name(m_enum::m_member, StringName(#m_member));        \
		_global_constants.push_back(_CoreConstant(enum_name, #m_name, (int64_t)m_enum::m_member, false, true)); \
		_global_constants_map[#m_name] = _global_constants.size() - 1;                                          \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);            \
	}

#define BIND_CORE_ENUM_CLASS_CONSTANT_NO_VAL(m_enum, m_prefix, m_member)                                                 \
	{                                                                                                                    \
		StringName enum_name = _gde_constant_get_enum_name(m_enum::m_member, StringName(#m_member));                     \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member, true)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                                   \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);                     \
	}

#define BIND_CORE_ENUM_CONSTANT_CUSTOM(m_custom_name, m_constant)                                    \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_enum_name(m_constant, StringName(#m_constant));     \
		_global_constants.push_back(_CoreConstant(enum_name, m_custom_name, m_constant));            \
		_global_constants_map[m_custom_name] = _global_constants.size() - 1;                         \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_CONSTANT_NO_VAL(m_constant)                                                \
	_global_constants.push_back(_CoreConstant(StringName(), #m_constant, m_constant, true)); \
	_global_constants_map[#m_constant] = _global_constants.size() - 1;

#define BIND_CORE_ENUM_CONSTANT_NO_VAL(m_constant)                                                   \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_enum_name(m_constant, StringName(#m_constant));     \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant, true));        \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_ENUM_CONSTANT_CUSTOM_NO_VAL(m_custom_name, m_constant)                             \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_enum_name(m_constant, StringName(#m_constant));     \
		_global_constants.push_back(_CoreConstant(enum_name, m_custom_name, m_constant, true));      \
		_global_constants_map[m_custom_name] = _global_constants.size() - 1;                         \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#else

#define BIND_CORE_CONSTANT(m_constant)                                                 \
	_global_constants.push_back(_CoreConstant(StringName(), #m_constant, m_constant)); \
	_global_constants_map[#m_constant] = _global_constants.size() - 1;

#define BIND_CORE_ENUM_CONSTANT(m_constant)                                                          \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_enum_name(m_constant, StringName(#m_constant));     \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant));              \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_BITFIELD_FLAG(m_constant)                                                          \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_bitfield_name(m_constant, StringName(#m_constant)); \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant));              \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

// This just binds enum classes as if they were regular enum constants.
#define BIND_CORE_ENUM_CLASS_CONSTANT(m_enum, m_prefix, m_member)                                                  \
	{                                                                                                              \
		StringName enum_name = _gde_constant_get_enum_name(m_enum::m_member, StringName(#m_constant));             \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                             \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);               \
	}

#define BIND_CORE_BITFIELD_CLASS_FLAG(m_enum, m_prefix, m_member)                                                  \
	{                                                                                                              \
		StringName enum_name = _gde_constant_get_bitfield_name(m_enum::m_member, StringName(#m_constant));         \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                             \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);               \
	}

#define BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(m_enum, m_name, m_member)                                 \
	{                                                                                                  \
		StringName enum_name = _gde_constant_get_enum_name(m_enum::m_member, StringName(#m_constant)); \
		_global_constants.push_back(_CoreConstant(enum_name, #m_name, (int64_t)m_enum::m_member));     \
		_global_constants_map[#m_name] = _global_constants.size() - 1;                                 \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);   \
	}

#define BIND_CORE_BITFIELD_CLASS_FLAG_CUSTOM(m_enum, m_name, m_member)                                     \
	{                                                                                                      \
		StringName enum_name = _gde_constant_get_bitfield_name(m_enum::m_member, StringName(#m_constant)); \
		_global_constants.push_back(_CoreConstant(enum_name, #m_name, (int64_t)m_enum::m_member));         \
		_global_constants_map[#m_name] = _global_constants.size() - 1;                                     \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);       \
	}

#define BIND_CORE_ENUM_CLASS_CONSTANT_NO_VAL(m_enum, m_prefix, m_member)                                           \
	{                                                                                                              \
		StringName enum_name = _gde_constant_get_enum_name(m_enum::m_member, StringName(#m_constant));             \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                             \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);               \
	}

#define BIND_CORE_ENUM_CONSTANT_CUSTOM(m_custom_name, m_constant)                                    \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_enum_name(m_constant, StringName(#m_constant));     \
		_global_constants.push_back(_CoreConstant(enum_name, m_custom_name, m_constant));            \
		_global_constants_map[m_custom_name] = _global_constants.size() - 1;                         \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_CONSTANT_NO_VAL(m_constant)                                          \
	_global_constants.push_back(_CoreConstant(StringName(), #m_constant, m_constant)); \
	_global_constants_map[#m_constant] = _global_constants.size() - 1;

#define BIND_CORE_ENUM_CONSTANT_NO_VAL(m_constant)                                                   \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_enum_name(m_constant, StringName(#m_constant));     \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant));              \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_ENUM_CONSTANT_CUSTOM_NO_VAL(m_custom_name, m_constant)                             \
	{                                                                                                \
		StringName enum_name = _gde_constant_get_enum_name(m_constant, StringName(#m_constant));     \
		_global_constants.push_back(_CoreConstant(enum_name, m_custom_name, m_constant));            \
		_global_constants_map[m_custom_name] = _global_constants.size() - 1;                         \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#endif	// DEBUG_ENABLED

void CoreConstants::register_global_constants() {
	BIND_CORE_ENUM_CONSTANT(SIDE_LEFT);
	BIND_CORE_ENUM_CONSTANT(SIDE_TOP);
	BIND_CORE_ENUM_CONSTANT(SIDE_RIGHT);
	BIND_CORE_ENUM_CONSTANT(SIDE_BOTTOM);

	BIND_CORE_ENUM_CONSTANT(CORNER_TOP_LEFT);
	BIND_CORE_ENUM_CONSTANT(CORNER_TOP_RIGHT);
	BIND_CORE_ENUM_CONSTANT(CORNER_BOTTOM_RIGHT);
	BIND_CORE_ENUM_CONSTANT(CORNER_BOTTOM_LEFT);

	BIND_CORE_ENUM_CONSTANT(VERTICAL);
	BIND_CORE_ENUM_CONSTANT(HORIZONTAL);

	BIND_CORE_ENUM_CONSTANT(CLOCKWISE);
	BIND_CORE_ENUM_CONSTANT(COUNTERCLOCKWISE);

	BIND_CORE_ENUM_CONSTANT(HORIZONTAL_ALIGNMENT_LEFT);
	BIND_CORE_ENUM_CONSTANT(HORIZONTAL_ALIGNMENT_CENTER);
	BIND_CORE_ENUM_CONSTANT(HORIZONTAL_ALIGNMENT_RIGHT);
	BIND_CORE_ENUM_CONSTANT(HORIZONTAL_ALIGNMENT_FILL);

	BIND_CORE_ENUM_CONSTANT(VERTICAL_ALIGNMENT_TOP);
	BIND_CORE_ENUM_CONSTANT(VERTICAL_ALIGNMENT_CENTER);
	BIND_CORE_ENUM_CONSTANT(VERTICAL_ALIGNMENT_BOTTOM);
	BIND_CORE_ENUM_CONSTANT(VERTICAL_ALIGNMENT_FILL);

	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TOP_TO);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_CENTER_TO);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_BASELINE_TO);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_BOTTOM_TO);

	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TO_TOP);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TO_CENTER);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TO_BASELINE);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TO_BOTTOM);

	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TOP);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_CENTER);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_BOTTOM);

	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_IMAGE_MASK);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TEXT_MASK);

	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, EULER_ORDER_XYZ);
	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, EULER_ORDER_XZY);
	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, EULER_ORDER_YXZ);
	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, EULER_ORDER_YZX);
	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, EULER_ORDER_ZXY);
	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, EULER_ORDER_ZYX);

	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_NONE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_SPECIAL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_ESCAPE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_TAB);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_BACKTAB);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_BACKSPACE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_ENTER);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_ENTER);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_INSERT);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_DELETE, KEY_DELETE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_PAUSE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_PRINT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_SYSREQ);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_CLEAR);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_HOME);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_END);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_UP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_RIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_DOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_PAGEUP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_PAGEDOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_SHIFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_CTRL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_META);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_ALT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_CAPSLOCK);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_NUMLOCK);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_SCROLLLOCK);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F1);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F2);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F3);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F4);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F5);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F6);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F7);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F8);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F9);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F10);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F11);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F12);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F13);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F14);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F15);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F16);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F17);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F18);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F19);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F20);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F21);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F22);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F23);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F24);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F25);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F26);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F27);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F28);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F29);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F30);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F31);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F32);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F33);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F34);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F35);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_MULTIPLY);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_DIVIDE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_SUBTRACT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_PERIOD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_ADD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_0);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_1);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_2);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_3);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_4);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_5);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_6);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_7);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_8);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KP_9);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_MENU);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_HYPER);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_HELP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_BACK);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_FORWARD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_STOP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_REFRESH);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_VOLUMEDOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_VOLUMEMUTE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_VOLUMEUP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_MEDIAPLAY);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_MEDIASTOP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_MEDIAPREVIOUS);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_MEDIANEXT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_MEDIARECORD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_HOMEPAGE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_FAVORITES);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_SEARCH);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_STANDBY);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_OPENURL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCHMAIL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCHMEDIA);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCH0);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCH1);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCH2);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCH3);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCH4);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCH5);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCH6);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCH7);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCH8);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCH9);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCHA);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCHB);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCHC);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCHD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCHE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LAUNCHF);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_GLOBE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_KEYBOARD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_JIS_EISU);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_JIS_KANA);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_UNKNOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_SPACE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_EXCLAM);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_QUOTEDBL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_NUMBERSIGN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_DOLLAR);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_PERCENT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_AMPERSAND);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_APOSTROPHE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_PARENLEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_PARENRIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_ASTERISK);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_PLUS);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_COMMA);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_MINUS);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_PERIOD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_SLASH);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_0, KEY_0);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_1, KEY_1);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_2, KEY_2);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_3, KEY_3);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_4, KEY_4);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_5, KEY_5);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_6, KEY_6);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_7, KEY_7);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_8, KEY_8);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_9, KEY_9);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_COLON);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_SEMICOLON);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_LESS);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_EQUAL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_GREATER);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_QUESTION);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_AT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_A);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_B);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_C);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_D);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_E);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_F);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_G);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_H);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_I);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_J);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_K);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_L);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_M);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_N);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_O);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_P);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_Q);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_R);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_S);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_T);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_U);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_V);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_W);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_X);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_Y);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_Z);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_BRACKETLEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_BACKSLASH);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_BRACKETRIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_ASCIICIRCUM);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_UNDERSCORE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_QUOTELEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_BRACELEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_BAR);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_BRACERIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_ASCIITILDE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_YEN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEY_SECTION);

	BIND_CORE_BITFIELD_CLASS_FLAG_CUSTOM(KeyModifierMask, KEY_CODE_MASK, KEY_CODE_MASK);
	BIND_CORE_BITFIELD_CLASS_FLAG_CUSTOM(KeyModifierMask, KEY_MODIFIER_MASK, KEY_MODIFIER_MASK);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, KEY_MASK_CMD_OR_CTRL);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, KEY_MASK_SHIFT);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, KEY_MASK_ALT);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, KEY_MASK_META);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, KEY_MASK_CTRL);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, KEY_MASK_KPAD);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, KEY_MASK_GROUP_SWITCH);

	BIND_CORE_ENUM_CLASS_CONSTANT(KeyLocation, KEY_LOCATION, KEY_LOCATION_UNSPECIFIED);
	BIND_CORE_ENUM_CLASS_CONSTANT(KeyLocation, KEY_LOCATION, KEY_LOCATION_LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(KeyLocation, KEY_LOCATION, KEY_LOCATION_RIGHT);

	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, MOUSE_BUTTON_NONE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, MOUSE_BUTTON_LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, MOUSE_BUTTON_RIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, MOUSE_BUTTON_MIDDLE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, MOUSE_BUTTON_WHEEL_UP);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, MOUSE_BUTTON_WHEEL_DOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, MOUSE_BUTTON_WHEEL_LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, MOUSE_BUTTON_WHEEL_RIGHT);

	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(MouseButton, MOUSE_BUTTON_XBUTTON1, MOUSE_BUTTON_XBUTTON1);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(MouseButton, MOUSE_BUTTON_XBUTTON2, MOUSE_BUTTON_XBUTTON2);

	BIND_CORE_BITFIELD_CLASS_FLAG(MouseButtonMask, MOUSE_BUTTON_MASK, MOUSE_BUTTON_MASK_LEFT);
	BIND_CORE_BITFIELD_CLASS_FLAG(MouseButtonMask, MOUSE_BUTTON_MASK, MOUSE_BUTTON_MASK_RIGHT);
	BIND_CORE_BITFIELD_CLASS_FLAG(MouseButtonMask, MOUSE_BUTTON_MASK, MOUSE_BUTTON_MASK_MIDDLE);
	BIND_CORE_BITFIELD_CLASS_FLAG(MouseButtonMask, MOUSE_BUTTON_MASK, MOUSE_BUTTON_MASK_MB_XBUTTON1);
	BIND_CORE_BITFIELD_CLASS_FLAG(MouseButtonMask, MOUSE_BUTTON_MASK, MOUSE_BUTTON_MASK_MB_XBUTTON2);

	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_INVALID);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_A);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_B);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_X);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_Y);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_BACK);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_GUIDE);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_START);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_LEFT_STICK);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_RIGHT_STICK);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_LEFT_SHOULDER);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_RIGHT_SHOULDER);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_DPAD_UP);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_DPAD_DOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_DPAD_LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_DPAD_RIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_MISC1);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_PADDLE1);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_PADDLE2);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_PADDLE3);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_PADDLE4);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_TOUCHPAD);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_SDL_MAX);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, JOY_BUTTON_MAX);

	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, JOY_AXIS_INVALID);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, JOY_AXIS_LEFT_X);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, JOY_AXIS_LEFT_Y);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, JOY_AXIS_RIGHT_X);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, JOY_AXIS_RIGHT_Y);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, JOY_AXIS_TRIGGER_LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, JOY_AXIS_TRIGGER_RIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, JOY_AXIS_SDL_MAX);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, JOY_AXIS_MAX);

	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_NONE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_NOTE_OFF);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_NOTE_ON);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_AFTERTOUCH);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_CONTROL_CHANGE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_PROGRAM_CHANGE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_CHANNEL_PRESSURE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_PITCH_BEND);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_SYSTEM_EXCLUSIVE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_QUARTER_FRAME);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_SONG_POSITION_POINTER);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_SONG_SELECT);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_TUNE_REQUEST);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_TIMING_CLOCK);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_START);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_CONTINUE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_STOP);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_ACTIVE_SENSING);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, MIDI_MESSAGE_SYSTEM_RESET);

	// error list

	BIND_CORE_ENUM_CONSTANT(OK);  // (0)
	BIND_CORE_ENUM_CONSTANT(FAILED);
	BIND_CORE_ENUM_CONSTANT(ERR_UNAVAILABLE);
	BIND_CORE_ENUM_CONSTANT(ERR_UNCONFIGURED);
	BIND_CORE_ENUM_CONSTANT(ERR_UNAUTHORIZED);
	BIND_CORE_ENUM_CONSTANT(ERR_PARAMETER_RANGE_ERROR);	 // (5)
	BIND_CORE_ENUM_CONSTANT(ERR_OUT_OF_MEMORY);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_NOT_FOUND);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_BAD_DRIVE);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_BAD_PATH);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_NO_PERMISSION);  // (10)
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_ALREADY_IN_USE);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_CANT_OPEN);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_CANT_WRITE);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_CANT_READ);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_UNRECOGNIZED);	 // (15)
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_CORRUPT);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_MISSING_DEPENDENCIES);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_EOF);
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_OPEN);
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_CREATE);  // (20)
	BIND_CORE_ENUM_CONSTANT(ERR_QUERY_FAILED);
	BIND_CORE_ENUM_CONSTANT(ERR_ALREADY_IN_USE);
	BIND_CORE_ENUM_CONSTANT(ERR_LOCKED);
	BIND_CORE_ENUM_CONSTANT(ERR_TIMEOUT);
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_CONNECT);	// (25)
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_RESOLVE);
	BIND_CORE_ENUM_CONSTANT(ERR_CONNECTION_ERROR);
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_ACQUIRE_RESOURCE);
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_FORK);
	BIND_CORE_ENUM_CONSTANT(ERR_INVALID_DATA);	// (30)
	BIND_CORE_ENUM_CONSTANT(ERR_INVALID_PARAMETER);
	BIND_CORE_ENUM_CONSTANT(ERR_ALREADY_EXISTS);
	BIND_CORE_ENUM_CONSTANT(ERR_DOES_NOT_EXIST);
	BIND_CORE_ENUM_CONSTANT(ERR_DATABASE_CANT_READ);
	BIND_CORE_ENUM_CONSTANT(ERR_DATABASE_CANT_WRITE);  // (35)
	BIND_CORE_ENUM_CONSTANT(ERR_COMPILATION_FAILED);
	BIND_CORE_ENUM_CONSTANT(ERR_METHOD_NOT_FOUND);
	BIND_CORE_ENUM_CONSTANT(ERR_LINK_FAILED);
	BIND_CORE_ENUM_CONSTANT(ERR_SCRIPT_FAILED);
	BIND_CORE_ENUM_CONSTANT(ERR_CYCLIC_LINK);  // (40)
	BIND_CORE_ENUM_CONSTANT(ERR_INVALID_DECLARATION);
	BIND_CORE_ENUM_CONSTANT(ERR_DUPLICATE_SYMBOL);
	BIND_CORE_ENUM_CONSTANT(ERR_PARSE_ERROR);
	BIND_CORE_ENUM_CONSTANT(ERR_BUSY);
	BIND_CORE_ENUM_CONSTANT(ERR_SKIP);	// (45)
	BIND_CORE_ENUM_CONSTANT(ERR_HELP);
	BIND_CORE_ENUM_CONSTANT(ERR_BUG);
	BIND_CORE_ENUM_CONSTANT(ERR_PRINTER_ON_FIRE);

	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_NONE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_RANGE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_ENUM);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_ENUM_SUGGESTION);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_EXP_EASING);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LINK);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_FLAGS);

	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_2D_RENDER);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_2D_PHYSICS);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_2D_NAVIGATION);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_3D_RENDER);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_3D_PHYSICS);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_3D_NAVIGATION);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_AVOIDANCE);

	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_FILE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_DIR);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_GLOBAL_FILE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_GLOBAL_DIR);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_RESOURCE_TYPE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_MULTILINE_TEXT);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_EXPRESSION);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_PLACEHOLDER_TEXT);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_COLOR_NO_ALPHA);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_OBJECT_ID);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_TYPE_STRING);

	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_OBJECT_TOO_BIG);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_NODE_PATH_VALID_TYPES);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_SAVE_FILE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_GLOBAL_SAVE_FILE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_INT_IS_OBJECTID);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_INT_IS_POINTER);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_ARRAY_TYPE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_DICTIONARY_TYPE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LOCALE_ID);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LOCALIZABLE_STRING);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_NODE_TYPE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_HIDE_QUATERNION_EDIT);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_PASSWORD);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_TOOL_BUTTON);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_ONESHOT);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_GROUP_ENABLE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_INPUT_NAME);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_FILE_PATH);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_MAX);

	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NONE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_STORAGE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_EDITOR);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_INTERNAL);

	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_CHECKABLE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_CHECKED);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_GROUP);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_CATEGORY);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_SUBGROUP);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_CLASS_IS_BITFIELD);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NO_INSTANCE_STATE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_RESTART_IF_CHANGED);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_SCRIPT_VARIABLE);

	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_STORE_IF_NULL);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_SCRIPT_DEFAULT_VALUE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_CLASS_IS_ENUM);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NIL_IS_VARIANT);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_ARRAY);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_ALWAYS_DUPLICATE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NEVER_DUPLICATE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_HIGH_END_GFX);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_KEYING_INCREMENTS);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_DEFERRED_SET_RESOURCE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_EDITOR_BASIC_SETTING);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_READ_ONLY);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_SECRET);

	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_DEFAULT);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NO_EDITOR);

	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_NORMAL);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_EDITOR);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_CONST);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_VIRTUAL);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_VARARG);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_STATIC);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_OBJECT_CORE);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_VIRTUAL_REQUIRED);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAGS_DEFAULT);

	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_NIL", Variant::NIL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_BOOL", Variant::BOOL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_INT", Variant::INT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_FLOAT", Variant::FLOAT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_STRING", Variant::STRING);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR2", Variant::VECTOR2);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR2I", Variant::VECTOR2I);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_RECT2", Variant::RECT2);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_RECT2I", Variant::RECT2I);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR3", Variant::VECTOR3);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR3I", Variant::VECTOR3I);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_TRANSFORM2D", Variant::TRANSFORM2D);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR4", Variant::VECTOR4);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR4I", Variant::VECTOR4I);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PLANE", Variant::PLANE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_QUATERNION", Variant::QUATERNION);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_AABB", Variant::AABB);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_BASIS", Variant::BASIS);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_TRANSFORM3D", Variant::TRANSFORM3D);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PROJECTION", Variant::PROJECTION);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_COLOR", Variant::COLOR);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_STRING_NAME", Variant::STRING_NAME);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_NODE_PATH", Variant::NODE_PATH);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_RID", Variant::RID);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_OBJECT", Variant::OBJECT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_CALLABLE", Variant::CALLABLE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_SIGNAL", Variant::SIGNAL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_DICTIONARY", Variant::DICTIONARY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_ARRAY", Variant::ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_BYTE_ARRAY", Variant::PACKED_BYTE_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_INT32_ARRAY", Variant::PACKED_INT32_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_INT64_ARRAY", Variant::PACKED_INT64_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_FLOAT32_ARRAY", Variant::PACKED_FLOAT32_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_FLOAT64_ARRAY", Variant::PACKED_FLOAT64_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_STRING_ARRAY", Variant::PACKED_STRING_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_VECTOR2_ARRAY", Variant::PACKED_VECTOR2_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_VECTOR3_ARRAY", Variant::PACKED_VECTOR3_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_COLOR_ARRAY", Variant::PACKED_COLOR_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_VECTOR4_ARRAY", Variant::PACKED_VECTOR4_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_MAX", Variant::VARIANT_MAX);

	// comparison
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_EQUAL", Variant::OP_EQUAL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_NOT_EQUAL", Variant::OP_NOT_EQUAL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_LESS", Variant::OP_LESS);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_LESS_EQUAL", Variant::OP_LESS_EQUAL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_GREATER", Variant::OP_GREATER);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_GREATER_EQUAL", Variant::OP_GREATER_EQUAL);
	// mathematic
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_ADD", Variant::OP_ADD);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_SUBTRACT", Variant::OP_SUBTRACT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_MULTIPLY", Variant::OP_MULTIPLY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_DIVIDE", Variant::OP_DIVIDE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_NEGATE", Variant::OP_NEGATE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_POSITIVE", Variant::OP_POSITIVE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_MODULE", Variant::OP_MODULE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_POWER", Variant::OP_POWER);
	// bitwise
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_SHIFT_LEFT", Variant::OP_SHIFT_LEFT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_SHIFT_RIGHT", Variant::OP_SHIFT_RIGHT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_BIT_AND", Variant::OP_BIT_AND);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_BIT_OR", Variant::OP_BIT_OR);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_BIT_XOR", Variant::OP_BIT_XOR);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_BIT_NEGATE", Variant::OP_BIT_NEGATE);
	// logic
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_AND", Variant::OP_AND);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_OR", Variant::OP_OR);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_XOR", Variant::OP_XOR);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_NOT", Variant::OP_NOT);
	// containment
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_IN", Variant::OP_IN);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_MAX", Variant::OP_MAX);
}

void CoreConstants::unregister_global_constants() {
	_global_constants.clear();
	_global_constants_map.clear();
	_global_enums.clear();
}

int CoreConstants::get_global_constant_count() {
	return _global_constants.size();
}

StringName CoreConstants::get_global_constant_enum(int p_idx) {
	return _global_constants[p_idx].enum_name;
}

#ifdef DEBUG_ENABLED
bool CoreConstants::is_global_constant_bitfield(int p_idx) {
	return _global_constants[p_idx].is_bitfield;
}

bool CoreConstants::get_ignore_value_in_docs(int p_idx) {
	return _global_constants[p_idx].ignore_value_in_docs;
}
#else
bool CoreConstants::is_global_constant_bitfield(int p_idx) {
	return false;
}

bool CoreConstants::get_ignore_value_in_docs(int p_idx) {
	return false;
}
#endif	// DEBUG_ENABLED

const char* CoreConstants::get_global_constant_name(int p_idx) {
	return _global_constants[p_idx].name;
}

int64_t CoreConstants::get_global_constant_value(int p_idx) {
	return _global_constants[p_idx].value;
}

bool CoreConstants::is_global_constant(const StringName& p_name) {
	return _global_constants_map.has(p_name);
}

int CoreConstants::get_global_constant_index(const StringName& p_name) {
	ERR_FAIL_COND_V_MSG(!_global_constants_map.has(p_name), -1, "Trying to get index of non-existing constant.");
	return _global_constants_map[p_name];
}

bool CoreConstants::is_global_enum(const StringName& p_enum) {
	return _global_enums.has(p_enum);
}

void CoreConstants::get_enum_values(const StringName& p_enum, HashMap<StringName, int64_t>* p_values) {
	ERR_FAIL_NULL_MSG(p_values, "Trying to get enum values with null map.");
	ERR_FAIL_COND_MSG(!_global_enums.has(p_enum), "Trying to get values of non-existing enum.");
	for (const _CoreConstant& constant : _global_enums[p_enum]) {
		(*p_values)[constant.name] = constant.value;
	}
}

#ifdef TOOLS_ENABLED

void CoreConstants::get_global_enums(List<StringName>* r_values) {
	for (const KeyValue<StringName, Vector<_CoreConstant>>& global_enum : _global_enums) {
		r_values->push_back(global_enum.key);
	}
}

#endif
