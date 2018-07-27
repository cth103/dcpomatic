/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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
class Hints;

class HintsDialog : public wxDialog
{
public:
	HintsDialog (wxWindow* parent, boost::weak_ptr<Film>, bool ok);

private:
	void film_changed ();
	void shut_up (wxCommandEvent& ev);
	void update ();
	void hint (std::string text);
	void pulse ();
	void finished ();
	void progress (std::string m);

	boost::weak_ptr<Film> _film;
	wxGauge* _gauge;
	wxStaticText* _gauge_message;
	wxRichTextCtrl* _text;
	boost::shared_ptr<Hints> _hints;
	std::list<std::string> _current;

	boost::signals2::scoped_connection _film_changed_connection;
	boost::signals2::scoped_connection _film_content_changed_connection;
};
