/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#include "film_viewer.h"
#include "gl_video_view.h"
#include "system_information_dialog.h"
#include "wx_util.h"


#ifdef DCPOMATIC_OSX
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>
#else
#include <GL/glu.h>
#include <GL/glext.h>
#endif


using std::string;
using std::weak_ptr;
using std::shared_ptr;


#if wxCHECK_VERSION(3, 1, 0)

SystemInformationDialog::SystemInformationDialog(wxWindow* parent, FilmViewer const& viewer)
	: TableDialog (parent, _("System information"), 2, 1, false)
{
	auto gl = std::dynamic_pointer_cast<const GLVideoView>(viewer.video_view());

	if (!gl) {
		add (_("OpenGL version"), true);
		add (_("unknown (OpenGL not enabled in DCP-o-matic)"), false);
	} else {
		auto information = gl->information();
		auto add_string = [this, &information](GLenum name, wxString label) {
			add (label, true);
			auto i = information.find(name);
			if (i != information.end()) {
				add (std_to_wx(i->second), false);
			} else {
				add (_("unknown"), false);
			}
		};

		add_string (GL_VENDOR, _("Vendor"));
		add_string (GL_RENDERER, _("Renderer"));
		add_string (GL_VERSION, _("Version"));
		add_string (GL_SHADING_LANGUAGE_VERSION, _("Shading language version"));

		add (_("vsync"), true);
		add (gl->vsync_enabled() ? _("enabled") : _("not enabled"), false);
	}

	layout ();
}

#else

SystemInformationDialog::SystemInformationDialog(wxWindow* parent, FilmViewer const&)
	: TableDialog (parent, _("System information"), 2, 1, false)
{
	add (_("OpenGL version"), true);
	add (_("OpenGL renderer not supported by this DCP-o-matic version"), false);
}

#endif
