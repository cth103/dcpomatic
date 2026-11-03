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


#include "types.h"
#include <memory>


class AudioBuffers;
class Content;
class Film;


/** @return true if @param content is a single WAV file with no video, whose whole length can be kept in the DCP's sound
 *  asset: its trims are whole frames of the film, and the picture it plays with does not change its speed.
 */
bool wav_keeps_whole_length(std::shared_ptr<const Film> film, std::shared_ptr<const Content> content);

/** @return an earlier WAV of the film that is a whole reel with exactly the sound of @param content, also a whole reel, so
 *  that the reel of @param content can refer to that sound instead of writing it again; or null if there is none.
 */
std::shared_ptr<const Content> wav_shared_source(std::shared_ptr<const Film> film, std::shared_ptr<const Content> content);

/** @return the part of a WAV that its start trim cuts off, ready for the film's sound asset (mapped, at the film's
 *  sampling rate and with the content's gain), or nothing if there is no start trim.
 */
std::shared_ptr<const AudioBuffers> wav_head_audio(std::shared_ptr<const Film> film, std::shared_ptr<const Content> content);

/** @return the length of wav_head_audio() in frames of the film */
Frame wav_head_frames(std::shared_ptr<const Film> film, std::shared_ptr<const Content> content);

/** @return the part of a WAV that its end trim cuts off, as wav_head_audio() */
std::shared_ptr<const AudioBuffers> wav_tail_audio(std::shared_ptr<const Film> film, std::shared_ptr<const Content> content);
