/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#include "content.h"
#include <dcp/types.h>


class VideoMXFContent : public Content
{
public:
	VideoMXFContent(boost::filesystem::path path);
	VideoMXFContent(cxml::ConstNodePtr node, boost::optional<boost::filesystem::path> film_directory, int version);

	std::shared_ptr<VideoMXFContent> shared_from_this() {
		return std::dynamic_pointer_cast<VideoMXFContent>(Content::shared_from_this());
	}

	std::shared_ptr<const VideoMXFContent> shared_from_this() const {
		return std::dynamic_pointer_cast<const VideoMXFContent>(Content::shared_from_this());
	}

	void examine(std::shared_ptr<const Film> film, std::shared_ptr<Job> job, bool tolerant) override;
	std::string summary() const override;
	std::string technical_summary() const override;
	std::string identifier() const override;

	void as_xml(
		xmlpp::Element* element,
		bool with_paths,
		PathBehaviour path_behaviour,
		boost::optional<boost::filesystem::path> film_directory
		) const override;

	dcpomatic::DCPTime full_length(std::shared_ptr<const Film> film) const override;
	dcpomatic::DCPTime approximate_length() const override;
	void add_properties(std::shared_ptr<const Film> film, std::list<UserProperty>& p) const override;

	/** @return true if the image the film would write for this content is exactly the image of the file, so that the
	 *  file can be used in the DCP as it is, with any trims in the CPL.  If not, @param why_not says why.
	 */
	bool can_reference_video(std::shared_ptr<const Film> film, std::string& why_not) const;

	static bool valid_mxf(boost::filesystem::path path);

private:
	/** Standard of the file, if it is 2D; a 3D file is not used as it is */
	boost::optional<dcp::Standard> _mono_standard;
};
