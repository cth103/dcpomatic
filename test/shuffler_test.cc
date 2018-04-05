#include "lib/shuffler.h"
#include "lib/piece.h"
#include "lib/content_video.h"
#include <boost/test/unit_test.hpp>

using std::list;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::optional;

static void
push (Shuffler& s, int frame, Eyes eyes)
{
	shared_ptr<Piece> piece (new Piece (shared_ptr<Content>(), shared_ptr<Decoder>(), FrameRateChange(24, 24)));
	ContentVideo cv;
	cv.frame = frame;
	cv.eyes = eyes;
	s.video (piece, cv);
}

list<ContentVideo> pending_cv;

static void
receive (weak_ptr<Piece>, ContentVideo cv)
{
	pending_cv.push_back (cv);
}

static void
check (int frame, Eyes eyes, int line)
{
	BOOST_REQUIRE_MESSAGE (!pending_cv.empty(), "Check at " << line << " failed.");
	BOOST_CHECK_MESSAGE (pending_cv.front().frame == frame, "Check at " << line << " failed.");
	BOOST_CHECK_MESSAGE (pending_cv.front().eyes == eyes, "Check at " << line << " failed.");
	pending_cv.pop_front();
}

/** A perfect sequence */
BOOST_AUTO_TEST_CASE (shuffler_test1)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	for (int i = 0; i < 10; ++i) {
		push (s, i, EYES_LEFT);
		push (s, i, EYES_RIGHT);
		check (i, EYES_LEFT, __LINE__);
		check (i, EYES_RIGHT, __LINE__);
	}
}

/** Everything present but some simple shuffling needed */
BOOST_AUTO_TEST_CASE (shuffler_test2)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	for (int i = 0; i < 10; i += 2) {
		push (s, i, EYES_LEFT);
		push (s, i + 1, EYES_LEFT);
		push (s, i, EYES_RIGHT);
		push (s, i + 1, EYES_RIGHT);
		check (i, EYES_LEFT, __LINE__);
		check (i, EYES_RIGHT, __LINE__);
		check (i + 1, EYES_LEFT, __LINE__);
		check (i + 1, EYES_RIGHT, __LINE__);
	}
}

/** One missing left eye image */
BOOST_AUTO_TEST_CASE (shuffler_test3)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	push (s, 0, EYES_LEFT);
	check (0, EYES_LEFT, __LINE__);
	push (s, 0, EYES_RIGHT);
	check (0, EYES_RIGHT, __LINE__);
	push (s, 1, EYES_LEFT);
	check (1, EYES_LEFT, __LINE__);
	push (s, 1, EYES_RIGHT);
	check (1, EYES_RIGHT, __LINE__);
	push (s, 2, EYES_RIGHT);
	push (s, 3, EYES_LEFT);
	push (s, 3, EYES_RIGHT);
	push (s, 4, EYES_LEFT);
	push (s, 4, EYES_RIGHT);
	s.flush ();
	check (2, EYES_RIGHT, __LINE__);
	check (3, EYES_LEFT, __LINE__);
	check (3, EYES_RIGHT, __LINE__);
	check (4, EYES_LEFT, __LINE__);
	check (4, EYES_RIGHT, __LINE__);
}

/** One missing right eye image */
BOOST_AUTO_TEST_CASE (shuffler_test4)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	push (s, 0, EYES_LEFT);
	check (0, EYES_LEFT, __LINE__);
	push (s, 0, EYES_RIGHT);
	check (0, EYES_RIGHT, __LINE__);
	push (s, 1, EYES_LEFT);
	check (1, EYES_LEFT, __LINE__);
	push (s, 1, EYES_RIGHT);
	check (1, EYES_RIGHT, __LINE__);
	push (s, 2, EYES_LEFT);
	push (s, 3, EYES_LEFT);
	push (s, 3, EYES_RIGHT);
	push (s, 4, EYES_LEFT);
	push (s, 4, EYES_RIGHT);
	s.flush ();
	check (2, EYES_LEFT, __LINE__);
	check (3, EYES_LEFT, __LINE__);
	check (3, EYES_RIGHT, __LINE__);
	check (4, EYES_LEFT, __LINE__);
	check (4, EYES_RIGHT, __LINE__);
}

/** Only one eye */
BOOST_AUTO_TEST_CASE (shuffler_test5)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	/* One left should come out straight away */
	push (s, 0, EYES_LEFT);
	check (0, EYES_LEFT, __LINE__);

	/* More lefts should be kept in the shuffler in the hope that some rights arrive */
	for (int i = 0; i < 8; ++i) {
		push (s, i + 1, EYES_LEFT);
	}
	BOOST_CHECK (pending_cv.empty ());

	/* If enough lefts come the shuffler should conclude that there's no rights and start
	   giving out the lefts.
	*/
	push (s, 9, EYES_LEFT);
	check (1, EYES_LEFT, __LINE__);
}
