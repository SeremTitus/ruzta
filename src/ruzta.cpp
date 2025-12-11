/**************************************************************************/
/*  ruzta.cpp                                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                                RUZTA                                   */
/*                    https://seremtitus.co.ke/ruzta                      */
/**************************************************************************/
//* Copyright (c) 2025-present Ruzta contributors (see AUTHORS.md).       */
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

#include "ruzta.h"

#include "ruzta_analyzer.h"
#include "ruzta_cache.h"
#include "ruzta_compiler.h"
#include "ruzta_parser.h"
#include "ruzta_project_settings.h"
#include "ruzta_rpc_callable.h"
#include "ruzta_tokenizer_buffer.h"
#include "ruzta_warning.h"
#include "ruzta_script_server.h"

#ifdef TOOLS_ENABLED
#include "editor/ruzta_docgen.h"
#endif

#ifdef TESTS_ENABLED
#include "tests/ruzta_test_runner.h"
#endif

#include <godot_cpp/classes/engine.hpp>			   // original: core/config/engine.h
#include <godot_cpp/classes/project_settings.hpp>  // original: core/config/project_settings.h
#include "ruzta_variant/core_constants.h" // original: core/core_constants.h
#include <godot_cpp/classes/file_access.hpp>   // original: core/io/file_access.h
#include <godot_cpp/classes/packed_scene.hpp>  // original: scene/resources/packed_scene.h
// TODO: #include "scene/scene_string_names.h" // original: scene/scene_string_names.h

#ifdef TOOLS_ENABLED
#include <godot_cpp/classes/editor_paths.hpp>  // original: editor/file_system/editor_paths.h
#include <godot_cpp/core/class_db.hpp>		   // original: core/extension/gdextension_manager.h
#endif

#include <godot_cpp/classes/engine_debugger.hpp>   // original:
#include <godot_cpp/classes/global_constants.hpp>  // original:
#include <godot_cpp/core/mutex_lock.hpp>		   // original:

///////////////////////////
bool RuztaNativeClass::_get(const StringName& p_name, Variant& r_ret) const {
	if (ClassDB::class_has_integer_constant(name, p_name)) {
		r_ret = ClassDB::class_get_integer_constant(name, p_name);
		return true;
	}

	MethodBind* method = ClassDB::get_method(name, p_name);
	if (method && method->is_static()) {
		// Native static method.
		r_ret = Callable(const_cast<RuztaNativeClass*>(this), p_name);
		return true;
	}

	return false;
}

Variant RuztaNativeClass::_new() {
	Object* o = instantiate();
	ERR_FAIL_NULL_V_MSG(o, Variant(), "Class type: '" + String(name) + "' is not instantiable.");

	RefCounted* rc = Object::cast_to<RefCounted>(o);
	if (rc) {
		return Ref<RefCounted>(rc);
	} else {
		return o;
	}
}

Object* RuztaNativeClass::instantiate() {
	if (!ClassDB::can_instantiate(name)) {
		return nullptr;
	}
	return ClassDB::instantiate(name);
}

Variant RuztaNativeClass::callp(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) {
	if (p_method == StringName("new")) {
		// Constructor.
		// return Object::callp(p_method, p_args, p_argcount, r_error);
		// Am 60% convinced with this replacement but we find out if it will work.
		Variant result = Variant();
		Variant(this).callp(p_method, p_args, p_argcount, result, r_error);
		return result;
	}
	MethodBind* method = ClassDB::get_method(name, p_method);
	if (method && method->is_static()) {
		// Native static method.
		return method->call(nullptr, reinterpret_cast<GDExtensionConstVariantPtr*>(p_args), p_argcount, r_error);
	}

	r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_INVALID_METHOD;
	return Variant();
}

Ruzta::UpdatableFuncPtr::UpdatableFuncPtr(RuztaFunction* p_function) {
	if (p_function == nullptr) {
		return;
	}

	ptr = p_function;
	script = ptr->get_script();
	ERR_FAIL_NULL(script);

	MutexLock script_lock(script->func_ptrs_to_update_mutex);
	list_element = script->func_ptrs_to_update.push_back(this);
}

Ruzta::UpdatableFuncPtr::~UpdatableFuncPtr() {
	ERR_FAIL_NULL(script);

	if (list_element) {
		MutexLock script_lock(script->func_ptrs_to_update_mutex);
		list_element->erase();
		list_element = nullptr;
	}
}

void Ruzta::_recurse_replace_function_ptrs(const HashMap<RuztaFunction*, RuztaFunction*>& p_replacements) const {
	MutexLock lock(func_ptrs_to_update_mutex);
	for (UpdatableFuncPtr* updatable : func_ptrs_to_update) {
		HashMap<RuztaFunction*, RuztaFunction*>::ConstIterator replacement = p_replacements.find(updatable->ptr);
		if (replacement) {
			updatable->ptr = replacement->value;
		} else {
			// Probably a lambda from another reload, ignore.
			updatable->ptr = nullptr;
		}
	}

	for (HashMap<StringName, Ref<Ruzta>>::ConstIterator subscript = subclasses.begin(); subscript; ++subscript) {
		subscript->value->_recurse_replace_function_ptrs(p_replacements);
	}
}

#ifdef TOOLS_ENABLED

void Ruzta::_save_old_static_data() {
	old_static_variables_indices = static_variables_indices;
	old_static_variables = static_variables;
	for (KeyValue<StringName, Ref<Ruzta>>& inner : subclasses) {
		inner.value->_save_old_static_data();
	}
}

void Ruzta::_restore_old_static_data() {
	for (KeyValue<StringName, MemberInfo>& E : old_static_variables_indices) {
		if (static_variables_indices.has(E.key)) {
			static_variables.write[static_variables_indices[E.key].index] = old_static_variables[E.value.index];
		}
	}
	old_static_variables_indices.clear();
	old_static_variables.clear();
	for (KeyValue<StringName, Ref<Ruzta>>& inner : subclasses) {
		inner.value->_restore_old_static_data();
	}
}

#endif

#ifdef TOOLS_ENABLED

void Ruzta::_update_exports_values(HashMap<StringName, Variant>& values, List<PropertyInfo>& propnames) {
	for (const KeyValue<StringName, Variant>& E : member_default_values_cache) {
		values[E.key] = E.value;
	}

	for (const PropertyInfo& E : members_cache) {
		propnames.push_back(E);
	}

	if (base_cache.is_valid()) {
		base_cache->_update_exports_values(values, propnames);
	}
}

void Ruzta::_add_doc(const RuztaDocData::ClassDoc& p_doc) {
	doc_class_name = p_doc.name;
	if (_owner) {  // Only the top-level class stores doc info.
		_owner->_add_doc(p_doc);
	} else {  // Remove old docs, add new.
		for (int i = 0; i < docs.size(); i++) {
			if (docs[i].name == p_doc.name) {
				docs.remove_at(i);
				break;
			}
		}
		docs.append(p_doc);
	}
}

void Ruzta::_clear_doc() {
	doc_class_name = StringName();
	doc = RuztaDocData::ClassDoc();
	docs.clear();
}

#endif

Error Ruzta::_static_init() {
	if (likely(valid) && static_initializer) {
		GDExtensionCallError call_err;
		static_initializer->call(nullptr, nullptr, 0, call_err);
		if (call_err.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
			return ERR_CANT_CREATE;
		}
	}
	Error err = OK;
	for (KeyValue<StringName, Ref<Ruzta>>& inner : subclasses) {
		err = inner.value->_static_init();
		if (err) {
			break;
		}
	}
	return err;
}

void Ruzta::_static_default_init() {
	for (const KeyValue<StringName, MemberInfo>& E : static_variables_indices) {
		const RuztaDataType& type = E.value.data_type;
		// Only initialize builtin types, which are not expected to be `null`.
		if (type.kind != RuztaDataType::BUILTIN) {
			continue;
		}
		if (type.builtin_type == Variant::ARRAY && type.has_container_element_type(0)) {
			const RuztaDataType element_type = type.get_container_element_type(0);
			Array default_value;
			default_value.set_typed(element_type.builtin_type, element_type.native_type, element_type.script_type);
			static_variables.write[E.value.index] = default_value;
		} else if (type.builtin_type == Variant::DICTIONARY && type.has_container_element_types()) {
			const RuztaDataType key_type = type.get_container_element_type_or_variant(0);
			const RuztaDataType value_type = type.get_container_element_type_or_variant(1);
			Dictionary default_value;
			default_value.set_typed(key_type.builtin_type, key_type.native_type, key_type.script_type, value_type.builtin_type, value_type.native_type, value_type.script_type);
			static_variables.write[E.value.index] = default_value;
		} else {
			Variant default_value;
			GDExtensionCallError err;
			RuztaVariantExtension::construct(type.builtin_type, default_value, nullptr, 0, err);
			static_variables.write[E.value.index] = default_value;
		}
	}
}

RuztaFunction* Ruzta::_super_constructor(Ruzta* p_script) {
	if (likely(p_script->valid) && p_script->initializer) {
		return p_script->initializer;
	} else {
		Ruzta* base_src = p_script->base.ptr();
		if (base_src != nullptr) {
			return _super_constructor(base_src);
		} else {
			return nullptr;
		}
	}
}

void Ruzta::_super_implicit_constructor(Ruzta* p_script, RuztaInstance* p_instance, GDExtensionCallError& r_error) {
	Ruzta* base_src = p_script->base.ptr();
	if (base_src != nullptr) {
		_super_implicit_constructor(base_src, p_instance, r_error);
		if (r_error.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
			return;
		}
	}
	ERR_FAIL_NULL(p_script->implicit_initializer);
	if (likely(p_script->valid)) {
		p_script->implicit_initializer->call(p_instance, nullptr, 0, r_error);
	} else {
		r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_INVALID_METHOD;
	}
}

RuztaInstance* Ruzta::_create_instance(const Variant** p_args, int p_argcount, Object* p_owner, GDExtensionCallError& r_error) {
	/* STEP 1, CREATE */

	RuztaInstance* instance = memnew(RuztaInstance);
	instance->members.resize(member_indices.size());
	instance->script = Ref<Ruzta>(this);
	instance->owner = p_owner;
	instance->owner_id = ObjectID(p_owner->get_instance_id());
#ifdef DEBUG_ENABLED
	// needed for hot reloading
	for (const KeyValue<StringName, MemberInfo>& E : member_indices) {
		instance->member_indices_cache[E.key] = E.value.index;
	}
#endif
	// TODO: find way to store and retrive RuztaInstance given object
	// instance->owner->set_script_instance(instance);

	/* STEP 2, INITIALIZE AND CONSTRUCT */
	{
		MutexLock lock(RuztaLanguage::singleton->mutex);
		instances.insert(instance->owner);
	}

	auto get_call_error_text = [](Object* p_base, const StringName& p_method, const Variant** p_argptrs, int p_argcount, const GDExtensionCallError& ce) -> String {
		String err_text;

		if (ce.error == GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_INVALID_ARGUMENT) {
			int errorarg = ce.argument;
			if (p_argptrs) {
				err_text = "Cannot convert argument " + itos(errorarg + 1) + " from " + Variant::get_type_name(p_argptrs[errorarg]->get_type()) + " to " + Variant::get_type_name(Variant::Type(ce.expected));
			} else {
				err_text = "Cannot convert argument " + itos(errorarg + 1) + " from [missing argptr, type unknown] to " + Variant::get_type_name(Variant::Type(ce.expected));
			}
		} else if (ce.error == GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_TOO_MANY_ARGUMENTS) {
			err_text = "Method expected " + itos(ce.expected) + " argument(s), but called with " + itos(p_argcount);
		} else if (ce.error == GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_TOO_FEW_ARGUMENTS) {
			err_text = "Method expected " + itos(ce.expected) + " argument(s), but called with " + itos(p_argcount);
		} else if (ce.error == GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_INVALID_METHOD) {
			err_text = "Method not found";
		} else if (ce.error == GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_INSTANCE_IS_NULL) {
			err_text = "Instance is null";
		} else if (ce.error == GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_METHOD_NOT_CONST) {
			err_text = "Method not const in const instance";
		} else if (ce.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
			return "Call OK";
		}

		String base_text;
		if (p_base) {
			base_text = p_base->get_class();
			Ref<Resource> script = p_base->get_script();
			String script_path = script->get_path();
			if (script.is_valid() && script_path.begins_with("res://") && script_path.find("::") == -1) {
				base_text += "(" + script_path.get_file() + ")";
			}
			base_text += "::";
		}
		return "'" + base_text + String(p_method) + "': " + err_text;
	};

	_super_implicit_constructor(this, instance, r_error);
	if (r_error.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
		String error_text = get_call_error_text(instance->owner, "@implicit_new", nullptr, 0, r_error);
		instance->script = Ref<Ruzta>();
		// TODO: find way to store and retrive RuztaInstance given object
		// instance->owner->set_script_instance(nullptr);
		{
			MutexLock lock(RuztaLanguage::singleton->mutex);
			instances.erase(p_owner);
		}
		ERR_FAIL_V_MSG(nullptr, "Error constructing a RuztaInstance: " + error_text);
	}

	if (p_argcount < 0) {
		return instance;
	}

	RuztaFunction* applicable_initializer = _super_constructor(this);
	if (applicable_initializer != nullptr) {
		applicable_initializer->call(instance, p_args, p_argcount, r_error);
		if (r_error.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
			String error_text = get_call_error_text(instance->owner, "_init", p_args, p_argcount, r_error);
			instance->script = Ref<Ruzta>();
			// TODO: find way to store and retrive RuztaInstance given object
			// instance->owner->set_script_instance(nullptr);
			{
				MutexLock lock(RuztaLanguage::singleton->mutex);
				instances.erase(p_owner);
			}
			ERR_FAIL_V_MSG(nullptr, "Error constructing a RuztaInstance: " + error_text);
		}
	}
	//@TODO make thread safe
	return instance;
}

String Ruzta::_get_debug_path() const {
	if (is_built_in() && !get_name().is_empty()) {
		return vformat("%s(%s)", get_name(), get_script_path());
	} else {
		return get_script_path();
	}
}

#ifdef TOOLS_ENABLED
void Ruzta::_update_exports_down(bool p_base_exports_changed) {
	bool cyclic_error = false;
	bool changed = _update_exports(&cyclic_error, false, nullptr, p_base_exports_changed);

	if (cyclic_error) {
		return;
	}

	HashSet<ObjectID> copy = inheriters_cache;	// might get modified

	for (const ObjectID& E : copy) {
		Object* id = ObjectDB::get_instance(E);
		Ruzta* s = Object::cast_to<Ruzta>(id);

		if (!s) {
			continue;
		}
		s->_update_exports_down(p_base_exports_changed || changed);
	}
}
#endif

bool Ruzta::_update_exports(bool* r_err, bool p_recursive_call, PlaceHolderScriptInstance* p_instance_to_update, bool p_base_exports_changed) {
#ifdef TOOLS_ENABLED

	static Vector<Ruzta*> base_caches;
	if (!p_recursive_call) {
		base_caches.clear();
	}
	base_caches.append(this);

	bool changed = p_base_exports_changed;

	if (source_changed_cache) {
		source_changed_cache = false;
		changed = true;

		String basedir = path;

		if (basedir.is_empty()) {
			basedir = get_path();
		}

		if (!basedir.is_empty()) {
			basedir = basedir.get_base_dir();
		}

		RuztaParser parser;
		RuztaAnalyzer analyzer(&parser);
		Error err = parser.parse(source, path, false);

		if (err == OK && analyzer.analyze() == OK) {
			const RuztaParser::ClassNode* c = parser.get_tree();

			if (base_cache.is_valid()) {
				base_cache->inheriters_cache.erase(ObjectID(get_instance_id()));
				base_cache = Ref<Ruzta>();
			}

			RuztaParser::DataType base_type = parser.get_tree()->base_type;
			if (base_type.kind == RuztaParser::DataType::CLASS) {
				Ref<Ruzta> bf = RuztaCache::get_full_script(base_type.script_path, err, path);
				if (err == OK) {
					bf = Ref<Ruzta>(bf->find_class(base_type.class_type->fqcn));
					if (bf.is_valid()) {
						base_cache = bf;
						bf->inheriters_cache.insert(ObjectID(get_instance_id()));
					}
				}
			}

			members_cache.clear();
			member_default_values_cache.clear();
			_signals.clear();

			for (int i = 0; i < c->members.size(); i++) {
				const RuztaParser::ClassNode::Member& member = c->members[i];

				switch (member.type) {
					case RuztaParser::ClassNode::Member::VARIABLE: {
						if (!member.variable->exported) {
							continue;
						}

						members_cache.push_back(member.variable->export_info);
						Variant default_value = analyzer.make_variable_default_value(member.variable);
						member_default_values_cache[member.variable->identifier->name] = default_value;
					} break;
					case RuztaParser::ClassNode::Member::SIGNAL: {
						_signals[member.signal->identifier->name] = member.signal->method_info;
					} break;
					case RuztaParser::ClassNode::Member::GROUP: {
						members_cache.push_back(member.annotation->export_info);
					} break;
					default:
						break;	// Nothing.
				}
			}
		} else {
			placeholder_fallback_enabled = true;
			return false;
		}
	} else if (placeholder_fallback_enabled) {
		return false;
	}

	placeholder_fallback_enabled = false;

	if (base_cache.is_valid() && base_cache->_is_valid()) {
		for (int i = 0; i < base_caches.size(); i++) {
			if (base_caches[i] == base_cache.ptr()) {
				if (r_err) {
					*r_err = true;
				}
				valid = false;	// to show error in the editor
				base_cache->valid = false;
				base_cache->inheriters_cache.clear();  // to prevent future stackoverflows
				base_cache.unref();
				base.unref();
				ERR_FAIL_V_MSG(false, "Cyclic inheritance in script class.");
			}
		}
		if (base_cache->_update_exports(r_err, true)) {
			if (r_err && *r_err) {
				return false;
			}
			changed = true;
		}
	}

	if ((changed || p_instance_to_update) && placeholders.size()) {	 // hm :(

		// update placeholders if any
		HashMap<StringName, Variant> values;
		List<PropertyInfo> propnames;
		_update_exports_values(values, propnames);

		if (changed) {
			for (PlaceHolderScriptInstance* E : placeholders) {
				E->update(propnames, values);
			}
		} else {
			p_instance_to_update->update(propnames, values);
		}
	}

	return changed;

#else
	return false;
#endif
}

void Ruzta::_save_orphaned_subclasses(ClearData* p_clear_data) {
	struct ClassRefWithName {
		ObjectID id;
		String fully_qualified_name;
	};
	Vector<ClassRefWithName> weak_subclasses;
	// collect subclasses ObjectID and name
	for (KeyValue<StringName, Ref<Ruzta>>& E : subclasses) {
		E.value->_owner = nullptr;	// bye, you are no longer owned cause I died
		ClassRefWithName subclass;
		subclass.id = ObjectID(E.value->get_instance_id());
		subclass.fully_qualified_name = E.value->fully_qualified_name;
		weak_subclasses.push_back(subclass);
	}

	// clear subclasses to allow unused subclasses to be deleted
	for (KeyValue<StringName, Ref<Ruzta>>& E : subclasses) {
		p_clear_data->scripts.insert(E.value);
	}
	subclasses.clear();
	// subclasses are also held by constants, clear those as well
	for (KeyValue<StringName, Variant>& E : constants) {
		Ruzta* gdscr = _get_ruzta_from_variant(E.value);
		if (gdscr != nullptr) {
			p_clear_data->scripts.insert(gdscr);
		}
	}
	constants.clear();

	// keep orphan subclass only for subclasses that are still in use
	for (int i = 0; i < weak_subclasses.size(); i++) {
		ClassRefWithName subclass = weak_subclasses[i];
		Object* obj = ObjectDB::get_instance(subclass.id);
		if (!obj) {
			continue;
		}
		// subclass is not released
		RuztaLanguage::get_singleton()->add_orphan_subclass(subclass.fully_qualified_name, subclass.id);
	}
}

Ruzta* Ruzta::_get_ruzta_from_variant(const Variant& p_variant) {
	Object* obj = p_variant;
	if (obj == nullptr || ObjectID(obj->get_instance_id()).is_null()) {
		return nullptr;
	}
	return Object::cast_to<Ruzta>(obj);
}

void Ruzta::_collect_function_dependencies(RuztaFunction* p_func, RBSet<Ruzta*>& p_dependencies, const Ruzta* p_except) {
	if (p_func == nullptr) {
		return;
	}
	for (RuztaFunction* lambda : p_func->lambdas) {
		_collect_function_dependencies(lambda, p_dependencies, p_except);
	}
	for (const Variant& V : p_func->constants) {
		Ruzta* scr = _get_ruzta_from_variant(V);
		if (scr != nullptr && scr != p_except) {
			scr->_collect_dependencies(p_dependencies, p_except);
		}
	}
}

void Ruzta::_collect_dependencies(RBSet<Ruzta*>& p_dependencies, const Ruzta* p_except) {
	if (p_dependencies.has(this)) {
		return;
	}
	if (this != p_except) {
		p_dependencies.insert(this);
	}

	for (const KeyValue<StringName, RuztaFunction*>& E : member_functions) {
		_collect_function_dependencies(E.value, p_dependencies, p_except);
	}

	if (implicit_initializer) {
		_collect_function_dependencies(implicit_initializer, p_dependencies, p_except);
	}

	if (implicit_ready) {
		_collect_function_dependencies(implicit_ready, p_dependencies, p_except);
	}

	if (static_initializer) {
		_collect_function_dependencies(static_initializer, p_dependencies, p_except);
	}

	for (KeyValue<StringName, Ref<Ruzta>>& E : subclasses) {
		if (E.value != p_except) {
			E.value->_collect_dependencies(p_dependencies, p_except);
		}
	}

	for (const KeyValue<StringName, Variant>& E : constants) {
		Ruzta* scr = _get_ruzta_from_variant(E.value);
		if (scr != nullptr && scr != p_except) {
			scr->_collect_dependencies(p_dependencies, p_except);
		}
	}
}

bool Ruzta::_get(const StringName& p_name, Variant& r_ret) const {
	if (p_name == RuztaLanguage::get_singleton()->strings._script_source) {
		r_ret = get_source_code();
		return true;
	}

	const Ruzta* top = this;
	while (top) {
		{
			HashMap<StringName, Variant>::ConstIterator E = top->constants.find(p_name);
			if (E) {
				r_ret = E->value;
				return true;
			}
		}

		{
			HashMap<StringName, MemberInfo>::ConstIterator E = top->static_variables_indices.find(p_name);
			if (E) {
				if (likely(top->valid) && !E->value.getter.is_empty()) {
					GDExtensionCallError ce;
					const Variant ret = const_cast<Ruzta*>(this)->callp(E->value.getter, nullptr, 0, ce);
					r_ret = (ce.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK) ? ret : Variant();
					return true;
				}
				r_ret = top->static_variables[E->value.index];
				return true;
			}
		}

		if (likely(top->valid)) {
			HashMap<StringName, RuztaFunction*>::ConstIterator E = top->member_functions.find(p_name);
			if (E && E->value->is_static()) {
				if (top->rpc_config.has(p_name)) {
					r_ret = Callable(memnew(RuztaRPCCallable(const_cast<Ruzta*>(top), E->key)));
				} else {
					r_ret = Callable(const_cast<Ruzta*>(top), E->key);
				}
				return true;
			}
		}

		{
			HashMap<StringName, Ref<Ruzta>>::ConstIterator E = top->subclasses.find(p_name);
			if (E) {
				r_ret = E->value;
				return true;
			}
		}

		top = top->base.ptr();
	}

	return false;
}

bool Ruzta::_set(const StringName& p_name, const Variant& p_value) {
	if (p_name == RuztaLanguage::get_singleton()->strings._script_source) {
		set_source_code(p_value);
		reload(true);
		return true;
	}

	Ruzta* top = this;
	while (top) {
		HashMap<StringName, MemberInfo>::ConstIterator E = top->static_variables_indices.find(p_name);
		if (E) {
			const MemberInfo* member = &E->value;
			Variant value = p_value;
			if (!member->data_type.is_type(value)) {
				const Variant* args = &p_value;
				GDExtensionCallError err;
				RuztaVariantExtension::construct(member->data_type.builtin_type, value, &args, 1, err);
				if (err.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK || !member->data_type.is_type(value)) {
					return false;
				}
			}
			if (likely(top->valid) && !member->setter.is_empty()) {
				const Variant* args = &value;
				GDExtensionCallError err;
				callp(member->setter, &args, 1, err);
				return err.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK;
			} else {
				top->static_variables.write[member->index] = value;
				return true;
			}
		}

		top = top->base.ptr();
	}

	return false;
}

struct _RuztaMemberSort {
	int index = 0;
	StringName name;
	_FORCE_INLINE_ bool operator<(const _RuztaMemberSort& p_member) const { return index < p_member.index; }
};

void Ruzta::_get_property_list(List<PropertyInfo>* p_properties) const {
	p_properties->push_back(PropertyInfo(Variant::STRING, "script/source", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));

	List<const Ruzta*> classes;
	const Ruzta* top = this;
	while (top) {
		classes.push_back(top);
		top = top->base.ptr();
	}

	for (const List<const Ruzta*>::Element* E = classes.back(); E; E = E->prev()) {
		Vector<_RuztaMemberSort> msort;
		for (const KeyValue<StringName, MemberInfo>& F : E->get()->static_variables_indices) {
			_RuztaMemberSort ms;
			ms.index = F.value.index;
			ms.name = F.key;
			msort.push_back(ms);
		}
		msort.sort();

		for (int i = 0; i < msort.size(); i++) {
			p_properties->push_back(E->get()->static_variables_indices[msort[i].name].property_info);
		}
	}
}

Variant Ruzta::callp(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) {
	Ruzta* top = this;
	while (top) {
		if (likely(top->valid)) {
			HashMap<StringName, RuztaFunction*>::Iterator E = top->member_functions.find(p_method);
			if (E) {
				ERR_FAIL_COND_V_MSG(!E->value->is_static(), Variant(), "Can't call non-static function '" + String(p_method) + "' in script.");

				return E->value->call(nullptr, p_args, p_argcount, r_error);
			}
		}
		top = top->base.ptr();
	}

	// none found, regular

	Variant result = Variant();
	Variant(this).callp(p_method, p_args, p_argcount, result, r_error);
	return result;
	// return Script::callp(p_method, p_args, p_argcount, r_error);
}

Variant Ruzta::_new(const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) {
	/* STEP 1, CREATE */

	if (!valid) {
		r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_OK;
	Ref<RefCounted> ref;
	Object* owner = nullptr;

	Ruzta* _baseptr = this;
	while (_baseptr->base.ptr()) {
		_baseptr = _baseptr->base.ptr();
	}

	ERR_FAIL_COND_V(_baseptr->native.is_null(), Variant());
	if (_baseptr->native.ptr()) {
		owner = _baseptr->native->instantiate();
	} else {
		owner = memnew(RefCounted);	 // by default, no base means use reference
	}
	ERR_FAIL_NULL_V_MSG(owner, Variant(), "Can't inherit from a virtual class.");

	RefCounted* r = Object::cast_to<RefCounted>(owner);
	if (r) {
		ref = Ref<RefCounted>(r);
	}

	RuztaInstance* instance = _create_instance(p_args, p_argcount, owner, r_error);
	if (!instance) {
		if (ref.is_null()) {
			memdelete(owner);  // no owner, sorry
		}
		return Variant();
	}

	if (ref.is_valid()) {
		return ref;
	} else {
		return owner;
	}
}

bool Ruzta::_inherits_script(const Ref<Script>& p_script) const {
	Ref<Ruzta> rz = p_script;
	if (rz.is_null()) {
		return false;
	}

	const Ruzta* s = this;

	while (s) {
		if (s == p_script.ptr()) {
			return true;
		}
		s = s->base.ptr();
	}

	return false;
}

StringName Ruzta::_get_instance_base_type() const {
	if (native.is_valid()) {
		return native->get_name();
	}
	if (base.is_valid() && base->_is_valid()) {
		return base->get_instance_base_type();
	}
	return StringName();
}

void* Ruzta::_instance_create(Object* p_this) const {
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "Script is invalid!");

	Ruzta* top = const_cast<Ruzta*>(this);
	while (top->base.ptr()) {
		top = top->base.ptr();
	}

	if (top->native.is_valid()) {
		if (!ClassDB::is_parent_class(p_this->get_class(), top->native->get_name())) {
			if (EngineDebugger::get_singleton()->is_active()) {
				RuztaLanguage::get_singleton()->debug_break_parse(_get_debug_path(), 1, "Script inherits from native type '" + String(top->native->get_name()) + "', so it can't be assigned to an object of type: '" + p_this->get_class() + "'");
			}
			ERR_FAIL_V_MSG(nullptr, "Script inherits from native type '" + String(top->native->get_name()) + "', so it can't be assigned to an object of type '" + p_this->get_class() + "'" + ".");
		}
	}

	GDExtensionCallError unchecked_error;
	return const_cast<Ruzta*>(this)->_create_instance(nullptr, 0, p_this, unchecked_error);
}

void* Ruzta::_placeholder_instance_create(Object* p_this) const {
#ifdef TOOLS_ENABLED
	PlaceHolderScriptInstance* si = memnew(PlaceHolderScriptInstance(RuztaLanguage::get_singleton(), Ref<Script>(this), p_this));
	// placeholders.insert(si);
	// _update_exports(nullptr, false, si);
	return si;
#else
	return nullptr;
#endif
}

bool Ruzta::_instance_has(Object* p_this) const {
	MutexLock lock(RuztaLanguage::singleton->mutex);

	return instances.has((Object*)p_this);
}

void Ruzta::_set_source_code(const String& p_code) {
	if (source == p_code) {
		return;
	}
	source = p_code;
#ifdef TOOLS_ENABLED
	source_changed_cache = true;
#endif
}

Error Ruzta::_reload(bool p_keep_state) {
	if (reloading) {
		return OK;
	}
	reloading = true;

	bool has_instances;
	{
		MutexLock lock(RuztaLanguage::singleton->mutex);

		has_instances = instances.size();
	}

	// Check condition but reset flag before early return
	if (!p_keep_state && has_instances) {
		reloading = false;	// Reset flag before returning

		ERR_FAIL_V_MSG(ERR_ALREADY_IN_USE, "Cannot reload script while instances exist.");
	}

	String basedir = path;

	if (basedir.is_empty()) {
		basedir = get_path();
	}

	if (!basedir.is_empty()) {
		basedir = basedir.get_base_dir();
	}

	// Loading a template, don't parse.
	// #ifdef TOOLS_ENABLED
	// 	if (EditorPaths::get_singleton() && basedir.begins_with(EditorPaths::get_singleton()->get_project_script_templates_dir())) {
	// 		reloading = false;
	// 		return OK;
	// 	}
	// #endif

	{
		String source_path = path;
		if (source_path.is_empty()) {
			source_path = get_path();
		}
		if (!source_path.is_empty()) {
			if (RuztaCache::get_cached_script(source_path).is_null()) {
				MutexLock lock(RuztaCache::singleton->mutex);
				RuztaCache::singleton->shallow_ruzta_cache[source_path] = Ref<Ruzta>(this);
			}
			if (RuztaCache::has_parser(source_path)) {
				Error err = OK;
				Ref<RuztaParserRef> parser_ref = RuztaCache::get_parser(source_path, RuztaParserRef::EMPTY, err);
				if (parser_ref.is_valid()) {
					uint32_t source_hash;
					if (!binary_tokens.is_empty()) {
						source_hash = hash_djb2_buffer(binary_tokens.ptr(), binary_tokens.size());
					} else {
						source_hash = source.hash();
					}
					if (parser_ref->get_source_hash() != source_hash) {
						RuztaCache::remove_parser(source_path);
					}
				}
			}
		}
	}

	bool can_run = RuztaScriptServer::is_scripting_enabled() || is_tool();

#ifdef TOOLS_ENABLED
	if (p_keep_state && can_run && _is_valid()) {
		_save_old_static_data();
	}
#endif

	valid = false;
	RuztaParser parser;
	Error err;
	if (!binary_tokens.is_empty()) {
		err = parser.parse_binary(binary_tokens, path);
	} else {
		err = parser.parse(source, path, false);
	}
	if (err) {
		if (EngineDebugger::get_singleton()->is_active()) {
			RuztaLanguage::get_singleton()->debug_break_parse(_get_debug_path(), parser.get_errors().front()->get().line, "Parser Error: " + parser.get_errors().front()->get().message);
		}
		// TODO: Show all error messages.
		_err_print_error("Ruzta::reload", path.is_empty() ? "built-in" : (const char*)path.utf8().get_data(), parser.get_errors().front()->get().line, ("Parse Error: " + parser.get_errors().front()->get().message).utf8().get_data(), false, true);
		reloading = false;
		return ERR_PARSE_ERROR;
	}

	RuztaAnalyzer analyzer(&parser);
	err = analyzer.analyze();

	if (err) {
		if (EngineDebugger::get_singleton()->is_active()) {
			RuztaLanguage::get_singleton()->debug_break_parse(_get_debug_path(), parser.get_errors().front()->get().line, "Parser Error: " + parser.get_errors().front()->get().message);
		}

		const List<RuztaParser::ParserError>::Element* e = parser.get_errors().front();
		while (e != nullptr) {
			_err_print_error("Ruzta::reload", path.is_empty() ? "built-in" : (const char*)path.utf8().get_data(), e->get().line, ("Parse Error: " + e->get().message).utf8().get_data(), false, true);
			e = e->next();
		}
		reloading = false;
		return ERR_PARSE_ERROR;
	}

	can_run = RuztaScriptServer::is_scripting_enabled() || parser.is_tool();

	RuztaCompiler compiler;
	err = compiler.compile(&parser, this, p_keep_state);

	if (err) {
		// TODO: Provide the script function as the first argument.
		_err_print_error("Ruzta::reload", path.is_empty() ? "built-in" : (const char*)path.utf8().get_data(), compiler.get_error_line(), ("Compile Error: " + compiler.get_error()).utf8().get_data(), false, true);
		if (can_run) {
			if (EngineDebugger::get_singleton()->is_active()) {
				RuztaLanguage::get_singleton()->debug_break_parse(_get_debug_path(), compiler.get_error_line(), "Parser Error: " + compiler.get_error());
			}
			reloading = false;
			return ERR_COMPILATION_FAILED;
		} else {
			reloading = false;
			return err;
		}
	}

#ifdef TOOLS_ENABLED
	// Done after compilation because it needs the Ruzta object's inner class Ruzta objects,
	// which are made by calling make_scripts() within compiler.compile() above.
	RuztaDocGen::generate_docs(this, parser.get_tree());
#endif

#ifdef DEBUG_ENABLED
	for (const RuztaWarning& warning : parser.get_warnings()) {
		if (EngineDebugger::get_singleton()->is_active()) {
			Vector<RuztaLanguage::StackInfo> si;
			// TODO: Provide the script function as the first argument.
			// EngineDebugger::get_singleton()->get_script_debugger()->send_error("Ruzta::reload", get_script_path(), warning.start_line, warning.get_name(), warning.get_message(), false, ERR_HANDLER_WARNING, si);
		}
	}
#endif

	if (can_run) {
		err = _static_init();
		if (err) {
			return err;
		}
	}

#ifdef TOOLS_ENABLED
	if (can_run && p_keep_state) {
		_restore_old_static_data();
	}

	if (p_keep_state) {
		// Update the properties in the inspector.
		_update_exports();
	}
#endif

	reloading = false;
	return OK;
}

#ifdef TOOLS_ENABLED

godot::TypedArray<Dictionary> Ruzta::_get_documentation() const {
	godot::TypedArray<Dictionary> r_docs;
	for (const RuztaDocData::ClassDoc& doc : docs) {
		r_docs.append(RuztaDocData::ClassDoc::to_dict(doc));
	}
	return r_docs;
}

#endif

bool Ruzta::_has_static_method(const StringName& p_method) const {
	return member_functions.has(p_method) && member_functions[p_method]->is_static();
}

Variant Ruzta::_get_script_method_argument_count(const StringName& p_method) const {
	HashMap<StringName, RuztaFunction*>::ConstIterator E = member_functions.find(p_method);
	if (!E) {
		return 0;
	}
	return E->value->get_argument_count();
}

Dictionary Ruzta::_get_method_info(const StringName& p_method) const {
	HashMap<StringName, RuztaFunction*>::ConstIterator E = member_functions.find(p_method);
	if (!E) {
		return {};
	}
	return E->value->get_method_info().operator Dictionary();
}

ScriptLanguage* Ruzta::_get_language() const { 
	return RuztaLanguage::get_singleton();
}

bool Ruzta::_has_script_signal(const StringName& p_signal) const {
	if (_signals.has(p_signal)) {
		return true;
	}
	if (base.is_valid()) {
		return base->has_script_signal(p_signal);
	}
#ifdef TOOLS_ENABLED
	else if (base_cache.is_valid()) {
		return base_cache->has_script_signal(p_signal);
	}
#endif
	return false;
}

godot::TypedArray<Dictionary> Ruzta::_get_script_signal_list() const {
	godot::TypedArray<Dictionary> result;
	for (const KeyValue<StringName, MethodInfo>& E : _signals) {
		result.append(E.value.operator Dictionary());
	}
	return result;
}

bool Ruzta::_has_property_default_value(const StringName& p_property) const {
#ifdef TOOLS_ENABLED

	HashMap<StringName, Variant>::ConstIterator E = member_default_values_cache.find(p_property);
	if (E) {
		return true;
	}

	if (base_cache.is_valid()) {
		return base_cache->_has_property_default_value(p_property);
	}
#endif
	return false;
}

Variant Ruzta::_get_property_default_value(const StringName& p_property) const {
#ifdef TOOLS_ENABLED

	HashMap<StringName, Variant>::ConstIterator E = member_default_values_cache.find(p_property);
	if (E) {
		return E->value;
	}

	if (base_cache.is_valid()) {
		return base_cache->_get_property_default_value(p_property);
	}
#endif
	return Variant();
}

void Ruzta::_update_exports() {
#ifdef TOOLS_ENABLED
	_update_exports_down(false);
#endif
}

godot::TypedArray<Dictionary> Ruzta::_get_script_method_list() const {
	godot::TypedArray<Dictionary> r_list;
	for (const KeyValue<StringName, RuztaFunction*>& E : member_functions) {
		r_list.push_back(E.value->get_method_info().operator Dictionary());
	}
	return r_list;
}

godot::TypedArray<Dictionary> Ruzta::_get_script_property_list() const {
	godot::TypedArray<Dictionary> r_list;
	List<PropertyInfo> props;
	Vector<_RuztaMemberSort> msort;
	for (const KeyValue<StringName, MemberInfo>& E : member_indices) {
		if (!members.has(E.key)) {
			continue;  // Skip base class members.
		}
		_RuztaMemberSort ms;
		ms.index = E.value.index;
		ms.name = E.key;
		msort.push_back(ms);
	}

	msort.sort();
	msort.reverse();
	for (int i = 0; i < msort.size(); i++) {
		props.push_front(member_indices[msort[i].name].property_info);
	}

	for (const PropertyInfo& E : props) {
		r_list.push_back(E.operator Dictionary());
	}
	return r_list;
}

Dictionary Ruzta::_get_constants() const {
	Dictionary r_constants;
	for (const KeyValue<StringName, Variant>& E : constants) {
		r_constants[E.key] = E.value;
	}
	return r_constants;
}

godot::TypedArray<StringName> Ruzta::_get_members() const {
	godot::TypedArray<StringName> r_members;
	for (const StringName& E : members) {
		r_members.append(E);
	}
	return r_members;
}

#ifdef DEBUG_ENABLED
String Ruzta::debug_get_script_name(const Ref<Script>& p_script) {
	if (p_script.is_valid()) {
		Ref<Ruzta> ruzta = p_script;
		if (ruzta.is_valid()) {
			if (ruzta->get_local_name() != StringName()) {
				return ruzta->get_local_name();
			}
			return ruzta->get_fully_qualified_name().get_file();
		}

		if (p_script->get_global_name() != StringName()) {
			return p_script->get_global_name();
		} else if (!p_script->get_path().is_empty()) {
			return p_script->get_path().get_file();
		} else if (!p_script->get_name().is_empty()) {
			return p_script->get_name();  // Resource name.
		}
	}

	return "<unknown script>";
}
#endif

String Ruzta::canonicalize_path(const String& p_path) {
	if (p_path.get_extension() == "rzc") {
		return p_path.get_basename() + ".rz";
	}
	return p_path;
}

void Ruzta::clear(ClearData* p_clear_data) {
	if (clearing) {
		return;
	}
	clearing = true;

	ClearData data;
	ClearData* clear_data = p_clear_data;
	bool is_root = false;

	// If `clear_data` is `nullptr`, it means that it's the root.
	// The root is in charge to clear functions and scripts of itself and its dependencies
	if (clear_data == nullptr) {
		clear_data = &data;
		is_root = true;
	}

	{
		MutexLock lock(func_ptrs_to_update_mutex);
		for (UpdatableFuncPtr* updatable : func_ptrs_to_update) {
			updatable->ptr = nullptr;
		}
	}

	// If we're in the process of shutting things down then every single script will be cleared
	// anyway, so we can safely skip this very costly operation.
	if (!RuztaLanguage::singleton->finishing) {
		RBSet<Ruzta*> must_clear_dependencies = get_must_clear_dependencies();
		for (Ruzta* E : must_clear_dependencies) {
			clear_data->scripts.insert(E);
			E->clear(clear_data);
		}
	}

	for (const KeyValue<StringName, RuztaFunction*>& E : member_functions) {
		clear_data->functions.insert(E.value);
	}
	member_functions.clear();

	for (KeyValue<StringName, MemberInfo>& E : member_indices) {
		clear_data->scripts.insert(E.value.data_type.script_type_ref);
		E.value.data_type.script_type_ref = Ref<Script>();
	}

	for (KeyValue<StringName, MemberInfo>& E : static_variables_indices) {
		clear_data->scripts.insert(E.value.data_type.script_type_ref);
		E.value.data_type.script_type_ref = Ref<Script>();
	}
	static_variables.clear();
	static_variables_indices.clear();

	if (implicit_initializer) {
		clear_data->functions.insert(implicit_initializer);
		implicit_initializer = nullptr;
	}

	if (implicit_ready) {
		clear_data->functions.insert(implicit_ready);
		implicit_ready = nullptr;
	}

	if (static_initializer) {
		clear_data->functions.insert(static_initializer);
		static_initializer = nullptr;
	}

	_save_orphaned_subclasses(clear_data);

#ifdef TOOLS_ENABLED
	// Clearing inner class doc, script doc only cleared when the script source deleted.
	if (_owner) {
		_clear_doc();
	}
#endif

	// If it's not the root, skip clearing the data
	if (is_root) {
		// All dependencies have been accounted for
		for (RuztaFunction* E : clear_data->functions) {
			memdelete(E);
		}
		for (Ref<Script>& E : clear_data->scripts) {
			Ref<Ruzta> gdscr = E;
			if (gdscr.is_valid()) {
				RuztaCache::remove_script(gdscr->get_path());
			}
		}
		clear_data->clear();
	}
}

void Ruzta::cancel_pending_functions(bool warn) {
	MutexLock lock(RuztaLanguage::get_singleton()->mutex);

	while (SelfList<RuztaFunctionState>* E = pending_func_states.first()) {
		// Order matters since clearing the stack may already cause
		// the RuztaFunctionState to be destroyed and thus removed from the list.
		pending_func_states.remove(E);
		RuztaFunctionState* state = E->self();
#ifdef DEBUG_ENABLED
		if (warn) {
			WARN_PRINT("Canceling suspended execution of \"" + state->get_readable_function() + "\" due to a script reload.");
		}
#endif
		ObjectID state_id = ObjectID(state->get_instance_id());
		state->_clear_connections();
		if (ObjectDB::get_instance(state_id)) {
			state->_clear_stack();
		}
	}
}

Ruzta* Ruzta::find_class(const String& p_qualified_name) {
	String first = p_qualified_name.get_slice("::", 0);

	PackedStringArray class_names;
	Ruzta* result = nullptr;
	// Empty initial name means start here.
	if (first.is_empty() || first == global_name) {
		class_names = p_qualified_name.split("::");
		result = this;
	} else if (p_qualified_name.begins_with(get_root_script()->path)) {
		// Script path could have a class path separator("::") in it.
		class_names = p_qualified_name.trim_prefix(get_root_script()->path).split("::");
		result = get_root_script();
	} else if (HashMap<StringName, Ref<Ruzta>>::Iterator E = subclasses.find(first)) {
		class_names = p_qualified_name.split("::");
		result = E->value.ptr();
	} else if (_owner != nullptr) {
		// Check parent scope.
		return _owner->find_class(p_qualified_name);
	}

	// Starts at index 1 because index 0 was handled above.
	for (int i = 1; result != nullptr && i < class_names.size(); i++) {
		if (HashMap<StringName, Ref<Ruzta>>::Iterator E = result->subclasses.find(class_names[i])) {
			result = E->value.ptr();
		} else {
			// Couldn't find inner class.
			return nullptr;
		}
	}

	return result;
}

bool Ruzta::has_class(const Ruzta* p_script) {
	String fqn = p_script->fully_qualified_name;
	if (fully_qualified_name.is_empty() && fqn.get_slice("::", 0).is_empty()) {
		return p_script == this;
	} else if (fqn.begins_with(fully_qualified_name)) {
		return p_script == find_class(fqn.trim_prefix(fully_qualified_name));
	}
	return false;
}

Ruzta* Ruzta::get_root_script() {
	Ruzta* result = this;
	while (result->_owner) {
		result = result->_owner;
	}
	return result;
}

RBSet<Ruzta*> Ruzta::get_dependencies() {
	RBSet<Ruzta*> dependencies;

	_collect_dependencies(dependencies, this);
	dependencies.erase(this);

	return dependencies;
}

HashMap<Ruzta*, RBSet<Ruzta*>> Ruzta::get_all_dependencies() {
	HashMap<Ruzta*, RBSet<Ruzta*>> all_dependencies;

	List<Ruzta*> scripts;
	{
		MutexLock lock(RuztaLanguage::singleton->mutex);

		SelfList<Ruzta>* elem = RuztaLanguage::singleton->script_list.first();
		while (elem) {
			scripts.push_back(elem->self());
			elem = elem->next();
		}
	}

	for (Ruzta* scr : scripts) {
		if (scr == nullptr || scr->destructing) {
			continue;
		}
		all_dependencies.insert(scr, scr->get_dependencies());
	}

	return all_dependencies;
}

RBSet<Ruzta*> Ruzta::get_must_clear_dependencies() {
	RBSet<Ruzta*> dependencies = get_dependencies();
	RBSet<Ruzta*> must_clear_dependencies;
	HashMap<Ruzta*, RBSet<Ruzta*>> all_dependencies = get_all_dependencies();

	RBSet<Ruzta*> cant_clear;
	for (KeyValue<Ruzta*, RBSet<Ruzta*>>& E : all_dependencies) {
		if (dependencies.has(E.key)) {
			continue;
		}
		for (Ruzta* F : E.value) {
			if (dependencies.has(F)) {
				cant_clear.insert(F);
			}
		}
	}

	for (Ruzta* E : dependencies) {
		if (cant_clear.has(E) || RuztaScriptServer::is_global_class(E->get_fully_qualified_name())) {
			continue;
		}
		must_clear_dependencies.insert(E);
	}

	cant_clear.clear();
	dependencies.clear();
	all_dependencies.clear();
	return must_clear_dependencies;
}

StringName Ruzta::debug_get_member_by_index(int p_idx) const {
	for (const KeyValue<StringName, MemberInfo>& E : member_indices) {
		if (E.value.index == p_idx) {
			return E.key;
		}
	}

	return "<error>";
}

StringName Ruzta::debug_get_static_var_by_index(int p_idx) const {
	for (const KeyValue<StringName, MemberInfo>& E : static_variables_indices) {
		if (E.value.index == p_idx) {
			return E.key;
		}
	}

	return "<error>";
}

void Ruzta::set_path(const String& p_path, bool p_take_over) {
	if (is_root_script()) {
		if (p_take_over) {
			Script::take_over_path(p_path);
		} else {
			Script::set_path(p_path);
		}
	}

	String old_path = path;
	path = p_path;
	path_valid = true;
	RuztaCache::move_script(old_path, p_path);

	for (KeyValue<StringName, Ref<Ruzta>>& kv : subclasses) {
		if (p_take_over) {
			kv.value->take_over_path(p_path);
		} else {
			kv.value->set_path(p_path);
		}
	}
}

String Ruzta::get_script_path() const {
	if (!path_valid && !get_path().is_empty()) {
		return get_path();
	}
	return path;
}

Error Ruzta::load_source_code(const String& p_path) {
	if (p_path.is_empty() || p_path.begins_with("ruzta://") || ResourceLoader::get_singleton()->get_recognized_extensions_for_type("PackedScene").has(p_path.get_extension())) {
		return OK;
	}

	Vector<uint8_t> sourcef;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	Error err = FileAccess::get_open_error();
	if (err) {
		String err_name;
		if (err < 0 || err >= ERR_PRINTER_ON_FIRE) {
			err_name = "(invalid error code)";
		} else {
			err_name = UtilityFunctions::error_string(err);
		}
		ERR_FAIL_COND_V_MSG(err, err, "Attempt to open script '" + p_path + "' resulted in error '" + err_name + "'.");
	}

	uint64_t len = f->get_length();
	sourcef.resize(len + 1);
	uint8_t* w = sourcef.ptrw();
	uint64_t r = f->get_buffer(w, len);
	ERR_FAIL_COND_V(r != len, ERR_CANT_OPEN);
	w[len] = 0;

	String s;
	if (s.utf8((const char*)w, len) != OK) {
		ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Script '" + p_path + "' contains invalid unicode (UTF-8), so it was not loaded. Please ensure that scripts are saved in valid UTF-8 unicode.");
	}

	source = s;
	path = p_path;
	path_valid = true;
#ifdef TOOLS_ENABLED
	source_changed_cache = true;
	// set_edited(false);
	// set_last_modified_time(FileAccess::get_modified_time(path));
#endif	// TOOLS_ENABLED
	return OK;
}

Vector<uint8_t> Ruzta::get_as_binary_tokens() const {
	RuztaTokenizerBuffer tokenizer;
	return tokenizer.parse_code_string(source, RuztaTokenizerBuffer::COMPRESS_NONE);
}

Ruzta::Ruzta() : script_list(this) {
	{
		MutexLock lock(RuztaLanguage::get_singleton()->mutex);

		RuztaLanguage::get_singleton()->script_list.add(&script_list);
	}

	path = vformat("ruzta://%d.rz", ObjectID(get_instance_id()));
}

Ruzta::~Ruzta() {
	if (destructing) {
		return;
	}
	destructing = true;

	if (is_print_verbose_enabled()) {
		MutexLock lock(func_ptrs_to_update_mutex);
		if (!func_ptrs_to_update.is_empty()) {
			print_line(vformat("Ruzta: %d orphaned lambdas becoming invalid at destruction of script '%s'.", func_ptrs_to_update.size(), fully_qualified_name));
		}
	}

	clear();

	cancel_pending_functions(false);

	{
		MutexLock lock(RuztaLanguage::get_singleton()->mutex);

		script_list.remove_from_list();
	}
}

//////////////////////////////
//         INSTANCE         //
//////////////////////////////

bool RuztaInstance::set(const StringName& p_name, const Variant& p_value) {
	{
		HashMap<StringName, Ruzta::MemberInfo>::Iterator E = script->member_indices.find(p_name);
		if (E) {
			const Ruzta::MemberInfo* member = &E->value;
			Variant value = p_value;
			if (!member->data_type.is_type(value)) {
				const Variant* args = &p_value;
				GDExtensionCallError err;
				RuztaVariantExtension::construct(member->data_type.builtin_type, value, &args, 1, err);
				if (err.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK || !member->data_type.is_type(value)) {
					return false;
				}
			}
			if (likely(script->valid) && !member->setter.is_empty()) {
				const Variant* args = &value;
				GDExtensionCallError err;
				callp(member->setter, &args, 1, err);
				return err.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK;
			} else {
				members.write[member->index] = value;
				return true;
			}
		}
	}

	Ruzta* sptr = script.ptr();
	while (sptr) {
		{
			HashMap<StringName, Ruzta::MemberInfo>::ConstIterator E = sptr->static_variables_indices.find(p_name);
			if (E) {
				const Ruzta::MemberInfo* member = &E->value;
				Variant value = p_value;
				if (!member->data_type.is_type(value)) {
					const Variant* args = &p_value;
					GDExtensionCallError err;
					RuztaVariantExtension::construct(member->data_type.builtin_type, value, &args, 1, err);
					if (err.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK || !member->data_type.is_type(value)) {
						return false;
					}
				}
				if (likely(sptr->valid) && !member->setter.is_empty()) {
					const Variant* args = &value;
					GDExtensionCallError err;
					callp(member->setter, &args, 1, err);
					return err.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK;
				} else {
					sptr->static_variables.write[member->index] = value;
					return true;
				}
			}
		}

		if (likely(sptr->valid)) {
			HashMap<StringName, RuztaFunction*>::Iterator E = sptr->member_functions.find(RuztaLanguage::get_singleton()->strings._set);
			if (E) {
				Variant name = p_name;
				const Variant* args[2] = {&name, &p_value};

				GDExtensionCallError err;
				Variant ret = E->value->call(this, (const Variant**)args, 2, err);
				if (err.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK && ret.get_type() == Variant::BOOL && ret.operator bool()) {
					return true;
				}
			}
		}

		sptr = sptr->base.ptr();
	}

	return false;
}

bool RuztaInstance::get(const StringName& p_name, Variant& r_ret) const {
	{
		HashMap<StringName, Ruzta::MemberInfo>::ConstIterator E = script->member_indices.find(p_name);
		if (E) {
			if (likely(script->valid) && !E->value.getter.is_empty()) {
				GDExtensionCallError err;
				const Variant ret = const_cast<RuztaInstance*>(this)->callp(E->value.getter, nullptr, 0, err);
				r_ret = (err.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK) ? ret : Variant();
				return true;
			}
			r_ret = members[E->value.index];
			return true;
		}
	}

	const Ruzta* sptr = script.ptr();
	while (sptr) {
		{
			HashMap<StringName, Variant>::ConstIterator E = sptr->constants.find(p_name);
			if (E) {
				r_ret = E->value;
				return true;
			}
		}

		{
			HashMap<StringName, Ruzta::MemberInfo>::ConstIterator E = sptr->static_variables_indices.find(p_name);
			if (E) {
				if (likely(sptr->valid) && !E->value.getter.is_empty()) {
					GDExtensionCallError ce;
					const Variant ret = const_cast<Ruzta*>(sptr)->callp(E->value.getter, nullptr, 0, ce);
					r_ret = (ce.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK) ? ret : Variant();
					return true;
				}
				r_ret = sptr->static_variables[E->value.index];
				return true;
			}
		}

		{
			HashMap<StringName, MethodInfo>::ConstIterator E = sptr->_signals.find(p_name);
			if (E) {
				r_ret = Signal(owner, E->key);
				return true;
			}
		}

		if (likely(sptr->valid)) {
			HashMap<StringName, RuztaFunction*>::ConstIterator E = sptr->member_functions.find(p_name);
			if (E) {
				if (sptr->rpc_config.has(p_name)) {
					r_ret = Callable(memnew(RuztaRPCCallable(owner, E->key)));
				} else {
					r_ret = Callable(owner, E->key);
				}
				return true;
			}
		}

		{
			HashMap<StringName, Ref<Ruzta>>::ConstIterator E = sptr->subclasses.find(p_name);
			if (E) {
				r_ret = E->value;
				return true;
			}
		}

		if (likely(sptr->valid)) {
			HashMap<StringName, RuztaFunction*>::ConstIterator E = sptr->member_functions.find(RuztaLanguage::get_singleton()->strings._get);
			if (E) {
				Variant name = p_name;
				const Variant* args[1] = {&name};

				GDExtensionCallError err;
				Variant ret = E->value->call(const_cast<RuztaInstance*>(this), (const Variant**)args, 1, err);
				if (err.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK && ret.get_type() != Variant::NIL) {
					r_ret = ret;
					return true;
				}
			}
		}
		sptr = sptr->base.ptr();
	}

	return false;
}

Variant::Type RuztaInstance::get_property_type(const StringName& p_name, bool* r_is_valid) const {
	if (script->member_indices.has(p_name)) {
		if (r_is_valid) {
			*r_is_valid = true;
		}
		return script->member_indices[p_name].property_info.type;
	}

	if (r_is_valid) {
		*r_is_valid = false;
	}
	return Variant::NIL;
}

void RuztaInstance::validate_property(PropertyInfo& p_property) const {
	const Ruzta* sptr = script.ptr();
	while (sptr) {
		if (likely(sptr->valid)) {
			HashMap<StringName, RuztaFunction*>::ConstIterator E = sptr->member_functions.find(RuztaLanguage::get_singleton()->strings._validate_property);
			if (E) {
				Variant property = (Dictionary)p_property;
				const Variant* args[1] = {&property};

				GDExtensionCallError err;
				Variant ret = E->value->call(const_cast<RuztaInstance*>(this), args, 1, err);
				if (err.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
					p_property = PropertyInfo::from_dict(property);
					return;
				}
			}
		}
		sptr = sptr->base.ptr();
	}
}

void RuztaInstance::get_property_list(List<PropertyInfo>* p_properties) const {
	// exported members, not done yet!

	const Ruzta* sptr = script.ptr();
	List<PropertyInfo> props;

	while (sptr) {
		if (likely(sptr->valid)) {
			HashMap<StringName, RuztaFunction*>::ConstIterator E = sptr->member_functions.find(RuztaLanguage::get_singleton()->strings._get_property_list);
			if (E) {
				GDExtensionCallError err;
				Variant ret = E->value->call(const_cast<RuztaInstance*>(this), nullptr, 0, err);
				if (err.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
					ERR_FAIL_COND_MSG(ret.get_type() != Variant::ARRAY, "Wrong type for _get_property_list, must be an array of dictionaries.");

					Array arr = ret;
					for (int i = 0; i < arr.size(); i++) {
						Dictionary d = arr[i];
						ERR_CONTINUE(!d.has("name"));
						ERR_CONTINUE(!d.has("type"));

						PropertyInfo pinfo;
						pinfo.name = d["name"];
						pinfo.type = Variant::Type(d["type"].operator int());
						if (d.has("hint")) {
							pinfo.hint = PropertyHint(d["hint"].operator int());
						}
						if (d.has("hint_string")) {
							pinfo.hint_string = d["hint_string"];
						}
						if (d.has("usage")) {
							pinfo.usage = d["usage"];
						}
						if (d.has("class_name")) {
							pinfo.class_name = d["class_name"];
						}

						ERR_CONTINUE(pinfo.name.is_empty() && (pinfo.usage & PROPERTY_USAGE_STORAGE));
						ERR_CONTINUE(pinfo.type < 0 || pinfo.type >= Variant::VARIANT_MAX);

						props.push_back(pinfo);
					}
				}
			}
		}

		// instance a fake script for editing the values

		Vector<_RuztaMemberSort> msort;
		for (const KeyValue<StringName, Ruzta::MemberInfo>& F : sptr->member_indices) {
			if (!sptr->members.has(F.key)) {
				continue;  // Skip base class members.
			}
			_RuztaMemberSort ms;
			ms.index = F.value.index;
			ms.name = F.key;
			msort.push_back(ms);
		}

		msort.sort();
		msort.reverse();
		for (int i = 0; i < msort.size(); i++) {
			props.push_front(sptr->member_indices[msort[i].name].property_info);
		}

		for (PropertyInfo& prop : props) {
			validate_property(prop);
			p_properties->push_back(prop);
		}

		props.clear();

		sptr = sptr->base.ptr();
	}
}

bool RuztaInstance::property_can_revert(const StringName& p_name) const {
	Variant name = p_name;
	const Variant* args[1] = {&name};

	const Ruzta* sptr = script.ptr();
	while (sptr) {
		if (likely(sptr->valid)) {
			HashMap<StringName, RuztaFunction*>::ConstIterator E = sptr->member_functions.find(RuztaLanguage::get_singleton()->strings._property_can_revert);
			if (E) {
				GDExtensionCallError err;
				Variant ret = E->value->call(const_cast<RuztaInstance*>(this), args, 1, err);
				if (err.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK && ret.get_type() == Variant::BOOL && ret.operator bool()) {
					return true;
				}
			}
		}
		sptr = sptr->base.ptr();
	}

	return false;
}

bool RuztaInstance::property_get_revert(const StringName& p_name, Variant& r_ret) const {
	Variant name = p_name;
	const Variant* args[1] = {&name};

	const Ruzta* sptr = script.ptr();
	while (sptr) {
		if (likely(sptr->valid)) {
			HashMap<StringName, RuztaFunction*>::ConstIterator E = sptr->member_functions.find(RuztaLanguage::get_singleton()->strings._property_get_revert);
			if (E) {
				GDExtensionCallError err;
				Variant ret = E->value->call(const_cast<RuztaInstance*>(this), args, 1, err);
				if (err.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK && ret.get_type() != Variant::NIL) {
					r_ret = ret;
					return true;
				}
			}
		}
		sptr = sptr->base.ptr();
	}

	return false;
}

void RuztaInstance::get_method_list(List<MethodInfo>* p_list) const {
	const Ruzta* sptr = script.ptr();
	while (sptr) {
		for (const KeyValue<StringName, RuztaFunction*>& E : sptr->member_functions) {
			p_list->push_back(E.value->get_method_info());
		}
		sptr = sptr->base.ptr();
	}
}

bool RuztaInstance::has_method(const StringName& p_method) const {
	const Ruzta* sptr = script.ptr();
	while (sptr) {
		HashMap<StringName, RuztaFunction*>::ConstIterator E = sptr->member_functions.find(p_method);
		if (E) {
			return true;
		}
		sptr = sptr->base.ptr();
	}

	return false;
}

int RuztaInstance::get_method_argument_count(const StringName& p_method, bool* r_is_valid) const {
	const Ruzta* sptr = script.ptr();
	while (sptr) {
		HashMap<StringName, RuztaFunction*>::ConstIterator E = sptr->member_functions.find(p_method);
		if (E) {
			if (r_is_valid) {
				*r_is_valid = true;
			}
			return E->value->get_argument_count();
		}
		sptr = sptr->base.ptr();
	}

	if (r_is_valid) {
		*r_is_valid = false;
	}
	return 0;
}

void RuztaInstance::_call_implicit_ready_recursively(Ruzta* p_script) {
	// Call base class first.
	if (p_script->base.ptr()) {
		_call_implicit_ready_recursively(p_script->base.ptr());
	}
	if (likely(p_script->valid) && p_script->implicit_ready) {
		GDExtensionCallError err;
		p_script->implicit_ready->call(this, nullptr, 0, err);
	}
}

Variant RuztaInstance::callp(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) {
	Ruzta* sptr = script.ptr();
	if (unlikely(p_method == StringName("_ready"))) {
		// Call implicit ready first, including for the super classes recursively.
		_call_implicit_ready_recursively(sptr);
	}
	while (sptr) {
		if (likely(sptr->valid)) {
			HashMap<StringName, RuztaFunction*>::Iterator E = sptr->member_functions.find(p_method);
			if (E) {
				return E->value->call(this, p_args, p_argcount, r_error);
			}
		}
		sptr = sptr->base.ptr();
	}

	r_error.error = GDExtensionCallErrorType::GDEXTENSION_CALL_ERROR_INVALID_METHOD;
	return Variant();
}

void RuztaInstance::notification(int p_notification, bool p_reversed) {
	if (unlikely(!script->valid)) {
		return;
	}

	// notification is not virtual, it gets called at ALL levels just like in C.
	Variant value = p_notification;
	const Variant* args[1] = {&value};
	const StringName& notification_str = RuztaLanguage::get_singleton()->strings._notification;

	LocalVector<Ruzta*> script_stack;
	uint32_t script_count = 0;
	for (Ruzta* sptr = script.ptr(); sptr; sptr = sptr->base.ptr(), ++script_count) {
		script_stack.push_back(sptr);
	}

	const int start = p_reversed ? 0 : script_count - 1;
	const int end = p_reversed ? script_count : -1;
	const int step = p_reversed ? 1 : -1;

	for (int idx = start; idx != end; idx += step) {
		Ruzta* sc = script_stack[idx];
		if (likely(sc->valid)) {
			HashMap<StringName, RuztaFunction*>::Iterator E = sc->member_functions.find(notification_str);
			if (E) {
				GDExtensionCallError err;
				E->value->call(this, args, 1, err);
				if (err.error != GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
					// print error about notification call
				}
			}
		}
	}
}

String RuztaInstance::to_string(bool* r_valid) {
	if (has_method(StringName("_to_string"))) {
		GDExtensionCallError ce;
		Variant ret = callp(StringName("_to_string"), nullptr, 0, ce);
		if (ce.error == GDExtensionCallErrorType::GDEXTENSION_CALL_OK) {
			if (ret.get_type() != Variant::STRING) {
				if (r_valid) {
					*r_valid = false;
				}
				ERR_FAIL_V_MSG(String(), "Wrong type for " + StringName("_to_string") + ", must be a String.");
			}
			if (r_valid) {
				*r_valid = true;
			}
			return ret.operator String();
		}
	}
	if (r_valid) {
		*r_valid = false;
	}
	return String();
}

Ref<Script> RuztaInstance::get_script() const {
	return script;
}

ScriptLanguage* RuztaInstance::get_language() {
	return RuztaLanguage::get_singleton();
}

const Variant RuztaInstance::get_rpc_config() const {
	return script->get_rpc_config();
}

void RuztaInstance::reload_members() {
#ifdef DEBUG_ENABLED

	Vector<Variant> new_members;
	new_members.resize(script->member_indices.size());

	// pass the values to the new indices
	for (KeyValue<StringName, Ruzta::MemberInfo>& E : script->member_indices) {
		if (member_indices_cache.has(E.key)) {
			Variant value = members[member_indices_cache[E.key]];
			new_members.write[E.value.index] = value;
		}
	}

	members.resize(new_members.size());	 // resize

	// apply
	members = new_members;

	// pass the values to the new indices
	member_indices_cache.clear();
	for (const KeyValue<StringName, Ruzta::MemberInfo>& E : script->member_indices) {
		member_indices_cache[E.key] = E.value.index;
	}

#endif
}

RuztaInstance::~RuztaInstance() {
	MutexLock lock(RuztaLanguage::get_singleton()->mutex);

	while (SelfList<RuztaFunctionState>* E = pending_func_states.first()) {
		// Order matters since clearing the stack may already cause
		// the RuztaFunctionState to be destroyed and thus removed from the list.
		pending_func_states.remove(E);
		RuztaFunctionState* state = E->self();
		ObjectID state_id = ObjectID(state->get_instance_id());
		state->_clear_connections();
		if (ObjectDB::get_instance(state_id)) {
			state->_clear_stack();
		}
	}

	if (script.is_valid() && owner) {
		script->instances.erase(owner);
	}
}

/************* SCRIPT LANGUAGE **************/

RuztaLanguage* RuztaLanguage::singleton = nullptr;

String RuztaLanguage::_get_name() const {
	return "Ruzta";
}

/* LANGUAGE FUNCTIONS */

void RuztaLanguage::_add_global(const StringName& p_name, const Variant& p_value) {
	if (globals.has(p_name)) {
		// overwrite existing
		global_array.write[globals[p_name]] = p_value;
		return;
	}

	if (global_array_empty_indexes.size()) {
		int index = global_array_empty_indexes[global_array_empty_indexes.size() - 1];
		globals[p_name] = index;
		global_array.write[index] = p_value;
		global_array_empty_indexes.resize(global_array_empty_indexes.size() - 1);
	} else {
		globals[p_name] = global_array.size();
		global_array.push_back(p_value);
		_global_array = global_array.ptrw();
	}
}

void RuztaLanguage::_remove_global(const StringName& p_name) {
	if (!globals.has(p_name)) {
		return;
	}
	global_array_empty_indexes.push_back(globals[p_name]);
	global_array.write[globals[p_name]] = Variant::NIL;
	globals.erase(p_name);
}

void RuztaLanguage::_add_global_constant(const StringName& p_variable, const Variant& p_value) {
	_add_global(p_variable, p_value);
}

void RuztaLanguage::_add_named_global_constant(const StringName& p_name, const Variant& p_value) {
	named_globals[p_name] = p_value;
}

Variant RuztaLanguage::get_any_global_constant(const StringName& p_name) {
	if (named_globals.has(p_name)) {
		return named_globals[p_name];
	}
	if (globals.has(p_name)) {
		return _global_array[globals[p_name]];
	}
	ERR_FAIL_V_MSG(Variant(), vformat("Could not find any global constant with name: %s.", p_name));
}

void RuztaLanguage::_remove_named_global_constant(const StringName& p_name) {
	ERR_FAIL_COND(!named_globals.has(p_name));
	named_globals.erase(p_name);
}

void RuztaLanguage::_init() {
	// populate global constants
	int gcc = CoreConstants::get_global_constant_count();
	for (int i = 0; i < gcc; i++) {
		_add_global(StringName(CoreConstants::get_global_constant_name(i)), CoreConstants::get_global_constant_value(i));
	}

	_add_global(StringName("PI"), Math_PI);
	_add_global(StringName("TAU"), Math_TAU);
	_add_global(StringName("INF"), Math_INF);
	_add_global(StringName("NAN"), Math_NAN);

	// populate native classes

	PackedStringArray class_list = ClassDB::get_class_list();
	for (const StringName& class_name : class_list) {
		if (globals.has(class_name)) {
			continue;
		}
		Ref<RuztaNativeClass> nc = memnew(RuztaNativeClass(class_name));
		_add_global(class_name, nc);
	}

	// populate singletons

	for (const StringName& E : Engine::get_singleton()->get_singleton_list()) {
		_add_global(E, Engine::get_singleton()->get_singleton(E));
	}

#ifdef TOOLS_ENABLED
	// if (Engine::get_singleton()->is_editor_hint()) {
	// 	GDExtensionManager::get_singleton()->connect("extension_loaded", callable_mp(this, &RuztaLanguage::_extension_loaded));
	// 	GDExtensionManager::get_singleton()->connect("extension_unloading", callable_mp(this, &RuztaLanguage::_extension_unloading));
	// }
#endif	// TOOLS_ENABLED

#ifdef DEBUG_ENABLED
	RuztaParser::update_project_settings();
	if (!ProjectSettings::get_singleton()->is_connected("settings_changed", callable_mp_static(&RuztaParser::update_project_settings))) {
		ProjectSettings::get_singleton()->connect("settings_changed", callable_mp_static(&RuztaParser::update_project_settings));
	}
#endif	// DEBUG_ENABLED

#ifdef TESTS_ENABLED
	RuztaTests::RuztaTestRunner::handle_cmdline();
#endif	// TESTS_ENABLED
}

String RuztaLanguage::_get_name() const {
	return "Ruzta";
}

String RuztaLanguage::_get_type() const {
	return "Ruzta";
}

String RuztaLanguage::_get_extension() const {
	return "rz";
}

void RuztaLanguage::_finish() {
	if (finishing) {
		return;
	}
	finishing = true;

	// Clear the cache before parsing the script_list
	RuztaCache::clear();

	if (finishing) {
		return;
	}
	finishing = true;

	// Clear the cache before parsing the script_list
	RuztaCache::clear();

	// Clear dependencies between scripts, to ensure cyclic references are broken
	// (to avoid leaks at exit).
	SelfList<Ruzta>* s = script_list.first();
	while (s) {
		// This ensures the current script is not released before we can check
		// what's the next one in the list (we can't get the next upfront because we
		// don't know if the reference breaking will cause it -or any other after
		// it, for that matter- to be released so the next one is not the same as
		// before).
		Ref<Ruzta> scr = s->self();
		if (scr.is_valid()) {
			for (KeyValue<StringName, RuztaFunction*>& E : scr->member_functions) {
				RuztaFunction* func = E.value;
				for (int i = 0; i < func->argument_types.size(); i++) {
					func->argument_types.write[i].script_type_ref = Ref<Script>();
				}
				func->return_type.script_type_ref = Ref<Script>();
			}
			for (KeyValue<StringName, Ruzta::MemberInfo>& E : scr->member_indices) {
				E.value.data_type.script_type_ref = Ref<Script>();
			}

			// Clear backup for scripts that could slip out of the cyclic reference
			// check
			scr->clear();
		}
		s = s->next();
	}
	script_list.clear();
	function_list.clear();

	finishing = false;
}

void RuztaLanguage::_profiling_start() {
#ifdef DEBUG_ENABLED
	MutexLock lock(mutex);

	SelfList<RuztaFunction>* elem = function_list.first();
	while (elem) {
		elem->self()->profile.call_count.set(0);
		elem->self()->profile.self_time.set(0);
		elem->self()->profile.total_time.set(0);
		elem->self()->profile.frame_call_count.set(0);
		elem->self()->profile.frame_self_time.set(0);
		elem->self()->profile.frame_total_time.set(0);
		elem->self()->profile.last_frame_call_count = 0;
		elem->self()->profile.last_frame_self_time = 0;
		elem->self()->profile.last_frame_total_time = 0;
		elem->self()->profile.native_calls.clear();
		elem->self()->profile.last_native_calls.clear();
		elem = elem->next();
	}

	profiling = true;
#endif
}

void RuztaLanguage::_profiling_set_save_native_calls(bool p_enable) {
#ifdef DEBUG_ENABLED
	MutexLock lock(mutex);
	profile_native_calls = p_enable;
#endif
}

void RuztaLanguage::_profiling_stop() {
#ifdef DEBUG_ENABLED
	MutexLock lock(mutex);

	profiling = false;
#endif
}

int32_t RuztaLanguage::_profiling_get_accumulated_data(ScriptLanguageExtensionProfilingInfo* p_info_arr, int32_t p_info_max) {
	int current = 0;
#ifdef DEBUG_ENABLED

	MutexLock lock(mutex);

	profiling_collate_native_call_data(true);
	SelfList<RuztaFunction>* elem = function_list.first();
	while (elem) {
		if (current >= p_info_max) {
			break;
		}
		int last_non_internal = current;
		p_info_arr[current].call_count = elem->self()->profile.call_count.get();
		p_info_arr[current].self_time = elem->self()->profile.self_time.get();
		p_info_arr[current].total_time = elem->self()->profile.total_time.get();
		p_info_arr[current].signature = elem->self()->profile.signature;
		current++;

		int nat_time = 0;
		HashMap<String, RuztaFunction::Profile::NativeProfile>::ConstIterator nat_calls = elem->self()->profile.native_calls.begin();
		while (nat_calls) {
			p_info_arr[current].call_count = nat_calls->value.call_count;
			p_info_arr[current].total_time = nat_calls->value.total_time;
			p_info_arr[current].self_time = nat_calls->value.total_time;
			p_info_arr[current].signature = nat_calls->value.signature;
			nat_time += nat_calls->value.total_time;
			current++;
			++nat_calls;
		}
		// p_info_arr[last_non_internal].internal_time = nat_time;
		elem = elem->next();
	}
#endif

	return current;
}

int32_t RuztaLanguage::_profiling_get_frame_data(ScriptLanguageExtensionProfilingInfo* p_info_arr, int32_t p_info_max) {
	int current = 0;

#ifdef DEBUG_ENABLED
	MutexLock lock(mutex);

	profiling_collate_native_call_data(false);
	SelfList<RuztaFunction>* elem = function_list.first();
	while (elem) {
		if (current >= p_info_max) {
			break;
		}
		if (elem->self()->profile.last_frame_call_count > 0) {
			int last_non_internal = current;
			p_info_arr[current].call_count = elem->self()->profile.last_frame_call_count;
			p_info_arr[current].self_time = elem->self()->profile.last_frame_self_time;
			p_info_arr[current].total_time = elem->self()->profile.last_frame_total_time;
			p_info_arr[current].signature = elem->self()->profile.signature;
			current++;

			int nat_time = 0;
			HashMap<String, RuztaFunction::Profile::NativeProfile>::ConstIterator nat_calls = elem->self()->profile.last_native_calls.begin();
			while (nat_calls) {
				p_info_arr[current].call_count = nat_calls->value.call_count;
				p_info_arr[current].total_time = nat_calls->value.total_time;
				p_info_arr[current].self_time = nat_calls->value.total_time;
				// p_info_arr[current].internal_time = nat_calls->value.total_time;
				p_info_arr[current].signature = nat_calls->value.signature;
				nat_time += nat_calls->value.total_time;
				current++;
				++nat_calls;
			}
			// p_info_arr[last_non_internal].internal_time = nat_time;
		}
		elem = elem->next();
	}
#endif

	return current;
}

void RuztaLanguage::profiling_collate_native_call_data(bool p_accumulated) {
#ifdef DEBUG_ENABLED
	// The same native call can be called from multiple functions, so join them together here.
	// Only use the name of the function (ie signature.split[2]).
	HashMap<String, RuztaFunction::Profile::NativeProfile*> seen_nat_calls;
	SelfList<RuztaFunction>* elem = function_list.first();
	while (elem) {
		HashMap<String, RuztaFunction::Profile::NativeProfile>* nat_calls = p_accumulated ? &elem->self()->profile.native_calls : &elem->self()->profile.last_native_calls;
		HashMap<String, RuztaFunction::Profile::NativeProfile>::Iterator it = nat_calls->begin();

		while (it != nat_calls->end()) {
			PackedStringArray sig = it->value.signature.split("::");
			HashMap<String, RuztaFunction::Profile::NativeProfile*>::ConstIterator already_found = seen_nat_calls.find(sig[2]);
			if (already_found) {
				already_found->value->total_time += it->value.total_time;
				already_found->value->call_count += it->value.call_count;
				elem->self()->profile.last_native_calls.remove(it);
			} else {
				seen_nat_calls.insert(sig[2], &it->value);
			}
			++it;
		}
		elem = elem->next();
	}
#endif
}

struct RuztaDepSort {
	// must support sorting so inheritance works properly (parent must be reloaded first)
	bool operator()(const Ref<Ruzta>& A, const Ref<Ruzta>& B) const {
		if (A == B) {
			return false;  // shouldn't happen but..
		}
		const Ruzta* I = B->get_base().ptr();
		while (I) {
			if (I == A.ptr()) {
				// A is a base of B
				return true;
			}

			I = I->get_base().ptr();
		}

		return false;  // not a base
	}
};

void RuztaLanguage::_reload_all_scripts() {
#ifdef DEBUG_ENABLED
	print_verbose("Ruzta: Reloading all scripts");
	Array scripts;
	{
		MutexLock lock(mutex);

		SelfList<Ruzta>* elem = script_list.first();
		while (elem) {
			String file_path = elem->self()->get_path();
			if (file_path.begins_with("res://") && file_path.find("::") == -1) {
				print_verbose("Ruzta: Found: " + file_path);
				scripts.push_back(Ref<Ruzta>(elem->self()));  // cast to ruzta to avoid being erased by accident
			}
			elem = elem->next();
		}

#ifdef TOOLS_ENABLED
		if (Engine::get_singleton()->is_editor_hint()) {
			// Reload all pointers to existing singletons so that tool scripts can work with the reloaded extensions.
			for (const String &singleton_name : Engine::get_singleton()->get_singleton_list()) {
				_add_global(singleton_name, Engine::get_singleton()->get_singleton(singleton_name));
			}
		}
#endif	// TOOLS_ENABLED
	}

	_reload_scripts(scripts, true);
#endif	// DEBUG_ENABLED
}

void RuztaLanguage::_reload_scripts(const Array& p_scripts, bool p_soft_reload) {
#ifdef DEBUG_ENABLED

	List<Ref<Ruzta>> scripts;
	{
		MutexLock lock(mutex);

		SelfList<Ruzta>* elem = script_list.first();
		while (elem) {
			// Scripts will reload all subclasses, so only reload root scripts.
			if (elem->self()->is_root_script() && !elem->self()->get_path().is_empty()) {
				scripts.push_back(Ref<Ruzta>(elem->self()));  // cast to ruzta to avoid being erased by accident
			}
			elem = elem->next();
		}
	}

	// when someone asks you why dynamically typed languages are easier to write....

	HashMap<Ref<Ruzta>, HashMap<ObjectID, List<Pair<StringName, Variant>>>> to_reload;

	// as scripts are going to be reloaded, must proceed without locking here

	scripts.sort_custom<RuztaDepSort>();  // update in inheritance dependency order

	for (Ref<Ruzta>& scr : scripts) {
		bool reload = p_scripts.has(scr) || to_reload.has(scr->get_base());

		if (!reload) {
			continue;
		}

		to_reload.insert(scr, HashMap<ObjectID, List<Pair<StringName, Variant>>>());

		if (!p_soft_reload) {
			// save state and remove script from instances
			HashMap<ObjectID, List<Pair<StringName, Variant>>>& map = to_reload[scr];

			while (scr->instances.front()) {
				Object* obj = scr->instances.front()->get();
				// save instance info
				List<Pair<StringName, Variant>> state;
				ScriptInstance *si = static_cast<ScriptInstance *>(godot::internal::gdextension_interface_object_get_script_instance(obj, RuztaLanguage::get_singleton()));
				if (si) {
					si->get_property_state(state);
					map[ObjectID(obj->get_instance_id())] = state;
					obj->set_script(Variant());
				}
			}

			// same thing for placeholders
#ifdef TOOLS_ENABLED

			while (scr->placeholders.size()) {
				Object* obj = (*scr->placeholders.begin())->get_owner();

				// save instance info
				ScriptInstance *si = static_cast<ScriptInstance *>(godot::internal::gdextension_interface_object_get_script_instance(obj, RuztaLanguage::get_singleton()));
				if (si) {
					map.insert(ObjectID(obj->get_instance_id()), List<Pair<StringName, Variant>>());
					List<Pair<StringName, Variant>>& state = map[ObjectID(obj->get_instance_id())];
					si->get_property_state(state);
					obj->set_script(Variant());
				} else {
					// no instance found. Let's remove it so we don't loop forever
					scr->placeholders.erase(*scr->placeholders.begin());
				}
			}

#endif	// TOOLS_ENABLED

			for (const KeyValue<ObjectID, List<Pair<StringName, Variant>>>& F : scr->pending_reload_state) {
				map[F.key] = F.value;  // pending to reload, use this one instead
			}
		}
	}

	for (KeyValue<Ref<Ruzta>, HashMap<ObjectID, List<Pair<StringName, Variant>>>>& E : to_reload) {
		Ref<Ruzta> scr = E.key;
		print_verbose("Ruzta: Reloading: " + scr->get_path());
		if (scr->is_built_in()) {
			// TODO: It would be nice to do it more efficiently than loading the whole scene again.
			Ref<PackedScene> scene = ResourceLoader::get_singleton()->load(scr->get_path().get_slice("::", 0), "", ResourceLoader::CACHE_MODE_IGNORE_DEEP);
			ERR_CONTINUE(scene.is_null());

			// TODO: Extract resource from scene
			// Ref<SceneState> state = scene->get_state();			
			// Ref<Ruzta> fresh = state->get_sub_resource(scr->get_path());
			// ERR_CONTINUE(fresh.is_null());

			// scr->set_source_code(fresh->get_source_code());
		} else {
			scr->load_source_code(scr->get_path());
		}
		scr->reload(p_soft_reload);

		// restore state if saved
		for (KeyValue<ObjectID, List<Pair<StringName, Variant>>>& F : E.value) {
			List<Pair<StringName, Variant>>& saved_state = F.value;

			Object* obj = ObjectDB::get_instance(F.key);
			if (!obj) {
				continue;
			}

			if (!p_soft_reload) {
				// clear it just in case (may be a pending reload state)
				obj->set_script(Variant());
			}
			obj->set_script(scr);

			ScriptInstance *script_inst = static_cast<ScriptInstance *>(godot::internal::gdextension_interface_object_get_script_instance(obj, RuztaLanguage::get_singleton()));
			if (!script_inst) {
				// failed, save reload state for next time if not saved
				if (!scr->pending_reload_state.has(ObjectID(obj->get_instance_id()))) {
					scr->pending_reload_state[ObjectID(obj->get_instance_id())] = saved_state;
				}
				continue;
			}

			if (script_inst->is_placeholder() && scr->is_placeholder_fallback_enabled()) {
				PlaceHolderScriptInstance *placeholder = static_cast<PlaceHolderScriptInstance *>(script_inst);
				for (List<Pair<StringName, Variant>>::Element* G = saved_state.front(); G; G = G->next()) {
					placeholder->property_set_fallback(G->get().first, G->get().second);
				}
			} else {
				for (List<Pair<StringName, Variant>>::Element* G = saved_state.front(); G; G = G->next()) {
					script_inst->set(G->get().first, G->get().second);
				}
			}

			scr->pending_reload_state.erase(ObjectID(obj->get_instance_id()));	// as it reloaded, remove pending state
		}

		// if instance states were saved, set them!
	}

#endif	// DEBUG_ENABLED
}

void RuztaLanguage::_reload_tool_script(const Ref<Script>& p_script, bool p_soft_reload) {
	Array scripts = {p_script};
	_reload_scripts(scripts, p_soft_reload);
}

void RuztaLanguage::_frame() {
#ifdef DEBUG_ENABLED
	if (profiling) {
		MutexLock lock(mutex);

		SelfList<RuztaFunction>* elem = function_list.first();
		while (elem) {
			elem->self()->profile.last_frame_call_count = elem->self()->profile.frame_call_count.get();
			elem->self()->profile.last_frame_self_time = elem->self()->profile.frame_self_time.get();
			elem->self()->profile.last_frame_total_time = elem->self()->profile.frame_total_time.get();
			elem->self()->profile.last_native_calls = elem->self()->profile.native_calls;
			elem->self()->profile.frame_call_count.set(0);
			elem->self()->profile.frame_self_time.set(0);
			elem->self()->profile.frame_total_time.set(0);
			elem->self()->profile.native_calls.clear();
			elem = elem->next();
		}
	}

#endif
}

/* EDITOR FUNCTIONS */
PackedStringArray RuztaLanguage::_get_reserved_words() const {
	// Please keep alphabetical order within categories.
	static const PackedStringArray ret = {
		// Control flow.
		"break",
		"continue",
		"elif",
		"else",
		"for",
		"if",
		"match",
		"pass",
		"return",
		"when",
		"while",
		// Declarations.
		"class",
		"class_name",
		"const",
		"enum",
		"extends",
		"func",
		"namespace",  // Reserved for potential future use.
		"signal",
		"static",
		"trait",  // Reserved for potential future use.
		"var",
		// Other keywords.
		"await",
		"breakpoint",
		"self",
		"super",
		"yield",  // Reserved for potential future use.
		// Operators.
		"and",
		"as",
		"in",
		"is",
		"not",
		"or",
		// Special values (tokenizer treats them as literals, not as tokens).
		"false",
		"null",
		"true",
		// Constants.
		"INF",
		"NAN",
		"PI",
		"TAU",
		// Functions (highlighter uses global function color instead).
		"assert",
		"preload",
		// Types (highlighter uses type color instead).
		"void",
	};

	return ret;
}

bool RuztaLanguage::_is_control_flow_keyword(const String& p_keyword) const {
	// Please keep alphabetical order.
	return p_keyword == "break" ||
		   p_keyword == "continue" ||
		   p_keyword == "elif" ||
		   p_keyword == "else" ||
		   p_keyword == "for" ||
		   p_keyword == "if" ||
		   p_keyword == "match" ||
		   p_keyword == "pass" ||
		   p_keyword == "return" ||
		   p_keyword == "when" ||
		   p_keyword == "while";
}

bool RuztaLanguage::_handles_global_class_type(const String& p_type) const {
	return p_type == "Ruzta";
}

Dictionary RuztaLanguage::_get_global_class_name(const String& p_path) const {
	Dictionary result;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	Error err = FileAccess::get_open_error();
	if (err) {
		return result;
	}

	String source = f->get_as_text();

	RuztaParser parser;
	err = parser.parse(source, p_path, false, false);

	const RuztaParser::ClassNode* c = parser.get_tree();
	if (!c) {
		return result;	// No class parsed.
	}

	if (c->identifier) {
		result["name"] = String(c->identifier->name);
	}
	result["icon_path"] = c->simplified_icon_path;
	result["is_abstract"] = c->is_abstract;
	result["is_tool"] = parser.is_tool();

	String base_type;
	const RuztaParser::ClassNode* subclass = c;
	String path = p_path;
	RuztaParser subparser;
	while (subclass) {
		if (subclass->extends_used) {
			if (!subclass->extends_path.is_empty()) {
				if (subclass->extends.is_empty()) {
					// We only care about the referenced class_name.
					Dictionary base_result = _get_global_class_name(subclass->extends_path);
					if (base_result.has("name")) {
						base_type = base_result["name"];
					} else if (base_result.has("base_type")) {
						base_type = base_result["base_type"];
					}
					subclass = nullptr;
					break;
				} else {
					Vector<RuztaParser::IdentifierNode*> extend_classes = subclass->extends;

					Ref<FileAccess> subfile = FileAccess::open(subclass->extends_path, FileAccess::READ);
					if (subfile.is_null()) {
						break;
					}
					String subsource = subfile->get_as_text();

					if (subsource.is_empty()) {
						break;
					}
					String subpath = subclass->extends_path;
					if (subpath.is_relative_path()) {
						subpath = path.get_base_dir().path_join(subpath).simplify_path();
					}

					if (OK != subparser.parse(subsource, subpath, false)) {
						break;
					}
					path = subpath;
					subclass = subparser.get_tree();

					while (extend_classes.size() > 0) {
						bool found = false;
						for (int i = 0; i < subclass->members.size(); i++) {
							if (subclass->members[i].type != RuztaParser::ClassNode::Member::CLASS) {
								continue;
							}

							const RuztaParser::ClassNode* inner_class = subclass->members[i].m_class;
							if (inner_class->identifier->name == extend_classes[0]->name) {
								extend_classes.remove_at(0);
								found = true;
								subclass = inner_class;
								break;
							}
						}
						if (!found) {
							subclass = nullptr;
							break;
						}
					}
				}
			} else if (subclass->extends.size() == 1) {
				base_type = subclass->extends[0]->name;
				subclass = nullptr;
			} else {
				break;
			}
		} else {
			base_type = "RefCounted";
			subclass = nullptr;
		}
	}
	result["base_type"] = base_type;

	return result;
}

thread_local RuztaLanguage::CallLevel* RuztaLanguage::_call_stack = nullptr;
thread_local uint32_t RuztaLanguage::_call_stack_size = 0;

RuztaLanguage::CallLevel* RuztaLanguage::_get_stack_level(uint32_t p_level) {
	ERR_FAIL_UNSIGNED_INDEX_V(p_level, _call_stack_size, nullptr);
	CallLevel* level = _call_stack;	 // Start from top
	uint32_t level_index = 0;
	while (p_level > level_index) {
		level_index++;
		level = level->prev;
	}
	return level;
}

RuztaLanguage::RuztaLanguage() {
	ERR_FAIL_COND(singleton);
	singleton = this;
	strings._init = StringName("_init");
	strings._static_init = StringName("_static_init");
	strings._notification = StringName("_notification");
	strings._set = StringName("_set");
	strings._get = StringName("_get");
	strings._get_property_list = StringName("_get_property_list");
	strings._validate_property = StringName("_validate_property");
	strings._property_can_revert = StringName("_property_can_revert");
	strings._property_get_revert = StringName("_property_get_revert");
	strings._script_source = StringName("script/source");
	_debug_parse_err_line = -1;
	_debug_parse_err_file = "";

#ifdef DEBUG_ENABLED
	profiling = false;
	profile_native_calls = false;
	script_frame_time = 0;
#endif	// DEBUG_ENABLED

	_debug_max_call_stack = GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "debug/settings/ruzta/max_call_stack", PROPERTY_HINT_RANGE, "512," + itos(RuztaFunction::MAX_CALL_DEPTH - 1) + ",1"), 1024);
	track_call_stack = GLOBAL_DEF_RST("debug/settings/ruzta/always_track_call_stacks", false);
	track_locals = GLOBAL_DEF_RST("debug/settings/ruzta/always_track_local_variables", false);

#ifdef DEBUG_ENABLED
	track_call_stack = true;
	track_locals = track_locals || EngineDebugger::get_singleton()->is_active();

	GLOBAL_DEF("debug/ruzta/warnings/enable", true);

	Dictionary dict;
	dict.get_or_add(String("res://addons"), RuztaParser::WarningDirectoryRule::DECISION_EXCLUDE);

	GLOBAL_DEF(PropertyInfo(Variant::DICTIONARY, "debug/ruzta/warnings/directory_rules", PROPERTY_HINT_TYPE_STRING,
							vformat("%d/%d:;%d/%d:Exclude,Include", Variant::STRING, PROPERTY_HINT_DIR, Variant::INT, PROPERTY_HINT_ENUM)),
			   dict);

	for (int i = 0; i < (int)RuztaWarning::WARNING_MAX; i++) {
		const RuztaWarning::Code code = (RuztaWarning::Code)i;
		const Variant default_value = RuztaWarning::get_default_value(code);
		GLOBAL_DEF(RuztaWarning::get_property_info(code), default_value);

#ifndef DISABLE_DEPRECATED
		if (i >= RuztaWarning::FIRST_DEPRECATED_WARNING) {
			const String setting_path = RuztaWarning::get_setting_path_from_code(code);
			ProjectSettings::get_singleton()->set_as_internal(setting_path, true);
		}
#endif	// DISABLE_DEPRECATED
	}
#endif	// DEBUG_ENABLED
}

RuztaLanguage::~RuztaLanguage() {
	singleton = nullptr;
}

void RuztaLanguage::add_orphan_subclass(const String& p_qualified_name, const ObjectID& p_subclass) {
	orphan_subclasses[p_qualified_name] = p_subclass;
}

Ref<Ruzta> RuztaLanguage::get_orphan_subclass(const String& p_qualified_name) {
	HashMap<String, ObjectID>::Iterator orphan_subclass_element = orphan_subclasses.find(p_qualified_name);
	if (!orphan_subclass_element) {
		return Ref<Ruzta>();
	}
	ObjectID orphan_subclass = orphan_subclass_element->value;
	Object* obj = ObjectDB::get_instance(orphan_subclass);
	orphan_subclasses.remove(orphan_subclass_element);
	if (!obj) {
		return Ref<Ruzta>();
	}
	return Ref<Ruzta>(Object::cast_to<Ruzta>(obj));
}

Ref<Ruzta> RuztaLanguage::get_script_by_fully_qualified_name(const String& p_name) {
	{
		MutexLock lock(mutex);

		SelfList<Ruzta>* elem = script_list.first();
		while (elem) {
			Ruzta* scr = elem->self();
			if (scr->fully_qualified_name == p_name) {
				return scr;
			}
			elem = elem->next();
		}
	}

	Ref<Ruzta> scr;
	scr.instantiate();
	scr->fully_qualified_name = p_name;
	return scr;
}

/*************** RESOURCE ***************/

Variant ResourceFormatLoaderRuzta::_load(const String& p_path, const String& p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const {
	Error err;
	bool ignoring = p_cache_mode == CACHE_MODE_IGNORE || p_cache_mode == CACHE_MODE_IGNORE_DEEP;
	Ref<Ruzta> scr = RuztaCache::get_full_script(p_original_path, err, "", ignoring);

	if (err && scr.is_valid()) {
		// If !scr.is_valid(), the error was likely from scr->load_source_code(), which already generates an error.
		ERR_PRINT_ED(vformat(R"(Failed to load script "%s" with error "%s".)", p_original_path, UtilityFunctions::error_string(err)));
	}

	return scr;
}

PackedStringArray ResourceFormatLoaderRuzta::_get_recognized_extensions() const {
	PackedStringArray extensions;
	extensions.push_back("rz");
	extensions.push_back("rzc");
	return extensions;
}

bool ResourceFormatLoaderRuzta::_handles_type(const StringName& p_type) const {
	return (p_type == StringName("Script") || p_type == StringName("Ruzta"));
}

String ResourceFormatLoaderRuzta::_get_resource_type(const String& p_path) const {
	String el = p_path.get_extension().to_lower();
	if (el == "rz" || el == "rzc") {
		return "Ruzta";
	}
	return "";
}

PackedStringArray ResourceFormatLoaderRuzta::_get_dependencies(const String& p_path, bool p_add_types) const {
	PackedStringArray dependencies;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(file.is_null(), dependencies, "Cannot open file '" + p_path + "'.");

	String source = file->get_as_text();
	if (source.is_empty()) {
		return dependencies;
	}

	RuztaParser parser;
	if (OK != parser.parse(source, p_path, false)) {
		return dependencies;
	}

	for (const String& E : parser.get_dependencies()) {
		dependencies.push_back(E);
	}
	return dependencies;
}

PackedStringArray ResourceFormatLoaderRuzta::_get_classes_used(const String& p_path) const {
	PackedStringArray classes;
	Ref<Ruzta> scr = ResourceLoader::get_singleton()->load(p_path);
	if (scr.is_null()) {
		return classes;
	}

	HashSet<StringName> r_classes;
	const String source = scr->get_source_code();
	RuztaTokenizerText tokenizer;
	tokenizer.set_source_code(source);
	RuztaTokenizer::Token current = tokenizer.scan();
	while (current.type != RuztaTokenizer::Token::TK_EOF) {
		if (!current.is_identifier()) {
			current = tokenizer.scan();
			continue;
		}

		int insert_idx = 0;
		for (int i = 0; i < current.start_line - 1; i++) {
			insert_idx = source.find("\n", insert_idx) + 1;
		}
		// Insert the "cursor" character, needed for the lookup to work.
		const String source_with_cursor = source.insert(insert_idx + current.start_column, String::chr(0xFFFF));

		Dictionary result = RuztaLanguage::get_singleton()->_lookup_code(source_with_cursor, current.get_identifier(), p_path, nullptr);
		if (result.size() > 0) {
			if (!String(result["class_name"]).is_empty() && ClassDB::class_exists(result["class_name"])) {
				r_classes.insert(result["class_name"]);
			}

			if (int(result["type"]) == RuztaLanguage::LOOKUP_RESULT_CLASS_PROPERTY) {
				PropertyInfo prop;
				bool found_prop = false;
				for (Dictionary prop_dict :  ClassDB::class_get_property_list(result["class_name"])) {
					if (prop_dict.has("name") && prop_dict["name"] == result["class_member"]) {
						prop = PropertyInfo::from_dict(prop_dict);
						found_prop = true;
					}
				}
				if (found_prop) {
					if (!prop.class_name.is_empty() && ClassDB::class_exists(prop.class_name)) {
						r_classes.insert(prop.class_name);
					}
					if (!prop.hint_string.is_empty() && ClassDB::class_exists(prop.hint_string)) {
						r_classes.insert(prop.hint_string);
					}
				}
			} else if (int(result["type"]) == RuztaLanguage::LOOKUP_RESULT_CLASS_METHOD) {
				MethodInfo met;
				bool found_method = false;
				for (Dictionary method_dict :  ClassDB::class_get_method_list(result["class_name"])) {
					if (method_dict.has("name") && method_dict["name"] == result["class_member"]) {
						met = MethodInfo::from_dict(method_dict);
						found_method = true;
					}
				}
				if (found_method) {
					if (!met.return_val.class_name.is_empty() && ClassDB::class_exists(met.return_val.class_name)) {
						r_classes.insert(met.return_val.class_name);
					}
					if (!met.return_val.hint_string.is_empty() && ClassDB::class_exists(met.return_val.hint_string)) {
						r_classes.insert(met.return_val.hint_string);
					}
				}
			}
		}

		current = tokenizer.scan();
	}

	// Convert HashSet to PackedStringArray
	for (const StringName& class_name : r_classes) {
		classes.push_back(class_name);
	}
	return classes;
}

Error ResourceFormatSaverRuzta::_save(const Ref<Resource>& p_resource, const String& p_path, uint32_t p_flags) {
	Ref<Ruzta> sqscr = p_resource;
	ERR_FAIL_COND_V(sqscr.is_null(), ERR_INVALID_PARAMETER);

	String source = sqscr->get_source_code();

	{
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
		Error err = FileAccess::get_open_error();

		ERR_FAIL_COND_V_MSG(err, err, "Cannot save Ruzta file '" + p_path + "'.");

		file->store_string(source);
		if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
			return ERR_CANT_CREATE;
		}
	}

	if (RuztaScriptServer::is_reload_scripts_on_save_enabled()) {
		RuztaLanguage::get_singleton()->_reload_tool_script(p_resource, true);
	}

	return OK;
}

PackedStringArray ResourceFormatSaverRuzta::_get_recognized_extensions(const Ref<Resource>& p_resource) const {
	PackedStringArray extensions;
	if (Object::cast_to<Ruzta>(*p_resource)) {
		extensions.push_back("rz");
	}
	return extensions;
}

bool ResourceFormatSaverRuzta::_recognize(const Ref<Resource>& p_resource) const {
	return Object::cast_to<Ruzta>(*p_resource) != nullptr;
}
