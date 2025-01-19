/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "dcp_subtitle.h"
#include "content.h"

class DCPSubtitleContent : public DCPSubtitle, public Content
{
public:
	DCPSubtitleContent (boost::filesystem::path);
	DCPSubtitleContent (cxml::ConstNodePtr, boost::optional<boost::filesystem::path> film_directory, int);

	void examine (std::shared_ptr<const Film> film, std::shared_ptr<Job>, bool tolerant) override;
	std::string summary () const override;
	std::string technical_summary () const override;

	void as_xml(
		xmlpp::Element* element,
		bool with_paths,
		PathBehaviour path_behaviour,
		boost::optional<boost::filesystem::path> film_directory
		) const override;

	dcpomatic::DCPTime full_length (std::shared_ptr<const Film> film) const override;
	dcpomatic::DCPTime approximate_length () const override;

private:
	void add_fonts(std::shared_ptr<TextContent> content, std::shared_ptr<dcp::TextAsset> subtitle_asset);

	dcpomatic::ContentTime _length;
};
