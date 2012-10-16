/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <wx/dialog.h>
#include <wx/textctrl.h>

class Film;

class DCINameDialog : public wxDialog
{
public:
	DCINameDialog (wxWindow *, Film *);

private:
	void audio_language_changed (wxCommandEvent &);
	void subtitle_language_changed (wxCommandEvent &);
	void territory_changed (wxCommandEvent &);
	void rating_changed (wxCommandEvent &);
	void studio_changed (wxCommandEvent &);
	void facility_changed (wxCommandEvent &);
	void package_type_changed (wxCommandEvent &);
	
	wxTextCtrl* _audio_language;
	wxTextCtrl* _subtitle_language;
	wxTextCtrl* _territory;
	wxTextCtrl* _rating;
	wxTextCtrl* _studio;
	wxTextCtrl* _facility;
	wxTextCtrl* _package_type;

	Film* _film;
};
