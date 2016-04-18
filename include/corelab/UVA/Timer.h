#ifndef CORELAB_OFFLOAD_TIMER_H
#define CORELAB_OFFLOAD_TIMER_H

#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>

namespace corelab
{
	namespace Offload
	{
		namespace Timer
		{
			static unsigned base;
			static unsigned last;

			static inline unsigned
			GetTimeInSec ()
			{
				struct timeval tval;
				gettimeofday (&tval, NULL);
				return tval.tv_sec * 1000 + tval.tv_usec / 1000;
			}

			inline void
			Set ()
			{
				last = base = GetTimeInSec();
			}

			inline unsigned
			ElapsedFromBase ()
			{
				unsigned time = GetTimeInSec();
				last = time;
				return time - base;
			}
			
			inline unsigned
			ElapsedFromLast ()
			{
				unsigned time = GetTimeInSec();
				unsigned elapsed = time - last;
				last = time;
				return elapsed;
			}
		}
	}
}

#endif
