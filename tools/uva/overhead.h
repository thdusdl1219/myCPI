/***
 * overhead.h: Overhead Tester
 *
 * Test discrete overhead of code sections, 
 * printing usec time span.
 * written by: gwangmu
 * modified by: bongjun
 *
 * **/

#ifndef CORELAB_OFFLOAD_OVERHEAD_TESTER_H
#define CORELAB_OFFLOAD_OVERHEAD_TESTER_H

#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>
#include <cstdio>
#include <stack>
#include <map>
#include <string>

#include "timer.h"

#define OVERHEAD_TEST_DECL \
	using corelab::UVA::UsecTime;														\
	static corelab::UVA::OverheadTest* __overhead_test__;		\
	static corelab::UVA::TimeWorker* 	__time_worker__;

#define OHDTEST_SETUP() \
	{																																\
		__overhead_test__ = corelab::UVA::OverheadTest::get ();		\
		__time_worker__ = corelab::UVA::TimeWorker::get ();				\
	}

#define OHDTEST_PUSH_SECTION(x) \
	{																									\
		UsecTime tCheck = __time_worker__->getTime ();	\
		__overhead_test__->pushSection (x, tCheck);			\
	}

#define OHDTEST_CHANGE_SECTION(x, y) \
	{																										\
		UsecTime tCheck = __time_worker__->getTime ();		\
		__overhead_test__->changeSection (x, y, tCheck);	\
	}

#define OHDTEST_POP_SECTION(x) \
	{																									\
		UsecTime tCheck = __time_worker__->getTime (); 	\
		__overhead_test__->popSection (x, tCheck); 			\
	}

#define OHDTEST_RESET() \
		__overhead_test__->reset ();

#define OHDTEST_PRINT_RESULT(stream) \
		__overhead_test__->printResult (stream);

using namespace std;

namespace corelab {
	namespace UVA {
		class OverheadTest {
		public:
			static OverheadTest *get ();

			void pushSection (const char *ID, UsecTime ts);
			void popSection (const char *ID, UsecTime ts);
			void changeSection (const char *fromID, const char *toID, UsecTime ts);
			void reset ();

			void printResult (FILE *stream);
		
		private:
			OverheadTest ();

			stack<const char *> stkID;
			map<const char *, unsigned> mapIDToTime;
			unsigned tLast;
			TimeWorker *timeWorker;

			string stackToStr ();
		};
	}
}

#endif
