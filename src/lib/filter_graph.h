#ifndef DVDOMATIC_FILTER_GRAPH_H
#define DVDOMATIC_FILTER_GRAPH_H

#include "util.h"

class Decoder;
class Image;
class Film;

class FilterGraph
{
public:
	FilterGraph (boost::shared_ptr<Film> film, Decoder* decoder, bool crop, Size s, AVPixelFormat p);

	bool can_process (Size s, AVPixelFormat p) const;
	std::list<boost::shared_ptr<Image> > process (AVFrame* frame);

private:
	AVFilterContext* _buffer_src_context;
	AVFilterContext* _buffer_sink_context;
	Size _size;
	AVPixelFormat _pixel_format;
};

#endif
