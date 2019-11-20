/*
    Copyright (C) 2018-2019 Carl Hetherington <cth@carlh.net>

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

#include "gl_video_view.h"
#include "film_viewer.h"
#include "wx_util.h"
#include "lib/image.h"
#include "lib/dcpomatic_assert.h"
#include "lib/exceptions.h"
#include "lib/cross.h"
#include "lib/player_video.h"
#include "lib/butler.h"
#include <boost/bind.hpp>
#include <iostream>

#ifdef DCPOMATIC_OSX
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#endif

#ifdef DCPOMATIC_LINUX
#include <GL/glu.h>
#include <GL/glext.h>
#include <GL/glxext.h>
#endif

#ifdef DCPOMATIC_WINDOWS
#include <GL/glu.h>
#include <GL/glext.h>
#include <GL/wglext.h>
#endif

using std::cout;
using boost::shared_ptr;
using boost::optional;

GLVideoView::GLVideoView (FilmViewer* viewer, wxWindow *parent)
	: VideoView (viewer)
	, _vsync_enabled (false)
	, _thread (0)
	, _playing (false)
	, _one_shot (false)
{
	_canvas = new wxGLCanvas (parent, wxID_ANY, 0, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE);
	_canvas->Bind (wxEVT_PAINT, boost::bind(&GLVideoView::update, this));
	_canvas->Bind (wxEVT_SIZE, boost::bind(boost::ref(Sized)));
	_canvas->Bind (wxEVT_CREATE, boost::bind(&GLVideoView::create, this));

	_canvas->Bind (wxEVT_TIMER, boost::bind(&GLVideoView::check_for_butler_errors, this));
	_timer.reset (new wxTimer(_canvas));
	_timer->Start (2000);

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

	glGenTextures (1, &_id);
	glBindTexture (GL_TEXTURE_2D, _id);
	glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
}

GLVideoView::~GLVideoView ()
{
	_thread->interrupt ();
	_thread->join ();
	delete _thread;

	glDeleteTextures (1, &_id);
}

void
GLVideoView::check_for_butler_errors ()
{
	try {
		_viewer->butler()->rethrow ();
	} catch (DecodeError& e) {
		error_dialog (get(), e.what());
	}
}

static void
check_gl_error (char const * last)
{
	GLenum const e = glGetError ();
	if (e != GL_NO_ERROR) {
		throw GLError (last, e);
	}
}

void
GLVideoView::update ()
{
	if (!_canvas->IsShownOnScreen()) {
		return;
	}
	request_one_shot ();
}

void
GLVideoView::draw ()
{
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	check_gl_error ("glClear");

	glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
	check_gl_error ("glClearColor");
	glEnable (GL_TEXTURE_2D);
	check_gl_error ("glEnable GL_TEXTURE_2D");
	glEnable (GL_BLEND);
	check_gl_error ("glEnable GL_BLEND");
	glDisable (GL_DEPTH_TEST);
	check_gl_error ("glDisable GL_DEPTH_TEST");
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (_canvas->GetSize().x < 64 || _canvas->GetSize().y < 0) {
		return;
	}

	glViewport (0, 0, _canvas->GetSize().x, _canvas->GetSize().y);
	check_gl_error ("glViewport");
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();

	gluOrtho2D (0, _canvas->GetSize().x, _canvas->GetSize().y, 0);
	check_gl_error ("gluOrtho2d");
	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	glTranslatef (0, 0, 0);

	dcp::Size const out_size = _viewer->out_size ();

	if (_size) {
		/* Render our image (texture) */
		glBegin (GL_QUADS);
		glTexCoord2f (0, 1);
		glVertex2f (0, _size->height);
		glTexCoord2f (1, 1);
		glVertex2f (_size->width, _size->height);
		glTexCoord2f (1, 0);
		glVertex2f (_size->width, 0);
		glTexCoord2f (0, 0);
		glVertex2f (0, 0);
		glEnd ();
	} else {
		/* No image, so just fill with black */
		glBegin (GL_QUADS);
		glColor3ub (0, 0, 0);
		glVertex2f (0, 0);
		glVertex2f (out_size.width, 0);
		glVertex2f (out_size.width, out_size.height);
		glVertex2f (0, out_size.height);
		glVertex2f (0, 0);
		glEnd ();
	}

	wxSize const canvas_size = _canvas->GetSize ();

	if (!_viewer->pad_black() && out_size.width < canvas_size.GetWidth()) {
		glBegin (GL_QUADS);
		/* XXX: these colours are right for GNOME; may need adjusting for other OS */
		glColor3ub (240, 240, 240);
		glVertex2f (out_size.width, 0);
		glVertex2f (canvas_size.GetWidth(), 0);
		glVertex2f (canvas_size.GetWidth(), canvas_size.GetHeight());
		glVertex2f (out_size.width, canvas_size.GetHeight());
		glEnd ();
		glColor3ub (255, 255, 255);
	}

	if (!_viewer->pad_black() && out_size.height < canvas_size.GetHeight()) {
		glColor3ub (240, 240, 240);
		int const gap = (canvas_size.GetHeight() - out_size.height) / 2;
		glBegin (GL_QUADS);
		glVertex2f (0, 0);
		glVertex2f (canvas_size.GetWidth(), 0);
		glVertex2f (canvas_size.GetWidth(), gap);
		glVertex2f (0, gap);
		glEnd ();
		glBegin (GL_QUADS);
		glVertex2f (0, gap + out_size.height + 1);
		glVertex2f (canvas_size.GetWidth(), gap + out_size.height + 1);
		glVertex2f (canvas_size.GetWidth(), 2 * gap + out_size.height + 2);
		glVertex2f (0, 2 * gap + out_size.height + 2);
		glEnd ();
		glColor3ub (255, 255, 255);
	}

	if (_viewer->outline_content()) {
		glColor3ub (255, 0, 0);
		glBegin (GL_LINE_LOOP);
		Position<int> inter_position = _viewer->inter_position ();
		dcp::Size inter_size = _viewer->inter_size ();
		glVertex2f (inter_position.x, inter_position.y + (canvas_size.GetHeight() - out_size.height) / 2);
		glVertex2f (inter_position.x + inter_size.width, inter_position.y + (canvas_size.GetHeight() - out_size.height) / 2);
		glVertex2f (inter_position.x + inter_size.width, inter_position.y + (canvas_size.GetHeight() - out_size.height) / 2 + inter_size.height);
		glVertex2f (inter_position.x, inter_position.y + (canvas_size.GetHeight() - out_size.height) / 2 + inter_size.height);
		glEnd ();
		glColor3ub (255, 255, 255);
	}

	glFlush();
	_canvas->SwapBuffers();
}

void
GLVideoView::set_image (shared_ptr<const Image> image)
{
	if (!image) {
		_size = optional<dcp::Size>();
		return;
	}

	DCPOMATIC_ASSERT (image->pixel_format() == AV_PIX_FMT_RGB24);
	DCPOMATIC_ASSERT (!image->aligned());

	_size = image->size ();
	glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
	check_gl_error ("glPixelStorei");
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB8, _size->width, _size->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image->data()[0]);
	check_gl_error ("glTexImage2D");

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
	boost::mutex::scoped_lock lm (_playing_mutex);
	_playing = true;
	_playing_condition.notify_all ();
}

void
GLVideoView::stop ()
{
	boost::mutex::scoped_lock lm (_playing_mutex);
	_playing = false;
}

void
GLVideoView::thread ()
try
{
	/* XXX_b: check all calls and signal emissions in this method & protect them if necessary */
	_context = new wxGLContext (_canvas);
	_canvas->SetCurrent (*_context);

	while (true) {
		boost::mutex::scoped_lock lm (_playing_mutex);
		while (!_playing && !_one_shot) {
			_playing_condition.wait (lm);
		}
		_one_shot = false;
		lm.unlock ();

		if (length() != dcpomatic::DCPTime()) {
			dcpomatic::DCPTime const next = position() + one_video_frame();

			if (next >= length()) {
				_viewer->stop ();
				_viewer->emit_finished ();
				continue;
			}

			get_next_frame (false);
			set_image (player_video().first->image(bind(&PlayerVideo::force, _1, AV_PIX_FMT_RGB24), false, true));
		}
		draw ();

		while (time_until_next_frame() < 5) {
			get_next_frame (true);
		}

		boost::this_thread::interruption_point ();
		dcpomatic_sleep_milliseconds (time_until_next_frame());
	}

	delete _context;
}
catch (boost::thread_interrupted& e)
{
	/* XXX_b: store exceptions here */
	delete _context;
	return;
}

bool
GLVideoView::display_next_frame (bool non_blocking)
{
	bool const r = get_next_frame (non_blocking);
	request_one_shot ();
	return r;
}

void
GLVideoView::request_one_shot ()
{
	boost::mutex::scoped_lock lm (_playing_mutex);
	_one_shot = true;
	_playing_condition.notify_all ();
}

void
GLVideoView::create ()
{
	if (!_thread) {
		_thread = new boost::thread (boost::bind(&GLVideoView::thread, this));
	}
}
