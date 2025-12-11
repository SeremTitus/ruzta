#include "ruzta_script_server.h"

#include <godot_cpp/classes/config_file.hpp>	   // original:
#include <godot_cpp/classes/script_backtrace.hpp>  // original:
#include <godot_cpp/core/error_macros.hpp>		   // original:
#include <godot_cpp/core/mutex_lock.hpp>		   // original:
#include <godot_cpp/templates/hash_set.hpp>		   // original:

#include "ruzta_project_settings.h"

ScriptLanguage* RuztaScriptServer::_languages[MAX_LANGUAGES];
int RuztaScriptServer::_language_count = 0;
bool RuztaScriptServer::languages_ready = false;
Mutex RuztaScriptServer::languages_mutex;
thread_local bool RuztaScriptServer::thread_entered = false;

bool RuztaScriptServer::scripting_enabled = true;
bool RuztaScriptServer::reload_scripts_on_save = false;
ScriptEditRequestFunction RuztaScriptServer::edit_request_func = nullptr;

void RuztaScriptServer::set_scripting_enabled(bool p_enabled) {
	scripting_enabled = p_enabled;
}

bool RuztaScriptServer::is_scripting_enabled() {
	return scripting_enabled;
}

ScriptLanguage* RuztaScriptServer::get_language(int p_idx) {
	MutexLock lock(languages_mutex);
	ERR_FAIL_INDEX_V(p_idx, _language_count, nullptr);
	return _languages[p_idx];
}

ScriptLanguage* RuztaScriptServer::get_language_for_extension(const String& p_extension) {
	MutexLock lock(languages_mutex);

	// for (int i = 0; i < _language_count; i++) {
	// 	if (_languages[i] && _languages[i]->get_extension() == p_extension) {
	// 		return _languages[i];
	// 	}
	// }

	return nullptr;
}

Error RuztaScriptServer::register_language(ScriptLanguage* p_language) {
	MutexLock lock(languages_mutex);
	ERR_FAIL_NULL_V(p_language, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(_language_count >= MAX_LANGUAGES, ERR_UNAVAILABLE, "Script languages limit has been reach, cannot register more.");
	for (int i = 0; i < _language_count; i++) {
		const ScriptLanguage* other_language = _languages[i];
		// ERR_FAIL_COND_V_MSG(other_language->get_extension() == p_language->get_extension(), ERR_ALREADY_EXISTS, vformat("A script language with extension '%s' is already registered.", p_language->get_extension()));
		// ERR_FAIL_COND_V_MSG(other_language->get_name() == p_language->get_name(), ERR_ALREADY_EXISTS, vformat("A script language with name '%s' is already registered.", p_language->get_name()));
		// ERR_FAIL_COND_V_MSG(other_language->get_type() == p_language->get_type(), ERR_ALREADY_EXISTS, vformat("A script language with type '%s' is already registered.", p_language->get_type()));
	}
	_languages[_language_count++] = p_language;
	return OK;
}

Error RuztaScriptServer::unregister_language(const ScriptLanguage* p_language) {
	MutexLock lock(languages_mutex);

	for (int i = 0; i < _language_count; i++) {
		if (_languages[i] == p_language) {
			_language_count--;
			if (i < _language_count) {
				SWAP(_languages[i], _languages[_language_count]);
			}
			return OK;
		}
	}
	return ERR_DOES_NOT_EXIST;
}

void RuztaScriptServer::init_languages() {
	{  // Load global classes.
		global_classes_clear();
#ifndef DISABLE_DEPRECATED
		if (ProjectSettings::get_singleton()->has_setting("_global_script_classes")) {
			Array script_classes = GLOBAL_GET("_global_script_classes");

			for (const Variant& script_class : script_classes) {
				Dictionary c = script_class;
				if (!c.has("class") || !c.has("language") || !c.has("path") || !c.has("base") || !c.has("is_abstract") || !c.has("is_tool")) {
					continue;
				}
				add_global_class(c["class"], c["base"], c["language"], c["path"], c["is_abstract"], c["is_tool"]);
			}
			ProjectSettings::get_singleton()->clear("_global_script_classes");
		}
#endif

		Array script_classes = ProjectSettings::get_singleton()->get_global_class_list();
		for (const Variant& script_class : script_classes) {
			Dictionary c = script_class;
			if (!c.has("class") || !c.has("language") || !c.has("path") || !c.has("base") || !c.has("is_abstract") || !c.has("is_tool")) {
				continue;
			}
			add_global_class(c["class"], c["base"], c["language"], c["path"], c["is_abstract"], c["is_tool"]);
		}
	}

	HashSet<ScriptLanguage*> langs_to_init;
	{
		MutexLock lock(languages_mutex);
		for (int i = 0; i < _language_count; i++) {
			if (_languages[i]) {
				langs_to_init.insert(_languages[i]);
			}
		}
	}

	// for (ScriptLanguage *E : langs_to_init) {
	// 	E->init();
	// }

	{
		MutexLock lock(languages_mutex);
		languages_ready = true;
	}
}

void RuztaScriptServer::finish_languages() {
	HashSet<ScriptLanguage*> langs_to_finish;

	{
		MutexLock lock(languages_mutex);
		for (int i = 0; i < _language_count; i++) {
			if (_languages[i]) {
				langs_to_finish.insert(_languages[i]);
			}
		}
	}

	// for (ScriptLanguage *E : langs_to_finish) {
	// 	if (CoreBind::OS::get_singleton()) {
	// 		CoreBind::OS::get_singleton()->remove_script_loggers(E); // Unregister loggers using this script language.
	// 	}
	// 	E->finish();
	// }

	{
		MutexLock lock(languages_mutex);
		languages_ready = false;
	}

	global_classes_clear();
}

bool RuztaScriptServer::are_languages_initialized() {
	MutexLock lock(languages_mutex);
	return languages_ready;
}

bool RuztaScriptServer::thread_is_entered() {
	return thread_entered;
}

void RuztaScriptServer::set_reload_scripts_on_save(bool p_enable) {
	reload_scripts_on_save = p_enable;
}

bool RuztaScriptServer::is_reload_scripts_on_save_enabled() {
	return reload_scripts_on_save;
}

void RuztaScriptServer::thread_enter() {
	if (thread_entered) {
		return;
	}

	MutexLock lock(languages_mutex);
	if (!languages_ready) {
		return;
	}
	// for (int i = 0; i < _language_count; i++) {
	// 	_languages[i]->thread_enter();
	// }

	thread_entered = true;
}

void RuztaScriptServer::thread_exit() {
	if (!thread_entered) {
		return;
	}

	MutexLock lock(languages_mutex);
	if (!languages_ready) {
		return;
	}
	// for (int i = 0; i < _language_count; i++) {
	// 	_languages[i]->thread_exit();
	// }

	thread_entered = false;
}

HashMap<StringName, RuztaScriptServer::GlobalScriptClass> RuztaScriptServer::global_classes;
HashMap<StringName, Vector<StringName>> RuztaScriptServer::inheriters_cache;
bool RuztaScriptServer::inheriters_cache_dirty = true;

void RuztaScriptServer::global_classes_clear() {
	global_classes.clear();
	inheriters_cache.clear();
}

void RuztaScriptServer::add_global_class(const StringName& p_class, const StringName& p_base, const StringName& p_language, const String& p_path, bool p_is_abstract, bool p_is_tool) {
	ERR_FAIL_COND_MSG(p_class == p_base || (global_classes.has(p_base) && get_global_class_native_base(p_base) == p_class), "Cyclic inheritance in script class.");
	GlobalScriptClass* existing = global_classes.getptr(p_class);
	if (existing) {
		// Update an existing class (only set dirty if something changed).
		if (existing->base != p_base || existing->path != p_path || existing->language != p_language) {
			existing->base = p_base;
			existing->path = p_path;
			existing->language = p_language;
			existing->is_abstract = p_is_abstract;
			existing->is_tool = p_is_tool;
			inheriters_cache_dirty = true;
		}
	} else {
		// Add new class.
		GlobalScriptClass g;
		g.language = p_language;
		g.path = p_path;
		g.base = p_base;
		g.is_abstract = p_is_abstract;
		g.is_tool = p_is_tool;
		global_classes[p_class] = g;
		inheriters_cache_dirty = true;
	}
}

void RuztaScriptServer::remove_global_class(const StringName& p_class) {
	global_classes.erase(p_class);
	inheriters_cache_dirty = true;
}

template <typename L, typename R>
constexpr int64_t str_compare(const L* l_ptr, const R* r_ptr) {
	while (true) {
		const char32_t l = *l_ptr;
		const char32_t r = *r_ptr;

		if (l == 0 || l != r) {
			return static_cast<int64_t>(l) - static_cast<int64_t>(r);
		}

		l_ptr++;
		r_ptr++;
	}
}

struct AlphCompare {
	template <typename LT, typename RT>
	bool operator()(const LT& l, const RT& r) const {
		return compare(l, r);
	}
	static bool compare(const StringName& l, const StringName& r) {
		return str_compare(String(l).ptr(), String(r).ptr()) < 0;
	}
	static bool compare(const String& l, const StringName& r) {
		return str_compare(String(l).ptr(), String(r).ptr()) < 0;
	}
	static bool compare(const StringName& l, const String& r) {
		return str_compare(String(l).ptr(), String(r).ptr()) < 0;
	}
	static bool compare(const String& l, const String& r) {
		return str_compare(l.ptr(), r.ptr()) < 0;
	}
};

void RuztaScriptServer::get_inheriters_list(const StringName& p_base_type, List<StringName>* r_classes) {
	if (inheriters_cache_dirty) {
		inheriters_cache.clear();
		for (const KeyValue<StringName, GlobalScriptClass>& K : global_classes) {
			if (!inheriters_cache.has(K.value.base)) {
				inheriters_cache[K.value.base] = Vector<StringName>();
			}
			inheriters_cache[K.value.base].push_back(K.key);
		}
		for (KeyValue<StringName, Vector<StringName>>& K : inheriters_cache) {
			K.value.sort_custom<AlphCompare>();
		}
		inheriters_cache_dirty = false;
	}

	if (!inheriters_cache.has(p_base_type)) {
		return;
	}

	const Vector<StringName>& v = inheriters_cache[p_base_type];
	for (int i = 0; i < v.size(); i++) {
		r_classes->push_back(v[i]);
	}
}

void RuztaScriptServer::remove_global_class_by_path(const String& p_path) {
	for (const KeyValue<StringName, GlobalScriptClass>& kv : global_classes) {
		if (kv.value.path == p_path) {
			global_classes.erase(kv.key);
			inheriters_cache_dirty = true;
			return;
		}
	}
}

bool RuztaScriptServer::is_global_class(const StringName& p_class) {
	return global_classes.has(p_class);
}

StringName RuztaScriptServer::get_global_class_language(const StringName& p_class) {
	ERR_FAIL_COND_V(!global_classes.has(p_class), StringName());
	return global_classes[p_class].language;
}

String RuztaScriptServer::get_global_class_path(const String& p_class) {
	ERR_FAIL_COND_V(!global_classes.has(p_class), String());
	return global_classes[p_class].path;
}

StringName RuztaScriptServer::get_global_class_base(const String& p_class) {
	ERR_FAIL_COND_V(!global_classes.has(p_class), String());
	return global_classes[p_class].base;
}

StringName RuztaScriptServer::get_global_class_native_base(const String& p_class) {
	ERR_FAIL_COND_V(!global_classes.has(p_class), String());
	String base = global_classes[p_class].base;
	while (global_classes.has(base)) {
		base = global_classes[base].base;
	}
	return base;
}

bool RuztaScriptServer::is_global_class_abstract(const String& p_class) {
	ERR_FAIL_COND_V(!global_classes.has(p_class), false);
	return global_classes[p_class].is_abstract;
}

bool RuztaScriptServer::is_global_class_tool(const String& p_class) {
	ERR_FAIL_COND_V(!global_classes.has(p_class), false);
	return global_classes[p_class].is_tool;
}

// This function only sorts items added by this function.
// If `r_global_classes` is not empty before calling and a global sort is needed, caller must handle that separately.
void RuztaScriptServer::get_global_class_list(LocalVector<StringName>& r_global_classes) {
	if (global_classes.is_empty()) {
		return;
	}
	r_global_classes.reserve(r_global_classes.size() + global_classes.size());
	for (const KeyValue<StringName, GlobalScriptClass>& global_class : global_classes) {
		r_global_classes.push_back(global_class.key);
	}
	SortArray<StringName> sorter;
	sorter.sort(&r_global_classes[r_global_classes.size() - global_classes.size()], global_classes.size());
}

void RuztaScriptServer::save_global_classes() {
	Dictionary class_icons;

	Array script_classes = ProjectSettings::get_singleton()->get_global_class_list();
	for (const Variant& script_class : script_classes) {
		Dictionary d = script_class;
		if (!d.has("name") || !d.has("icon")) {
			continue;
		}
		class_icons[d["name"]] = d["icon"];
	}

	LocalVector<StringName> gc;
	get_global_class_list(gc);
	Array gcarr;
	for (const StringName& class_name : gc) {
		const GlobalScriptClass& global_class = global_classes[class_name];
		Dictionary d;
		d["class"] = class_name;
		d["language"] = global_class.language;
		d["path"] = global_class.path;
		d["base"] = global_class.base;
		d["icon"] = class_icons.get(class_name, "");
		d["is_abstract"] = global_class.is_abstract;
		d["is_tool"] = global_class.is_tool;
		gcarr.push_back(d);
	}

	Ref<ConfigFile> cf;
	cf.instantiate();
	cf->set_value("", "list", gcarr);
	cf->save(String("res://").path_join("global_ruzta_class_cache.cfg"));
}

Vector<Ref<ScriptBacktrace>> RuztaScriptServer::capture_script_backtraces(bool p_include_variables) {
	// if (is_program_exiting) {
	// 	return Vector<Ref<ScriptBacktrace>>();
	// }

	MutexLock lock(languages_mutex);
	if (!languages_ready) {
		return Vector<Ref<ScriptBacktrace>>();
	}

	Vector<Ref<ScriptBacktrace>> result;
	result.resize(_language_count);
	// for (int i = 0; i < _language_count; i++) {
	// 	result.write[i].instantiate(_languages[i], p_include_variables);
	// }

	return result;
}
