/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/signals2.hpp>

namespace dcp {
	class OpenJPEGImage;
}

class PlayerVideo;
class Image;
class Film;
class FilmViewer;

class VideoWaveformPlot : public wxPanel
{
public:
	VideoWaveformPlot (wxWindow* parent, boost::weak_ptr<const Film> film, boost::weak_ptr<FilmViewer> viewer);

	void set_enabled (bool e);
	void set_component (int c);
	void set_contrast (int b);

	/** Emitted when the mouse is moved over the waveform.  The parameters
	    are:
	    - (int, int): image x range
	    - (int, int): component value range
	*/
	boost::signals2::signal<void (int, int, int, int)> MouseMoved;

private:
	void paint ();
	void sized (wxSizeEvent &);
	void create_waveform ();
	void set_image (boost::shared_ptr<PlayerVideo>);
	void mouse_moved (wxMouseEvent &);

	boost::weak_ptr<const Film> _film;
	boost::shared_ptr<dcp::OpenJPEGImage> _image;
	boost::shared_ptr<const Image> _waveform;
	bool _dirty;
	bool _enabled;
	int _component;
	int _contrast;

	static int const _vertical_margin;
	static int const _pixel_values;
	static int const _x_axis_width;

	boost::signals2::connection _viewer_connection;
};
