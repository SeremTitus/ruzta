#pragma once

#include <godot_cpp/variant/variant.hpp>

using namespace godot;

class RuztaHelper {
public:
    typedef void (*ValidatedOperatorEvaluator)(const Variant *left, const Variant *right, Variant *r_ret);
    typedef void (*ValidatedSetter)(Variant *base, const Variant *value);
    typedef void (*ValidatedGetter)(const Variant *base, Variant *value);
    typedef void (*ValidatedKeyedSetter)(Variant *base, const Variant *key, const Variant *value, bool *valid);
    typedef void (*ValidatedKeyedGetter)(const Variant *base, const Variant *key, Variant *value, bool *valid);
    typedef void (*ValidatedIndexedSetter)(Variant *base, int64_t index, const Variant *value, bool *oob);
    typedef void (*ValidatedIndexedGetter)(const Variant *base, int64_t index, Variant *value, bool *oob);
    typedef void (*ValidatedBuiltInMethod)(Variant *base, const Variant **p_args, int p_argcount, Variant *r_ret);
    typedef void (*ValidatedConstructor)(Variant *r_base, const Variant **p_args);
    typedef void (*ValidatedUtilityFunction)(Variant *r_ret, const Variant **p_args, int p_argcount);
};
