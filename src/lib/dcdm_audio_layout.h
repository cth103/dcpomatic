/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


/** @file  src/lib/dcdm_audio_layout.h
 *  @brief Helpers that lay out the audio channels of a DCDM.
 */


#ifndef DCPOMATIC_DCDM_AUDIO_LAYOUT_H
#define DCPOMATIC_DCDM_AUDIO_LAYOUT_H


#include <dcp/types.h>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>


class AudioBuffers;


/** The only audio layouts a DCDM export writes */
enum class DCDMLayout
{
	FIVE_ONE,
	SEVEN_ONE
};


/** @param used DCP channels that are in use.
 *  @return SEVEN_ONE if BSL or BSR is used, otherwise FIVE_ONE.  Other channels do not affect the layout.
 */
DCDMLayout dcdm_layout(std::set<dcp::Channel> used);

/** @param used DCP channels that are in use.
 *  @return the extra channels (beyond the 5.1 / 7.1 layout) that are in use and are written too,
 *  with their file labels, in DCP channel order: HI, VI and sign language (SL).
 *  Lc and Rc are never included, because DCP-o-matic always writes them silent in a DCP;
 *  nor are motion data and the Atmos sync signal.
 */
std::vector<std::pair<dcp::Channel, std::string>> dcdm_extra_channels(std::set<dcp::Channel> used);

/** @return file labels for a layout, in output order: L R C LFE Ls Rs, plus Lrs Rrs for 7.1 */
std::vector<std::string> dcdm_channel_labels(DCDMLayout layout);

/** @return the DCP channels written for a layout, in the same order as dcdm_channel_labels() */
std::vector<dcp::Channel> dcdm_channels(DCDMLayout layout);

/** Pick @param channels out of buffers whose channel index is the DCP channel index, in that order.
 *  Channels that the input does not have are silent.
 */
std::shared_ptr<AudioBuffers> dcdm_pick_channels(std::shared_ptr<const AudioBuffers> in, std::vector<dcp::Channel> const& channels);

/** Pick the layout's channels; see dcdm_pick_channels() */
std::shared_ptr<AudioBuffers> dcdm_layout_channels(std::shared_ptr<const AudioBuffers> in, DCDMLayout layout);


#endif
