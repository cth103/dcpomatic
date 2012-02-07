#include "ratio.h"

class Film
{
public:

private:
	std::string _name;
	std::string _content;
	Ratio       _ratio;
	bool        _dvd;
	int         _dvd_title;
	bool        _deinterlace;
	int         _left_crop;
	int         _right_crop;
	int         _top_crop;
	int         _bottom_crop;

	int         _width;
	int         _height;
	float       _length;
};
