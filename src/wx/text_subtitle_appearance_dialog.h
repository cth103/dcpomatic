/*
    Copyright (C) 2015-2016 Carl Hetherington <cth@carlh.net>

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

#include "table_dialog.h"
#include "timecode.h"
#include <boost/shared_ptr.hpp>

class wxRadioButton;
class wxColourPickerCtrl;
class Content;

class TextSubtitleAppearanceDialog : public TableDialog
{
public:
	TextSubtitleAppearanceDialog (wxWindow* parent, boost::shared_ptr<Content> content);

	void apply ();

private:
	wxColourPickerCtrl* _colour;
	wxRadioButton* _outline;
	wxRadioButton* _shadow;
	wxColourPickerCtrl* _effect_colour;
	Timecode<ContentTime>* _fade_in;
	Timecode<ContentTime>* _fade_out;

	boost::shared_ptr<Content> _content;
};
