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
