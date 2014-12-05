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
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>

#include "i18n.h"

using std::list;
using std::string;
using std::cout;
using std::vector;
using boost::shared_ptr;
using boost::optional;
using dcp::raw_convert;

ColourConversion::ColourConversion ()
	: input_gamma (2.4)
	, input_gamma_linearised (true)
	, matrix (3, 3)
	, output_gamma (2.6)
{
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			matrix (i, j) = dcp::colour_matrix::srgb_to_xyz[i][j];
		}
	}
}

ColourConversion::ColourConversion (double i, bool il, double const m[3][3], double o)
	: input_gamma (i)
	, input_gamma_linearised (il)
	, matrix (3, 3)
	, output_gamma (o)
{
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			matrix (i, j) = m[i][j];
		}
	}
}

ColourConversion::ColourConversion (cxml::NodePtr node)
	: matrix (3, 3)
{
	input_gamma = node->number_child<double> ("InputGamma");
	input_gamma_linearised = node->bool_child ("InputGammaLinearised");

	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			matrix (i, j) = 0;
		}
	}

	list<cxml::NodePtr> m = node->node_children ("Matrix");
	for (list<cxml::NodePtr>::iterator i = m.begin(); i != m.end(); ++i) {
		int const ti = (*i)->number_attribute<int> ("i");
		int const tj = (*i)->number_attribute<int> ("j");
		matrix(ti, tj) = raw_convert<double> ((*i)->content ());
	}

	output_gamma = node->number_child<double> ("OutputGamma");
}

boost::optional<ColourConversion>
ColourConversion::from_xml (cxml::NodePtr node)
{
	if (!node->optional_node_child ("InputGamma")) {
		return boost::optional<ColourConversion> ();
	}

	return ColourConversion (node);
}

void
ColourConversion::as_xml (xmlpp::Node* node) const
{
	node->add_child("InputGamma")->add_child_text (raw_convert<string> (input_gamma));
	node->add_child("InputGammaLinearised")->add_child_text (input_gamma_linearised ? "1" : "0");

	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			xmlpp::Element* m = node->add_child("Matrix");
			m->set_attribute ("i", raw_convert<string> (i));
			m->set_attribute ("j", raw_convert<string> (j));
			m->add_child_text (raw_convert<string> (matrix (i, j)));
		}
	}

	node->add_child("OutputGamma")->add_child_text (raw_convert<string> (output_gamma));
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
	
	digester.add (input_gamma);
	digester.add (input_gamma_linearised);
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			digester.add (matrix (i, j));
		}
	}
	digester.add (output_gamma);
	
	return digester.get ();
}

PresetColourConversion::PresetColourConversion ()
	: name (_("Untitled"))
{

}

PresetColourConversion::PresetColourConversion (string n, double i, bool il, double const m[3][3], double o)
	: name (n)
	, conversion (i, il, m, o)
{

}

PresetColourConversion::PresetColourConversion (cxml::NodePtr node)
	: conversion (node)
{
	name = node->string_child ("Name");
}

void
PresetColourConversion::as_xml (xmlpp::Node* node) const
{
	conversion.as_xml (node);
	node->add_child("Name")->add_child_text (name);
}

static bool
about_equal (double a, double b)
{
	static const double eps = 1e-6;
	return fabs (a - b) < eps;
}

bool
operator== (ColourConversion const & a, ColourConversion const & b)
{
	if (
		!about_equal (a.input_gamma, b.input_gamma) ||
		a.input_gamma_linearised != b.input_gamma_linearised ||
		!about_equal (a.output_gamma, b.output_gamma)) {
		return false;
	}

	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			if (!about_equal (a.matrix (i, j), b.matrix (i, j))) {
				return false;
			}
		}
	}

	return true;
}

bool
operator!= (ColourConversion const & a, ColourConversion const & b)
{
	return !(a == b);
}
