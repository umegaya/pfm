#if !defined(__LL_H__)
#define __LL_H__

#include "ll/lua.h"
namespace pfm {
typedef lua ll_impl;
class ll : public ll_impl {
public:
	ll(class object_factory &of, class world_factory &wf, 
		class serializer &sr, class msgid_generator &seed) : 
	ll_impl(of, wf, sr, seed) {}
	static inline class ll *to_ll(ll_impl *li);
};
inline class ll *ll::to_ll(ll_impl *li) { return (class ll *)li; }
}

#endif
