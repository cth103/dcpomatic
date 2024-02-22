#include <cairomm/cairomm.h>
#include <pangomm.h>
#include <pangomm/init.h>
#include <glibmm.h>
#include <stdint.h>

int main ()
{
#ifdef __MINGW32__
	putenv ("PANGOCAIRO_BACKEND=fontconfig");
	putenv ("FONTCONFIG_PATH=.");
#endif

	Pango::init ();

	int const width = 640;
	int const height = 480;
	uint8_t* data = new uint8_t[width * height * 4];

	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create (
		data,
		Cairo::ImageSurface::Format::ARGB32,
		width, height,
		/* Cairo ARGB32 means first byte blue, second byte green, third byte red, fourth byte alpha */
		Cairo::ImageSurface::format_stride_for_width(Cairo::ImageSurface::Format::ARGB32, width)
		);

	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (surface);

	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

	context->set_source_rgb (1, 0.2, 0.3);
	context->rectangle (0, 0, width, height);
	context->fill ();

	layout->set_alignment (Pango::Alignment::LEFT);

	context->set_line_width (1);
	// Cairo::FontOptions fo;
	// context->get_font_options (fo);
	// fo.set_antialias (Cairo::ANTIALIAS_NONE);
	// context->set_font_options (fo);

	Pango::FontDescription font ("Arial");
	layout->set_font_description (font);
	layout->set_markup ("Hello world!");

	layout->update_from_cairo_context (context);

	context->set_source_rgb (1, 1, 1);
	context->set_line_width (0);
	context->move_to (0, 0);
	context->scale (2, 2);
	layout->show_in_cairo_context (context);

	surface->write_to_png ("text.png");

	return 0;
}
