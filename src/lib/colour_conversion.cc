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

#include <boost/lexical_cast.hpp>
#include <libxml++/libxml++.h>
#include <libdcp/colour_matrix.h>
#include <libcxml/cxml.h>
#include "colour_conversion.h"

#include "i18n.h"

using std::list;
using std::string;
using boost::shared_ptr;
using boost::lexical_cast;

ColourConversion::ColourConversion ()
	: name (_("Untitled"))
	, input_gamma (2.4)
	, input_gamma_linearised (true)
	, matrix (3, 3)
	, output_gamma (2.6)
{
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			matrix (i, j) = libdcp::colour_matrix::xyz_to_rgb[i][j];
		}
	}
}

ColourConversion::ColourConversion (string n, float i, bool il, float const m[3][3], float o)
	: name (n)
	, input_gamma (i)
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

ColourConversion::ColourConversion (shared_ptr<cxml::Node> node)
	: matrix (3, 3)
{
	name = node->string_child ("Name");
	input_gamma = node->number_child<float> ("InputGamma");
	input_gamma_linearised = node->bool_child ("InputGammaLinearised");

	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			matrix (i, j) = 0;
		}
	}

	list<shared_ptr<cxml::Node> > m = node->node_children ("Matrix");
	for (list<shared_ptr<cxml::Node> >::iterator i = m.begin(); i != m.end(); ++i) {
		int const ti = (*i)->number_attribute<int> ("i");
		int const tj = (*i)->number_attribute<int> ("j");
		matrix(ti, tj) = lexical_cast<float> ((*i)->content ());
	}

	output_gamma = node->number_child<float> ("OutputGamma");
}

void
ColourConversion::as_xml (xmlpp::Node* node) const
{
	node->add_child("Name")->add_child_text (name);
	node->add_child("InputGamma")->add_child_text (lexical_cast<string> (input_gamma));
	node->add_child("InputGammaLinearised")->add_child_text (input_gamma_linearised ? "1" : "0");

	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			xmlpp::Element* m = node->add_child("Matrix");
			m->set_attribute ("i", lexical_cast<string> (i));
			m->set_attribute ("j", lexical_cast<string> (j));
			m->add_child_text (lexical_cast<string> (matrix (i, j)));
		}
	}

	node->add_child("OutputGamma")->add_child_text (lexical_cast<string> (output_gamma));
}
