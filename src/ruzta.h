/**************************************************************************/
/*  ruzta.h                                                               */
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

#pragma once

#include <godot_cpp/classes/engine_debugger.hpp>  // original: core/debugger/engine_debugger.h

#include "ruzta_function.h"
// TODO: #include "core/debugger/script_debugger.h" // original: core/debugger/script_debugger.h
#include <godot_cpp/classes/mutex.hpp>						// original:
#include <godot_cpp/classes/resource_format_loader.hpp>		// original:
#include <godot_cpp/classes/resource_format_saver.hpp>		// original:
#include <godot_cpp/classes/resource_loader.hpp>			// original: core/io/resource_loader.h
#include <godot_cpp/classes/resource_saver.hpp>				// original: core/io/resource_saver.h
#include <godot_cpp/classes/script_extension.hpp>			// original:
#include <godot_cpp/classes/script_language_extension.hpp>	// original:
#include <godot_cpp/templates/hash_set.hpp>					// original:
#include <godot_cpp/templates/list.hpp>						// original: core/templates/list.h
#include <godot_cpp/templates/rb_set.hpp>					// original: core/templates/rb_set.h

#include "ruzta_variant/core_constants.h"  // original:
#include "ruzta_variant/doc_data.h"		   // original: doc_data.h

class RuztaNativeClass : public RefCounted {
	GDCLASS(RuztaNativeClass, RefCounted);

	StringName name;

   protected:
	bool _get(const StringName& p_name, Variant& r_ret) const;
	static void _bind_methods() { ClassDB::bind_method(D_METHOD("new"), &RuztaNativeClass::_new); }

   public:
	_FORCE_INLINE_ const StringName& get_name() const { return name; }
	Variant _new();
	Object* instantiate();
	Variant callp(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error);
	RuztaNativeClass(const StringName& p_name) { name = p_name; };
};

class Ruzta : public ScriptExtension {
	GDCLASS(Ruzta, ScriptExtension);
	bool tool = false;
	bool valid = false;
	bool reloading = false;
	bool is_abstract = false;

	struct MemberInfo {
		int index = 0;
		StringName setter;
		StringName getter;
		RuztaDataType data_type;
		PropertyInfo property_info;
	};

	struct ClearData {
		RBSet<RuztaFunction*> functions;
		RBSet<Ref<Script>> scripts;
		void clear() {
			functions.clear();
			scripts.clear();
		}
	};

	friend class RuztaInstance;
	friend class RuztaFunction;
	friend class RuztaAnalyzer;
	friend class RuztaCompiler;
	friend class RuztaDocGen;
	friend class RuztaLambdaCallable;
	friend class RuztaLambdaSelfCallable;
	friend class RuztaLanguage;
	friend struct RuztaUtilityFunctionsDefinitions;

	Ref<RuztaNativeClass> native;
	Ref<Ruzta> base;
	Ruzta* _owner = nullptr;  // for subclasses

	// Members are just indices to the instantiated script.
	HashMap<StringName, MemberInfo> member_indices;	 // Includes member info of all base Ruzta classes.
	HashSet<StringName> members;					 // Only members of the current class.

	// Only static variables of the current class.
	HashMap<StringName, MemberInfo> static_variables_indices;
	Vector<Variant> static_variables;  // Static variable values.

	HashMap<StringName, Variant> constants;
	HashMap<StringName, RuztaFunction*> member_functions;
	HashMap<StringName, Ref<Ruzta>> subclasses;
	HashMap<StringName, MethodInfo> _signals;
	Dictionary rpc_config;

   public:
	struct LambdaInfo {
		int capture_count;
		bool use_self;
	};

   private:
	HashMap<RuztaFunction*, LambdaInfo> lambda_info;

   public:
	class UpdatableFuncPtr {
		friend class Ruzta;

		RuztaFunction* ptr = nullptr;
		Ruzta* script = nullptr;
		List<UpdatableFuncPtr*>::Element* list_element = nullptr;

	   public:
		RuztaFunction* operator->() const { return ptr; }
		operator RuztaFunction*() const { return ptr; }

		UpdatableFuncPtr(RuztaFunction* p_function);
		~UpdatableFuncPtr();
	};

   private:
	// List is used here because a ptr to elements are stored, so the memory locations need to be stable
	List<UpdatableFuncPtr*> func_ptrs_to_update;
	Mutex func_ptrs_to_update_mutex;

	void _recurse_replace_function_ptrs(const HashMap<RuztaFunction*, RuztaFunction*>& p_replacements) const;

#ifdef TOOLS_ENABLED
	// For static data storage during hot-reloading.
	HashMap<StringName, MemberInfo> old_static_variables_indices;
	Vector<Variant> old_static_variables;
	void _save_old_static_data();
	void _restore_old_static_data();

	HashMap<StringName, int> member_lines;
	HashMap<StringName, Variant> member_default_values;
	List<PropertyInfo> members_cache;
	HashMap<StringName, Variant> member_default_values_cache;
	Ref<Ruzta> base_cache;
	HashSet<ObjectID> inheriters_cache;
	bool source_changed_cache = false;
	bool placeholder_fallback_enabled = false;
	void _update_exports_values(HashMap<StringName, Variant>& values, List<PropertyInfo>& propnames);

	StringName doc_class_name;
	RuztaDocData::ClassDoc doc;
	Vector<RuztaDocData::ClassDoc> docs;
	void _add_doc(const RuztaDocData::ClassDoc& p_doc);
	void _clear_doc();
#endif

	RuztaFunction* initializer = nullptr;  // Direct pointer to `new()`/`_init()` member function, faster to locate.

	RuztaFunction* implicit_initializer = nullptr;	// `@implicit_new()` special function.
	RuztaFunction* implicit_ready = nullptr;		// `@implicit_ready()` special function.
	RuztaFunction* static_initializer = nullptr;	// `@static_initializer()` special function.

	Error _static_init();
	void _static_default_init();  // Initialize static variables with default values based on their types.

	RBSet<Object*> instances;
	bool destructing = false;
	bool clearing = false;
	// exported members
	String source;
	Vector<uint8_t> binary_tokens;
	String path;
	bool path_valid = false;  // False if using default path.
	StringName local_name;	  // Inner class identifier or `class_name`.
	StringName global_name;	  // `class_name`.
	String fully_qualified_name;
	String simplified_icon_path;
	SelfList<Ruzta> script_list;

	SelfList<RuztaFunctionState>::List pending_func_states;

	RuztaFunction* _super_constructor(Ruzta* p_script);
	void _super_implicit_constructor(Ruzta* p_script, RuztaInstance* p_instance, GDExtensionCallError& r_error);
	RuztaInstance* _create_instance(const Variant** p_args, int p_argcount, Object* p_owner, GDExtensionCallError& r_error);

	String _get_debug_path() const;

#ifdef TOOLS_ENABLED
	HashSet<PlaceHolderScriptInstance*> placeholders;
	void _update_exports_down(bool p_base_exports_changed);
#endif

#ifdef DEBUG_ENABLED
	HashMap<ObjectID, List<Pair<StringName, Variant>>> pending_reload_state;
#endif

	bool _update_exports(bool* r_err, bool p_recursive_call = false, PlaceHolderScriptInstance* p_instance_to_update = nullptr, bool p_base_exports_changed = false);

	void _save_orphaned_subclasses(Ruzta::ClearData* p_clear_data);

	Ruzta* _get_ruzta_from_variant(const Variant& p_variant);
	void _collect_function_dependencies(RuztaFunction* p_func, RBSet<Ruzta*>& p_dependencies, const Ruzta* p_except);
	void _collect_dependencies(RBSet<Ruzta*>& p_dependencies, const Ruzta* p_except);

   protected:
	bool _get(const StringName& p_name, Variant& r_ret) const;
	bool _set(const StringName& p_name, const Variant& p_value);
	void _get_property_list(List<PropertyInfo>* p_properties) const;

	Variant callp(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error);

	static void _bind_methods() { ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "new", &Ruzta::_new, MethodInfo("new")); }

   public:
	Variant _new(const Variant** p_args, int p_argcount, GDExtensionCallError& r_error);
#ifdef TOOL_ENABLED
	virtual bool _editor_can_reload_from_file() override { return true; }
	virtual void _placeholder_erased(void* p_placeholder) override { placeholders.erase(p_placeholder); }
#endif
	virtual bool _can_instantiate() const override { return valid; }
	virtual Ref<Script> _get_base_script() const override { return base; }
	virtual StringName _get_global_name() const override { return global_name; }
	bool _inherits_script(const Ref<Script>& p_script) const override;
	virtual StringName _get_instance_base_type() const override;  // this may not work in all scripts, will return empty if so
	virtual void* _instance_create(Object* p_this) const override;
	virtual void* _placeholder_instance_create(Object* p_this) const override;
	virtual bool _instance_has(Object* p_this) const override;
	virtual bool _has_source_code() const override { return !source.is_empty(); }
	virtual String _get_source_code() const override { return source; }
	virtual void _set_source_code(const String& p_code) override;
	virtual Error _reload(bool p_keep_state = false) override;
#ifdef TOOLS_ENABLED
	virtual StringName _get_doc_class_name() const override { return doc_class_name; }
	virtual godot::TypedArray<Dictionary> _get_documentation() const override;
	virtual String _get_class_icon_path() const override { return simplified_icon_path; }
#endif
	virtual bool _has_method(const StringName& p_method) const override { return member_functions.has(p_method); }
	virtual bool _has_static_method(const StringName& p_method) const override;
	virtual Variant _get_script_method_argument_count(const StringName& p_method) const override;
	virtual Dictionary _get_method_info(const StringName& p_method) const override;
	virtual bool _is_tool() const override { return tool; }
	virtual bool _is_valid() const override { return valid; }
	virtual bool _is_abstract() const override { return is_abstract; }
	virtual ScriptLanguage* _get_language() const override { return RuztaLanguage::get_singleton(); }
	virtual bool _has_script_signal(const StringName& p_signal) const override;
	virtual godot::TypedArray<Dictionary> _get_script_signal_list() const override;
	virtual bool _has_property_default_value(const StringName& p_property) const override;
	virtual Variant _get_property_default_value(const StringName& p_property) const override;
	virtual void _update_exports() override;
	virtual godot::TypedArray<Dictionary> _get_script_method_list() const override;
	virtual godot::TypedArray<Dictionary> _get_script_property_list() const override;

	virtual int _get_member_line(const StringName& p_member) const override {
#ifdef TOOLS_ENABLED
		if (member_lines.has(p_member)) {
			return member_lines[p_member];
		}
#endif
		return -1;
	}

	virtual Dictionary _get_constants() const override;
	virtual godot::TypedArray<StringName> _get_members() const override;
#ifdef TOOLS_ENABLED
	bool is_placeholder_fallback_enabled() const { return placeholder_fallback_enabled; }
#endif
	virtual Variant _get_rpc_config() const override { return rpc_config; }

#ifdef DEBUG_ENABLED
	static String debug_get_script_name(const Ref<Script>& p_script);
#endif

	static String canonicalize_path(const String& p_path);
	_FORCE_INLINE_ static bool is_canonically_equal_paths(const String& p_path_a, const String& p_path_b) {
		return canonicalize_path(p_path_a) == canonicalize_path(p_path_b);
	}

	_FORCE_INLINE_ StringName get_local_name() const { return local_name; }

	void clear(Ruzta::ClearData* p_clear_data = nullptr);

	// Cancels all functions of the script that are are waiting to be resumed after using await.
	void cancel_pending_functions(bool warn);

	Ruzta* find_class(const String& p_qualified_name);
	bool has_class(const Ruzta* p_script);
	Ruzta* get_root_script();
	bool is_root_script() const { return _owner == nullptr; }
	String get_fully_qualified_name() const { return fully_qualified_name; }
	const HashMap<StringName, Ref<Ruzta>>& get_subclasses() const { return subclasses; }
	const HashMap<StringName, Variant>& get_constants() const { return constants; }
	const HashSet<StringName>& get_members() const { return members; }
	const RuztaDataType& get_member_type(const StringName& p_member) const {
		CRASH_COND(!member_indices.has(p_member));
		return member_indices[p_member].data_type;
	}
	const Ref<RuztaNativeClass>& get_native() const { return native; }

	_FORCE_INLINE_ const HashMap<StringName, RuztaFunction*>& get_member_functions() const { return member_functions; }
	_FORCE_INLINE_ const HashMap<RuztaFunction*, LambdaInfo>& get_lambda_info() const { return lambda_info; }

	_FORCE_INLINE_ const RuztaFunction* get_implicit_initializer() const { return implicit_initializer; }
	_FORCE_INLINE_ const RuztaFunction* get_implicit_ready() const { return implicit_ready; }
	_FORCE_INLINE_ const RuztaFunction* get_static_initializer() const { return static_initializer; }

	RBSet<Ruzta*> get_dependencies();
	HashMap<Ruzta*, RBSet<Ruzta*>> get_all_dependencies();
	RBSet<Ruzta*> get_must_clear_dependencies();

	Ref<Ruzta> get_base() const { return base; }

	const HashMap<StringName, MemberInfo>& debug_get_member_indices() const { return member_indices; }
	const HashMap<StringName, RuztaFunction*>& debug_get_member_functions() const { return member_functions; }	// this is debug only
	StringName debug_get_member_by_index(int p_idx) const;
	StringName debug_get_static_var_by_index(int p_idx) const;

	void set_path(const String& p_path, bool p_take_over = false);
	String get_script_path() const;
	Error load_source_code(const String& p_path);

	void set_binary_tokens_source(const Vector<uint8_t>& p_binary_tokens) { binary_tokens = p_binary_tokens; }
	const Vector<uint8_t>& get_binary_tokens_source() const { return binary_tokens; }
	Vector<uint8_t> get_as_binary_tokens() const;

	void unload_static() const { RuztaCache::remove_script(fully_qualified_name); }

	Ruzta();
	~Ruzta();
};

class ScriptInstance {
   public:
	virtual bool set(const StringName& p_name, const Variant& p_value) = 0;
	virtual bool get(const StringName& p_name, Variant& r_ret) const = 0;
	virtual void get_property_list(List<PropertyInfo>* p_properties) const = 0;
	virtual Variant::Type get_property_type(const StringName& p_name, bool* r_is_valid = nullptr) const = 0;
	virtual void validate_property(PropertyInfo& p_property) const = 0;

	virtual bool property_can_revert(const StringName& p_name) const = 0;
	virtual bool property_get_revert(const StringName& p_name, Variant& r_ret) const = 0;

	virtual Object* get_owner() { return nullptr; }
	virtual void get_property_state(List<Pair<StringName, Variant>>& state);

	virtual void get_method_list(List<MethodInfo>* p_list) const = 0;
	virtual bool has_method(const StringName& p_method) const = 0;

	virtual int get_method_argument_count(const StringName& p_method, bool* r_is_valid = nullptr) const;

	virtual Variant callp(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) = 0;

	template <typename... VarArgs>
	Variant call(const StringName& p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = {p_args..., Variant()};  // +1 makes sure zero sized arrays are also supported.
		const Variant* argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		GDExtensionCallError cerr;
		return callp(p_method, sizeof...(p_args) == 0 ? nullptr : (const Variant**)argptrs, sizeof...(p_args), cerr);
	}

	virtual Variant call_const(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error);	// implement if language supports const functions
	virtual void notification(int p_notification, bool p_reversed = false) = 0;
	virtual String to_string(bool* r_valid) {
		if (r_valid) {
			*r_valid = false;
		}
		return String();
	}

	// this is used by script languages that keep a reference counter of their own
	// you can make Ref<> not die when it reaches zero, so deleting the reference
	// depends entirely from the script

	virtual void refcount_incremented() {}
	virtual bool refcount_decremented() { return true; }  // return true if it can die

	virtual Ref<Script> get_script() const = 0;

	virtual bool is_placeholder() const { return false; }

	virtual void property_set_fallback(const StringName& p_name, const Variant& p_value, bool* r_valid);
	virtual Variant property_get_fallback(const StringName& p_name, bool* r_valid);

	virtual const Variant get_rpc_config() const;

	virtual ScriptLanguage* get_language() = 0;
	virtual ~ScriptInstance() {}
};

class PlaceHolderScriptInstance : public ScriptInstance {
	Object* owner = nullptr;
	List<PropertyInfo> properties;
	HashMap<StringName, Variant> values;
	HashMap<StringName, Variant> constants;
	ScriptLanguage* language = nullptr;
	Ref<Script> script;

   public:
	virtual bool set(const StringName& p_name, const Variant& p_value) override;
	virtual bool get(const StringName& p_name, Variant& r_ret) const override;
	virtual void get_property_list(List<PropertyInfo>* p_properties) const override;
	virtual Variant::Type get_property_type(const StringName& p_name, bool* r_is_valid = nullptr) const override;
	virtual void validate_property(PropertyInfo& p_property) const override {}

	virtual bool property_can_revert(const StringName& p_name) const override { return false; }
	virtual bool property_get_revert(const StringName& p_name, Variant& r_ret) const override { return false; }

	virtual void get_method_list(List<MethodInfo>* p_list) const override;
	virtual bool has_method(const StringName& p_method) const override;

	virtual int get_method_argument_count(const StringName& p_method, bool* r_is_valid = nullptr) const override {
		if (r_is_valid) {
			*r_is_valid = false;
		}
		return 0;
	}

	virtual Variant callp(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error) override;
	virtual void notification(int p_notification, bool p_reversed = false) override {}

	virtual Ref<Script> get_script() const override { return script; }

	virtual ScriptLanguage* get_language() override { return language; }

	Object* get_owner() override { return owner; }

	void update(const List<PropertyInfo>& p_properties, const HashMap<StringName, Variant>& p_values);	// likely changed in editor

	virtual bool is_placeholder() const override { return true; }

	virtual void property_set_fallback(const StringName& p_name, const Variant& p_value, bool* r_valid = nullptr) override;
	virtual Variant property_get_fallback(const StringName& p_name, bool* r_valid = nullptr) override;

	virtual const Variant get_rpc_config() const override { return Variant(); }

	PlaceHolderScriptInstance(ScriptLanguage* p_language, Ref<Script> p_script, Object* p_owner);
	~PlaceHolderScriptInstance();
};

class RuztaInstance : public ScriptInstance {
	friend class Ruzta;
	friend class RuztaFunction;
	friend class RuztaLambdaCallable;
	friend class RuztaLambdaSelfCallable;
	friend class RuztaCompiler;
	friend class RuztaCache;
	friend struct RuztaUtilityFunctionsDefinitions;

	ObjectID owner_id;
	Object* owner = nullptr;
	Ref<Ruzta> script;
#ifdef DEBUG_ENABLED
	HashMap<StringName, int> member_indices_cache;	// used only for hot script reloading
#endif
	Vector<Variant> members;

	SelfList<RuztaFunctionState>::List pending_func_states;

	void _call_implicit_ready_recursively(Ruzta* p_script);

   public:
	virtual Object* get_owner() { return owner; }

	virtual bool set(const StringName& p_name, const Variant& p_value);
	virtual bool get(const StringName& p_name, Variant& r_ret) const;
	virtual void get_property_list(List<PropertyInfo>* p_properties) const;
	virtual Variant::Type get_property_type(const StringName& p_name, bool* r_is_valid = nullptr) const;
	virtual void validate_property(PropertyInfo& p_property) const;

	virtual bool property_can_revert(const StringName& p_name) const;
	virtual bool property_get_revert(const StringName& p_name, Variant& r_ret) const;

	virtual void get_method_list(List<MethodInfo>* p_list) const;
	virtual bool has_method(const StringName& p_method) const;

	virtual int get_method_argument_count(const StringName& p_method, bool* r_is_valid = nullptr) const;

	virtual Variant callp(const StringName& p_method, const Variant** p_args, int p_argcount, GDExtensionCallError& r_error);

	Variant debug_get_member_by_index(int p_idx) const { return members[p_idx]; }

	virtual void notification(int p_notification, bool p_reversed = false);
	String to_string(bool* r_valid);

	virtual Ref<Script> get_script() const;

	virtual ScriptLanguage* get_language();

	void set_path(const String& p_path);

	void reload_members();

	virtual const Variant get_rpc_config() const;

	RuztaInstance() {}
	~RuztaInstance();
};

class RuztaLanguage : public ScriptLanguageExtension {
	friend class RuztaFunctionState;

	static RuztaLanguage* singleton;

	bool finishing = false;

	Variant* _global_array = nullptr;
	Vector<Variant> global_array;
	HashMap<StringName, int> globals;
	HashMap<StringName, Variant> named_globals;
	Vector<int> global_array_empty_indexes;

	struct StackInfo {
		String file;
		String func;
		int line;
	};

	struct CallLevel {
		Variant* stack = nullptr;
		RuztaFunction* function = nullptr;
		RuztaInstance* instance = nullptr;
		int* ip = nullptr;
		int* line = nullptr;
		CallLevel* prev = nullptr;	// Reverse linked list (stack).
	};

	static thread_local int _debug_parse_err_line;
	static thread_local String _debug_parse_err_file;
	static thread_local String _debug_error;

	static thread_local CallLevel* _call_stack;
	static thread_local uint32_t _call_stack_size;
	uint32_t _debug_max_call_stack = 0;

	bool track_call_stack = false;
	bool track_locals = false;

	static CallLevel* _get_stack_level(uint32_t p_level);

	void _add_global(const StringName& p_name, const Variant& p_value);
	void _remove_global(const StringName& p_name);

	friend class RuztaInstance;

	Mutex mutex;

	friend class Ruzta;

	SelfList<Ruzta>::List script_list;
	friend class RuztaFunction;

	SelfList<RuztaFunction>::List function_list;
#ifdef DEBUG_ENABLED
	bool profiling;
	bool profile_native_calls;
	uint64_t script_frame_time;
#endif

	HashMap<String, ObjectID> orphan_subclasses;

#ifdef TOOLS_ENABLED
	// void _extension_loaded(const Ref<GDExtension> &p_extension);
	// void _extension_unloading(const Ref<GDExtension> &p_extension);
#endif

   public:
	bool debug_break(const String& p_error, bool p_allow_continue = true);
	bool debug_break_parse(const String& p_file, int p_line, const String& p_error);

	_FORCE_INLINE_ void enter_function(CallLevel* call_level, RuztaInstance* p_instance, RuztaFunction* p_function, Variant* p_stack, int* p_ip, int* p_line) {
		if (!track_call_stack) {
			return;
		}

#ifdef DEBUG_ENABLED
		EngineDebugger* script_debugger = EngineDebugger::get_singleton();
		if (script_debugger != nullptr && script_debugger->get_lines_left() > 0 && script_debugger->get_depth() >= 0) {
			script_debugger->set_depth(script_debugger->get_depth() + 1);
		}
#endif

		if (unlikely(_call_stack_size >= _debug_max_call_stack)) {
			_debug_error = vformat("Stack overflow (stack size: %s). Check for infinite recursion in your script.", _debug_max_call_stack);

#ifdef DEBUG_ENABLED
			if (script_debugger != nullptr) {
				script_debugger->debug(this);
			}
#endif

			return;
		}

		call_level->prev = _call_stack;
		_call_stack = call_level;
		call_level->stack = p_stack;
		call_level->instance = p_instance;
		call_level->function = p_function;
		call_level->ip = p_ip;
		call_level->line = p_line;
		_call_stack_size++;
	}

	_FORCE_INLINE_ void exit_function() {
		if (!track_call_stack) {
			return;
		}

#ifdef DEBUG_ENABLED
		EngineDebugger* script_debugger = EngineDebugger::get_singleton();
		if (script_debugger && script_debugger->get_lines_left() > 0 && script_debugger->get_depth() >= 0) {
			script_debugger->set_depth(script_debugger->get_depth() - 1);
		}
#endif

		if (unlikely(_call_stack_size == 0)) {
#ifdef DEBUG_ENABLED
			if (script_debugger) {
				_debug_error = "Stack Underflow (Engine Bug)";
				script_debugger->debug(this);
			} else {
				ERR_PRINT("Stack underflow! (Engine Bug)");
			}
#else  // !DEBUG_ENABLED
			ERR_PRINT("Stack underflow! (Engine Bug)");
#endif
			return;
		}

		_call_stack_size--;
		_call_stack = _call_stack->prev;
	}

	virtual TypedArray<Dictionary> _debug_get_current_stack_info() override {
		TypedArray<Dictionary> csi;
		csi.resize(_call_stack_size);
		CallLevel* cl = _call_stack;
		uint32_t idx = 0;
		while (cl) {
			Dictionary d;
			d["line"] = *cl->line;
			if (cl->function) {
				d["func"] = cl->function->get_name();
				d["file"] = cl->function->get_script()->get_script_path();
			}
			csi[idx] = d;
			idx++;
			cl = cl->prev;
		}
		return csi;
	}

	struct {
		StringName _init;
		StringName _static_init;
		StringName _notification;
		StringName _set;
		StringName _get;
		StringName _get_property_list;
		StringName _validate_property;
		StringName _property_can_revert;
		StringName _property_get_revert;
		StringName _script_source;

	} strings;

	_FORCE_INLINE_ bool should_track_call_stack() const { return track_call_stack; }
	_FORCE_INLINE_ bool should_track_locals() const { return track_locals; }
	_FORCE_INLINE_ int get_global_array_size() const { return global_array.size(); }
	_FORCE_INLINE_ Variant* get_global_array() { return _global_array; }
	_FORCE_INLINE_ const HashMap<StringName, int>& get_global_map() const { return globals; }
	_FORCE_INLINE_ const HashMap<StringName, Variant>& get_named_globals_map() const { return named_globals; }
	// These two functions should be used when behavior needs to be consistent between in-editor and running the scene
	bool has_any_global_constant(const StringName& p_name) { return named_globals.has(p_name) || globals.has(p_name); }
	Variant get_any_global_constant(const StringName& p_name);

	_FORCE_INLINE_ static RuztaLanguage* get_singleton() { return singleton; }

	virtual String _get_name() const override;

	/* LANGUAGE FUNCTIONS */
	virtual void _init() override;
	virtual String _get_type() const override;
	virtual String _get_extension() const override;
	virtual void _finish() override;

	/* EDITOR FUNCTIONS */
	virtual PackedStringArray _get_reserved_words() const override;
	virtual bool _is_control_flow_keyword(const String& p_keywords) const override;
	virtual PackedStringArray _get_comment_delimiters() const override;
	virtual PackedStringArray _get_doc_comment_delimiters() const override;
	virtual PackedStringArray _get_string_delimiters() const override;
	virtual bool _is_using_templates() override;
	virtual Ref<Script> _make_template(const String& p_template, const String& p_class_name, const String& p_base_class_name) const override;
	virtual TypedArray<Dictionary> _get_built_in_templates(const StringName& p_object) const override;
	virtual Dictionary _validate(const String& p_script, const String& p_path, bool p_validate_functions, bool p_validate_errors, bool p_validate_warnings, bool p_validate_safe_lines) const override;
	virtual String _validate_path(const String& p_path) const override;
	virtual Object* _create_script() const override;
	virtual bool _has_named_classes() const override;
	virtual bool _supports_builtin_mode() const override;
	virtual bool _supports_documentation() const override;
	virtual bool _can_inherit_from_file() const override { return true; }
	virtual int32_t _find_function(const String& p_function, const String& p_code) const override;
	virtual String _make_function(const String& p_class_name, const String& p_function_name, const PackedStringArray& p_function_args) const override;
	virtual bool _can_make_function() const override;
	virtual Dictionary _complete_code(const String& p_code, const String& p_path, Object* p_owner) const override;
#ifdef TOOLS_ENABLED
	virtual Dictionary _lookup_code(const String& p_code, const String& p_symbol, const String& p_path, Object* p_owner) const override;
#endif
	virtual Error _open_in_external_editor(const Ref<Script>& p_script, int32_t p_line, int32_t p_column) override;
	virtual bool _overrides_external_editor() override;
	virtual ScriptLanguage::ScriptNameCasing _preferred_file_name_casing() const override;
	virtual String _get_indentation() const;
	virtual String _auto_indent_code(const String& p_code, int32_t p_from_line, int32_t p_to_line) const override;
	virtual void _add_global_constant(const StringName& p_variable, const Variant& p_value) override;
	virtual void _add_named_global_constant(const StringName& p_name, const Variant& p_value) override;
	virtual void _remove_named_global_constant(const StringName& p_name) override;
	virtual void _thread_enter() override;
	virtual void _thread_exit() override;

	/* DEBUGGER FUNCTIONS */

	virtual String _debug_get_error() const override;
	virtual int32_t _debug_get_stack_level_count() const override;
	virtual int32_t _debug_get_stack_level_line(int32_t p_level) const override;
	virtual String _debug_get_stack_level_function(int32_t p_level) const override;
	virtual String _debug_get_stack_level_source(int32_t p_level) const override;
	virtual Dictionary _debug_get_stack_level_locals(int32_t p_level, int32_t p_max_subitems, int32_t p_max_depth) override;
	virtual Dictionary _debug_get_stack_level_members(int32_t p_level, int32_t p_max_subitems, int32_t p_max_depth) override;
	virtual void* _debug_get_stack_level_instance(int32_t p_level) override;
	virtual Dictionary _debug_get_globals(int32_t p_max_subitems, int32_t p_max_depth) override;
	virtual String _debug_parse_stack_level_expression(int32_t p_level, const String& p_expression, int32_t p_max_subitems, int32_t p_max_depth) override;

	virtual void _reload_all_scripts() override;
	virtual void _reload_scripts(const Array& p_scripts, bool p_soft_reload) override;
	virtual void _reload_tool_script(const Ref<Script>& p_script, bool p_soft_reload) override;

	virtual void _frame() override;

	virtual TypedArray<Dictionary> _get_public_functions() const override;
	virtual Dictionary _get_public_constants() const override;
	virtual TypedArray<Dictionary> _get_public_annotations() const override;

	virtual void _profiling_start() override;
	virtual void _profiling_stop() override;
	virtual void _profiling_set_save_native_calls(bool p_enable) override;
	void profiling_collate_native_call_data(bool p_accumulated);

	virtual int32_t _profiling_get_accumulated_data(ScriptLanguageExtensionProfilingInfo* p_info_arr, int32_t p_info_max) override;
	virtual int32_t _profiling_get_frame_data(ScriptLanguageExtensionProfilingInfo* p_info_arr, int32_t p_info_max) override;

	/* LOADER FUNCTIONS */

	virtual PackedStringArray _get_recognized_extensions() const override;

	/* GLOBAL CLASSES */

	virtual bool _handles_global_class_type(const String& p_type) const override;
	virtual Dictionary _get_global_class_name(const String& p_path) const override;

	void add_orphan_subclass(const String& p_qualified_name, const ObjectID& p_subclass);
	Ref<Ruzta> get_orphan_subclass(const String& p_qualified_name);

	Ref<Ruzta> get_script_by_fully_qualified_name(const String& p_name);

	RuztaLanguage();
	~RuztaLanguage();
};

class ResourceFormatLoaderRuzta : public ResourceFormatLoader {
	GDCLASS(ResourceFormatLoaderRuzta, ResourceFormatLoader);

   protected:
	static void _bind_methods() {}

   public:
	virtual PackedStringArray _get_recognized_extensions() const override;
	virtual bool _handles_type(const StringName& p_type) const override;
	virtual String _get_resource_type(const String& p_path) const override;
	virtual PackedStringArray _get_classes_used(const String& p_path) const override;
	virtual Variant _load(const String& p_path, const String& p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const override;
	virtual PackedStringArray _get_dependencies(const String& p_path, bool p_add_types) const override;
};

class ResourceFormatSaverRuzta : public ResourceFormatSaver {
	GDCLASS(ResourceFormatSaverRuzta, ResourceFormatSaver);

   protected:
	static void _bind_methods() {}

   public:
	virtual Error _save(const Ref<Resource>& p_resource, const String& p_path, uint32_t p_flags) override;
	virtual PackedStringArray _get_recognized_extensions(const Ref<Resource>& p_resource) const override;
	virtual bool _recognize(const Ref<Resource>& p_resource) const override;
};
