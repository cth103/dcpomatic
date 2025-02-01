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


#include "lib/change_signaller.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>


class wxRichTextCtrl;
class Film;
class Hints;

class HintsDialog : public wxDialog
{
public:
	HintsDialog(wxWindow* parent, std::weak_ptr<Film>, bool ok);

private:
	void film_change(ChangeType);
	void film_content_change(ChangeType type);
	void shut_up(wxCommandEvent& ev);
	void update();
	void hint(std::string text);
	void pulse();
	void finished();
	void progress(std::string m);

	std::weak_ptr<Film> _film;
	wxGauge* _gauge;
	wxStaticText* _gauge_message;
	wxRichTextCtrl* _text;
	boost::scoped_ptr<Hints> _hints;
	std::list<std::string> _current;
	bool _finished;

	boost::signals2::scoped_connection _film_change_connection;
	boost::signals2::scoped_connection _film_content_change_connection;
	boost::signals2::scoped_connection _hints_hint_connection;
	boost::signals2::scoped_connection _hints_progress_connection;
	boost::signals2::scoped_connection _hints_pulse_connection;
	boost::signals2::scoped_connection _hints_finished_connection;
};
