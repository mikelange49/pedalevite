
// g++ --std=c++11 -I.. -l wiringPi -l pthread test-7920-03.cpp ../mfx/ui/DisplayInterface.cpp ../mfx/ui/DisplayPi3St7920.cpp ../mfx/ui/TimeShareThread.cpp ../mfx/ui/TimeShareCbInterface.cpp

#include "mfx/ui/DisplayPi3St7920.h"
#include "mfx/ui/TimeShareThread.h"

#include <unistd.h>

#include <wiringPi.h>
#include <wiringPiSPI.h>

#include <cstdint>
#include <cstdio>
#include <ctime>



static const int MAIN_pin_reset = 18;



static int64_t MAIN_get_time ()
{
	timespec       tp;
	clock_gettime (CLOCK_REALTIME, &tp);

	const long     ns_mul = 1000L * 1000L * 1000L;
	return int64_t (tp.tv_sec) * ns_mul + tp.tv_nsec;
}



int main (int argc, char *argv [])
{
	::wiringPiSetupPhys ();

	::pinMode (MAIN_pin_reset, OUTPUT);

	::digitalWrite (MAIN_pin_reset, LOW);
	::delay (100);
	::digitalWrite (MAIN_pin_reset, HIGH);
	::delay (100);

	int             hnd = ::wiringPiSPISetup (0, 1000 * 1000);
	if (hnd == -1)
	{
		exit (-1);
	}

	mfx::ui::TimeShareThread tst (0);
	mfx::ui::DisplayPi3St7920 display (tst);

	uint8_t *       p_ptr  = display.use_screen_buf ();
	const int       scr_w  = display.get_width ();
	const int       scr_h  = display.get_height ();
	const int       scr_s  = display.get_stride ();

	double          a = 0;
	const int       r = 16;
	int             count = 0;
	while (true)
	{
		if ((count % 10) == 0)
		{
			const int       cx = scr_w * 0.5 + int (cos (a) * scr_w * 0.25);
			const int       cy = scr_h * 0.5 + int (sin (a) * scr_h * 0.25);
			for (int y = 0; y < scr_h; ++y)
			{
				const int       dy  = y - cy;
				const int       dy2 = dy * dy;
				for (int x = 0; x < scr_w; ++x)
				{
					const int     dx  = x - cx;
					const int     dx2 = dx * dx;
					uint8_t       val = 0;
					if (dy2 + dx2 <= r * r)
					{
						val = 255;
					}
					p_ptr [y * scr_s + x] = val;
				}
			}
			
			display.refresh (0, 0, scr_w, scr_h);
			a += 0.05;
		}
		
		if (true)
		{
			tst.process_single_task ();
		}

		if (true)
		{
			static const int  s = 3;
			for (int i = 0; i < 8; ++i)
			{
				const int      chn     = i;
				const int      msg_len = 3;
				uint8_t        buffer [msg_len] =
				{
					uint8_t ((0x01 << s) + ((chn << (s + 4)) >> 8)),
					uint8_t (chn << (s + 4)),
					0
				};
				::wiringPiSPIDataRW (0, &buffer [0], msg_len);
			}
		}

		::delay (20);
		
		++ count;
	}
	
	close (hnd);
	
	return 0;
}

