#ifndef LOG_H
#define LOG_H
#include<stdio.h>

#define ERROR(fmt, ...) \
                  { fprintf (stderr, "ERROR[%s:%d]: ", __func__,__LINE__); \
                                      fprintf (stderr, fmt, ## __VA_ARGS__);}

#ifndef NDEBUG
/* For debug */
#define LOG(fmt, ...) \
                  { fprintf (stderr, "LOG[%s:%d]: ", __PRETTY_FUNCTION__,__LINE__); \
                                      fprintf (stderr, fmt, ## __VA_ARGS__);}
#else
/* For release */
#define LOG(fmt, ...)
#endif

#endif
