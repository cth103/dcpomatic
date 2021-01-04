/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "monitor_dialog.h"
#include "wx_util.h"
#include "lib/encode_server.h"
#include <dcp/locale_convert.h>

using std::string;
using dcp::locale_convert;
using std::shared_ptr;
using boost::optional;

MonitorDialog::MonitorDialog (wxWindow* parent)
	: TableDialog (parent, _("Device"), 2, 1, true)
{
	add (_("Manufacturer ID"), true);
	_manufacturer_id = add (new wxTextCtrl(this, wxID_ANY, wxT("")));
	add (_("Manufacturer product code"), true);
	_manufacturer_product_code = add (new wxTextCtrl(this, wxID_ANY, wxT("")));
	add (_("Serial number"), true);
	_serial_number = add (new wxTextCtrl(this, wxID_ANY, wxT("")));
	add (_("Week of manufacture"), true);
	_week_of_manufacture = add (new wxTextCtrl(this, wxID_ANY, wxT("")));
	add (_("Year of manufacture"), true);
	_year_of_manufacture = add (new wxTextCtrl(this, wxID_ANY, wxT("")));

	layout ();

	_manufacturer_id->SetFocus ();
}

void
MonitorDialog::set (Monitor monitor)
{
	_manufacturer_id->SetValue (std_to_wx(monitor.manufacturer_id));
	_manufacturer_product_code->SetValue (std_to_wx(locale_convert<string>(monitor.manufacturer_product_code)));
	_serial_number->SetValue (std_to_wx(locale_convert<string>(monitor.serial_number)));
	_week_of_manufacture->SetValue (std_to_wx(locale_convert<string>(monitor.week_of_manufacture)));
	_year_of_manufacture->SetValue (std_to_wx(locale_convert<string>(monitor.year_of_manufacture)));
}

optional<Monitor>
MonitorDialog::get () const
{
	Monitor m;
	m.manufacturer_id = wx_to_std (_manufacturer_id->GetValue());
	m.manufacturer_product_code = locale_convert<uint16_t>(wx_to_std(_manufacturer_product_code->GetValue()));
	m.serial_number = locale_convert<uint32_t>(wx_to_std(_serial_number->GetValue()));
	m.week_of_manufacture = locale_convert<uint8_t>(wx_to_std(_week_of_manufacture->GetValue()));
	m.year_of_manufacture = locale_convert<uint8_t>(wx_to_std (_year_of_manufacture->GetValue()));
	return m;
}
