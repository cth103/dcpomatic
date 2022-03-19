/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "kdm_choice.h"


KDMChoice::KDMChoice (wxWindow* parent)
	: wxChoice (parent, wxID_ANY)
{
	Append("Modified Transitional 1", reinterpret_cast<void*>(dcp::Formulation::MODIFIED_TRANSITIONAL_1));
	Append("DCI Any", reinterpret_cast<void*>(dcp::Formulation::DCI_ANY));
	Append("DCI Specific", reinterpret_cast<void*>(dcp::Formulation::DCI_SPECIFIC));
	Append("Multiple Modified Transitional 1", reinterpret_cast<void*>(dcp::Formulation::MULTIPLE_MODIFIED_TRANSITIONAL_1));
}


dcp::Formulation
KDMChoice::get () const
{
	return static_cast<dcp::Formulation>(reinterpret_cast<intptr_t>(GetClientData(GetSelection())));
}

