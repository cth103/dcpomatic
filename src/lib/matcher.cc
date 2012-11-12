#include "matcher.h"
#include "image.h"
#include "log.h"

using boost::shared_ptr;

Matcher::Matcher (Log* log, int sample_rate, float frames_per_second)
	: AudioVideoProcessor (log)
	, _sample_rate (sample_rate)
	, _frames_per_second (frames_per_second)
	, _video_frames (0)
	, _audio_frames (0)
{

}

void
Matcher::process_video (boost::shared_ptr<Image> i, boost::shared_ptr<Subtitle> s)
{
	Video (i, s);
	_video_frames++;

	_pixel_format = i->pixel_format ();
	_size = i->size ();
}

void
Matcher::process_audio (boost::shared_ptr<AudioBuffers> b)
{
	Audio (b);
	_audio_frames += b->frames ();

	_channels = b->channels ();
}

void
Matcher::process_end ()
{
	if (_audio_frames == 0 || !_pixel_format || !_size || !_channels) {
		/* We won't do anything */
		return;
	}
	
	int64_t audio_short_by_frames = video_frames_to_audio_frames (_video_frames, _sample_rate, _frames_per_second) - _audio_frames;

	_log->log (
		String::compose (
			"Matching processor has seen %1 video frames (which equals %2 audio frames) and %3 audio frames",
			_video_frames,
			video_frames_to_audio_frames (_video_frames, _sample_rate, _frames_per_second),
			_audio_frames
			)
		);
	
	if (audio_short_by_frames < 0) {
		
		_log->log (String::compose ("%1 too many audio frames", -audio_short_by_frames));
		
		/* We have seen more audio than video.  Emit enough black video frames so that we reverse this */
		int const black_video_frames = ceil (-audio_short_by_frames * _frames_per_second / _sample_rate);
		
		_log->log (String::compose ("Emitting %1 frames of black video", black_video_frames));

		shared_ptr<Image> black (new CompactImage (_pixel_format.get(), _size.get()));
		black->make_black ();
		for (int i = 0; i < black_video_frames; ++i) {
			Video (black, shared_ptr<Subtitle>());
		}
		
		/* Now recompute our check value */
		audio_short_by_frames = video_frames_to_audio_frames (_video_frames, _sample_rate, _frames_per_second) - _audio_frames;
	}
	
	if (audio_short_by_frames > 0) {
		_log->log (String::compose ("Emitted %1 too few audio frames", audio_short_by_frames));
		shared_ptr<AudioBuffers> b (new AudioBuffers (_channels.get(), audio_short_by_frames));
		b->make_silent ();
		Audio (b);
		_audio_frames += b->frames ();
	}
}
