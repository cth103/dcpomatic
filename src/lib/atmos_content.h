/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "content_part.h"


class Content;


class AtmosContentProperty
{
public:
	static int const EDIT_RATE;
};


class AtmosContent : public ContentPart
{
public:
	explicit AtmosContent (Content* parent);
	AtmosContent (Content* parent, cxml::ConstNodePtr node);

	static std::shared_ptr<AtmosContent> from_xml (Content* parent, cxml::ConstNodePtr node);

	void as_xml (xmlpp::Node* node) const;

	void set_length (Frame len);

	Frame length () const {
		return _length;
	}

	void set_edit_rate (dcp::Fraction rate);

	dcp::Fraction edit_rate () const {
		return _edit_rate;
	}

private:
	Frame _length;
	dcp::Fraction _edit_rate;
};

