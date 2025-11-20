/**************************************************************************/
/*  ruzta_cache.h                                                      */
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

#pragma once

#include "ruzta.h"

#include <godot_cpp/classes/ref_counted.hpp> // original: core/object/ref_counted.h
// TODO: #include "core/os/safe_binary_mutex.h" // original: core/os/safe_binary_mutex.h
#include <godot_cpp/templates/hash_map.hpp> // original: core/templates/hash_map.h
#include <godot_cpp/templates/hash_set.hpp> // original: core/templates/hash_set.h

class RuztaAnalyzer;
class RuztaParser;

class RuztaParserRef : public RefCounted {
	GDSOFTCLASS(RuztaParserRef, RefCounted);

public:
	enum Status {
		EMPTY,
		PARSED,
		INHERITANCE_SOLVED,
		INTERFACE_SOLVED,
		FULLY_SOLVED,
	};

private:
	RuztaParser *parser = nullptr;
	RuztaAnalyzer *analyzer = nullptr;
	Status status = EMPTY;
	Error result = OK;
	String path;
	uint32_t source_hash = 0;
	bool clearing = false;
	bool abandoned = false;

	friend class RuztaCache;
	friend class Ruzta;

public:
	Status get_status() const;
	String get_path() const;
	uint32_t get_source_hash() const;
	RuztaParser *get_parser();
	RuztaAnalyzer *get_analyzer();
	Error raise_status(Status p_new_status);
	void clear();

	RuztaParserRef() {}
	~RuztaParserRef();
};

class RuztaCache {
	// String key is full path.
	HashMap<String, RuztaParserRef *> parser_map;
	HashMap<String, Vector<ObjectID>> abandoned_parser_map;
	HashMap<String, Ref<Ruzta>> shallow_ruzta_cache;
	HashMap<String, Ref<Ruzta>> full_ruzta_cache;
	HashMap<String, Ref<Ruzta>> static_ruzta_cache;
	HashMap<String, HashSet<String>> dependencies;
	HashMap<String, HashSet<String>> parser_inverse_dependencies;

	friend class Ruzta;
	friend class RuztaParserRef;
	friend class RuztaInstance;

	static RuztaCache *singleton;

	bool cleared = false;

public:
	static const int BINARY_MUTEX_TAG = 2;

private:
	static SafeBinaryMutex<BINARY_MUTEX_TAG> mutex;
	friend SafeBinaryMutex<BINARY_MUTEX_TAG> &_get_ruzta_cache_mutex();

public:
	static void move_script(const String &p_from, const String &p_to);
	static void remove_script(const String &p_path);
	static Ref<RuztaParserRef> get_parser(const String &p_path, RuztaParserRef::Status status, Error &r_error, const String &p_owner = String());
	static bool has_parser(const String &p_path);
	static void remove_parser(const String &p_path);
	static String get_source_code(const String &p_path);
	static Vector<uint8_t> get_binary_tokens(const String &p_path);
	static Ref<Ruzta> get_shallow_script(const String &p_path, Error &r_error, const String &p_owner = String());
	static Ref<Ruzta> get_full_script(const String &p_path, Error &r_error, const String &p_owner = String(), bool p_update_from_disk = false);
	static Ref<Ruzta> get_cached_script(const String &p_path);
	static Error finish_compiling(const String &p_owner);
	static void add_static_script(Ref<Ruzta> p_script);
	static void remove_static_script(const String &p_fqcn);

	static void clear();

	RuztaCache();
	~RuztaCache();
};
