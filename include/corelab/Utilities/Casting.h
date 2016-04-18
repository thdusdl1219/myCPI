/***
 *
 * Casting.h
 *
 * "Casting" class contains "castTo" function, which cast value 'from' to the type of value 'to'.
 * Cast procedure are composed with two phase. 
 * First one is to change the original type to int type, accompanying bitcast in need. 
 * In second step, cast to target type from the result of first step.
 *
 */
#ifndef LLVM_CORELAB_CASTING_H
#define LLVM_CORELAB_CASTING_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "corelab/Utilities/InstInsertPt.h"

namespace corelab {
	using namespace llvm;
	using namespace std;
	
	class Casting {
		public:
			static Value* castTo(Value* from, Value* to, InstInsertPt& out, const DataLayout* dl) {
				LLVMContext &Context = from->getContext();
				const size_t fromSize = dl->getTypeSizeInBits( from->getType() );
				const size_t toSize = dl->getTypeSizeInBits( to->getType() );

				// First make it an integer
				if( ! from->getType()->isIntegerTy() ) {
					// cast to integer of same size of bits
					Type *integer = IntegerType::get(Context, fromSize);
					Instruction *cast;
					if( from->getType()->getTypeID() == Type::PointerTyID )
						cast = new PtrToIntInst(from, integer);
					else {
						cast = new BitCastInst(from, integer);
					}
					out << cast;
					from = cast;
				} 

				// Next, make it have the same size
				if( fromSize < toSize ) {
					Type *integer = IntegerType::get(Context, toSize);
					Instruction *cast = new ZExtInst(from, integer);
					out << cast;
					from = cast;
				} else if ( fromSize > toSize ) {
					Type *integer = IntegerType::get(Context, toSize);
					Instruction *cast = new TruncInst(from, integer);
					out << cast;
					from = cast;
				}

				// possibly bitcast it to the approriate type
				if( to->getType() != from->getType() ) {
					Instruction *cast;
					if( to->getType()->getTypeID() == Type::PointerTyID )
						cast = new IntToPtrInst(from, to->getType() );
					else {
						cast = new BitCastInst(from, to->getType() );
					}

					out << cast;
					from = cast;
				}

				return from;
			}
	};
}

#endif
