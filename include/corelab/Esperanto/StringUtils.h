#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/IR/DataLayout.h"

namespace corelab{
	using namespace llvm;
	using namespace std;

	class StringUtils{
		public:
			static StringRef Substring(StringRef source,int start, int size){
				char* returnAddress = (char*)malloc(size+1);
				for(int i=0;i<size;i++){
					returnAddress[i] = source.data()[start+i];
				}
				returnAddress[size] = '\0';
				std::string ret;
				ret.append(returnAddress);
				return StringRef(ret);
			}
			
			static StringRef DropFront(StringRef source,int size){
				int newSize = (int)source.size()-size;
				char* returnAddress = (char*)malloc(newSize+1);
				for(int i=0;i<newSize;i++)
					returnAddress[i] = source.data()[i+size];
				returnAddress[newSize] = '\0';
				return StringRef(returnAddress);

			}
	};
}

#endif
