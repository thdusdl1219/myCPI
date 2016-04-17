#ifndef LLVM_CORELAB_TRED_H
#define LLVM_CORELAB_TRED_H

//how to define PATH_TO_TRED ?? by juhyun
//#define PATH_TO_TRED 0

namespace corelab
{
  // Run the graphviz 'tred' utility.  Performs
  // transitive reduction on a graph.
  void runTred(const char *infile, const char *outfile, unsigned timeout=30u);
}

#endif

