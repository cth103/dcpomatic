/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "video_waveform_dialog.h"
#include "video_waveform_plot.h"
#include "film_viewer.h"
#include "wx_util.h"
#include <boost/bind.hpp>
#include <iostream>

using std::cout;
using boost::bind;
using boost::weak_ptr;

VideoWaveformDialog::VideoWaveformDialog (wxWindow* parent, weak_ptr<const Film> film, FilmViewer* viewer)
	: wxDialog (
		parent,
		wxID_ANY,
		_("Video Waveform"),
		wxDefaultPosition,
		wxSize (640, 512),
#ifdef DCPOMATIC_OSX
		/* I can't get wxFRAME_FLOAT_ON_PARENT to work on OS X, and although wxSTAY_ON_TOP keeps
		   the window above all others (and not just our own) it's better than nothing for now.
		*/
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxSTAY_ON_TOP
#else
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxFULL_REPAINT_ON_RESIZE | wxFRAME_FLOAT_ON_PARENT
#endif
		)
	, _viewer (viewer)
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);

	wxBoxSizer* controls = new wxBoxSizer (wxHORIZONTAL);

	_component = new wxChoice (this, wxID_ANY);
	_component->Append (wxT ("X"));
	_component->Append (wxT ("Y"));
	_component->Append (wxT ("Z"));
	add_label_to_sizer (controls, this, _("Component"), true);
	controls->Add (_component, 1, wxALL, DCPOMATIC_SIZER_X_GAP);

	add_label_to_sizer (controls, this, _("Contrast"), true);
	_contrast = new wxSlider (this, wxID_ANY, 0, 0, 256);
	controls->Add (_contrast, 1, wxALL, DCPOMATIC_SIZER_X_GAP);

	overall_sizer->Add (controls, 0, wxALL | wxEXPAND, DCPOMATIC_SIZER_X_GAP);

	wxBoxSizer* position = new wxBoxSizer (wxHORIZONTAL);
	add_label_to_sizer (position, this, _("Image X position"), true);
	_x_position = new wxStaticText (this, wxID_ANY, "");
	_x_position->SetMinSize (wxSize (64, -1));
	position->Add (_x_position, 0, wxALL, DCPOMATIC_SIZER_X_GAP);
	add_label_to_sizer (position, this, _("component value"), true);
	_value = new wxStaticText (this, wxID_ANY, "");
	_value->SetMinSize (wxSize (64, -1));
	position->Add (_value, 0, wxALL, DCPOMATIC_SIZER_X_GAP);
	overall_sizer->Add (position, 0, wxEXPAND | wxALL, DCPOMATIC_SIZER_Y_GAP);

	_plot = new VideoWaveformPlot (this, film, _viewer);
	overall_sizer->Add (_plot, 1, wxALL | wxEXPAND, 12);

#ifdef DCPOMATIC_LINUX
	wxSizer* buttons = CreateSeparatedButtonSizer (wxCLOSE);
	if (buttons) {
		overall_sizer->Add (buttons, wxSizerFlags().Expand().DoubleBorder());
	}
#endif

	SetSizer (overall_sizer);
	overall_sizer->Layout ();
	overall_sizer->SetSizeHints (this);

	Bind (wxEVT_SHOW, bind (&VideoWaveformDialog::shown, this, _1));
	_component->Bind (wxEVT_CHOICE, bind (&VideoWaveformDialog::component_changed, this));
	_contrast->Bind (wxEVT_SCROLL_THUMBTRACK, bind (&VideoWaveformDialog::contrast_changed, this));
	_plot->MouseMoved.connect (bind (&VideoWaveformDialog::mouse_moved, this, _1, _2, _3, _4));

	_component->SetSelection (0);
	_contrast->SetValue (32);

	component_changed ();
	contrast_changed ();
}

void
VideoWaveformDialog::shown (wxShowEvent& ev)
{
	_plot->set_enabled (ev.IsShown ());
	if (ev.IsShown ()) {
		_viewer->slow_refresh ();
	}
}

void
VideoWaveformDialog::component_changed ()
{
	_plot->set_component (_component->GetCurrentSelection ());
}

void
VideoWaveformDialog::contrast_changed ()
{
	_plot->set_contrast (_contrast->GetValue ());
}

void
VideoWaveformDialog::mouse_moved (int x1, int x2, int y1, int y2)
{
	if (x1 != x2) {
		_x_position->SetLabel (wxString::Format ("%d-%d", x1, x2));
	} else {
		_x_position->SetLabel (wxString::Format ("%d", x1));
	}

	if (y1 != y2) {
		_value->SetLabel (wxString::Format ("%d-%d", y1, y2));
	} else {
		_value->SetLabel (wxString::Format ("%d", y1));
	}
}
