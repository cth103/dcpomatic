/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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
#include <boost/signals2.hpp>

class wxRichTextCtrl;
class Film;

class HintsDialog : public wxDialog
{
public:
	HintsDialog (wxWindow* parent, boost::weak_ptr<Film>);

private:
	void film_changed ();

	boost::weak_ptr<Film> _film;
	wxRichTextCtrl* _text;

	boost::signals2::scoped_connection _film_changed_connection;
	boost::signals2::scoped_connection _film_content_changed_connection;
};
