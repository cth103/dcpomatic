/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/colour_conversion.h"
#include "lib/safe_stringstream.h"
#include "lib/raw_convert.h"
#include "wx_util.h"
#include "colour_conversion_editor.h"
#include <dcp/gamma_transfer_function.h>
#include <dcp/modified_gamma_transfer_function.h>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>
#include <boost/lexical_cast.hpp>

using std::string;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::lexical_cast;

ColourConversionEditor::ColourConversionEditor (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_Y_GAP - 3, DCPOMATIC_SIZER_X_GAP);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	int r = 0;

	subhead (table, this, _("Input gamma correction"), r);

	_input_gamma_linearised = new wxCheckBox (this, wxID_ANY, _("Linearise input gamma curve for small values"));
	table->Add (_input_gamma_linearised, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Input gamma"), true, wxGBPosition (r, 0));
	_input_gamma = new wxSpinCtrlDouble (this);
	table->Add (_input_gamma, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Input power"), true, wxGBPosition (r, 0));
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

	subhead (table, this, _("YUV to RGB conversion"), r);
	
	add_label_to_grid_bag_sizer (table, this, _("YUV to RGB matrix"), true, wxGBPosition (r, 0));
	_yuv_to_rgb = new wxChoice (this, wxID_ANY);
	_yuv_to_rgb->Append (_("Rec. 601"));
	_yuv_to_rgb->Append (_("Rec. 709"));
	table->Add (_yuv_to_rgb, wxGBPosition (r, 1));
	++r;

	/* RGB to XYZ conversion */

	subhead (table, this, _("RGB to XYZ conversion"), r);

	add_label_to_grid_bag_sizer (table, this, _("x"), false, wxGBPosition (r, 1));
	add_label_to_grid_bag_sizer (table, this, _("y"), false, wxGBPosition (r, 2));
	add_label_to_grid_bag_sizer (table, this, _("Matrix"), false, wxGBPosition (r, 3));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Red chromaticity"), true, wxGBPosition (r, 0));
	_red_x = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_red_x, wxGBPosition (r, 1));
	_red_y = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_red_y, wxGBPosition (r, 2));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Green chromaticity"), true, wxGBPosition (r, 0));
	_green_x = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_green_x, wxGBPosition (r, 1));
	_green_y = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_green_y, wxGBPosition (r, 2));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Blue chromaticity"), true, wxGBPosition (r, 0));
	_blue_x = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_blue_x, wxGBPosition (r, 1));
	_blue_y = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
	table->Add (_blue_y, wxGBPosition (r, 2));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("White point"), true, wxGBPosition (r, 0));
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

	add_label_to_grid_bag_sizer (table, this, wxT (""), false, wxGBPosition (r, 0));
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

	subhead (table, this, _("Output gamma correction"), r);
	
	add_label_to_grid_bag_sizer (table, this, _("Output gamma"), true, wxGBPosition (r, 0));
	wxBoxSizer* output_sizer = new wxBoxSizer (wxHORIZONTAL);
	/// TRANSLATORS: this means the mathematical reciprocal operation, i.e. we are dividing 1 by the control that
	/// comes after it.
	add_label_to_sizer (output_sizer, this, _("1 / "), false);
	_output_gamma = new wxSpinCtrlDouble (this);
	output_sizer->Add (_output_gamma);
	table->Add (output_sizer, wxGBPosition (r, 1), wxGBSpan (1, 2));
	++r;

	_input_gamma->SetRange (0.1, 4.0);
	_input_gamma->SetDigits (2);
	_input_gamma->SetIncrement (0.1);
	_input_power->SetRange (0.1, 4.0);
	_input_power->SetDigits (6);
	_input_power->SetIncrement (0.1);
	_output_gamma->SetRange (0.1, 4.0);
	_output_gamma->SetDigits (2);
	_output_gamma->SetIncrement (0.1);

	_input_gamma->Bind (wxEVT_COMMAND_SPINCTRLDOUBLE_UPDATED, boost::bind (&ColourConversionEditor::changed, this, _input_gamma));
	_input_gamma_linearised->Bind (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&ColourConversionEditor::changed, this));
	_input_power->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::changed, this));
	_input_threshold->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::changed, this));
	_input_A->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::changed, this));
	_input_B->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::changed, this));
	_red_x->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::chromaticity_changed, this));
	_red_y->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::chromaticity_changed, this));
	_green_x->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::chromaticity_changed, this));
	_green_y->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::chromaticity_changed, this));
	_blue_x->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::chromaticity_changed, this));
	_blue_y->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::chromaticity_changed, this));
	_white_x->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::chromaticity_changed, this));
	_white_y->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::chromaticity_changed, this));
	_adjust_white->Bind (wxEVT_COMMAND_CHECKBOX_CLICKED, boost::bind (&ColourConversionEditor::adjusted_white_changed, this));
	_adjusted_white_x->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::adjusted_white_changed, this));
	_adjusted_white_y->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::adjusted_white_changed, this));
	_yuv_to_rgb->Bind (wxEVT_COMMAND_CHOICE_SELECTED, boost::bind (&ColourConversionEditor::changed, this));
	_output_gamma->Bind (wxEVT_COMMAND_SPINCTRLDOUBLE_UPDATED, boost::bind (&ColourConversionEditor::changed, this, _output_gamma));
}

void
ColourConversionEditor::subhead (wxGridBagSizer* sizer, wxWindow* parent, wxString text, int& row) const
{
	wxStaticText* m = new wxStaticText (parent, wxID_ANY, wxT (""));
	m->SetLabelMarkup ("<b>" + text + "</b>");
	sizer->Add (m, wxGBPosition (row, 0), wxGBSpan (1, 3), wxALIGN_CENTER_VERTICAL | wxTOP, 12);
	++row;
}

void
ColourConversionEditor::set (ColourConversion conversion)
{
	if (dynamic_pointer_cast<const dcp::GammaTransferFunction> (conversion.in ())) {
		shared_ptr<const dcp::GammaTransferFunction> tf = dynamic_pointer_cast<const dcp::GammaTransferFunction> (conversion.in ());
		_input_gamma_linearised->SetValue (false);
		set_spin_ctrl (_input_gamma, tf->gamma ());
	} else if (dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction> (conversion.in ())) {
		shared_ptr<const dcp::ModifiedGammaTransferFunction> tf = dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction> (conversion.in ());
		/* Arbitrary default; not used in this case (greyed out) */
		_input_gamma->SetValue (2.2);
		_input_gamma_linearised->SetValue (true);
		set_spin_ctrl (_input_power, tf->power ());
		set_text_ctrl (_input_threshold, tf->threshold ());
		set_text_ctrl (_input_A, tf->A ());
		set_text_ctrl (_input_B, tf->B ());
	}

	_yuv_to_rgb->SetSelection (conversion.yuv_to_rgb ());

	SafeStringStream s;
	s.setf (std::ios::fixed, std::ios::floatfield);
	s.precision (6);

	s << conversion.red().x;
	_red_x->SetValue (std_to_wx (s.str ()));

	s.str ("");
	s << conversion.red().y;
	_red_y->SetValue (std_to_wx (s.str ()));

	s.str ("");
	s << conversion.green().x;
	_green_x->SetValue (std_to_wx (s.str ()));

	s.str ("");
	s << conversion.green().y;
	_green_y->SetValue (std_to_wx (s.str ()));

	s.str ("");
	s << conversion.blue().x;
	_blue_x->SetValue (std_to_wx (s.str ()));

	s.str ("");
	s << conversion.blue().y;
	_blue_y->SetValue (std_to_wx (s.str ()));

	s.str ("");
	s << conversion.white().x;
	_white_x->SetValue (std_to_wx (s.str ()));

	s.str ("");
	s << conversion.white().y;
	_white_y->SetValue (std_to_wx (s.str ()));

	if (conversion.adjusted_white ()) {
		_adjust_white->SetValue (true);
		s.str ("");
		s << conversion.adjusted_white().get().x;
		_adjusted_white_x->SetValue (std_to_wx (s.str ()));
		s.str ("");
		s << conversion.adjusted_white().get().y;
		_adjusted_white_y->SetValue (std_to_wx (s.str ()));
	} else {
		_adjust_white->SetValue (false);
	}

	update_rgb_to_xyz ();
	update_bradford ();
	
	set_spin_ctrl (_output_gamma, dynamic_pointer_cast<const dcp::GammaTransferFunction> (conversion.out ())->gamma ());
}

ColourConversion
ColourConversionEditor::get () const
{
	ColourConversion conversion;

	if (_input_gamma_linearised->GetValue ()) {
		conversion.set_in (
			shared_ptr<dcp::ModifiedGammaTransferFunction> (
				new dcp::ModifiedGammaTransferFunction (
					_input_power->GetValue (),
					raw_convert<double> (wx_to_std (_input_threshold->GetValue ())),
					raw_convert<double> (wx_to_std (_input_A->GetValue ())),
					raw_convert<double> (wx_to_std (_input_B->GetValue ()))
					)
				)
			);
	} else {
		conversion.set_in (
			shared_ptr<dcp::GammaTransferFunction> (new dcp::GammaTransferFunction (_input_gamma->GetValue ()))
			);
	}

	conversion.set_yuv_to_rgb (static_cast<dcp::YUVToRGB> (_yuv_to_rgb->GetSelection ()));

	conversion.set_red (
		dcp::Chromaticity (raw_convert<double> (wx_to_std (_red_x->GetValue ())), raw_convert<double> (wx_to_std (_red_y->GetValue ())))
		);
	conversion.set_green (
		dcp::Chromaticity (raw_convert<double> (wx_to_std (_green_x->GetValue ())), raw_convert<double> (wx_to_std (_green_y->GetValue ())))
		);
	conversion.set_blue (
		dcp::Chromaticity (raw_convert<double> (wx_to_std (_blue_x->GetValue ())), raw_convert<double> (wx_to_std (_blue_y->GetValue ())))
		);
	conversion.set_white (
		dcp::Chromaticity (raw_convert<double> (wx_to_std (_white_x->GetValue ())), raw_convert<double> (wx_to_std (_white_y->GetValue ())))
		);

	if (_adjust_white->GetValue ()) {
		conversion.set_adjusted_white (
			dcp::Chromaticity (
				raw_convert<double> (wx_to_std (_adjusted_white_x->GetValue ())),
				raw_convert<double> (wx_to_std (_adjusted_white_y->GetValue ()))
				)
			);
	} else {
		conversion.unset_adjusted_white ();
	}

	conversion.set_out (shared_ptr<dcp::GammaTransferFunction> (new dcp::GammaTransferFunction (_output_gamma->GetValue ())));

	return conversion;
}

void
ColourConversionEditor::changed ()
{
	bool const lin = _input_gamma_linearised->GetValue ();
	_input_gamma->Enable (!lin);
	_input_power->Enable (lin);
	_input_threshold->Enable (lin);
	_input_A->Enable (lin);
	_input_B->Enable (lin);
	
	Changed ();
}

void
ColourConversionEditor::chromaticity_changed ()
{
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
			SafeStringStream s;
			s.setf (std::ios::fixed, std::ios::floatfield);
			s.precision (7);
			s << m (i, j);
			_bradford[i][j]->SetLabel (std_to_wx (s.str ()));
		}
	}
}

void
ColourConversionEditor::update_rgb_to_xyz ()
{
	boost::numeric::ublas::matrix<double> m = get().rgb_to_xyz ();
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			SafeStringStream s;
			s.setf (std::ios::fixed, std::ios::floatfield);
			s.precision (7);
			s << m (i, j);
			_rgb_to_xyz[i][j]->SetLabel (std_to_wx (s.str ()));
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
	SafeStringStream s;
	s.precision (7);
	s << value;
	control->SetValue (std_to_wx (s.str ()));
}
