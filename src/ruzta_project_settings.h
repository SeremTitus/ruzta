/**************************************************************************/
/*  ruzta_project_settings.h                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                RUZTA                                   */
/*                    https://seremtitus.co.ke/ruzta                      */
/**************************************************************************/
//* Copyright (c) 2025-present Ruzta contributors (see AUTHORS.md).        */
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

#include <godot_cpp/classes/project_settings.hpp> // original:
#include <godot_cpp/variant/variant.hpp> // original:
#include <godot_cpp/core/property_info.hpp> // original:

using namespace godot;

// Use this to mark property names for editor translation.
// Often for dynamic properties defined in _get_property_list().
// Property names defined directly inside EDITOR_DEF, GLOBAL_DEF, and ADD_PROPERTY macros don't need this.
#define PNAME(m_value) (m_value)

Variant _GLOBAL_DEF(const String &p_var, const Variant &p_default, bool p_restart_if_changed = false) {
    Variant ret;
	if (!ProjectSettings::get_singleton()->has_setting(p_var)) {
		ProjectSettings::get_singleton()->set(p_var, p_default);
	}
	ret = ProjectSettings::get_singleton()->get_setting_with_override(p_var);

	ProjectSettings::get_singleton()->set_initial_value(p_var, p_default);
    ProjectSettings::get_singleton()->set_restart_if_changed(p_var, p_restart_if_changed);
	return ret;
}

Variant _GLOBAL_DEF(const PropertyInfo &p_info, const Variant &p_default, bool p_restart_if_changed = false) {
    Variant ret = _GLOBAL_DEF(p_info.name, p_default, p_restart_if_changed);
	ProjectSettings::get_singleton()->add_property_info(p_info);
	return ret;
}

#define GLOBAL_DEF(m_var, m_value) _GLOBAL_DEF(m_var, m_value)
#define GLOBAL_DEF_RST(m_var, m_value) _GLOBAL_DEF(m_var, m_value, true)
#define GLOBAL_GET(m_var) ProjectSettings::get_singleton()->get_setting_with_override(m_var)