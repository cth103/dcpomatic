/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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

#include "gl_video_view.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "lib/image.h"
#include "lib/dcpomatic_assert.h"
#include "lib/exceptions.h"
#include "lib/cross.h"
#include "lib/player_video.h"
#include "lib/butler.h"
#include <boost/bind/bind.hpp>
#include <iostream>

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


using std::cout;
using std::shared_ptr;
using std::string;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static void
check_gl_error (char const * last)
{
	GLenum const e = glGetError ();
	if (e != GL_NO_ERROR) {
		throw GLError (last, e);
	}
}


GLVideoView::GLVideoView (FilmViewer* viewer, wxWindow *parent)
	: VideoView (viewer)
	, _context (nullptr)
	, _vsync_enabled (false)
	, _playing (false)
	, _one_shot (false)
{
	wxGLAttributes attributes;
	/* We don't need a depth buffer, and indeed there is apparently a bug with Windows/Intel HD 630
	 * which puts green lines over the OpenGL display if you have a non-zero depth buffer size.
	 * https://community.intel.com/t5/Graphics/Request-for-details-on-Intel-HD-630-green-lines-in-OpenGL-apps/m-p/1202179
	 */
	attributes.PlatformDefaults().MinRGBA(8, 8, 8, 8).DoubleBuffer().Depth(0).EndList();
	_canvas = new wxGLCanvas (
		parent, attributes, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE
	);
	_canvas->Bind (wxEVT_PAINT, boost::bind(&GLVideoView::update, this));
	_canvas->Bind (wxEVT_SIZE, boost::bind(&GLVideoView::size_changed, this, _1));

	_canvas->Bind (wxEVT_TIMER, boost::bind(&GLVideoView::check_for_butler_errors, this));
	_timer.reset (new wxTimer(_canvas));
	_timer->Start (2000);
}


void
GLVideoView::size_changed (wxSizeEvent const& ev)
{
	_canvas_size = ev.GetSize ();
	Sized ();
}



GLVideoView::~GLVideoView ()
{
	boost::this_thread::disable_interruption dis;

	try {
		_thread.interrupt ();
		_thread.join ();
	} catch (...) {}
}

void
GLVideoView::check_for_butler_errors ()
{
	if (!_viewer->butler()) {
		return;
	}

	try {
		_viewer->butler()->rethrow ();
	} catch (DecodeError& e) {
		error_dialog (get(), e.what());
	} catch (dcp::ReadError& e) {
		error_dialog (get(), wxString::Format(_("Could not read DCP: %s"), std_to_wx(e.what())));
	}
}


/** Called from the UI thread */
void
GLVideoView::update ()
{
	if (!_canvas->IsShownOnScreen()) {
		return;
	}

	/* It appears important to do this from the GUI thread; if we do it from the GL thread
	 * on Linux we get strange failures to create the context for any version of GL higher
	 * than 3.2.
	 */
	ensure_context ();

#ifdef DCPOMATIC_OSX
	/* macOS gives errors if we don't do this (and therefore [NSOpenGLContext setView:]) from the main thread */
	if (!_setup_shaders_done) {
		setup_shaders ();
		_setup_shaders_done = true;
	}
#endif

	if (!_thread.joinable()) {
		_thread = boost::thread (boost::bind(&GLVideoView::thread, this));
	}

	request_one_shot ();

	rethrow ();
}


static constexpr char vertex_source[] =
"#version 330 core\n"
"\n"
"layout (location = 0) in vec3 in_pos;\n"
"layout (location = 1) in vec2 in_tex_coord;\n"
"\n"
"out vec2 TexCoord;\n"
"\n"
"void main()\n"
"{\n"
"	gl_Position = vec4(in_pos, 1.0);\n"
"	TexCoord = in_tex_coord;\n"
"}\n";


/* Bicubic interpolation stolen from https://stackoverflow.com/questions/13501081/efficient-bicubic-filtering-code-in-glsl */
static constexpr char fragment_source[] =
"#version 330 core\n"
"\n"
"in vec2 TexCoord;\n"
"\n"
"uniform sampler2D texture_sampler;\n"
/* type = 0: draw border
 * type = 1: draw XYZ image
 * type = 2: draw RGB image
 */
"uniform int type = 0;\n"
"uniform vec4 border_colour;\n"
"uniform mat4 colour_conversion;\n"
"\n"
"out vec4 FragColor;\n"
"\n"
"vec4 cubic(float x)\n"
"\n"
"#define IN_GAMMA 2.2\n"
"#define OUT_GAMMA 0.384615385\n"       //  1 /  2.6
"#define DCI_COEFFICIENT 0.91655528\n"  // 48 / 53.37
"\n"
"{\n"
"    float x2 = x * x;\n"
"    float x3 = x2 * x;\n"
"    vec4 w;\n"
"    w.x =     -x3 + 3 * x2 - 3 * x + 1;\n"
"    w.y =  3 * x3 - 6 * x2         + 4;\n"
"    w.z = -3 * x3 + 3 * x2 + 3 * x + 1;\n"
"    w.w =  x3;\n"
"    return w / 6.f;\n"
"}\n"
"\n"
"vec4 texture_bicubic(sampler2D sampler, vec2 tex_coords)\n"
"{\n"
"   vec2 tex_size = textureSize(sampler, 0);\n"
"   vec2 inv_tex_size = 1.0 / tex_size;\n"
"\n"
"   tex_coords = tex_coords * tex_size - 0.5;\n"
"\n"
"   vec2 fxy = fract(tex_coords);\n"
"   tex_coords -= fxy;\n"
"\n"
"   vec4 xcubic = cubic(fxy.x);\n"
"   vec4 ycubic = cubic(fxy.y);\n"
"\n"
"   vec4 c = tex_coords.xxyy + vec2 (-0.5, +1.5).xyxy;\n"
"\n"
"   vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);\n"
"   vec4 offset = c + vec4 (xcubic.yw, ycubic.yw) / s;\n"
"\n"
"   offset *= inv_tex_size.xxyy;\n"
"\n"
"   vec4 sample0 = texture(sampler, offset.xz);\n"
"   vec4 sample1 = texture(sampler, offset.yz);\n"
"   vec4 sample2 = texture(sampler, offset.xw);\n"
"   vec4 sample3 = texture(sampler, offset.yw);\n"
"\n"
"   float sx = s.x / (s.x + s.y);\n"
"   float sy = s.z / (s.z + s.w);\n"
"\n"
"   return mix(\n"
"	   mix(sample3, sample2, sx), mix(sample1, sample0, sx)\n"
"	   , sy);\n"
"}\n"
"\n"
"void main()\n"
"{\n"
"	switch (type) {\n"
"		case 0:\n"
"			FragColor = border_colour;\n"
"			break;\n"
"		case 1:\n"
"			FragColor = texture_bicubic(texture_sampler, TexCoord);\n"
"			FragColor.x = pow(FragColor.x, IN_GAMMA) / DCI_COEFFICIENT;\n"
"			FragColor.y = pow(FragColor.y, IN_GAMMA) / DCI_COEFFICIENT;\n"
"			FragColor.z = pow(FragColor.z, IN_GAMMA) / DCI_COEFFICIENT;\n"
"			FragColor = colour_conversion * FragColor;\n"
"			FragColor.x = pow(FragColor.x, OUT_GAMMA);\n"
"			FragColor.y = pow(FragColor.y, OUT_GAMMA);\n"
"			FragColor.z = pow(FragColor.z, OUT_GAMMA);\n"
"			break;\n"
"		case 2:\n"
"			FragColor = texture_bicubic(texture_sampler, TexCoord);\n"
"			break;\n"
"	}\n"
"}\n";


void
GLVideoView::ensure_context ()
{
	if (!_context) {
		wxGLContextAttrs attrs;
		attrs.PlatformDefaults().CoreProfile().OGLVersion(4, 1).EndList();
		_context = new wxGLContext (_canvas, nullptr, &attrs);
		if (!_context->IsOK()) {
			throw GLError ("Making GL context", -1);
		}
	}
}


/* Offset of video texture triangles in indices */
static constexpr int indices_video_texture = 0;
/* Offset of subtitle texture triangles in indices */
static constexpr int indices_subtitle_texture = 6;
/* Offset of border lines in indices */
static constexpr int indices_border = 12;

static constexpr unsigned int indices[] = {
	0, 1, 3, // video texture triangle #1
	1, 2, 3, // video texture triangle #2
	4, 5, 7, // subtitle texture triangle #1
	5, 6, 7, // subtitle texture triangle #2
	8, 9,    // border line #1
	9, 10,   // border line #2
	10, 11,  // border line #3
	11, 8,   // border line #4
};


void
GLVideoView::setup_shaders ()
{
	DCPOMATIC_ASSERT (_canvas);
	DCPOMATIC_ASSERT (_context);
	auto r = _canvas->SetCurrent (*_context);
	DCPOMATIC_ASSERT (r);

#ifdef DCPOMATIC_WINDOWS
	r = glewInit();
	if (r != GLEW_OK) {
		throw GLError(reinterpret_cast<char const*>(glewGetErrorString(r)));
	}
#endif

	auto get_information = [this](GLenum name) {
		auto s = glGetString (name);
		if (s) {
			_information[name] = std::string (reinterpret_cast<char const *>(s));
		}
	};

	get_information (GL_VENDOR);
	get_information (GL_RENDERER);
	get_information (GL_VERSION);
	get_information (GL_SHADING_LANGUAGE_VERSION);

	glGenVertexArrays(1, &_vao);
	check_gl_error ("glGenVertexArrays");
	GLuint vbo;
	glGenBuffers(1, &vbo);
	check_gl_error ("glGenBuffers");
	GLuint ebo;
	glGenBuffers(1, &ebo);
	check_gl_error ("glGenBuffers");

	glBindVertexArray(_vao);
	check_gl_error ("glBindVertexArray");

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	check_gl_error ("glBindBuffer");

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	check_gl_error ("glBindBuffer");
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	check_gl_error ("glBufferData");

	/* position attribute to vertex shader (location = 0) */
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
	glEnableVertexAttribArray(0);
	/* texture coord attribute to vertex shader (location = 1) */
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	check_gl_error ("glEnableVertexAttribArray");

	auto compile = [](GLenum type, char const* source) -> GLuint {
		auto shader = glCreateShader(type);
		DCPOMATIC_ASSERT (shader);
		GLchar const * src[] = { static_cast<GLchar const *>(source) };
		glShaderSource(shader, 1, src, nullptr);
		check_gl_error ("glShaderSource");
		glCompileShader(shader);
		check_gl_error ("glCompileShader");
		GLint ok;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
		if (!ok) {
			GLint log_length;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
			string log;
			if (log_length > 0) {
				char* log_char = new char[log_length];
				glGetShaderInfoLog(shader, log_length, nullptr, log_char);
				log = string(log_char);
				delete[] log_char;
			}
			glDeleteShader(shader);
			throw GLError(String::compose("Could not compile shader (%1)", log).c_str(), -1);
		}
		return shader;
	};

	auto vertex_shader = compile (GL_VERTEX_SHADER, vertex_source);
	auto fragment_shader = compile (GL_FRAGMENT_SHADER, fragment_source);

	auto program = glCreateProgram();
	check_gl_error ("glCreateProgram");
	glAttachShader (program, vertex_shader);
	check_gl_error ("glAttachShader");
	glAttachShader (program, fragment_shader);
	check_gl_error ("glAttachShader");
	glLinkProgram (program);
	check_gl_error ("glLinkProgram");
	GLint ok;
	glGetProgramiv (program, GL_LINK_STATUS, &ok);
	if (!ok) {
		GLint log_length;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
		string log;
		if (log_length > 0) {
			char* log_char = new char[log_length];
			glGetProgramInfoLog(program, log_length, nullptr, log_char);
			log = string(log_char);
			delete[] log_char;
		}
		glDeleteProgram (program);
		throw GLError(String::compose("Could not link shader (%1)", log).c_str(), -1);
	}
	glDeleteShader (vertex_shader);
	glDeleteShader (fragment_shader);

	glUseProgram (program);

	_fragment_type = glGetUniformLocation (program, "type");
	check_gl_error ("glGetUniformLocation");
	set_border_colour (program);

	auto conversion = dcp::ColourConversion::rec709_to_xyz();
	boost::numeric::ublas::matrix<double> matrix = conversion.xyz_to_rgb ();
	GLfloat gl_matrix[] = {
		static_cast<float>(matrix(0, 0)), static_cast<float>(matrix(0, 1)), static_cast<float>(matrix(0, 2)), 0.0f,
		static_cast<float>(matrix(1, 0)), static_cast<float>(matrix(1, 1)), static_cast<float>(matrix(1, 2)), 0.0f,
		static_cast<float>(matrix(2, 0)), static_cast<float>(matrix(2, 1)), static_cast<float>(matrix(2, 2)), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		};

	auto colour_conversion = glGetUniformLocation (program, "colour_conversion");
	check_gl_error ("glGetUniformLocation");
	glUniformMatrix4fv (colour_conversion, 1, GL_TRUE, gl_matrix);

	glLineWidth (2.0f);
	glEnable (GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* Reserve space for the GL_ARRAY_BUFFER */
	glBufferData(GL_ARRAY_BUFFER, 12 * 5 * sizeof(float), nullptr, GL_STATIC_DRAW);
	check_gl_error ("glBufferData");
}


void
GLVideoView::set_border_colour (GLuint program)
{
	auto uniform = glGetUniformLocation (program, "border_colour");
	check_gl_error ("glGetUniformLocation");
	auto colour = outline_content_colour ();
	glUniform4f (uniform, colour.Red() / 255.0f, colour.Green() / 255.0f, colour.Blue() / 255.0f, 1.0f);
	check_gl_error ("glUniform4f");
}


void
GLVideoView::draw (Position<int>, dcp::Size)
{
	auto pad = pad_colour();
	glClearColor(pad.Red() / 255.0, pad.Green() / 255.0, pad.Blue() / 255.0, 1.0);
	glClear (GL_COLOR_BUFFER_BIT);
	check_gl_error ("glClear");

	auto const size = _canvas_size.load();
	int const width = size.GetWidth();
	int const height = size.GetHeight();

	if (width < 64 || height < 0) {
		return;
	}

	glViewport (0, 0, width, height);
	check_gl_error ("glViewport");

	glBindVertexArray(_vao);
	check_gl_error ("glBindVertexArray");
	glUniform1i(_fragment_type, _optimise_for_j2k ? 1 : 2);
	_video_texture->bind();
	glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_INT, reinterpret_cast<void*>(indices_video_texture * sizeof(int)));
	if (_have_subtitle_to_render) {
		glUniform1i(_fragment_type, 2);
		_subtitle_texture->bind();
		glDrawElements (GL_TRIANGLES, 6, GL_UNSIGNED_INT, reinterpret_cast<void*>(indices_subtitle_texture * sizeof(int)));
	}
	if (_viewer->outline_content()) {
		glUniform1i(_fragment_type, 0);
		glDrawElements (GL_LINES, 8, GL_UNSIGNED_INT, reinterpret_cast<void*>(indices_border * sizeof(int)));
		check_gl_error ("glDrawElements");
	}

	glFlush();
	check_gl_error ("glFlush");

	_canvas->SwapBuffers();
}


void
GLVideoView::set_image (shared_ptr<const PlayerVideo> pv)
{
	shared_ptr<const Image> video = _optimise_for_j2k ? pv->raw_image() : pv->image(bind(&PlayerVideo::force, _1, AV_PIX_FMT_RGB24), VideoRange::FULL, Image::Alignment::COMPACT, true);

	/* Only the player's black frames should be aligned at this stage, so this should
	 * almost always have no work to do.
	 */
	video = Image::ensure_alignment (video, Image::Alignment::COMPACT);

	/** If _optimise_for_j2k is true we render a XYZ image, doing the colourspace
	 *  conversion, scaling and video range conversion in the GL shader.
	 *  Othewise we render a RGB image without any shader-side processing.
	 */

	/* XXX: video range conversion */

	_video_texture->set (video);

	auto const text = pv->text();
	_have_subtitle_to_render = static_cast<bool>(text) && _optimise_for_j2k;
	if (_have_subtitle_to_render) {
		/* opt: only do this if it's a new subtitle? */
		DCPOMATIC_ASSERT (text->image->alignment() == Image::Alignment::COMPACT);
		_subtitle_texture->set (text->image);
	}


	auto const canvas_size = _canvas_size.load();
	int const canvas_width = canvas_size.GetWidth();
	int const canvas_height = canvas_size.GetHeight();
	auto inter_position = player_video().first->inter_position();
	auto inter_size = player_video().first->inter_size();
	auto out_size = player_video().first->out_size();

	_last_canvas_size.set_next (canvas_size);
	_last_video_size.set_next (video->size());
	_last_inter_position.set_next (inter_position);
	_last_inter_size.set_next (inter_size);
	_last_out_size.set_next (out_size);

	auto x_pixels_to_gl = [canvas_width](int x) {
		return (x * 2.0f / canvas_width) - 1.0f;
	};

	auto y_pixels_to_gl = [canvas_height](int y) {
		return (y * 2.0f / canvas_height) - 1.0f;
	};

	if (_last_canvas_size.changed() || _last_inter_position.changed() || _last_inter_size.changed() || _last_out_size.changed()) {
		float const video_x1 = x_pixels_to_gl(_optimise_for_j2k ? inter_position.x : 0);
		float const video_x2 = x_pixels_to_gl(_optimise_for_j2k ? (inter_position.x + inter_size.width) : out_size.width);
		float const video_y1 = y_pixels_to_gl(_optimise_for_j2k ? inter_position.y : 0);
		float const video_y2 = y_pixels_to_gl(_optimise_for_j2k ? (inter_position.y + inter_size.height) : out_size.height);
		float video_vertices[] = {
			 // positions              // texture coords
			video_x2, video_y2, 0.0f,  1.0f, 0.0f,   // video texture top right       (index 0)
			video_x2, video_y1, 0.0f,  1.0f, 1.0f,   // video texture bottom right    (index 1)
			video_x1, video_y1, 0.0f,  0.0f, 1.0f,   // video texture bottom left     (index 2)
			video_x1, video_y2, 0.0f,  0.0f, 0.0f,   // video texture top left        (index 3)
		};

		glBufferSubData (GL_ARRAY_BUFFER, 0, sizeof(video_vertices), video_vertices);
		check_gl_error ("glBufferSubData (video)");

		float const border_x1 = x_pixels_to_gl(inter_position.x);
		float const border_y1 = y_pixels_to_gl(inter_position.y);
		float const border_x2 = x_pixels_to_gl(inter_position.x + inter_size.width);
		float const border_y2 = y_pixels_to_gl(inter_position.y + inter_size.height);

		float border_vertices[] = {
			 // positions                 // texture coords
			 border_x1, border_y1, 0.0f,  0.0f, 0.0f,   // border bottom left         (index 4)
			 border_x1, border_y2, 0.0f,  0.0f, 0.0f,   // border top left            (index 5)
			 border_x2, border_y2, 0.0f,  0.0f, 0.0f,   // border top right           (index 6)
			 border_x2, border_y1, 0.0f,  0.0f, 0.0f,   // border bottom right        (index 7)
		};

		glBufferSubData (GL_ARRAY_BUFFER, 8 * 5 * sizeof(float), sizeof(border_vertices), border_vertices);
		check_gl_error ("glBufferSubData (border)");
	}

	if (_have_subtitle_to_render) {
		float const subtitle_x1 = x_pixels_to_gl(inter_position.x + text->position.x);
		float const subtitle_x2 = x_pixels_to_gl(inter_position.x + text->position.x + text->image->size().width);
		float const subtitle_y1 = y_pixels_to_gl(inter_position.y + text->position.y + text->image->size().height);
		float const subtitle_y2 = y_pixels_to_gl(inter_position.y + text->position.y);

		float vertices[] = {
			 // positions                     // texture coords
			 subtitle_x2, subtitle_y1, 0.0f,  1.0f, 0.0f,   // subtitle texture top right    (index 4)
			 subtitle_x2, subtitle_y2, 0.0f,  1.0f, 1.0f,   // subtitle texture bottom right (index 5)
			 subtitle_x1, subtitle_y2, 0.0f,  0.0f, 1.0f,   // subtitle texture bottom left  (index 6)
			 subtitle_x1, subtitle_y1, 0.0f,  0.0f, 0.0f,   // subtitle texture top left     (index 7)
		};

		glBufferSubData (GL_ARRAY_BUFFER, 4 * 5 * sizeof(float), sizeof(vertices), vertices);
		check_gl_error ("glBufferSubData (subtitle)");
	}

	/* opt: where should these go? */

	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	check_gl_error ("glTexParameteri");

	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	check_gl_error ("glTexParameterf");
}


void
GLVideoView::start ()
{
	VideoView::start ();

	boost::mutex::scoped_lock lm (_playing_mutex);
	_playing = true;
	_thread_work_condition.notify_all ();
}

void
GLVideoView::stop ()
{
	boost::mutex::scoped_lock lm (_playing_mutex);
	_playing = false;
}


void
GLVideoView::thread_playing ()
{
	if (length() != dcpomatic::DCPTime()) {
		auto const next = position() + one_video_frame();

		if (next >= length()) {
			_viewer->finished ();
			return;
		}

		get_next_frame (false);
		set_image_and_draw ();
	}

	while (true) {
		optional<int> n = time_until_next_frame();
		if (!n || *n > 5) {
			break;
		}
		get_next_frame (true);
		add_dropped ();
	}
}


void
GLVideoView::set_image_and_draw ()
{
	auto pv = player_video().first;
	if (pv) {
		set_image (pv);
		draw (pv->inter_position(), pv->inter_size());
		_viewer->image_changed (pv);
	}
}


void
GLVideoView::thread ()
try
{
	start_of_thread ("GLVideoView");

#if defined(DCPOMATIC_OSX)
	/* Without this we see errors like
	 * ../src/osx/cocoa/glcanvas.mm(194): assert ""context"" failed in SwapBuffers(): should have current context [in thread 700006970000]
	 */
	WXGLSetCurrentContext (_context->GetWXGLContext());
#else
	if (!_setup_shaders_done) {
		setup_shaders ();
		_setup_shaders_done = true;
	}
#endif

#if defined(DCPOMATIC_LINUX) && defined(DCPOMATIC_HAVE_GLX_SWAP_INTERVAL_EXT)
	if (_canvas->IsExtensionSupported("GLX_EXT_swap_control")) {
		/* Enable vsync */
		Display* dpy = wxGetX11Display();
		glXSwapIntervalEXT (dpy, DefaultScreen(dpy), 1);
		_vsync_enabled = true;
	}
#endif

#ifdef DCPOMATIC_WINDOWS
	if (_canvas->IsExtensionSupported("WGL_EXT_swap_control")) {
		/* Enable vsync */
		PFNWGLSWAPINTERVALEXTPROC swap = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
		if (swap) {
			swap (1);
			_vsync_enabled = true;
		}
	}

#endif

#ifdef DCPOMATIC_OSX
	/* Enable vsync */
	GLint swapInterval = 1;
	CGLSetParameter (CGLGetCurrentContext(), kCGLCPSwapInterval, &swapInterval);
	_vsync_enabled = true;
#endif

	_video_texture.reset(new Texture(_optimise_for_j2k ? 2 : 1));
	_subtitle_texture.reset(new Texture(1));

	while (true) {
		boost::mutex::scoped_lock lm (_playing_mutex);
		while (!_playing && !_one_shot) {
			_thread_work_condition.wait (lm);
		}
		lm.unlock ();

		if (_playing) {
			thread_playing ();
		} else if (_one_shot) {
			_one_shot = false;
			set_image_and_draw ();
		}

		boost::this_thread::interruption_point ();
		dcpomatic_sleep_milliseconds (time_until_next_frame().get_value_or(0));
	}

	/* XXX: leaks _context, but that seems preferable to deleting it here
	 * without also deleting the wxGLCanvas.
	 */
}
catch (...)
{
	store_current ();
}


VideoView::NextFrameResult
GLVideoView::display_next_frame (bool non_blocking)
{
	NextFrameResult const r = get_next_frame (non_blocking);
	request_one_shot ();
	return r;
}


void
GLVideoView::request_one_shot ()
{
	boost::mutex::scoped_lock lm (_playing_mutex);
	_one_shot = true;
	_thread_work_condition.notify_all ();
}


Texture::Texture (GLint unpack_alignment)
	: _unpack_alignment (unpack_alignment)
{
	glGenTextures (1, &_name);
	check_gl_error ("glGenTextures");
}


Texture::~Texture ()
{
	glDeleteTextures (1, &_name);
}


void
Texture::bind ()
{
	glBindTexture(GL_TEXTURE_2D, _name);
	check_gl_error ("glBindTexture");
}


void
Texture::set (shared_ptr<const Image> image)
{
	auto const create = !_size || image->size() != _size;
	_size = image->size();

	glPixelStorei (GL_UNPACK_ALIGNMENT, _unpack_alignment);
	check_gl_error ("glPixelStorei");

	DCPOMATIC_ASSERT (image->alignment() == Image::Alignment::COMPACT);

	GLint internal_format;
	GLenum format;
	GLenum type;

	switch (image->pixel_format()) {
	case AV_PIX_FMT_BGRA:
		internal_format = GL_RGBA8;
		format = GL_BGRA;
		type = GL_UNSIGNED_BYTE;
		break;
	case AV_PIX_FMT_RGBA:
		internal_format = GL_RGBA8;
		format = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		break;
	case AV_PIX_FMT_RGB24:
		internal_format = GL_RGBA8;
		format = GL_RGB;
		type = GL_UNSIGNED_BYTE;
		break;
	case AV_PIX_FMT_XYZ12:
		internal_format = GL_RGBA12;
		format = GL_RGB;
		type = GL_UNSIGNED_SHORT;
		break;
	default:
		throw PixelFormatError ("Texture::set", image->pixel_format());
	}

	bind ();

	if (create) {
		glTexImage2D (GL_TEXTURE_2D, 0, internal_format, _size->width, _size->height, 0, format, type, image->data()[0]);
		check_gl_error ("glTexImage2D");
	} else {
		glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, _size->width, _size->height, format, type, image->data()[0]);
		check_gl_error ("glTexSubImage2D");
	}
}

