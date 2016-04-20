/***
 * Debug.h : Debug Logging 
 *
 * Task-based debug logging module 
 * XXX======== USAGE MODEL ========XXX
	  (a) function-unit task
			BEGIN_TASK ();
	 		task_function ();
			END_TASK ();
			FINISH (); 		// finish logging
	
		(b) loop-unit task
			BEGIN_TASK ();
	 		for (...) {
	 			if (early termination) { 
	 				EXIT_TASK (); break;
				}
	 			if (continue condition) {
					PASS_TASK (); continue;
	 			}
	 			...
				PRINT (); 	// logging
	 			...
	 		}
	 		END_TASK ();
	
		(c) recursion
			BEGIN_RECURSION ();
			recursive_call ();
			END_RECURSION ();

	 	(d) normal logging
	 		PRINT ("log");
	 		...
	 		PRINT_SUB ("additional info");
 * XXX============================XXX
 * Un-terminated task is automatically poped out
 * when its parent task terminates.
 *
 * written by: gwangmu 
 *
 * **/

#ifndef CORELAB_DEBUG_H
#define CORELAB_DEBUG_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <execinfo.h>
#include <unistd.h>

#include <stdarg.h> // BONGJUN: this is required on some machine. I don't know why.

#define DEBUG_LEVEL_MAX 1024

using namespace std;

namespace corelab
{
	class Debug
	{
	private:
		const char *stkTask[DEBUG_LEVEL_MAX];
		int ilevel;

		const char *prefix;

		inline void printStr (const char *str, const char *postfix = NULL) {
			if (postfix != NULL)
				fprintf (stderr, "%s: %s. %s.\n", prefix, str, postfix);
			else
				fprintf (stderr, "%s: %s\n", prefix, str);
		}

		inline void printMsg (const char *msg, va_list args, const char *postfix = NULL) {
			fprintf (stderr, "%s: ", prefix);
			for (int i = 0; i < ilevel; ++i)
				fprintf (stderr, "   ");

			vfprintf (stderr, msg, args);
			if (postfix != NULL)	fprintf (stderr, ". %s.", postfix);
			fprintf (stderr, "\n");			
		}

		inline bool isDifferentStage (const char *s1, const char *s2) {
			return (s1 != s2);
		}

		inline void pushTask (const char *name) {
			stkTask[ilevel] = name;
			ilevel++;
			assert (ilevel < DEBUG_LEVEL_MAX && "debug level exceeded");
		}

		inline void popTask (const char *name) {
			if (ilevel > 0) ilevel--;
			if (isDifferentStage (stkTask[ilevel], name)) {
				bool restored = false;

				// Try to restore
				for (int i = ilevel - 1; i >= 0; i--) {
					if (!isDifferentStage (stkTask[i], name)) {
						ilevel = i;
						restored = true;
						break;
					}
				}
					
				if (!restored) {	
					fprintf (stderr, "!!WARNING!! debug stack mismatch detected");
					fprintf (stderr, "(in_stack:%s, param:%s)\n", stkTask[ilevel], name);
				}
			}
			stkTask[ilevel] = NULL;
		}

	public:
		Debug (const char *prefix) {
			memset (stkTask, 0, DEBUG_LEVEL_MAX * sizeof (const char *));
			ilevel = 0;
			this->prefix = prefix;
		}

		static inline void DUMP_BACKTRACE () {
			int j, nptrs;
			void *buffer[100];
			char **strings;

			nptrs = backtrace(buffer, 100);
			fprintf (stderr, "debug: backtrace() returned %d addresses\n", nptrs);

			/* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO)
				 would produce similar output to the following: */

			strings = backtrace_symbols (buffer, nptrs);
			if (strings == NULL)
			{
				perror ("backtrace_symbols");
				exit (EXIT_FAILURE);
			}

			for (j = 0; j < nptrs; j++)
				fprintf (stderr, "%s\n", strings[j]);

			free(strings);
		}


		/// Logging interface
		inline void PRINT (const char *msg, ...) {
			#ifndef NO_PRINT_DEBUG
			va_list args;
			va_start (args, msg);
			printMsg (msg, args);
			va_end (args);
			#endif
		}

		// XXX USE WHEN PRINTING SUB-INFORMATION OF PREVIOUS LOG XXX
		inline void PRINT_SUB (const char *msg, ...) {
			#ifndef NO_PRINT_DEBUG
			va_list args;
			ilevel++;
			va_start (args, msg);
			printMsg (msg, args);
			va_end (args);
			ilevel--;
			#endif
		}


		/// Task initiating interface
		inline void BEGIN_TASK (const char *name, const char *msg, ...) {
			#ifndef NO_PRINT_DEBUG
			va_list args;
			va_start (args, msg);
			printMsg (msg, args);
			va_end (args);

			pushTask (name);
			#endif
		}


		/// Recursion initiating interface
		inline void BEGIN_RECURSE () {
			#ifndef NO_PRINT_DEBUG
			pushTask ("REC");
			#endif
		}


		/// Task terminating interfaces
		// Use when the task 'NAME' will not reach END_TASK,
		// and will NOT BE FOLLOWED by the same kind of next task. 
		//  (e.g. break, early return)
		inline void EXIT_TASK (const char *name, const char *msg = NULL, ...) {
			#ifndef NO_PRINT_DEBUG
			if (msg) {
				va_list args;
				va_start (args, msg);
				printMsg (msg, args, "Stop");
				va_end (args);
			}

			popTask (name);
			#endif
		}

		// Use when the task 'NAME' will not reach END_TASK,
		// but will be CONTINUED by the same kind of next task. 
		// 	(e.g. continue)
		inline void PASS_TASK (const char *name, const char *msg = NULL, ...) {
			#ifndef NO_PRINT_DEBUG
			if (msg) {
				va_list args;
				va_start (args, msg);
				printMsg (msg, args, "Next.");
				va_end (args);
			}

			popTask (name);
			#endif
		}

		// Use when the task 'NAME' is finished normally
		// 	(e.g. return)
		inline void END_TASK (const char *name) {
			#ifndef NO_PRINT_DEBUG
			popTask (name);
			#endif
		}


		/// Recursion terminating interface
		inline void END_RECURSE () {
			#ifndef NO_PRINT_DEBUG
			popTask ("REC");
			#endif
		}


		inline void ABORT (const char *msg, ...) {
			#ifndef NO_PRINT_DEBUG
			va_list args;
			va_start (args, msg);
			printMsg (msg, args, "Aborted");
			va_end (args);

			exit (888);
			#endif
		}

		inline void FINISH () {
			#ifndef NO_PRINT_DEBUG
			printStr ("Done.");
			#endif
		}
	};
}

#endif
