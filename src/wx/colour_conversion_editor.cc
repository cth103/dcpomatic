/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "wx_util.h"
#include "colour_conversion_editor.h"
#include "lib/colour_conversion.h"
#include <dcp/locale_convert.h>
#include <dcp/gamma_transfer_function.h>
#include <dcp/identity_transfer_function.h>
#include <dcp/s_gamut3_transfer_function.h>
#include <dcp/modified_gamma_transfer_function.h>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>
#include <iostream>

using std::string;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::bind;
using dcp::locale_convert;

int const ColourConversionEditor::INPUT_GAMMA = 0;
int const ColourConversionEditor::INPUT_GAMMA_LINEARISED = 1;
int const ColourConversionEditor::INPUT_SGAMUT3 = 2;

ColourConversionEditor::ColourConversionEditor (wxWindow* parent, bool yuv)
	: wxPanel (parent, wxID_ANY)
	, _ignore_chromaticity_changed (false)
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_Y_GAP - 3, DCPOMATIC_SIZER_X_GAP);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	int r = 0;

	subhead (table, this, _("Input gamma correction"), r);

	add_label_to_sizer (table, this, _("Input transfer function"), true, wxGBPosition (r, 0));
	_input = new wxChoice (this, wxID_ANY);
	_input->Append (_("Gamma"));
	_input->Append (_("Gamma, linearised for small values"));
	_input->Append (_("S-Gamut3"));
	table->Add (_input, wxGBPosition (r, 1), wxGBSpan (1, 2));
	++r;

	add_label_to_sizer (table, this, _("Input gamma"), true, wxGBPosition (r, 0));
	_input_gamma = new wxSpinCtrlDouble (this);
	table->Add (_input_gamma, wxGBPosition (r, 1));
	++r;

	add_label_to_sizer (table, this, _("Input power"), true, wxGBPosition (r, 0));
	{
		wxBoxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		_input_power = new wxSpinCtrlDouble (this);
		s->Add (_input_power, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);
		add_label_to_sizer (s, this, _("threshold"), true);
		_input_threshold = new wxTextCtrl (this, wxID_ANY, wxT (""));
		s->Add (_input_threshold, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);
		add_label_to_sizer (s, this, _("A"), true);
		_input_A = new wxTextCtrl (this, wxID_ANY, wxT (""));
		s->Add (_input_A, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);
		add_label_to_sizer (s, this, _("B"), true);
		_input_B = new wxTextCtrl (this, wxID_ANY, wxT (""));
		s->Add (_input_B, 1, wxEXPAND | wxRIGHT, DCPOMATIC_SIZER_GAP);
		table->Add (s, wxGBPosition (r, 1), wxGBSpan (1, 3));
	}
	++r;

        wxClientDC dc (parent);
        wxSize size = dc.GetTextExtent (wxT ("-0.12345678901"));
        size.SetHeight (-1);

        wxTextValidator validator (wxFILTER_INCLUDE_CHAR_LIST);
        wxArrayString list;

        wxString n (wxT ("0123456789.-"));
        for (size_t i = 0; i < n.Length(); ++i) {
                list.Add (n[i]);
        }

        validator.SetIncludes (list);


	/* YUV to RGB conversion */

	wxStaticText* yuv_heading = subhead (table, this, _("YUV to RGB conversion"), r);

	wxStaticText* yuv_label = add_label_to_sizer (table, this, _("YUV to RGB matrix"), true, wxGBPosition (r, 0));
	_yuv_to_rgb = new wxChoice (this, wxID_ANY);
	_yuv_to_rgb->Append (_("Rec. 601"));
	_yuv_to_rgb->Append (_("Rec. 709"));
	table->Add (_yuv_to_rgb, wxGBPosition (r, 1));
	++r;

	if (!yuv) {
		yuv_heading->Enable (false);
		yuv_label->Enable (false);
		_yuv_to_rgb->Enable (false);
	}

	/* RGB to XYZ conversion */

	subhead (table, this, _("RGB to XYZ conversion"), r);

	add_label_to_sizer (table, this, _("x"), false, wxGBPosition (r, 1));
	add_label_to_sizer (table, this, _("y"), false, wxGBPosition (r, 2));
	add_label_to_sizer (table, this, _("Matrix"), false, wxGBPosition (r, 3));
	++r;

	add_label_to_sizer (table, this, _("Red chromaticity"), true, wxGBPosition (r, 0));
	_red_x = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_red_x, wxGBPosition (r, 1));
	_red_y = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_red_y, wxGBPosition (r, 2));
	++r;

	add_label_to_sizer (table, this, _("Green chromaticity"), true, wxGBPosition (r, 0));
	_green_x = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_green_x, wxGBPosition (r, 1));
	_green_y = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_green_y, wxGBPosition (r, 2));
	++r;

	add_label_to_sizer (table, this, _("Blue chromaticity"), true, wxGBPosition (r, 0));
	_blue_x = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_blue_x, wxGBPosition (r, 1));
	_blue_y = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_blue_y, wxGBPosition (r, 2));
	++r;

	add_label_to_sizer (table, this, _("White point"), true, wxGBPosition (r, 0));
	_white_x = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_white_x, wxGBPosition (r, 1));
	_white_y = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_white_y, wxGBPosition (r, 2));
	++r;

        size = dc.GetTextExtent (wxT ("0.12345678"));
        size.SetHeight (-1);

	wxFlexGridSizer* rgb_to_xyz_sizer = new wxFlexGridSizer (3, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			_rgb_to_xyz[i][j] = new wxStaticText (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0);
			rgb_to_xyz_sizer->Add (_rgb_to_xyz[i][j]);
		}
	}
	table->Add (rgb_to_xyz_sizer, wxGBPosition (r - 4, 3), wxGBSpan (4, 1));

	/* White point adjustment */

        size = dc.GetTextExtent (wxT ("-0.12345678901"));
        size.SetHeight (-1);

	subhead (table, this, _("White point adjustment"), r);

	_adjust_white = new wxCheckBox (this, wxID_ANY, _("Adjust white point to"));
	table->Add (_adjust_white, wxGBPosition (r, 0));
	_adjusted_white_x = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_adjusted_white_x, wxGBPosition (r, 1));
	_adjusted_white_y = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_adjusted_white_y, wxGBPosition (r, 2));
	++r;

	add_label_to_sizer (table, this, wxT (""), false, wxGBPosition (r, 0));
	++r;

        size = dc.GetTextExtent (wxT ("0.12345678"));
        size.SetHeight (-1);

	wxFlexGridSizer* bradford_sizer = new wxFlexGridSizer (3, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			_bradford[i][j] = new wxStaticText (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0);
			bradford_sizer->Add (_bradford[i][j]);
		}
	}
	table->Add (bradford_sizer, wxGBPosition (r - 2, 3), wxGBSpan (2, 1));
	++r;

	/* Output transfer function */

	_output = new wxCheckBox (this, wxID_ANY, _("Inverse 2.6 gamma correction on output"));
	table->Add (_output, wxGBPosition (r, 0), wxGBSpan (1, 2));

	_input_gamma->SetRange (0.1, 4.0);
	_input_gamma->SetDigits (2);
	_input_gamma->SetIncrement (0.1);
	_input_power->SetRange (0.1, 4.0);
	_input_power->SetDigits (6);
	_input_power->SetIncrement (0.1);

	_input->Bind (wxEVT_CHOICE, bind (&ColourConversionEditor::changed, this));
	_input_gamma->Bind (wxEVT_SPINCTRLDOUBLE, bind (&ColourConversionEditor::changed, this, _input_gamma));
	_input_power->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::changed, this));
	_input_threshold->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::changed, this));
	_input_A->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::changed, this));
	_input_B->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::changed, this));
	_red_x->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::chromaticity_changed, this));
	_red_y->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::chromaticity_changed, this));
	_green_x->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::chromaticity_changed, this));
	_green_y->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::chromaticity_changed, this));
	_blue_x->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::chromaticity_changed, this));
	_blue_y->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::chromaticity_changed, this));
	_white_x->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::chromaticity_changed, this));
	_white_y->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::chromaticity_changed, this));
	_adjust_white->Bind (wxEVT_CHECKBOX, bind (&ColourConversionEditor::adjusted_white_changed, this));
	_adjusted_white_x->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::adjusted_white_changed, this));
	_adjusted_white_y->Bind (wxEVT_TEXT, bind (&ColourConversionEditor::adjusted_white_changed, this));
	_yuv_to_rgb->Bind (wxEVT_CHOICE, bind (&ColourConversionEditor::changed, this));
	_output->Bind (wxEVT_CHECKBOX, bind (&ColourConversionEditor::changed, this));
}

wxStaticText *
ColourConversionEditor::subhead (wxGridBagSizer* sizer, wxWindow* parent, wxString text, int& row) const
{
	wxStaticText* m = new wxStaticText (parent, wxID_ANY, text);
	wxFont font (*wxNORMAL_FONT);
	font.SetWeight (wxFONTWEIGHT_BOLD);
	m->SetFont (font);
	sizer->Add (m, wxGBPosition (row, 0), wxGBSpan (1, 3), wxALIGN_CENTER_VERTICAL | wxTOP, 12);
	++row;
	return m;
}

void
ColourConversionEditor::set (ColourConversion conversion)
{
	if (dynamic_pointer_cast<const dcp::GammaTransferFunction> (conversion.in ())) {
		shared_ptr<const dcp::GammaTransferFunction> tf = dynamic_pointer_cast<const dcp::GammaTransferFunction> (conversion.in ());
		checked_set (_input, 0);
		set_spin_ctrl (_input_gamma, tf->gamma ());
	} else if (dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction> (conversion.in ())) {
		shared_ptr<const dcp::ModifiedGammaTransferFunction> tf = dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction> (conversion.in ());
		checked_set (_input, 1);
		/* Arbitrary default; not used in this case (greyed out) */
		_input_gamma->SetValue (2.2);
		set_spin_ctrl (_input_power, tf->power ());
		set_text_ctrl (_input_threshold, tf->threshold ());
		set_text_ctrl (_input_A, tf->A ());
		set_text_ctrl (_input_B, tf->B ());
	} else if (dynamic_pointer_cast<const dcp::SGamut3TransferFunction> (conversion.in ())) {
		checked_set (_input, 2);
	}

	_yuv_to_rgb->SetSelection (conversion.yuv_to_rgb ());

	_ignore_chromaticity_changed = true;

	char buffer[256];
	snprintf (buffer, sizeof (buffer), "%.6f", conversion.red().x);
	_red_x->SetValue (std_to_wx (buffer));
	snprintf (buffer, sizeof (buffer), "%.6f", conversion.red().y);
	_red_y->SetValue (std_to_wx (buffer));
	snprintf (buffer, sizeof (buffer), "%.6f", conversion.green().x);
	_green_x->SetValue (std_to_wx (buffer));
	snprintf (buffer, sizeof (buffer), "%.6f", conversion.green().y);
	_green_y->SetValue (std_to_wx (buffer));
	snprintf (buffer, sizeof (buffer), "%.6f", conversion.blue().x);
	_blue_x->SetValue (std_to_wx (buffer));
	snprintf (buffer, sizeof (buffer), "%.6f", conversion.blue().y);
	_blue_y->SetValue (std_to_wx (buffer));
	snprintf (buffer, sizeof (buffer), "%.6f", conversion.white().x);
	_white_x->SetValue (std_to_wx (buffer));
	snprintf (buffer, sizeof (buffer), "%.6f", conversion.white().y);
	_white_y->SetValue (std_to_wx (buffer));

	_ignore_chromaticity_changed = false;

	if (conversion.adjusted_white ()) {
		_adjust_white->SetValue (true);
		snprintf (buffer, sizeof (buffer), "%.6f", conversion.adjusted_white().get().x);
		_adjusted_white_x->SetValue (std_to_wx (buffer));
		snprintf (buffer, sizeof (buffer), "%.6f", conversion.adjusted_white().get().y);
		_adjusted_white_y->SetValue (std_to_wx (buffer));
	} else {
		_adjust_white->SetValue (false);
	}

	_output->SetValue (static_cast<bool> (dynamic_pointer_cast<const dcp::GammaTransferFunction> (conversion.out ())));

	update_rgb_to_xyz ();
	update_bradford ();
	changed ();
}

ColourConversion
ColourConversionEditor::get () const
{
	ColourConversion conversion;

	switch (_input->GetSelection ()) {
	case INPUT_GAMMA:
		conversion.set_in (
			shared_ptr<dcp::GammaTransferFunction> (new dcp::GammaTransferFunction (_input_gamma->GetValue ()))
			);
		break;
	case INPUT_GAMMA_LINEARISED:
		/* Linearised gamma */
		conversion.set_in (
			shared_ptr<dcp::ModifiedGammaTransferFunction> (
				new dcp::ModifiedGammaTransferFunction (
					_input_power->GetValue (),
					locale_convert<double> (wx_to_std (_input_threshold->GetValue ())),
					locale_convert<double> (wx_to_std (_input_A->GetValue ())),
					locale_convert<double> (wx_to_std (_input_B->GetValue ()))
					)
				)
			);
		break;
	case INPUT_SGAMUT3:
		/* SGamut3 */
		conversion.set_in (shared_ptr<dcp::SGamut3TransferFunction> (new dcp::SGamut3TransferFunction ()));
		break;
	}

	conversion.set_yuv_to_rgb (static_cast<dcp::YUVToRGB> (_yuv_to_rgb->GetSelection ()));

	conversion.set_red (
		dcp::Chromaticity (locale_convert<double> (wx_to_std (_red_x->GetValue ())), locale_convert<double> (wx_to_std (_red_y->GetValue ())))
		);
	conversion.set_green (
		dcp::Chromaticity (locale_convert<double> (wx_to_std (_green_x->GetValue ())), locale_convert<double> (wx_to_std (_green_y->GetValue ())))
		);
	conversion.set_blue (
		dcp::Chromaticity (locale_convert<double> (wx_to_std (_blue_x->GetValue ())), locale_convert<double> (wx_to_std (_blue_y->GetValue ())))
		);
	conversion.set_white (
		dcp::Chromaticity (locale_convert<double> (wx_to_std (_white_x->GetValue ())), locale_convert<double> (wx_to_std (_white_y->GetValue ())))
		);

	if (_adjust_white->GetValue ()) {
		conversion.set_adjusted_white (
			dcp::Chromaticity (
				locale_convert<double> (wx_to_std (_adjusted_white_x->GetValue ())),
				locale_convert<double> (wx_to_std (_adjusted_white_y->GetValue ()))
				)
			);
	} else {
		conversion.unset_adjusted_white ();
	}

	if (_output->GetValue ()) {
		conversion.set_out (shared_ptr<dcp::GammaTransferFunction> (new dcp::GammaTransferFunction (2.6)));
	} else {
		conversion.set_out (shared_ptr<dcp::IdentityTransferFunction> (new dcp::IdentityTransferFunction ()));
	}

	return conversion;
}

void
ColourConversionEditor::changed ()
{
	int const in = _input->GetSelection();
	_input_gamma->Enable (in == INPUT_GAMMA);
	_input_power->Enable (in == INPUT_GAMMA_LINEARISED);
	_input_threshold->Enable (in == INPUT_GAMMA_LINEARISED);
	_input_A->Enable (in == INPUT_GAMMA_LINEARISED);
	_input_B->Enable (in == INPUT_GAMMA_LINEARISED);

	Changed ();
}

void
ColourConversionEditor::chromaticity_changed ()
{
	if (_ignore_chromaticity_changed) {
		return;
	}

	update_rgb_to_xyz ();
	changed ();
}

void
ColourConversionEditor::adjusted_white_changed ()
{
	update_bradford ();
	changed ();
}

void
ColourConversionEditor::update_bradford ()
{
	_adjusted_white_x->Enable (_adjust_white->GetValue ());
	_adjusted_white_y->Enable (_adjust_white->GetValue ());

	boost::numeric::ublas::matrix<double> m = get().bradford ();
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			char buffer[256];
			snprintf (buffer, sizeof (buffer), "%.7f", m (i, j));
			_bradford[i][j]->SetLabel (std_to_wx (buffer));
		}
	}
}

void
ColourConversionEditor::update_rgb_to_xyz ()
{
	boost::numeric::ublas::matrix<double> m = get().rgb_to_xyz ();
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			char buffer[256];
			snprintf (buffer, sizeof (buffer), "%.7f", m (i, j));
			_rgb_to_xyz[i][j]->SetLabel (std_to_wx (buffer));
		}
	}
}

void
ColourConversionEditor::changed (wxSpinCtrlDouble* sc)
{
	/* On OS X, it seems that in some cases when a wxSpinCtrlDouble loses focus
	   it emits an erroneous changed signal, which messes things up.
	   Check for that here.
	*/
	if (fabs (_last_spin_ctrl_value[sc] - sc->GetValue()) < 1e-3) {
		return;
	}

	Changed ();
}

void
ColourConversionEditor::set_spin_ctrl (wxSpinCtrlDouble* control, double value)
{
	_last_spin_ctrl_value[control] = value;
	control->SetValue (value);
}

void
ColourConversionEditor::set_text_ctrl (wxTextCtrl* control, double value)
{
	char buffer[256];
	snprintf (buffer, sizeof (buffer), "%.7f", value);
	control->SetValue (std_to_wx (buffer));
}
