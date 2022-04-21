/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_spin_ctrl.h"
#include "lib/warnings.h"
#include <boost/bind/bind.hpp>
#include <boost/version.hpp>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS


#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


SpinCtrl::SpinCtrl (wxWindow* parent, int width)
	: wxSpinCtrl (parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1), wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER)
{
	auto enter = [](wxCommandEvent& ev) {
		dynamic_cast<wxWindow*>(ev.GetEventObject())->Navigate();
	};
	Bind (wxEVT_TEXT_ENTER, boost::bind<void>(enter, _1));
}

