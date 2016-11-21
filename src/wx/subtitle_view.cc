/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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
#include "lib/film.h"
#include "lib/text_subtitle_content.h"
#include "subtitle_view.h"
#include "wx_util.h"

using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

SubtitleView::SubtitleView (wxWindow* parent, shared_ptr<Film> film, shared_ptr<Decoder> decoder, DCPTime position)
	: wxDialog (parent, wxID_ANY, _("Subtitles"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
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

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

#if 0
	XXX

	list<ContentTextSubtitle> subs = decoder->subtitle->get_text (ContentTimePeriod (ContentTime(), ContentTime::max ()), true, true);
	FrameRateChange const frc = film->active_frame_rate_change (position);
	int n = 0;
	for (list<ContentTextSubtitle>::const_iterator i = subs.begin(); i != subs.end(); ++i) {
		for (list<dcp::SubtitleString>::const_iterator j = i->subs.begin(); j != i->subs.end(); ++j) {
			wxListItem list_item;
			list_item.SetId (n);
			_list->InsertItem (list_item);
			ContentTimePeriod const p = i->period ();
			_list->SetItem (n, 0, std_to_wx (p.from.timecode (frc.source)));
			_list->SetItem (n, 1, std_to_wx (p.to.timecode (frc.source)));
			_list->SetItem (n, 2, std_to_wx (j->text ()));
			++n;
		}
	}
#endif

	SetSizerAndFit (sizer);
}
