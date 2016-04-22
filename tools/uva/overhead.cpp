/***
 * overhead.cpp: Overhead Tester
 *
 * Test overhead of discrete code sections, 
 * printing usec time span.
 * written by: gwangmu
 * modified by: bongjun
 *
 * **/

#include "overhead.h"

using namespace std;

namespace corelab {
	namespace UVA {
		string OverheadTest::stackToStr () {
			stack<const char *> stkTmp;
			while (!stkID.empty ()) {
				stkTmp.push (stkID.top ());
				stkID.pop ();
			}

			string strStack;
			strStack = string ("[");	

			while (!stkTmp.empty ()) {
				strStack += (string (stkTmp.top ()) + "/ ");
				stkID.push (stkTmp.top ());
				stkTmp.pop ();
			}

			return strStack;
		}


		OverheadTest::OverheadTest () {
			timeWorker = TimeWorker::get ();
		}

		
		OverheadTest* OverheadTest::get () {
			static OverheadTest *overheadTest = NULL;
			
			if (!overheadTest) overheadTest = new OverheadTest ();
			return overheadTest;
		}


		void OverheadTest::pushSection (const char *ID, UsecTime ts) {
			// Calculate timespan
			unsigned timespan = ts - tLast;

			#ifdef OVERHEAD_VERBOSE
			fprintf (stderr, ">> pushing to stack.. (stack: %s, ID: %s)\n", stackToStr().c_str (), ID);
			#endif

			// Accumurate top overhead
			if (!stkID.empty ())
				mapIDToTime[stkID.top()] += timespan;

			// Push ID to stack
			stkID.push (ID);

			// Check the next timespan base
			tLast = timeWorker->getTime ();
		}

		void OverheadTest::popSection (const char *ID, UsecTime ts) {
			// Calculate timespan
			unsigned timespan = ts - tLast;

			#ifdef OVERHEAD_VERBOSE
			fprintf (stderr, ">> popping from stack.. (stack: %s, ID: %s)\n", stackToStr().c_str (), ID);
			#endif

			#ifdef OVERHEAD_VALIDITY_CHECK
			if (stkID.empty ()) {
				fprintf (stderr, "!WARNING! Section stack is empty\n");
				return;
			}

			if (ID != stkID.top ())
				fprintf (stderr, 
					"!WARNING! Section ID '%s' mismatches to the current section ID '%s'\n",
					ID, stkID.top ());
			#endif

			// Accumurate top overhead
			mapIDToTime[stkID.top()] += timespan;

			// Pop ID from stack
			stkID.pop ();

			// Check the next timespan base
			tLast = timeWorker->getTime ();
		}

		void OverheadTest::changeSection (const char *fromID, const char *toID,
				UsecTime ts) {
			// Calculate timespan
			unsigned timespan = ts - tLast;

			#ifdef OVERHEAD_VERBOSE
			fprintf (stderr, ">> changing section.. (stack: %s, fromID: %s, toID: %s)\n", 
				stackToStr().c_str (), fromID, toID);
			#endif

			#ifdef OVERHEAD_VALIDITY_CHECK
			if (stkID.empty ()) {
				fprintf (stderr, "!WARNING! Section stack is empty\n");
				return;
			}

			if (fromID != stkID.top ())
				fprintf (stderr, 
					"!WARNING! Section ID '%s' mismatches to the current section ID '%s'\n",
					fromID, stkID.top ());
			#endif

			// Accumurate top overhead
			mapIDToTime[stkID.top()] += timespan;

			// Pop FROM_ID from stack
			stkID.pop ();

			// Push TO_ID to stack
			stkID.push (toID);

			// Check the next timespan base
			tLast = timeWorker->getTime ();
		}

		void OverheadTest::reset () {
			mapIDToTime.clear ();
			while (stkID.empty ())
				stkID.pop ();
			tLast = 0;
		}


		void OverheadTest::printResult (FILE *stream) {
			unsigned total = 0;

			fprintf (stream, ">> Overhead test result (#section: %u):\n", mapIDToTime.size ());
			for (map<const char *, unsigned>::iterator it = mapIDToTime.begin ();
					 it != mapIDToTime.end (); ++it) {
				fprintf (stream, " - %s: %u usec\n", it->first, it->second);
				total += it->second;
			}
			fprintf (stream, " - [Total] : %u usec\n", total);
		}
	}
}
