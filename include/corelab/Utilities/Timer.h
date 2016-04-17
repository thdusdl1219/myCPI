// A convenience macro to measure how long something takes.
// TIME(name,cmd) will execute the statement cmd (which can
// be any valid C statement), and then prints out how
// long it took to execute, in seconds.  The name parameter
// lets you (mildly) customize the message.

// CAUTION: the command /cmd/ is run in a nested scope, and
// so if it declares any variables, they will not be accessible
// after this has run.
#ifndef LLVM_CORELAB_TIMER_H
#define LLVM_CORELAB_TIMER_H

#include "llvm/Support/Debug.h"

#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>

#ifndef LONG_TIME
/// Make a big deal about tasks which take longer than this many seconds
#define LONG_TIME   (0.10)
#endif

#define TIME(name, ...)                             \
  do                                                \
  {                                                 \
    struct timeval start, stop;                     \
    DEBUG(                                          \
      errs() << "Starting task " << name << ".\n";  \
      gettimeofday(&start,0);                       \
    );                                              \
    __VA_ARGS__;                                    \
    DEBUG(                                          \
      gettimeofday(&stop,0);                        \
      double latency = (stop.tv_sec - start.tv_sec) \
           + 1.e-6 * (stop.tv_usec - start.tv_usec);\
      static double sum = 0.0;                      \
      static int num = 0;                           \
      sum += latency;                               \
      num ++;                                       \
      if( latency > 1.0 )                           \
        errs() << "*******************************" \
                  "*******************************" \
                  "*******************************";\
      errs() << "Task " << name                     \
             << " completed in " << latency         \
             << " seconds; average is "             \
             << (sum/num) << " seconds.\n";         \
    );                                              \
  } while(0)                                        \



//made by juhyun
namespace corelab
{
  namespace Timer
  {
    static double base;
    static double last;

    static inline double
    GetTimeInSec ()
    {
      struct timeval tval;
      gettimeofday (&tval, NULL);
      return tval.tv_sec + tval.tv_usec / 1000000.0;
    }

    inline void
    Set ()
    {
      last = base = GetTimeInSec();
    }

    inline double
    ElapsedFromBase ()
    {
      double time = GetTimeInSec();
      last = time;
      return time - base;
    }
    
    inline double
    ElapsedFromLast ()
    {
      double time = GetTimeInSec();
      double elapsed = time - last;
      last = time;
      return elapsed;
    }
  }
}


#endif // LLVM_CORELAB_TIMER_H
