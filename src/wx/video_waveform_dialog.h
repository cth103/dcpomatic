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

#include <wx/wx.h>
#include <boost/weak_ptr.hpp>

class VideoWaveformPlot;
class FilmViewer;
class Film;

class VideoWaveformDialog : public wxDialog
{
public:
	VideoWaveformDialog (wxWindow* parent, boost::weak_ptr<const Film> film, FilmViewer* viewer);

private:
	void shown (wxShowEvent &);
	void component_changed ();
	void contrast_changed ();
	void mouse_moved (int x1, int x2, int y1, int y2);

	FilmViewer* _viewer;
	VideoWaveformPlot* _plot;
	wxChoice* _component;
	wxSlider* _contrast;
	wxStaticText* _x_position;
	wxStaticText* _value;
};
