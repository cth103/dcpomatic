/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "system_information_dialog.h"
#include "wx_util.h"
#include "gl_video_view.h"
#include "film_viewer.h"

#ifdef DCPOMATIC_OSX
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>
#else
#include <GL/glu.h>
#include <GL/glext.h>
#endif

using std::string;
using boost::weak_ptr;
using boost::shared_ptr;

SystemInformationDialog::SystemInformationDialog (wxWindow* parent, weak_ptr<FilmViewer> weak_viewer)
	: TableDialog (parent, _("System information"), 2, 1, false)
{
	shared_ptr<FilmViewer> viewer = weak_viewer.lock ();
	GLVideoView const * gl = viewer ? dynamic_cast<GLVideoView const *>(viewer->video_view()) : 0;

	if (!gl) {
		add (_("OpenGL version"), true);
		add (_("unknown (OpenGL not enabled in DCP-o-matic)"), false);
	} else {
		add (_("OpenGL version"), true);
		char const * v = (char const *) glGetString (GL_VERSION);
		if (v) {
			add (std_to_wx(v), false);
		} else {
			add (_("unknown"), false);
		}
		add (_("vsync"), true);
		add (gl->vsync_enabled() ? _("enabled") : _("not enabled"), false);
	}

	layout ();
}
