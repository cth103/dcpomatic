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


#include "job.h"
#include "player_text.h"
#include "text_type.h"


class Film;
class Content;


class AnalyseSubtitlesJob : public Job
{
public:
	AnalyseSubtitlesJob (std::shared_ptr<const Film> film, std::shared_ptr<Content> content);
	~AnalyseSubtitlesJob();

	std::string name () const override;
	std::string json_name () const override;
	void run () override;

	boost::filesystem::path path () const {
		return _path;
	}

private:
	void analyse(PlayerText const& text, TextType type);

	std::shared_ptr<const Film> _film;
	std::weak_ptr<Content> _content;
	boost::filesystem::path _path;
	boost::optional<dcpomatic::Rect<double>> _bounding_box;
};

