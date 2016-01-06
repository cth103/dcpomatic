/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include <dcp/util.h>

template <class T>
class FileDialogWrapper
{
public:
	FileDialogWrapper (wxWindow* parent, wxString title)
		: _parent (parent)
	{
		_dialog = new wxFileDialog (parent, title);
	}

	void set (T) {}

	T get ()
	{
		return T (dcp::file_to_string (wx_to_std (_dialog->GetPath ())));
	}

	int ShowModal ()
	{
		return _dialog->ShowModal ();
	}

	void Destroy ()
	{
		_dialog->Destroy ();
		/* eek! */
		delete this;
	}

private:
	wxWindow* _parent;
	wxFileDialog* _dialog;
};
