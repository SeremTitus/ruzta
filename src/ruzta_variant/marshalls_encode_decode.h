/**************************************************************************/
/*  marshalls_encode_decode.h                                                           */
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

#pragma once

#include <godot_cpp/variant/utility_functions.hpp> // original: core/math/math_defs.h
#include <godot_cpp/classes/ref_counted.hpp> // original: core/object/ref_counted.h
#include <godot_cpp/core/defs.hpp> // original: core/typedefs.h
#include <godot_cpp/variant/variant.hpp> // original: core/variant/variant.h

// uintr_t is only for pairing with real_t, and we only need it in here.
#ifdef REAL_T_IS_DOUBLE
typedef uint64_t uintr_t;
#else
typedef uint32_t uintr_t;
#endif

/**
 * Miscellaneous helpers for marshaling data types, and encoding
 * in an endian independent way
 */

union MarshallFloat {
	uint32_t i; ///< int
	float f; ///< float
};

union MarshallDouble {
	uint64_t l; ///< long long
	double d; ///< double
};

// Behaves like one of the above, depending on compilation setting.
union MarshallReal {
	uintr_t i;
	real_t r;
};

static inline unsigned int encode_uint16(uint16_t p_uint, uint8_t *p_arr) {
	for (int i = 0; i < 2; i++) {
		*p_arr = p_uint & 0xFF;
		p_arr++;
		p_uint >>= 8;
	}

	return sizeof(uint16_t);
}

static inline unsigned int encode_uint32(uint32_t p_uint, uint8_t *p_arr) {
	for (int i = 0; i < 4; i++) {
		*p_arr = p_uint & 0xFF;
		p_arr++;
		p_uint >>= 8;
	}

	return sizeof(uint32_t);
}

uint16_t make_half_float(float p_value) {
	union {
		float fv;
		uint32_t ui;
	} ci;
	ci.fv = p_value;

	uint32_t x = ci.ui;
	uint32_t sign = (unsigned short)(x >> 31);
	uint32_t mantissa;
	uint32_t exponent;
	uint16_t hf;

	// get mantissa
	mantissa = x & ((1 << 23) - 1);
	// get exponent bits
	exponent = x & (0xFF << 23);
	if (exponent >= 0x47800000) {
		// check if the original single precision float number is a NaN
		if (mantissa && (exponent == (0xFF << 23))) {
			// we have a single precision NaN
			mantissa = (1 << 23) - 1;
		} else {
			// 16-bit half-float representation stores number as Inf
			mantissa = 0;
		}
		hf = (((uint16_t)sign) << 15) | (uint16_t)((0x1F << 10)) |
				(uint16_t)(mantissa >> 13);
	}
	// check if exponent is <= -15
	else if (exponent <= 0x38000000) {
		/*
		// store a denorm half-float value or zero
		exponent = (0x38000000 - exponent) >> 23;
		mantissa >>= (14 + exponent);

		hf = (((uint16_t)sign) << 15) | (uint16_t)(mantissa);
		*/
		hf = 0; //denormals do not work for 3D, convert to zero
	} else {
		hf = (((uint16_t)sign) << 15) |
				(uint16_t)((exponent - 0x38000000) >> 13) |
				(uint16_t)(mantissa >> 13);
	}

	return hf;
}

static inline unsigned int encode_half(float p_float, uint8_t *p_arr) {
	encode_uint16(make_half_float(p_float), p_arr);

	return sizeof(uint16_t);
}

static inline unsigned int encode_float(float p_float, uint8_t *p_arr) {
	MarshallFloat mf;
	mf.f = p_float;
	encode_uint32(mf.i, p_arr);

	return sizeof(uint32_t);
}

static inline unsigned int encode_uint64(uint64_t p_uint, uint8_t *p_arr) {
	for (int i = 0; i < 8; i++) {
		*p_arr = p_uint & 0xFF;
		p_arr++;
		p_uint >>= 8;
	}

	return sizeof(uint64_t);
}

static inline unsigned int encode_double(double p_double, uint8_t *p_arr) {
	MarshallDouble md;
	md.d = p_double;
	encode_uint64(md.l, p_arr);

	return sizeof(uint64_t);
}

static inline unsigned int encode_uintr(uintr_t p_uint, uint8_t *p_arr) {
	for (size_t i = 0; i < sizeof(uintr_t); i++) {
		*p_arr = p_uint & 0xFF;
		p_arr++;
		p_uint >>= 8;
	}

	return sizeof(uintr_t);
}

static inline unsigned int encode_real(real_t p_real, uint8_t *p_arr) {
	MarshallReal mr;
	mr.r = p_real;
	encode_uintr(mr.i, p_arr);

	return sizeof(uintr_t);
}

static inline int encode_cstring(const char *p_string, uint8_t *p_data) {
	int len = 0;

	while (*p_string) {
		if (p_data) {
			*p_data = (uint8_t)*p_string;
			p_data++;
		}
		p_string++;
		len++;
	}

	if (p_data) {
		*p_data = 0;
	}
	return len + 1;
}

static inline uint16_t decode_uint16(const uint8_t *p_arr) {
	uint16_t u = 0;

	for (int i = 0; i < 2; i++) {
		uint16_t b = *p_arr;
		b <<= (i * 8);
		u |= b;
		p_arr++;
	}

	return u;
}

static inline uint32_t decode_uint32(const uint8_t *p_arr) {
	uint32_t u = 0;

	for (int i = 0; i < 4; i++) {
		uint32_t b = *p_arr;
		b <<= (i * 8);
		u |= b;
		p_arr++;
	}

	return u;
}

uint32_t halfbits_to_floatbits(uint16_t p_half) {
	uint16_t h_exp, h_sig;
	uint32_t f_sgn, f_exp, f_sig;

	h_exp = (p_half & 0x7c00u);
	f_sgn = ((uint32_t)p_half & 0x8000u) << 16;
	switch (h_exp) {
		case 0x0000u: /* 0 or subnormal */
			h_sig = (p_half & 0x03ffu);
			/* Signed zero */
			if (h_sig == 0) {
				return f_sgn;
			}
			/* Subnormal */
			h_sig <<= 1;
			while ((h_sig & 0x0400u) == 0) {
				h_sig <<= 1;
				h_exp++;
			}
			f_exp = ((uint32_t)(127 - 15 - h_exp)) << 23;
			f_sig = ((uint32_t)(h_sig & 0x03ffu)) << 13;
			return f_sgn + f_exp + f_sig;
		case 0x7c00u: /* inf or NaN */
			/* All-ones exponent and a copy of the significand */
			return f_sgn + 0x7f800000u + (((uint32_t)(p_half & 0x03ffu)) << 13);
		default: /* normalized */
			/* Just need to adjust the exponent and shift */
			return f_sgn + (((uint32_t)(p_half & 0x7fffu) + 0x1c000u) << 13);
	}
}

float halfptr_to_float(const uint16_t *p_half) {
	union {
		uint32_t u32;
		float f32;
	} u;

	u.u32 = halfbits_to_floatbits(*p_half);
	return u.f32;
}

float half_to_float(const uint16_t p_half) {
	return halfptr_to_float(&p_half);
}

static inline float decode_half(const uint8_t *p_arr) {
	return half_to_float(decode_uint16(p_arr));
}

static inline float decode_float(const uint8_t *p_arr) {
	MarshallFloat mf;
	mf.i = decode_uint32(p_arr);
	return mf.f;
}

static inline uint64_t decode_uint64(const uint8_t *p_arr) {
	uint64_t u = 0;

	for (int i = 0; i < 8; i++) {
		uint64_t b = (*p_arr) & 0xFF;
		b <<= (i * 8);
		u |= b;
		p_arr++;
	}

	return u;
}

static inline double decode_double(const uint8_t *p_arr) {
	MarshallDouble md;
	md.l = decode_uint64(p_arr);
	return md.d;
}
