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

#include <boost/utility.hpp>
#include <boost/numeric/ublas/matrix.hpp>

namespace cxml {
	class Node;
}

namespace xmlpp {
	class Node;
}

class ColourConversion : public boost::noncopyable
{
public:
	ColourConversion ();
	ColourConversion (std::string, float, bool, float const matrix[3][3], float);
	ColourConversion (boost::shared_ptr<cxml::Node>);

	void as_xml (xmlpp::Node *) const;

	std::string name;
	float input_gamma;
	bool input_gamma_linearised;
	boost::numeric::ublas::matrix<float> matrix;
	float output_gamma;
};
