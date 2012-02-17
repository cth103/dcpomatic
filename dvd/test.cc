#include <iostream>
#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_read.h>

using namespace std;

int dvdtime2msec(dvd_time_t *dt)
{
	float fps = 0;
	switch ((dt->frame_u & 0xc0) >> 6) {
	case 1:
		fps = 25;
		break;
	case 3:
		fps = 29.97;
		break;
	}
	
	int ms = (((dt->hour & 0xf0) >> 3) * 5 + (dt->hour & 0x0f)) * 3600000;
	ms += (((dt->minute & 0xf0) >> 3) * 5 + (dt->minute & 0x0f)) * 60000;
	ms += (((dt->second & 0xf0) >> 3) * 5 + (dt->second & 0x0f)) * 1000;

	if (fps > 0) {
		ms += ((dt->frame_u & 0x30) >> 3) * 5 + (dt->frame_u & 0x0f) * 1000.0 / fps;
	}

	return ms;
}

int main ()
{
	dvd_reader_t* dvd = DVDOpen ("/home/carl/Video/Asterix/VIDEO_TS");
	if (dvd == 0) {
		cerr << "bad.\n";
	}

	ifo_handle_t* zero = ifoOpen (dvd, 0);
	if (zero == 0) {
		cerr << "bad.\n";
	}

	ifo_handle_t** ifo = new ifo_handle_t*[zero->vts_atrt->nr_of_vtss + 1];
	ifo[0] = zero;
	for (int i = 1; i <= ifo[0]->vts_atrt->nr_of_vtss; ++i) {
		ifo[i] = ifoOpen (dvd, i);
		if (ifo[i] == 0) {
			cerr << "bad\n";
		}
	}

	cout << "ifos = " << zero->vts_atrt->nr_of_vtss << "\n";

	int const titles = ifo[0]->tt_srpt->nr_of_srpts;

	for (int i = 0; i < titles; ++i) {
		cout << "Title " << i << "\n";
		int const title_set_nr = ifo[0]->tt_srpt->title[i].title_set_nr;
		cout << "\ttitle_set_nr " << title_set_nr << "\n";
		int const vts_ttn = ifo[0]->tt_srpt->title[i].vts_ttn;
		pgcit_t* pgcit = ifo[title_set_nr]->vts_pgcit;
		pgc_t* pgc = pgcit->pgci_srp[ifo[title_set_nr]->vts_ptt_srpt->title[vts_ttn - 1].ptt[0].pgcn - 1].pgc;
		cout << "\t" << dvdtime2msec (&pgc->playback_time) / 1000 << " seconds\n";

		cout << "\t " << ((int) pgc->nr_of_programs) << " chapters.\n";
	}

	return 0;
}
