/*
    Copyright (C) 2015-2018 Carl Hetherington <cth@carlh.net>

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

#include "timecode.h"
#include "lib/rgba.h"
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>

class wxRadioButton;
class wxColourPickerCtrl;
class wxGridBagSizer;
class Content;
class RGBAColourPicker;
class FFmpegSubtitleStream;

class SubtitleAppearanceDialog : public wxDialog
{
public:
	SubtitleAppearanceDialog (wxWindow* parent, boost::shared_ptr<Content> content);

	void apply ();

private:
	void setup_sensitivity ();
	void restore ();

	wxCheckBox* _force_colour;
	wxColourPickerCtrl* _colour;
	wxChoice* _effect;
	wxColourPickerCtrl* _effect_colour;
	Timecode<ContentTime>* _fade_in;
	Timecode<ContentTime>* _fade_out;
	wxSpinCtrl* _outline_width;
	wxGridBagSizer* _table;
	std::map<RGBA, RGBAColourPicker*> _pickers;

	boost::shared_ptr<Content> _content;
	boost::shared_ptr<FFmpegSubtitleStream> _stream;

	boost::signals2::scoped_connection _content_connection;

	static int const NONE;
	static int const OUTLINE;
	static int const SHADOW;
};
