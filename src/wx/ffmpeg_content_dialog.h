/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

class wxSpinCtrl;
class FFmpegContent;
class AudioMappingView;

class FFmpegContentDialog : public wxDialog
{
public:
	FFmpegContentDialog (wxWindow *, boost::shared_ptr<FFmpegContent>);

private:
	void audio_stream_changed (wxCommandEvent &);
	void subtitle_stream_changed (wxCommandEvent &);

	boost::weak_ptr<FFmpegContent> _content;
	wxChoice* _audio_stream;
	wxStaticText* _audio_description;
	wxChoice* _subtitle_stream;
	AudioMappingView* _audio_mapping;
};
