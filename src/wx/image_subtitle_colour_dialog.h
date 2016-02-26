/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "table_dialog.h"
#include "lib/rgba.h"
#include <map>

class RGBAColourPicker;
class FFmpegContent;
class FFmpegSubtitleStream;

class ImageSubtitleColourDialog : public TableDialog
{
public:
	ImageSubtitleColourDialog (wxWindow* parent, boost::shared_ptr<FFmpegContent> content, boost::shared_ptr<FFmpegSubtitleStream> stream);

	void apply ();

private:
	boost::shared_ptr<FFmpegContent> _content;
	boost::shared_ptr<FFmpegSubtitleStream> _stream;
	std::map<RGBA, RGBAColourPicker*> _pickers;
};
