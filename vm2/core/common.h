#if !defined(__COMMON_H__)
#define __COMMON_H__

#include "sfc.hpp"
#include "nbr_pkt.h"
#include "macro.h"

#if defined(_TEST)
#define TEST_VIRTUAL virtual
#else
#define TEST_VIRTUAL
#endif

#define LOG TRACE	/* kari */

#define PREPARE_PACK(scr)		\
		char __b[4 * 1024];	\
		((serializer &)(scr)).pack_start(__b, sizeof(__b));

#define PREPARE_UNPACK(scr)		\
		((serializer &)(scr)).unpack_start(((serializer &)(scr)).p(),	\
						((serializer &)(scr)).len());

#define INIT_OR_DIE(cond, ret, ...)	\
	if (cond) {	\
		TRACE(__VA_ARGS__);	\
		return ret;	\
	}

#endif
