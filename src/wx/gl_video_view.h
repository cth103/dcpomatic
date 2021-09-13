/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "video_view.h"
#include "lib/signaller.h"
#include "lib/position.h"
#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/glcanvas.h>
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS
#include <dcp/util.h>
#include <boost/atomic.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#undef None
#undef Success
#undef Status


class Texture
{
public:
	Texture (GLint unpack_alignment);
	~Texture ();

	Texture (Texture const&) = delete;
	Texture& operator= (Texture const&) = delete;

	void bind ();
	bool set (std::shared_ptr<const Image> image);

private:
	GLuint _name;
	GLint _unpack_alignment;
	boost::optional<dcp::Size> _size;
};


class GLVideoView : public VideoView
{
public:
	GLVideoView (FilmViewer* viewer, wxWindow* parent);
	~GLVideoView ();

	wxWindow* get () const override {
		return _canvas;
	}
	void update () override;
	void start () override;
	void stop () override;

	NextFrameResult display_next_frame (bool) override;

	bool vsync_enabled () const {
		return _vsync_enabled;
	}

	std::map<GLenum, std::string> information () const {
		return _information;
	}

private:
	void set_image (std::shared_ptr<const PlayerVideo> pv);
	void set_image_and_draw ();
	void draw (Position<int> inter_position, dcp::Size inter_size);
	void thread ();
	void thread_playing ();
	void request_one_shot ();
	void check_for_butler_errors ();
	void ensure_context ();
	void size_changed (wxSizeEvent const &);
	void setup_shaders ();
	void set_border_colour (GLuint program);

	wxGLCanvas* _canvas;
	wxGLContext* _context;

	boost::atomic<wxSize> _canvas_size;
	std::unique_ptr<Texture> _video_texture;
	bool _vsync_enabled;
	boost::thread _thread;

	boost::mutex _playing_mutex;
	boost::condition _thread_work_condition;
	boost::atomic<bool> _playing;
	boost::atomic<bool> _one_shot;

	GLuint _vao;
	GLint _fragment_type;
	bool _setup_shaders_done = false;

	std::shared_ptr<wxTimer> _timer;

	std::map<GLenum, std::string> _information;
};
