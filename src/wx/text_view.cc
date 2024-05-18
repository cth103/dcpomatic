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


#include "film_viewer.h"
#include "text_view.h"
#include "wx_util.h"
#include "lib/audio_decoder.h"
#include "lib/config.h"
#include "lib/content_text.h"
#include "lib/film.h"
#include "lib/string_text_file_content.h"
#include "lib/string_text_file_decoder.h"
#include "lib/text_decoder.h"
#include "lib/video_decoder.h"


using std::dynamic_pointer_cast;
using std::list;
using std::shared_ptr;
using boost::bind;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


WindowMetrics TextView::_metrics;


TextView::TextView (
	wxWindow* parent, shared_ptr<Film> film, shared_ptr<Content> content, shared_ptr<TextContent> text, shared_ptr<Decoder> decoder, FilmViewer& viewer
	)
	: wxDialog(parent, wxID_ANY, _("Captions"), _metrics.position, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	, _content (content)
	, _film_viewer (viewer)
{
	_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, _metrics.size, wxLC_REPORT | wxLC_SINGLE_SEL);

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
		ip.SetText (_("Caption"));
		ip.SetWidth (640);
		_list->InsertColumn (2, ip);
	}

	auto sizer = new wxBoxSizer (wxVERTICAL);
	sizer->Add (_list, 1, wxEXPAND | wxALL, DCPOMATIC_SIZER_X_GAP);

	_list->Bind (wxEVT_LIST_ITEM_SELECTED, boost::bind (&TextView::subtitle_selected, this, _1));

	auto buttons = CreateSeparatedButtonSizer (wxOK);
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

	/* Find the decoder that is being used for our TextContent and attach to it */
	for (auto i: decoder->text) {
		if (i->content() == text) {
			i->PlainStart.connect (bind (&TextView::data_start, this, _1));
			i->Stop.connect (bind (&TextView::data_stop, this, _1));
		}
	}
	while (!decoder->pass ()) {}
	SetSizerAndFit (sizer);

	_list->Bind(wxEVT_SIZE, boost::bind(&TextView::list_sized, this, _1));
	Bind(wxEVT_MOVE, boost::bind(&TextView::moved, this, _1));
}


void
TextView::list_sized(wxSizeEvent& ev)
{
	_metrics.size = ev.GetSize();
	ev.Skip();
}


void
TextView::moved(wxMoveEvent& ev)
{
	_metrics.position = ClientToScreen({0, 0});
	ev.Skip();
}


void
TextView::show()
{
	_metrics.show(this);
}

void
TextView::data_start (ContentStringText cts)
{
	for (auto const& i: cts.subs) {
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
TextView::data_stop (ContentTime time)
{
	if (!_last_count) {
		return;
	}

	for (int i = _subs - *_last_count; i < _subs; ++i) {
		_list->SetItem (i, 1, std_to_wx (time.timecode (_frc->source)));
	}
}


void
TextView::subtitle_selected (wxListEvent& ev)
{
	if (!Config::instance()->jump_to_selected ()) {
		return;
	}

	DCPOMATIC_ASSERT (ev.GetIndex() < int(_start_times.size()));
	auto lc = _content.lock ();
	DCPOMATIC_ASSERT (lc);
	/* Add on a frame here to work around any rounding errors and make sure land in the subtitle */
	_film_viewer.seek(lc, _start_times[ev.GetIndex()] + ContentTime::from_frames(1, _frc->source), true);
}
