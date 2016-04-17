#ifndef STANDARD_LIBRARY_FUNCTIONS_NAME_H
#define STANDARD_LIBRARY_FUNCTIONS_NAME_H

namespace corelab {
	namespace utilities{
		void getStandardLibFunNameList(std::unordered_set<std::string> &stdLibFunli){
	  		
	  		stdLibFunli.insert("malloc");
	  		stdLibFunli.insert("realloc");
	  		stdLibFunli.insert("calloc");

	  		stdLibFunli.insert("free");

	  		stdLibFunli.insert("read");
	  		stdLibFunli.insert("close");

	  		stdLibFunli.insert("gets");

	  		stdLibFunli.insert("unlink");
  		}
  	}
}

#endif /* STANDARD_LIBRARY_FUNCTIONS_NAME_H */
