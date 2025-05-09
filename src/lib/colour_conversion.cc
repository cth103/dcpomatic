/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "colour_conversion.h"
#include "config.h"
#include "digester.h"
#include "util.h"
#include <dcp/chromaticity.h>
#include <dcp/gamma_transfer_function.h>
#include <dcp/identity_transfer_function.h>
#include <dcp/modified_gamma_transfer_function.h>
#include <dcp/raw_convert.h>
#include <dcp/s_gamut3_transfer_function.h>
#include <dcp/warnings.h>
#include <libcxml/cxml.h>
LIBDCP_DISABLE_WARNINGS
#include <libxml++/libxml++.h>
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using dcp::raw_convert;


vector<PresetColourConversion> PresetColourConversion::_presets;


ColourConversion::ColourConversion()
	: dcp::ColourConversion(dcp::ColourConversion::srgb_to_xyz())
{

}

ColourConversion::ColourConversion(dcp::ColourConversion conversion_)
	: dcp::ColourConversion(conversion_)
{

}

ColourConversion::ColourConversion(cxml::NodePtr node, int version)
{
	shared_ptr<dcp::TransferFunction> in;

	if (version >= 32) {

		/* Version 2.x */

		auto in_node = node->node_child("InputTransferFunction");
		auto in_type = in_node->string_child("Type");
		if (in_type == "Gamma") {
			_in = make_shared<dcp::GammaTransferFunction>(in_node->number_child<double>("Gamma"));
		} else if (in_type == "ModifiedGamma") {
			_in = make_shared<dcp::ModifiedGammaTransferFunction>(
					   in_node->number_child<double>("Power"),
					   in_node->number_child<double>("Threshold"),
					   in_node->number_child<double>("A"),
					   in_node->number_child<double>("B")
					   );
		} else if (in_type == "SGamut3") {
			_in = make_shared<dcp::SGamut3TransferFunction>();
		}

	} else {

		/* Version 1.x */

		if (node->bool_child("InputGammaLinearised")) {
			_in.reset(new dcp::ModifiedGammaTransferFunction(node->number_child<double>("InputGamma"), 0.04045, 0.055, 12.92));
		} else {
			_in.reset(new dcp::GammaTransferFunction(node->number_child<double>("InputGamma")));
		}
	}

	_yuv_to_rgb = static_cast<dcp::YUVToRGB>(node->optional_number_child<int>("YUVToRGB").get_value_or(static_cast<int>(dcp::YUVToRGB::REC601)));

	auto m = node->node_children("Matrix");
	if (!m.empty()) {
		/* Read in old <Matrix> nodes and convert them to chromaticities */
		boost::numeric::ublas::matrix<double> C(3, 3);
		for (auto i: m) {
			int const ti = i->number_attribute<int>("i");
			int const tj = i->number_attribute<int>("j");
			C(ti, tj) = raw_convert<double>(i->content());
		}

		double const rd = C(0, 0) + C(1, 0) + C(2, 0);
		_red = dcp::Chromaticity(C(0, 0) / rd, C(1, 0) / rd);
		double const gd = C(0, 1) + C(1, 1) + C(2, 1);
		_green = dcp::Chromaticity(C(0, 1) / gd, C(1, 1) / gd);
		double const bd = C(0, 2) + C(1, 2) + C(2, 2);
		_blue = dcp::Chromaticity(C(0, 2) / bd, C(1, 2) / bd);
		double const wd = C(0, 0) + C(0, 1) + C(0, 2) + C(1, 0) + C(1, 1) + C(1, 2) + C(2, 0) + C(2, 1) + C(2, 2);
		_white = dcp::Chromaticity((C(0, 0) + C(0, 1) + C(0, 2)) / wd,(C(1, 0) + C(1, 1) + C(1, 2)) / wd);
	} else {
		/* New-style chromaticities */
		_red = dcp::Chromaticity(node->number_child<double>("RedX"), node->number_child<double>("RedY"));
		_green = dcp::Chromaticity(node->number_child<double>("GreenX"), node->number_child<double>("GreenY"));
		_blue = dcp::Chromaticity(node->number_child<double>("BlueX"), node->number_child<double>("BlueY"));
		_white = dcp::Chromaticity(node->number_child<double>("WhiteX"), node->number_child<double>("WhiteY"));
		if (node->optional_node_child("AdjustedWhiteX")) {
			_adjusted_white = dcp::Chromaticity(
				node->number_child<double>("AdjustedWhiteX"), node->number_child<double>("AdjustedWhiteY")
				);
		}
	}

	if (auto gamma = node->optional_number_child<double>("OutputGamma")) {
		_out = make_shared<dcp::GammaTransferFunction>(node->number_child<double>("OutputGamma"));
	} else {
		_out = make_shared<dcp::IdentityTransferFunction>();
	}
}

boost::optional<ColourConversion>
ColourConversion::from_xml(cxml::NodePtr node, int version)
{
	if (!node->optional_node_child("InputTransferFunction")) {
		return boost::optional<ColourConversion>();
	}

	return ColourConversion(node, version);
}

void
ColourConversion::as_xml(xmlpp::Element* element) const
{
	auto in_node = cxml::add_child(element, "InputTransferFunction");
	if (dynamic_pointer_cast<const dcp::GammaTransferFunction>(_in)) {
		auto tf = dynamic_pointer_cast<const dcp::GammaTransferFunction>(_in);
		cxml::add_text_child(in_node, "Type", "Gamma");
		cxml::add_text_child(in_node, "Gamma", fmt::to_string(tf->gamma()));
	} else if (dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction>(_in)) {
		auto tf = dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction>(_in);
		cxml::add_text_child(in_node, "Type", "ModifiedGamma");
		cxml::add_text_child(in_node, "Power", fmt::to_string(tf->power()));
		cxml::add_text_child(in_node, "Threshold", fmt::to_string(tf->threshold()));
		cxml::add_text_child(in_node, "A", fmt::to_string(tf->A()));
		cxml::add_text_child(in_node, "B", fmt::to_string(tf->B()));
	} else if (dynamic_pointer_cast<const dcp::SGamut3TransferFunction>(_in)) {
		cxml::add_text_child(in_node, "Type", "SGamut3");
	}

	cxml::add_text_child(element, "YUVToRGB", fmt::to_string(static_cast<int>(_yuv_to_rgb)));
	cxml::add_text_child(element, "RedX", fmt::to_string(_red.x));
	cxml::add_text_child(element, "RedY", fmt::to_string(_red.y));
	cxml::add_text_child(element, "GreenX", fmt::to_string(_green.x));
	cxml::add_text_child(element, "GreenY", fmt::to_string(_green.y));
	cxml::add_text_child(element, "BlueX", fmt::to_string(_blue.x));
	cxml::add_text_child(element, "BlueY", fmt::to_string(_blue.y));
	cxml::add_text_child(element, "WhiteX", fmt::to_string(_white.x));
	cxml::add_text_child(element, "WhiteY", fmt::to_string(_white.y));
	if (_adjusted_white) {
		cxml::add_text_child(element, "AdjustedWhiteX", fmt::to_string(_adjusted_white.get().x));
		cxml::add_text_child(element, "AdjustedWhiteY", fmt::to_string(_adjusted_white.get().y));
	}

	if (auto gf = dynamic_pointer_cast<const dcp::GammaTransferFunction>(_out)) {
		cxml::add_text_child(element, "OutputGamma", fmt::to_string(gf->gamma()));
	}
}

optional<size_t>
ColourConversion::preset() const
{
	auto presets = PresetColourConversion::all();
	size_t i = 0;
	while (i < presets.size() && presets[i].conversion != *this) {
		++i;
	}

	if (i >= presets.size()) {
		return {};
	}

	return i;
}

string
ColourConversion::identifier() const
{
	Digester digester;

	if (dynamic_pointer_cast<const dcp::GammaTransferFunction>(_in)) {
		auto tf = dynamic_pointer_cast<const dcp::GammaTransferFunction>(_in);
		digester.add(tf->gamma());
	} else if (dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction>(_in)) {
		shared_ptr<const dcp::ModifiedGammaTransferFunction> tf = dynamic_pointer_cast<const dcp::ModifiedGammaTransferFunction>(_in);
		digester.add(tf->power());
		digester.add(tf->threshold());
		digester.add(tf->A());
		digester.add(tf->B());
	}

	digester.add(_red.x);
	digester.add(_red.y);
	digester.add(_green.x);
	digester.add(_green.y);
	digester.add(_blue.x);
	digester.add(_blue.y);
	digester.add(_white.x);
	digester.add(_white.y);

	if (_adjusted_white) {
		digester.add(_adjusted_white.get().x);
		digester.add(_adjusted_white.get().y);
	}

	digester.add(static_cast<int>(_yuv_to_rgb));

	if (auto gf = dynamic_pointer_cast<const dcp::GammaTransferFunction>(_out)) {
		digester.add(gf->gamma());
	}

	return digester.get();
}

PresetColourConversion::PresetColourConversion()
	: name(_("Untitled"))
{

}

PresetColourConversion::PresetColourConversion(string n, string i, dcp::ColourConversion conversion_)
	: conversion(conversion_)
	, name(n)
	, id(i)
{

}

PresetColourConversion::PresetColourConversion(cxml::NodePtr node, int version)
	: conversion(node, version)
	, name(node->string_child("Name"))
{

}

bool
operator==(ColourConversion const & a, ColourConversion const & b)
{
	return a.about_equal(b, 1e-6);
}

bool
operator!=(ColourConversion const & a, ColourConversion const & b)
{
	return !(a == b);
}

bool
operator==(PresetColourConversion const & a, PresetColourConversion const & b)
{
	return a.name == b.name && a.conversion == b.conversion;
}

void
PresetColourConversion::setup_colour_conversion_presets()
{
	_presets.push_back(PresetColourConversion(_("sRGB"), "srgb", dcp::ColourConversion::srgb_to_xyz()));
	_presets.push_back(PresetColourConversion(_("Rec. 601"), "rec601", dcp::ColourConversion::rec601_to_xyz()));
	_presets.push_back(PresetColourConversion(_("Rec. 709"), "rec709", dcp::ColourConversion::rec709_to_xyz()));
	_presets.push_back(PresetColourConversion(_("P3"), "p3", dcp::ColourConversion::p3_to_xyz()));
	_presets.push_back(PresetColourConversion(_("Rec. 1886"), "rec1886", dcp::ColourConversion::rec1886_to_xyz()));
	_presets.push_back(PresetColourConversion(_("Rec. 2020"), "rec2020", dcp::ColourConversion::rec2020_to_xyz()));
	_presets.push_back(PresetColourConversion(_("S-Gamut3/S-Log3"), "sgamut3", dcp::ColourConversion::s_gamut3_to_xyz()));
}

PresetColourConversion
PresetColourConversion::from_id(string s)
{
	for (auto const& i: _presets) {
		if (i.id == s) {
			return i;
		}
	}

	DCPOMATIC_ASSERT(false);
}
