#ifndef LLVM_CORELAB_DEPRECATED_H
#define LLVM_CORELAB_DEPRECATED_H

#ifdef __GNUC__

#define DEPRECATED          __attribute__ ((deprecated))
#define NOINLINE            __attribute__ ((noinline))
#define USED                __attribute__ ((used))

#else

#define DEPRECATED
#define NOINLINE
#define USED

#endif

#endif // LLVM_CORELAB_DEPRECATED_H
