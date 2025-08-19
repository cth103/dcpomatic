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


#ifdef DCPOMATIC_WINDOWS
#include <GL/glew.h>
#endif

#include "gl_util.h"
#include "lib/dcpomatic_assert.h"

#include <wx/version.h>
/* This will only build on an new-enough wxWidgets: see the comment in gl_util.h */
#if wxCHECK_VERSION(3,1,0)

#ifdef DCPOMATIC_OSX
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#endif

#ifdef DCPOMATIC_LINUX
#include <GL/glu.h>
#include <GL/glext.h>
#endif

#ifdef DCPOMATIC_WINDOWS
#include <GL/glu.h>
#include <GL/wglext.h>
#endif


using namespace dcpomatic::gl;


void
dcpomatic::gl::check_error(char const * last)
{
	auto const e = glGetError();
	if (e != GL_NO_ERROR) {
		throw GLError(last, e);
	}
}


Uniform::Uniform(int program, char const* name)
{
	setup(program, name);
}


void
Uniform::setup(int program, char const* name)
{
       _location = glGetUniformLocation(program, name);
       check_error("glGetUniformLocation");
}


UniformVec4f::UniformVec4f(int program, char const* name)
	: Uniform(program, name)
{

}


void
UniformVec4f::set(float a, float b, float c, float d)
{
       DCPOMATIC_ASSERT(_location != -1);
       glUniform4f(_location, a, b, c, d);
       check_error("glUniform4f");
}


Uniform1i::Uniform1i(int program, char const* name)
	: Uniform(program, name)
{

}


void
Uniform1i::set(int v)
{
       DCPOMATIC_ASSERT(_location != -1);
       glUniform1i(_location, v);
       check_error("glUniform1i");
}


UniformMatrix4fv::UniformMatrix4fv(int program, char const* name)
	: Uniform(program, name)
{

}


void
UniformMatrix4fv::set(float const* matrix)
{
       DCPOMATIC_ASSERT(_location != -1);
       glUniformMatrix4fv(_location, 1, GL_TRUE, matrix);
       check_error("glUniformMatrix4fv");
}


#endif

