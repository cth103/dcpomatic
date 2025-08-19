/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#ifndef DCPOMATIC_GL_UTIL_H
#define DCPOMATIC_GL_UTIL_H


namespace dcpomatic {
namespace gl {


extern void check_error(char const * last);


class Uniform
{
public:
	Uniform() = default;
	Uniform(int program, char const* name);

	void setup(int program, char const* name);

protected:
	int _location = -1;
};


class UniformVec4f : public Uniform
{
public:
	UniformVec4f() = default;
	UniformVec4f(int program, char const* name);

	void set(float a, float b, float c, float d);
};


class Uniform1i : public Uniform
{
public:
	Uniform1i() = default;
	Uniform1i(int program, char const* name);

	void set(int v);
};


class UniformMatrix4fv : public Uniform
{
public:
	UniformMatrix4fv() = default;
	UniformMatrix4fv(int program, char const* name);

	void set(float const* matrix);
};


}
}


#endif

