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

/* This will only build on an new-enough wxWidgets: see the comment in gl_video_view.h */
#if wxCHECK_VERSION(3,1,0)

#include "film_viewer.h"
#include "wx_util.h"
#include "lib/butler.h"
#include "lib/cross.h"
#include "lib/dcpomatic_assert.h"
#include "lib/dcpomatic_log.h"
#include "lib/exceptions.h"
#include "lib/image.h"
#include "lib/player_video.h"
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
	auto const scale = _canvas->GetDPIScaleFactor();
	int const width = std::round(ev.GetSize().GetWidth() * scale);
	int const height = std::round(ev.GetSize().GetHeight() * scale);
	_canvas_size = { width, height };
	LOG_GENERAL("GLVideoView canvas size changed to %1x%2", width, height);
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
/* type = 0: draw outline content rectangle
 * type = 1: draw XYZ image
 * type = 2: draw RGB image
 * See FragmentType enum below.
 */
"uniform int type = 0;\n"
"uniform vec4 outline_content_colour;\n"
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
"			FragColor = outline_content_colour;\n"
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


enum class FragmentType
{
	OUTLINE_CONTENT = 0,
	XYZ_IMAGE = 1,
	RGB_IMAGE = 2,
};


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


/* Offset and number of indices for the things in the indices array below */
static constexpr int indices_video_texture_offset = 0;
static constexpr int indices_video_texture_number = 6;
static constexpr int indices_subtitle_texture_offset = indices_video_texture_offset + indices_video_texture_number;
static constexpr int indices_subtitle_texture_number = 6;
static constexpr int indices_outline_content_offset = indices_subtitle_texture_offset + indices_subtitle_texture_number;
static constexpr int indices_outline_content_number = 8;

static constexpr unsigned int indices[] = {
	0, 1, 3, // video texture triangle #1
	1, 2, 3, // video texture triangle #2
	4, 5, 7, // subtitle texture triangle #1
	5, 6, 7, // subtitle texture triangle #2
	8, 9,    // outline content line #1
	9, 10,   // outline content line #2
	10, 11,  // outline content line #3
	11, 8,   // outline content line #4
};

/* Offsets of things in the GL_ARRAY_BUFFER */
static constexpr int array_buffer_video_offset = 0;
static constexpr int array_buffer_subtitle_offset = array_buffer_video_offset + 4 * 5 * sizeof(float);
static constexpr int array_buffer_outline_content_offset = array_buffer_subtitle_offset + 4 * 5 * sizeof(float);


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
	set_outline_content_colour (program);

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

	glLineWidth (1.0f);
	check_gl_error ("glLineWidth");
	glEnable (GL_BLEND);
	check_gl_error ("glEnable");
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	check_gl_error ("glBlendFunc");

	/* Reserve space for the GL_ARRAY_BUFFER */
	glBufferData(GL_ARRAY_BUFFER, 12 * 5 * sizeof(float), nullptr, GL_STATIC_DRAW);
	check_gl_error ("glBufferData");
}


void
GLVideoView::set_outline_content_colour (GLuint program)
{
	auto uniform = glGetUniformLocation (program, "outline_content_colour");
	check_gl_error ("glGetUniformLocation");
	auto colour = outline_content_colour ();
	glUniform4f (uniform, colour.Red() / 255.0f, colour.Green() / 255.0f, colour.Blue() / 255.0f, 1.0f);
	check_gl_error ("glUniform4f");
}


void
GLVideoView::draw ()
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
	glUniform1i(_fragment_type, static_cast<GLint>(_optimise_for_j2k ? FragmentType::XYZ_IMAGE : FragmentType::RGB_IMAGE));
	_video_texture->bind();
	glDrawElements (GL_TRIANGLES, indices_video_texture_number, GL_UNSIGNED_INT, reinterpret_cast<void*>(indices_video_texture_offset * sizeof(int)));
	if (_have_subtitle_to_render) {
		glUniform1i(_fragment_type, static_cast<GLint>(FragmentType::RGB_IMAGE));
		_subtitle_texture->bind();
		glDrawElements (GL_TRIANGLES, indices_subtitle_texture_number, GL_UNSIGNED_INT, reinterpret_cast<void*>(indices_subtitle_texture_offset * sizeof(int)));
	}
	if (_viewer->outline_content()) {
		glUniform1i(_fragment_type, static_cast<GLint>(FragmentType::OUTLINE_CONTENT));
		glDrawElements (GL_LINES, indices_outline_content_number, GL_UNSIGNED_INT, reinterpret_cast<void*>(indices_outline_content_offset * sizeof(int)));
		check_gl_error ("glDrawElements");
	}

	glFlush();
	check_gl_error ("glFlush");

	_canvas->SwapBuffers();
}


void
GLVideoView::set_image (shared_ptr<const PlayerVideo> pv)
{
	shared_ptr<const Image> video = _optimise_for_j2k ? pv->raw_image() : pv->image(boost::bind(&PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, true);

	/* Only the player's black frames should be aligned at this stage, so this should
	 * almost always have no work to do.
	 */
	video = Image::ensure_alignment (video, Image::Alignment::COMPACT);

	/** If _optimise_for_j2k is true we render a XYZ image, doing the colourspace
	 *  conversion, scaling and video range conversion in the GL shader.
	 *  Otherwise we render a RGB image without any shader-side processing.
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
	auto const inter_position = player_video().first->inter_position();
	auto const inter_size = player_video().first->inter_size();
	auto const out_size = player_video().first->out_size();

	auto x_offset = std::max(0, (canvas_width - out_size.width) / 2);
	auto y_offset = std::max(0, (canvas_height - out_size.height) / 2);

	_last_canvas_size.set_next (canvas_size);
	_last_video_size.set_next (video->size());
	_last_inter_position.set_next (inter_position);
	_last_inter_size.set_next (inter_size);
	_last_out_size.set_next (out_size);

	class Rectangle
	{
	public:
		Rectangle (wxSize canvas_size, float x, float y, dcp::Size size)
			: _canvas_size (canvas_size)
		{
			auto const x1 = x_pixels_to_gl(x);
			auto const y1 = y_pixels_to_gl(y);
			auto const x2 = x_pixels_to_gl(x + size.width);
			auto const y2 = y_pixels_to_gl(y + size.height);

			/* The texture coordinates here have to account for the fact that when we put images into the texture OpenGL
			 * expected us to start at the lower left but we actually started at the top left.  So although the
			 * top of the texture is at 1.0 we pretend it's the other way round.
			 */

			// bottom right
			_vertices[0] = x2;
			_vertices[1] = y2;
			_vertices[2] = 0.0f;
			_vertices[3] = 1.0f;
			_vertices[4] = 1.0f;

			// top right
			_vertices[5] = x2;
			_vertices[6] = y1;
			_vertices[7] = 0.0f;
			_vertices[8] = 1.0f;
			_vertices[9] = 0.0f;

			// top left
			_vertices[10] = x1;
			_vertices[11] = y1;
			_vertices[12] = 0.0f;
			_vertices[13] = 0.0f;
			_vertices[14] = 0.0f;

			// bottom left
			_vertices[15] = x1;
			_vertices[16] = y2;
			_vertices[17] = 0.0f;
			_vertices[18] = 0.0f;
			_vertices[19] = 1.0f;
		}

		float const * vertices () const {
			return _vertices;
		}

		int const size () const {
			return sizeof(_vertices);
		}

	private:
		/* @param x x position in pixels where 0 is left and canvas_width is right on screen */
		float x_pixels_to_gl(int x) const {
			return (x * 2.0f / _canvas_size.GetWidth()) - 1.0f;
		}

		/* @param y y position in pixels where 0 is top and canvas_height is bottom on screen */
		float y_pixels_to_gl(int y) const {
			return 1.0f - (y * 2.0f / _canvas_size.GetHeight());
		}

		wxSize _canvas_size;
		float _vertices[20];
	};

	if (_last_canvas_size.changed() || _last_inter_position.changed() || _last_inter_size.changed() || _last_out_size.changed()) {

		const auto video = _optimise_for_j2k ?
			Rectangle(canvas_size, inter_position.x + x_offset, inter_position.y + y_offset, inter_size)
			: Rectangle(canvas_size, x_offset, y_offset, out_size);

		glBufferSubData (GL_ARRAY_BUFFER, array_buffer_video_offset, video.size(), video.vertices());
		check_gl_error ("glBufferSubData (video)");

		const auto outline_content = Rectangle(canvas_size, inter_position.x + x_offset, inter_position.y + y_offset, inter_size);
		glBufferSubData (GL_ARRAY_BUFFER, array_buffer_outline_content_offset, outline_content.size(), outline_content.vertices());
		check_gl_error ("glBufferSubData (outline_content)");
	}

	if (_have_subtitle_to_render) {
		const auto subtitle = Rectangle(canvas_size, inter_position.x + x_offset + text->position.x, inter_position.y + y_offset + text->position.y, text->image->size());
		glBufferSubData (GL_ARRAY_BUFFER, array_buffer_subtitle_offset, subtitle.size(), subtitle.vertices());
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
	}

	draw ();

	if (pv) {
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

#endif
