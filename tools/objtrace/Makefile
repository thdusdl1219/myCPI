##===- projects/sample/lib/sample/Makefile -----------------*- Makefile -*-===##

#
# Indicate where we are relative to the top of the source tree.
#
LEVEL=../..

#
# Give the name of a library.  This will build a dynamic version.
#
LIBRARYNAME=objtrace
DONT_BUILD_RELINKED=1
SHARED_LIBRARY=1

CPP.BaseFlags += -O3 -I.
C.BaseFlags += -O3 -I.

# Include Makefile.common so we know what to do.
#
include $(LEVEL)/Makefile.common
