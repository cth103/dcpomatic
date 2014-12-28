/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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
#include "wx_util.h"
#include "colour_conversion_editor.h"
#include <dcp/gamma_transfer_function.h>
#include <dcp/modified_gamma_transfer_function.h>
#include <dcp/raw_convert.h>
#include <wx/spinctrl.h>
#include <wx/gbsizer.h>
#include <boost/lexical_cast.hpp>

using std::string;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::lexical_cast;
using dcp::raw_convert;

ColourConversionEditor::ColourConversionEditor (wxWindow* parent)
	: wxPanel (parent, wxID_ANY)
{
	wxBoxSizer* overall_sizer = new wxBoxSizer (wxVERTICAL);
	SetSizer (overall_sizer);

	wxGridBagSizer* table = new wxGridBagSizer (DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	overall_sizer->Add (table, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	int r = 0;

	_input_gamma_linearised = new wxCheckBox (this, wxID_ANY, _("Linearise input gamma curve for low values"));
	table->Add (_input_gamma_linearised, wxGBPosition (r, 0), wxGBSpan (1, 2));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Input gamma"), true, wxGBPosition (r, 0));
	_input_gamma = new wxSpinCtrlDouble (this);
	table->Add (_input_gamma, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Input power"), true, wxGBPosition (r, 0));
	_input_power = new wxSpinCtrlDouble (this);
	table->Add (_input_power, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Input threshold"), true, wxGBPosition (r, 0));
	_input_threshold = new wxTextCtrl (this, wxID_ANY, wxT (""));
	table->Add (_input_threshold, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Input A value"), true, wxGBPosition (r, 0));
	_input_A = new wxTextCtrl (this, wxID_ANY, wxT (""));
	table->Add (_input_A, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Input B value"), true, wxGBPosition (r, 0));
	_input_B = new wxTextCtrl (this, wxID_ANY, wxT (""));
	table->Add (_input_B, wxGBPosition (r, 1));
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

	add_label_to_grid_bag_sizer (table, this, _("Matrix"), true, wxGBPosition (r, 0));
	wxFlexGridSizer* matrix_sizer = new wxFlexGridSizer (3, DCPOMATIC_SIZER_X_GAP, DCPOMATIC_SIZER_Y_GAP);
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			_matrix[i][j] = new wxTextCtrl (this, wxID_ANY, wxT (""), wxDefaultPosition, size, 0, validator);
			matrix_sizer->Add (_matrix[i][j]);
		}
	}
	table->Add (matrix_sizer, wxGBPosition (r, 1));
	++r;

	add_label_to_grid_bag_sizer (table, this, _("Output gamma"), true, wxGBPosition (r, 0));
	wxBoxSizer* output_sizer = new wxBoxSizer (wxHORIZONTAL);
	/// TRANSLATORS: this means the mathematical reciprocal operation, i.e. we are dividing 1 by the control that
	/// comes after it.
	add_label_to_sizer (output_sizer, this, _("1 / "), false);
	_output_gamma = new wxSpinCtrlDouble (this);
	output_sizer->Add (_output_gamma);
	table->Add (output_sizer, wxGBPosition (r, 1));
	++r;

	_input_gamma->SetRange (0.1, 4.0);
	_input_gamma->SetDigits (2);
	_input_gamma->SetIncrement (0.1);
	_input_power->SetRange (0.1, 4.0);
	_input_power->SetDigits (2);
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
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			_matrix[i][j]->Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ColourConversionEditor::changed, this));
		}
	}
	_output_gamma->Bind (wxEVT_COMMAND_SPINCTRLDOUBLE_UPDATED, boost::bind (&ColourConversionEditor::changed, this, _output_gamma));
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
		/* Arbitrary default */
		_input_gamma->SetValue (2.2);
		_input_gamma_linearised->SetValue (true);
		set_spin_ctrl (_input_power, tf->power ());
		set_text_ctrl (_input_threshold, tf->threshold ());
		set_text_ctrl (_input_A, tf->A ());
		set_text_ctrl (_input_B, tf->B ());
	}

	boost::numeric::ublas::matrix<double> matrix = conversion.matrix ();
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			set_text_ctrl (_matrix[i][j], matrix(i, j));
		}
	}
	
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
			shared_ptr<dcp::GammaTransferFunction> (
				new dcp::GammaTransferFunction (
					_input_gamma->GetValue ()
					)
				)
			);
	}

	boost::numeric::ublas::matrix<double> matrix (3, 3);
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			string const v = wx_to_std (_matrix[i][j]->GetValue ());
			if (v.empty ()) {
				matrix (i, j) = 0;
			} else {
				matrix (i, j) = raw_convert<double> (v);
			}
		}
	}

	conversion.set_matrix (matrix);

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
