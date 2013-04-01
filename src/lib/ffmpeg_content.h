#ifndef DVDOMATIC_FFMPEG_CONTENT_H
#define DVDOMATIC_FFMPEG_CONTENT_H

#include <boost/enable_shared_from_this.hpp>
#include "video_content.h"
#include "audio_content.h"

class FFmpegAudioStream
{
public:
        FFmpegAudioStream (std::string n, int i, int f, int64_t c)
                : name (n)
                , id (i)
                , frame_rate (f)
                , channel_layout (c)
        {}

	FFmpegAudioStream (boost::shared_ptr<const cxml::Node>);

	void as_xml (xmlpp::Node *) const;
	
        int channels () const {
                return av_get_channel_layout_nb_channels (channel_layout);
        }
        
        std::string name;
        int id;
        int frame_rate;
        int64_t channel_layout;
};

extern bool operator== (FFmpegAudioStream const & a, FFmpegAudioStream const & b);

class FFmpegSubtitleStream
{
public:
        FFmpegSubtitleStream (std::string n, int i)
                : name (n)
                , id (i)
        {}
        
	FFmpegSubtitleStream (boost::shared_ptr<const cxml::Node>);

	void as_xml (xmlpp::Node *) const;
	
        std::string name;
        int id;
};

extern bool operator== (FFmpegSubtitleStream const & a, FFmpegSubtitleStream const & b);

class FFmpegContentProperty : public VideoContentProperty
{
public:
        static int const SUBTITLE_STREAMS;
        static int const SUBTITLE_STREAM;
        static int const AUDIO_STREAMS;
        static int const AUDIO_STREAM;
};

class FFmpegContent : public VideoContent, public AudioContent, public boost::enable_shared_from_this<FFmpegContent>
{
public:
	FFmpegContent (boost::filesystem::path);
	FFmpegContent (boost::shared_ptr<const cxml::Node>);
	
	void examine (boost::shared_ptr<Film>, boost::shared_ptr<Job>, bool);
	std::string summary () const;
	void as_xml (xmlpp::Node *) const;

        /* AudioContent */
        int audio_channels () const;
        ContentAudioFrame audio_length () const;
        int audio_frame_rate () const;
        int64_t audio_channel_layout () const;
	
        std::vector<FFmpegSubtitleStream> subtitle_streams () const {
                boost::mutex::scoped_lock lm (_mutex);
                return _subtitle_streams;
        }

        boost::optional<FFmpegSubtitleStream> subtitle_stream () const {
                boost::mutex::scoped_lock lm (_mutex);
                return _subtitle_stream;
        }

        std::vector<FFmpegAudioStream> audio_streams () const {
                boost::mutex::scoped_lock lm (_mutex);
                return _audio_streams;
        }
        
        boost::optional<FFmpegAudioStream> audio_stream () const {
                boost::mutex::scoped_lock lm (_mutex);
                return _audio_stream;
        }

        void set_subtitle_stream (FFmpegSubtitleStream);
        void set_audio_stream (FFmpegAudioStream);
	
private:
	std::vector<FFmpegSubtitleStream> _subtitle_streams;
	boost::optional<FFmpegSubtitleStream> _subtitle_stream;
	std::vector<FFmpegAudioStream> _audio_streams;
	boost::optional<FFmpegAudioStream> _audio_stream;
};

#endif
