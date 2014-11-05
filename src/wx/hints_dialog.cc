/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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

#include <boost/algorithm/string.hpp>
#include <wx/richtext/richtextctrl.h>
#include "lib/film.h"
#include "lib/ratio.h"
#include "hints_dialog.h"

using boost::shared_ptr;
using boost::dynamic_pointer_cast;

HintsDialog::HintsDialog (wxWindow* parent, boost::weak_ptr<Film> f)
	: wxDialog (parent, wxID_ANY, _("Hints"))
	, _film (f)
{
	wxBoxSizer* sizer = new wxBoxSizer (wxVERTICAL);
	_text = new wxRichTextCtrl (this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize (400, 300), wxRE_READONLY);
	sizer->Add (_text, 1, wxEXPAND | wxALL, 6);

	wxSizer* buttons = CreateSeparatedButtonSizer (wxOK);
	if (buttons) {
		sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}

	SetSizer (sizer);
	sizer->Layout ();
	sizer->SetSizeHints (this);

	_text->GetCaret()->Hide ();

	boost::shared_ptr<Film> film = _film.lock ();
	if (film) {
		film->Changed.connect (boost::bind (&HintsDialog::film_changed, this));
		film->ContentChanged.connect (boost::bind (&HintsDialog::film_changed, this));
	}

	film_changed ();
}

void
HintsDialog::film_changed ()
{
	_text->Clear ();
	bool hint = false;
	
	boost::shared_ptr<Film> film = _film.lock ();
	if (!film) {
		return;
	}

	_text->BeginStandardBullet (N_("standard/circle"), 1, 50);
	if (film->audio_channels() % 2) {
		hint = true;
		_text->WriteText (_("Your DCP has an odd number of audio channels.  This is very likely to cause problems on playback."));
		_text->Newline ();
	} else if (film->audio_channels() < 6) {
		_text->WriteText (_("Your DCP has fewer than 6 audio channels.  This may cause problems on some projectors."));
		_text->Newline ();
	} else if (film->audio_channels() == 0) {
		/* Carsten Kurz reckons having no audio can be a problem */
		hint = true;
		_text->WriteText (_("Your DCP has no audio channels.  This is likely to cause problems on playback."));
		_text->Newline ();
	}

	ContentList content = film->content ();
	int flat_or_narrower = 0;
	int scope = 0;
	for (ContentList::const_iterator i = content.begin(); i != content.end(); ++i) {
		shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (*i);
		if (vc) {
			Ratio const * r = vc->scale().ratio ();
			if (r && r->id() == "239") {
				++scope;
			} else if (r && r->id() != "239" && r->id() != "full-frame") {
				++flat_or_narrower;
			}
		}
	}

	if (scope && !flat_or_narrower && film->container()->id() == "185") {
		hint = true;
		_text->WriteText (_("All of your content is in Scope (2.39:1) but your DCP's container is Flat (1.85:1).  This will letter-box your content inside a Flat (1.85:1) frame.  You may prefer to set your DCP's container to Scope (2.39:1) in the \"DCP\" tab."));
		_text->Newline ();
	}

	if (!scope && flat_or_narrower && film->container()->id() == "239") {
		hint = true;
		_text->WriteText (_("All of your content is at 1.85:1 or narrower but your DCP's container is Scope (2.39:1).  This will pillar-box your content inside a Flat (1.85:1) frame.  You may prefer to set your DCP's container to Flat (1.85:1) in the \"DCP\" tab."));
		_text->Newline ();
	}
	
	if (film->video_frame_rate() != 24 && film->video_frame_rate() != 48) {
		hint = true;
		_text->WriteText (wxString::Format (_("Your DCP frame rate (%d fps) may cause problems in a few (mostly older) projectors.  Use 24 or 48 frames per second to be on the safe side."), film->video_frame_rate()));
		_text->Newline ();
	}

	if (film->j2k_bandwidth() >= 245000000) {
		hint = true;
		_text->WriteText (_("A few projectors have problems playing back very high bit-rate DCPs.  It is a good idea to drop the JPEG2000 bandwidth down to about 200Mbit/s; this is unlikely to have any visible effect on the image."));
		_text->Newline ();
	}

	int vob = 0;
	for (ContentList::const_iterator i = content.begin(); i != content.end(); ++i) {
		if (boost::algorithm::starts_with ((*i)->path(0).filename().string(), "VTS_")) {
			++vob;
		}
	}

	if (vob > 1) {
		hint = true;
		_text->WriteText (wxString::Format (_("You have %d files that look like they are VOB files from DVD. You should join them to ensure smooth joins between the files."), vob));
		_text->Newline ();
	}

	_text->EndSymbolBullet ();

	if (!hint) {
		_text->WriteText (_("There are no hints: everything looks good!"));
	}
}
