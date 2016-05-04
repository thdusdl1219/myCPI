#ifndef IOT_BENCHMARK_UTILITIES_TIMEUTIL_H
#define IOT_BENCHMARK_UTILITIES_TIMEUTIL_H

#include <sys/time.h>

class StopWatch {
  private:
    struct timeval start_time;
    struct timeval end_time;

  public:
    void start(){
      gettimeofday(&start_time, NULL);
    }

    void end() {
      gettimeofday(&end_time, NULL);
    }
    
    double diff() {
      double elapsed_time;

      elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0;      // sec to ms
      elapsed_time += (end_time.tv_usec - start_time.tv_usec) / 1000.0;

      return elapsed_time;
    }

    double diff_us() {
      double elapsed_time;

      elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000000.0;      // sec to ms
      elapsed_time += (end_time.tv_usec - start_time.tv_usec);

      return elapsed_time;
    }
};

#endif
