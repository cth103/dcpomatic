
BOOST_AUTO_TEST_CASE (aligned_image_test)
{
	SimpleImage* s = new SimpleImage (PIX_FMT_RGB24, libdcp::Size (50, 50), true);
	BOOST_CHECK_EQUAL (s->components(), 1);
	/* 160 is 150 aligned to the nearest 32 bytes */
	BOOST_CHECK_EQUAL (s->stride()[0], 160);
	BOOST_CHECK_EQUAL (s->line_size()[0], 150);
	BOOST_CHECK (s->data()[0]);
	BOOST_CHECK (!s->data()[1]);
	BOOST_CHECK (!s->data()[2]);
	BOOST_CHECK (!s->data()[3]);

	/* copy constructor */
	SimpleImage* t = new SimpleImage (*s);
	BOOST_CHECK_EQUAL (t->components(), 1);
	BOOST_CHECK_EQUAL (t->stride()[0], 160);
	BOOST_CHECK_EQUAL (t->line_size()[0], 150);
	BOOST_CHECK (t->data()[0]);
	BOOST_CHECK (!t->data()[1]);
	BOOST_CHECK (!t->data()[2]);
	BOOST_CHECK (!t->data()[3]);
	BOOST_CHECK (t->data() != s->data());
	BOOST_CHECK (t->data()[0] != s->data()[0]);
	BOOST_CHECK (t->line_size() != s->line_size());
	BOOST_CHECK (t->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (t->stride() != s->stride());
	BOOST_CHECK (t->stride()[0] == s->stride()[0]);

	/* assignment operator */
	SimpleImage* u = new SimpleImage (PIX_FMT_YUV422P, libdcp::Size (150, 150), false);
	*u = *s;
	BOOST_CHECK_EQUAL (u->components(), 1);
	BOOST_CHECK_EQUAL (u->stride()[0], 160);
	BOOST_CHECK_EQUAL (u->line_size()[0], 150);
	BOOST_CHECK (u->data()[0]);
	BOOST_CHECK (!u->data()[1]);
	BOOST_CHECK (!u->data()[2]);
	BOOST_CHECK (!u->data()[3]);
	BOOST_CHECK (u->data() != s->data());
	BOOST_CHECK (u->data()[0] != s->data()[0]);
	BOOST_CHECK (u->line_size() != s->line_size());
	BOOST_CHECK (u->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (u->stride() != s->stride());
	BOOST_CHECK (u->stride()[0] == s->stride()[0]);

	delete s;
	delete t;
	delete u;
}

BOOST_AUTO_TEST_CASE (compact_image_test)
{
	SimpleImage* s = new SimpleImage (PIX_FMT_RGB24, libdcp::Size (50, 50), false);
	BOOST_CHECK_EQUAL (s->components(), 1);
	BOOST_CHECK_EQUAL (s->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (s->line_size()[0], 50 * 3);
	BOOST_CHECK (s->data()[0]);
	BOOST_CHECK (!s->data()[1]);
	BOOST_CHECK (!s->data()[2]);
	BOOST_CHECK (!s->data()[3]);

	/* copy constructor */
	SimpleImage* t = new SimpleImage (*s);
	BOOST_CHECK_EQUAL (t->components(), 1);
	BOOST_CHECK_EQUAL (t->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (t->line_size()[0], 50 * 3);
	BOOST_CHECK (t->data()[0]);
	BOOST_CHECK (!t->data()[1]);
	BOOST_CHECK (!t->data()[2]);
	BOOST_CHECK (!t->data()[3]);
	BOOST_CHECK (t->data() != s->data());
	BOOST_CHECK (t->data()[0] != s->data()[0]);
	BOOST_CHECK (t->line_size() != s->line_size());
	BOOST_CHECK (t->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (t->stride() != s->stride());
	BOOST_CHECK (t->stride()[0] == s->stride()[0]);

	/* assignment operator */
	SimpleImage* u = new SimpleImage (PIX_FMT_YUV422P, libdcp::Size (150, 150), true);
	*u = *s;
	BOOST_CHECK_EQUAL (u->components(), 1);
	BOOST_CHECK_EQUAL (u->stride()[0], 50 * 3);
	BOOST_CHECK_EQUAL (u->line_size()[0], 50 * 3);
	BOOST_CHECK (u->data()[0]);
	BOOST_CHECK (!u->data()[1]);
	BOOST_CHECK (!u->data()[2]);
	BOOST_CHECK (!u->data()[3]);
	BOOST_CHECK (u->data() != s->data());
	BOOST_CHECK (u->data()[0] != s->data()[0]);
	BOOST_CHECK (u->line_size() != s->line_size());
	BOOST_CHECK (u->line_size()[0] == s->line_size()[0]);
	BOOST_CHECK (u->stride() != s->stride());
	BOOST_CHECK (u->stride()[0] == s->stride()[0]);

	delete s;
	delete t;
	delete u;
}

BOOST_AUTO_TEST_CASE (crop_image_test)
{
	/* This was to check out a bug with valgrind, and is probably not very useful */
	shared_ptr<SimpleImage> image (new SimpleImage (PIX_FMT_YUV420P, libdcp::Size (16, 16), true));
	image->make_black ();
	Crop crop;
	crop.top = 3;
	image->crop (crop, false);
}
