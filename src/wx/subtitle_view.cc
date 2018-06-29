/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#include "lib/text_subtitle_decoder.h"
#include "lib/content_subtitle.h"
#include "lib/video_decoder.h"
#include "lib/audio_decoder.h"
#include "lib/film.h"
#include "lib/text_subtitle_content.h"
#include "lib/config.h"
#include "subtitle_view.h"
#include "film_viewer.h"
#include "wx_util.h"

using std::list;
using boost::shared_ptr;
using boost::bind;
using boost::dynamic_pointer_cast;

SubtitleView::SubtitleView (wxWindow* parent, shared_ptr<Film> film, shared_ptr<Content> content, shared_ptr<Decoder> decoder, FilmViewer* viewer)
	: wxDialog (parent, wxID_ANY, _("Subtitles"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	, _content (content)
	, _film_viewer (viewer)
{
	_list = new wxListCtrl (this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);

	{
		wxListItem ip;
		ip.SetId (0);
		ip.SetText (_("Start"));
		ip.SetWidth (100);
		_list->InsertColumn (0, ip);
	}

	{
		wxListItem ip;
		ip.SetId (1);
		ip.SetText (_("End"));
		ip.SetWidth (100);
		_list->InsertColumn (1, ip);
	}

	{
		wxListItem ip;
		ip.SetId (2);
		ip.SetText (_("Subtitle"));
		ip.SetWidth (640);
		_list->InsertColumn (2, ip);
	}

	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	sizer->Add (_list, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);

	_list->Bind (wxEVT_LIST_ITEM_SELECTED, boost::bind (&SubtitleView::subtitle_selected, this, _1));

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	if (decoder->video) {
		decoder->video->set_ignore (true);
	}
	if (decoder->audio) {
		decoder->audio->set_ignore (true);
	}

	_subs = 0;
	_frc = film->active_frame_rate_change (content->position());
	decoder->subtitle->TextStart.connect (bind (&SubtitleView::data_start, this, _1));
	decoder->subtitle->Stop.connect (bind (&SubtitleView::data_stop, this, _1));
	while (!decoder->pass ()) {}
	SetSizerAndFit (sizer);
}

void
SubtitleView::data_start (ContentTextSubtitle cts)
{
	BOOST_FOREACH (dcp::SubtitleString const & i, cts.subs) {
		wxListItem list_item;
		list_item.SetId (_subs);
		_list->InsertItem (list_item);
		_list->SetItem (_subs, 0, std_to_wx (cts.from().timecode (_frc->source)));
		_list->SetItem (_subs, 2, std_to_wx (i.text ()));
		_start_times.push_back (cts.from ());
		++_subs;
	}

	_last_count = cts.subs.size ();
}

void
SubtitleView::data_stop (ContentTime time)
{
	if (!_last_count) {
		return;
	}

	for (int i = _subs - *_last_count; i < _subs; ++i) {
		_list->SetItem (i, 1, std_to_wx (time.timecode (_frc->source)));
	}
}

void
SubtitleView::subtitle_selected (wxListEvent& ev)
{
	if (!Config::instance()->jump_to_selected ()) {
		return;
	}

	DCPOMATIC_ASSERT (ev.GetIndex() < int(_start_times.size()));
	shared_ptr<Content> locked = _content.lock ();
	DCPOMATIC_ASSERT (locked);
	_film_viewer->set_position (locked, _start_times[ev.GetIndex()]);
}
