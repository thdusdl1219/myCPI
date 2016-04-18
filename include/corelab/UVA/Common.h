#ifndef CORELAB_OFFLOAD_OBJ_TRACER_COMMON_H
#define CORELAB_OFFLOAD_OBJ_TRACER_COMMON_H

#include <inttypes.h>

#define LOOP_TYPE 0
#define FUNC_TYPE 1

#define INSTR_ID_BITS 16
#define OBJ_ID_BITS 	16
#define FUNC_ID_BITS 	16
#define LOOP_ID_BITS 	16

namespace corelab {
	namespace Offload {
		typedef uint16_t 	InstrID;
		typedef uint16_t 	ObjID;
		typedef uint16_t 	FuncID;
		typedef uint16_t 	LoopID;

		inline char getRtnCharFromType (unsigned type) {
			switch (type) {
				case LOOP_TYPE:
					return 'L';
				case FUNC_TYPE:
					return 'F';
			}

			return 0;
		}

		inline unsigned getRtnTypeFromChar (char c) {
			switch (c) {
				case 'L':
					return LOOP_TYPE;
				case 'F':
					return FUNC_TYPE;
			}
				
			return 0;
		}
	}
}

#endif
