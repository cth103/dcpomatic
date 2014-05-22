/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "lib/subrip_decoder.h"
#include "lib/content_subtitle.h"
#include "subtitle_view.h"

using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

SubtitleView::SubtitleView (wxWindow* parent, shared_ptr<SubRipContent> content)
	: wxDialog (parent, wxID_ANY, _("Subtitles"))
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
	sizer->Add (_list, 1, wxEXPAND);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK | wxCANCEL);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	shared_ptr<SubRipDecoder> decoder (new SubRipDecoder (content));
	list<shared_ptr<ContentTextSubtitle> > subs = decoder->get_text_subtitles (ContentTimePeriod (ContentTime(), ContentTime::max ()));
	int n = 0;
	for (list<shared_ptr<ContentTextSubtitle> >::const_iterator i = subs.begin(); i != subs.end(); ++i) {
		for (list<dcp::SubtitleString>::const_iterator j = (*i)->subs.begin(); j != (*i)->subs.end(); ++j) {
			wxListItem list_item;
			list_item.SetId (n);
			_list->InsertItem (list_item);
			_list->SetItem (n, 2, j->text ());
			++n;
		}
	}

	SetSizerAndFit (sizer);
}

