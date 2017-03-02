/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include <wx/dialog.h>
#include <boost/shared_ptr.hpp>

class wxTextCtrl;
class wxFlexGridSizer;
class wxCheckBox;
class Film;

class ReportProblemDialog : public wxDialog
{
public:
	ReportProblemDialog (wxWindow* parent, boost::shared_ptr<Film> film);

	void report ();

private:
	boost::shared_ptr<Film> _film;

	wxSizer* _overall_sizer;
	wxFlexGridSizer* _table;
	wxTextCtrl* _summary;
	wxCheckBox* _send_logs;
	wxTextCtrl* _email;
};
