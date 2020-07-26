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

#include "lib/warnings.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <list>

class Marker;
class Film;
class FilmViewer;

class MarkersDialog : public wxDialog
{
public:
	MarkersDialog (wxWindow* parent, boost::weak_ptr<Film> film, boost::weak_ptr<FilmViewer> viewer);

private:
	std::list<boost::shared_ptr<Marker> > _markers;
	boost::weak_ptr<Film> _film;
};
