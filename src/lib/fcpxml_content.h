/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_FCPXML_CONTENT_H
#define DCPOMATIC_FCPXML_CONTENT_H


#include "content.h"



class FCPXMLContent : public Content
{
public:
	FCPXMLContent(boost::filesystem::path path);
	FCPXMLContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version, std::list<std::string>& notes);

	std::shared_ptr<FCPXMLContent> shared_from_this() {
		return std::dynamic_pointer_cast<FCPXMLContent>(Content::shared_from_this());
	}

	std::shared_ptr<const FCPXMLContent> shared_from_this() const {
		return std::dynamic_pointer_cast<const FCPXMLContent>(Content::shared_from_this());
	}

	void examine(std::shared_ptr<const Film> film, std::shared_ptr<Job>, bool tolerant) override;
	std::string summary() const override;
	std::string technical_summary() const override;
	void as_xml(xmlpp::Element*, bool with_paths, PathBehaviour path_behaviour, boost::optional<boost::filesystem::path> film_directory) const override;
	dcpomatic::DCPTime full_length(std::shared_ptr<const Film> film) const override;
	dcpomatic::DCPTime approximate_length() const override;

private:
	dcpomatic::ContentTime _length;
};



#endif
