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
#include <boost/shared_ptr.hpp>
#include "lib/dci_metadata.h"

class Film;

class DCIMetadataDialog : public wxDialog
{
public:
	DCIMetadataDialog (wxWindow *, DCIMetadata);

	DCIMetadata dci_metadata () const;

private:
	wxTextCtrl* _audio_language;
	wxTextCtrl* _subtitle_language;
	wxTextCtrl* _territory;
	wxTextCtrl* _rating;
	wxTextCtrl* _studio;
	wxTextCtrl* _facility;
	wxTextCtrl* _package_type;
};
