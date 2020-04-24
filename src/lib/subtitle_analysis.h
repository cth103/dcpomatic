/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "rect.h"
#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>


/** @class SubtitleAnalysis
 *  @brief Class to store the results of a SubtitleAnalysisJob.
 */

class SubtitleAnalysis : public boost::noncopyable
{
public:
	explicit SubtitleAnalysis (boost::filesystem::path path);

	SubtitleAnalysis (
			boost::optional<dcpomatic::Rect<double> > bounding_box,
			double analysis_x_offset_,
			double analysis_y_offset_
			)
		: _bounding_box (bounding_box)
		, _analysis_x_offset (analysis_x_offset_)
		, _analysis_y_offset (analysis_y_offset_)
	{}

	void write (boost::filesystem::path path) const;

	boost::optional<dcpomatic::Rect<double> > bounding_box () const {
		return _bounding_box;
	}

	double analysis_x_offset () const {
		return _analysis_x_offset;
	}

	double analysis_y_offset () const {
		return _analysis_y_offset;
	}

private:
	/** Smallest box which surrounds all subtitles in our content,
	 *  expressed as a proportion of screen size (i.e. 0 is left hand side/top,
	 *  1 is right hand side/bottom), or empty if no subtitles were found.
	 */
	boost::optional<dcpomatic::Rect<double> > _bounding_box;
	double _analysis_x_offset;
	double _analysis_y_offset;

	static int const _current_state_version;
};
