/*************************************************************************/
/*  noise.cpp                                                            */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "noise.h"

#include "core/core_string_names.h"

Noise::Noise() {
}

Noise::~Noise() {
}

void Noise::_bind_methods() {

	ClassDB::bind_method(D_METHOD("get_seed"), &Noise::get_seed);
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &Noise::set_seed);

	ClassDB::bind_method(D_METHOD("set_octaves", "octave_count"), &Noise::set_octaves);
	ClassDB::bind_method(D_METHOD("get_octaves"), &Noise::get_octaves);

	ClassDB::bind_method(D_METHOD("set_period", "period"), &Noise::set_period);
	ClassDB::bind_method(D_METHOD("get_period"), &Noise::get_period);

	ClassDB::bind_method(D_METHOD("set_persistence", "persistence"), &Noise::set_persistence);
	ClassDB::bind_method(D_METHOD("get_persistence"), &Noise::get_persistence);

	ClassDB::bind_method(D_METHOD("set_lacunarity", "lacunarity"), &Noise::set_lacunarity);
	ClassDB::bind_method(D_METHOD("get_lacunarity"), &Noise::get_lacunarity);

	ClassDB::bind_method(D_METHOD("get_image", "width", "height"), &Noise::get_image);
	ClassDB::bind_method(D_METHOD("get_seamless_image", "size"), &Noise::get_seamless_image);

	ClassDB::bind_method(D_METHOD("get_noise_2d", "x", "y"), &Noise::get_noise_2d);
	ClassDB::bind_method(D_METHOD("get_noise_3d", "x", "y", "z"), &Noise::get_noise_3d);
	ClassDB::bind_method(D_METHOD("get_noise_4d", "x", "y", "z", "w"), &Noise::get_noise_4d);

	ClassDB::bind_method(D_METHOD("get_noise_2dv", "pos"), &Noise::get_noise_2dv);
	ClassDB::bind_method(D_METHOD("get_noise_3dv", "pos"), &Noise::get_noise_3dv);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "octaves", PROPERTY_HINT_RANGE, "1,6,1"), "set_octaves", "get_octaves");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "period"), "set_period", "get_period");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "persistence", PROPERTY_HINT_RANGE, "0.0,1.0,0.001"), "set_persistence", "get_persistence");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "lacunarity", PROPERTY_HINT_RANGE, "0.1,4.0,0.01"), "set_lacunarity", "get_lacunarity");
}
