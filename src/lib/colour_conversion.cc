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

#include "config.h"
#include "colour_conversion.h"
#include "util.h"
#include "md5_digester.h"
#include <dcp/colour_matrix.h>
#include <dcp/raw_convert.h>
#include <dcp/gamma_transfer_function.h>
#include <dcp/modified_gamma_transfer_function.h>
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>

#include "i18n.h"

using std::list;
using std::string;
using std::cout;
using std::vector;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using dcp::raw_convert;

ColourConversion::ColourConversion ()
	: dcp::ColourConversion (dcp::ColourConversion::srgb_to_xyz ())
{
	
}

ColourConversion::ColourConversion (dcp::ColourConversion conversion_)
	: dcp::ColourConversion (conversion_)
{
	
}

ColourConversion::ColourConversion (cxml::NodePtr node, int version)
{
	shared_ptr<dcp::TransferFunction> in;

	if (version >= 32) {

		/* Version 2.x */

		cxml::ConstNodePtr in_node = node->node_child ("InputTransferFunction");
		string in_type = in_node->string_child ("Type");
		if (in_type == "Gamma") {
			_in.reset (new dcp::GammaTransferFunction (false, in_node->number_child<double> ("Gamma")));
		} else if (in_type == "ModifiedGamma") {
			_in.reset (new dcp::ModifiedGammaTransferFunction (
					   false,
					   in_node->number_child<double> ("Power"),
					   in_node->number_child<double> ("Threshold"),
					   in_node->number_child<double> ("A"),
					   in_node->number_child<double> ("B")
					   ));
		}

	} else {

		/* Version 1.x */
		
		if (node->bool_child ("InputGammaLinearised")) {
			_in.reset (new dcp::ModifiedGammaTransferFunction (false, node->number_child<float> ("InputGamma"), 0.04045, 0.055, 12.92));
		} else {
			_in.reset (new dcp::GammaTransferFunction (false, node->number_child<float> ("InputGamma")));
		}
	}

	list<cxml::NodePtr> m = node->node_children ("Matrix");
	for (list<cxml::NodePtr>::iterator i = m.begin(); i != m.end(); ++i) {
		int const ti = (*i)->number_attribute<int> ("i");
		int const tj = (*i)->number_attribute<int> ("j");
		_matrix(ti, tj) = raw_convert<double> ((*i)->content ());
	}
	
	_out.reset (new dcp::GammaTransferFunction (true, node->number_child<double> ("OutputGamma")));
}

boost::optional<ColourConversion>
ColourConversion::from_xml (cxml::NodePtr node, int version)
{
	if (!node->optional_node_child ("InputTransferFunction")) {
		return boost::optional<ColourConversion> ();
	}

	return ColourConversion (node, version);
}

void
ColourConversion::as_xml (xmlpp::Node* node) const
{
	xmlpp::Node* in_node = node->add_child ("InputTransferFunction");
	if (dynamic_pointer_cast<const dcp::GammaTransferFunction> (_in)) {
		shared_ptr<const dcp::GammaTransferFunction> tf = dynamic_pointer_cast<const dcp::GammaTransferFunction> (_in);
		in_node->add_child("Type")->add_child_text ("Gamma");
		in_node->add_child("Gamma")->add_child_text (raw_convert<string> (tf->gamma ()));
	} else if (dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction> (_in)) {
		shared_ptr<const dcp::ModifiedGammaTransferFunction> tf = dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction> (_in);
		in_node->add_child("Type")->add_child_text ("ModifiedGamma");
		in_node->add_child("Power")->add_child_text (raw_convert<string> (tf->power ()));
		in_node->add_child("Threshold")->add_child_text (raw_convert<string> (tf->threshold ()));
		in_node->add_child("A")->add_child_text (raw_convert<string> (tf->A ()));
		in_node->add_child("B")->add_child_text (raw_convert<string> (tf->B ()));
	}

	boost::numeric::ublas::matrix<double> matrix = _matrix;
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			xmlpp::Element* m = node->add_child("Matrix");
			m->set_attribute ("i", raw_convert<string> (i));
			m->set_attribute ("j", raw_convert<string> (j));
			m->add_child_text (raw_convert<string> (matrix (i, j)));
		}
	}

	node->add_child("OutputGamma")->add_child_text (raw_convert<string> (dynamic_pointer_cast<const dcp::GammaTransferFunction> (_out)->gamma ()));
}

optional<size_t>
ColourConversion::preset () const
{
	vector<PresetColourConversion> presets = Config::instance()->colour_conversions ();
	size_t i = 0;
	while (i < presets.size() && (presets[i].conversion != *this)) {
		++i;
	}

	if (i >= presets.size ()) {
		return optional<size_t> ();
	}

	return i;
}

string
ColourConversion::identifier () const
{
	MD5Digester digester;

	if (dynamic_pointer_cast<const dcp::GammaTransferFunction> (_in)) {
		shared_ptr<const dcp::GammaTransferFunction> tf = dynamic_pointer_cast<const dcp::GammaTransferFunction> (_in);
		digester.add (tf->gamma ());
	} else if (dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction> (_in)) {
		shared_ptr<const dcp::ModifiedGammaTransferFunction> tf = dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction> (_in);
		digester.add (tf->power ());
		digester.add (tf->threshold ());
		digester.add (tf->A ());
		digester.add (tf->B ());
	}

	boost::numeric::ublas::matrix<double> matrix = _matrix;
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			digester.add (matrix (i, j));
		}
	}

	digester.add (dynamic_pointer_cast<const dcp::GammaTransferFunction> (_out)->gamma ());
	
	return digester.get ();
}

PresetColourConversion::PresetColourConversion ()
	: name (_("Untitled"))
{

}

PresetColourConversion::PresetColourConversion (string n, dcp::ColourConversion conversion_)
	: name (n)
	, conversion (conversion_)
{

}

PresetColourConversion::PresetColourConversion (cxml::NodePtr node, int version)
	: conversion (node, version)
{
	name = node->string_child ("Name");
}

void
PresetColourConversion::as_xml (xmlpp::Node* node) const
{
	conversion.as_xml (node);
	node->add_child("Name")->add_child_text (name);
}

bool
operator== (ColourConversion const & a, ColourConversion const & b)
{
	return a.about_equal (b, 1e-6);
}

bool
operator!= (ColourConversion const & a, ColourConversion const & b)
{
	return !(a == b);
}
