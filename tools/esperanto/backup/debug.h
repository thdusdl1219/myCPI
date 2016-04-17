#ifndef CORELAB_OFFLOAD_DEBUG_H
#define CORELAB_OFFLOAD_DEBUG_H

#ifndef NDEBUG
#	define DEBUG_STMT(x) x
#else
#	define DEBUG_STMT(x)
#endif

#endif
