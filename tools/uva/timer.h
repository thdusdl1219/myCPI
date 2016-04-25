/**
 * timer.h: Micro-second timer
 *
 * Micro-second timer w/ dedicated worker thread
 * written by: gwangmu
 * modified by: bongjun
 *
 * **/

#ifndef CORELAB_UVA_TIMER_H
#define CORELAB_UVA_TIMER_H

#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <cassert>

namespace corelab
{
	namespace UVA
	{
		typedef long long UsecTime;
	
		class TimeWorker
		{
		private:
			UsecTime tCurrent;

			TimeWorker () {
				pthread_t worker;
				int workerID;

				updateTime (&tCurrent);
				workerID = pthread_create (&worker, NULL, timeWorker, (void*)&tCurrent);
				assert (workerID >= 0 && "Time worker creation failed");
			}
			
			inline static void updateTime (UsecTime *current) {
				struct timeval tval;
				gettimeofday (&tval, NULL);
				*current = tval.tv_sec * 1000000LL + tval.tv_usec;
			}

			inline static void* timeWorker (void *_current) {
				UsecTime *current = (UsecTime *)_current;

				while (true) 
					updateTime (current);

				return NULL;
			}
		
		public:
			inline static TimeWorker* get () {
				static TimeWorker* worker = NULL;

				if (!worker) worker = new TimeWorker ();
				return worker;
			}

			inline UsecTime getTime () {
				return tCurrent;
			}
		};

		class Timer
		{
		private:
			UsecTime base;
			UsecTime last;

			// Time worker
			TimeWorker *timeWorker;

		public:
			Timer () {
				timeWorker = TimeWorker::get ();
				last = base = timeWorker->getTime ();
			}

			inline void set () {
				last = base = timeWorker->getTime ();
			}

			inline UsecTime elapsedFromBase () {
				UsecTime time = timeWorker->getTime ();
				last = time;
				return time - base;
			}
			
			inline UsecTime elapsedFromLast () {
				UsecTime time = timeWorker->getTime ();
				UsecTime elapsed = time - last;
				last = time;
				return elapsed;
			}

			inline UsecTime getCurrentTime () {
				return timeWorker->getTime ();
			}
		};
	}
}

#endif
