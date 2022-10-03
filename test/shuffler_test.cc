#include "lib/shuffler.h"
#include "lib/piece.h"
#include "lib/content_video.h"
#include <boost/test/unit_test.hpp>

using std::list;
using std::shared_ptr;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


static void
push(Shuffler& s, int frame, Eyes eyes)
{
	shared_ptr<Piece> piece (new Piece (shared_ptr<Content>(), shared_ptr<Decoder>(), FrameRateChange(24, 24)));
	ContentVideo cv;
	cv.time = ContentTime::from_frames(frame, 24);
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
	auto const time = ContentTime::from_frames(frame, 24);
	BOOST_REQUIRE_MESSAGE (!pending_cv.empty(), "Check at " << line << " failed.");
	BOOST_CHECK_MESSAGE (pending_cv.front().time == time, "Check at " << line << " failed.");
	BOOST_CHECK_MESSAGE (pending_cv.front().eyes == eyes, "Check at " << line << " failed.");
	pending_cv.pop_front();
}

/** A perfect sequence */
BOOST_AUTO_TEST_CASE (shuffler_test1)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	for (int i = 0; i < 10; ++i) {
		push (s, i, Eyes::LEFT);
		push (s, i, Eyes::RIGHT);
		check (i, Eyes::LEFT, __LINE__);
		check (i, Eyes::RIGHT, __LINE__);
	}
}

/** Everything present but some simple shuffling needed */
BOOST_AUTO_TEST_CASE (shuffler_test2)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	for (int i = 0; i < 10; i += 2) {
		push (s, i, Eyes::LEFT);
		push (s, i + 1, Eyes::LEFT);
		push (s, i, Eyes::RIGHT);
		push (s, i + 1, Eyes::RIGHT);
		check (i, Eyes::LEFT, __LINE__);
		check (i, Eyes::RIGHT, __LINE__);
		check (i + 1, Eyes::LEFT, __LINE__);
		check (i + 1, Eyes::RIGHT, __LINE__);
	}
}

/** One missing left eye image */
BOOST_AUTO_TEST_CASE (shuffler_test3)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	push (s, 0, Eyes::LEFT);
	check (0, Eyes::LEFT, __LINE__);
	push (s, 0, Eyes::RIGHT);
	check (0, Eyes::RIGHT, __LINE__);
	push (s, 1, Eyes::LEFT);
	check (1, Eyes::LEFT, __LINE__);
	push (s, 1, Eyes::RIGHT);
	check (1, Eyes::RIGHT, __LINE__);
	push (s, 2, Eyes::RIGHT);
	push (s, 3, Eyes::LEFT);
	push (s, 3, Eyes::RIGHT);
	push (s, 4, Eyes::LEFT);
	push (s, 4, Eyes::RIGHT);
	s.flush ();
	check (2, Eyes::RIGHT, __LINE__);
	check (3, Eyes::LEFT, __LINE__);
	check (3, Eyes::RIGHT, __LINE__);
	check (4, Eyes::LEFT, __LINE__);
	check (4, Eyes::RIGHT, __LINE__);
}

/** One missing right eye image */
BOOST_AUTO_TEST_CASE (shuffler_test4)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	push (s, 0, Eyes::LEFT);
	check (0, Eyes::LEFT, __LINE__);
	push (s, 0, Eyes::RIGHT);
	check (0, Eyes::RIGHT, __LINE__);
	push (s, 1, Eyes::LEFT);
	check (1, Eyes::LEFT, __LINE__);
	push (s, 1, Eyes::RIGHT);
	check (1, Eyes::RIGHT, __LINE__);
	push (s, 2, Eyes::LEFT);
	push (s, 3, Eyes::LEFT);
	push (s, 3, Eyes::RIGHT);
	push (s, 4, Eyes::LEFT);
	push (s, 4, Eyes::RIGHT);
	s.flush ();
	check (2, Eyes::LEFT, __LINE__);
	check (3, Eyes::LEFT, __LINE__);
	check (3, Eyes::RIGHT, __LINE__);
	check (4, Eyes::LEFT, __LINE__);
	check (4, Eyes::RIGHT, __LINE__);
}

/** Only one eye */
BOOST_AUTO_TEST_CASE (shuffler_test5)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	/* One left should come out straight away */
	push (s, 0, Eyes::LEFT);
	check (0, Eyes::LEFT, __LINE__);

	/* More lefts should be kept in the shuffler in the hope that some rights arrive */
	for (int i = 0; i < s._max_size; ++i) {
		push (s, i + 1, Eyes::LEFT);
	}
	BOOST_CHECK (pending_cv.empty ());

	/* If enough lefts come the shuffler should conclude that there's no rights and start
	   giving out the lefts.
	*/
	push (s, s._max_size + 1, Eyes::LEFT);
	check (1, Eyes::LEFT, __LINE__);
}

/** One complete frame (L+R) missing.
    Shuffler should carry on, skipping this frame, as the player will cope with it.
*/
BOOST_AUTO_TEST_CASE (shuffler_test6)
{
	Shuffler s;
	s.Video.connect (boost::bind (&receive, _1, _2));

	push (s, 0, Eyes::LEFT);
	check (0, Eyes::LEFT, __LINE__);
	push (s, 0, Eyes::RIGHT);
	check (0, Eyes::RIGHT, __LINE__);

	push (s, 2, Eyes::LEFT);
	push (s, 2, Eyes::RIGHT);
	check (2, Eyes::LEFT, __LINE__);
	check (2, Eyes::RIGHT, __LINE__);

	push (s, 3, Eyes::LEFT);
	check (3, Eyes::LEFT, __LINE__);
	push (s, 3, Eyes::RIGHT);
	check (3, Eyes::RIGHT, __LINE__);
}
