#if !defined(__LL_H__)
#define __LL_H__

#include "ll/lua.h"
namespace pfm {
typedef lua ll_impl;
class ll : public ll_impl {
protected:
	THREAD m_thrd;
public:
	ll(class object_factory &of, class world_factory &wf, 
		class serializer &sr, THREAD thrd) :
		ll_impl(of, wf, sr), m_thrd(thrd) {}
	THREAD thrd() const { return m_thrd; }
	static inline class ll *cast(ll_impl *li);
	static inline class ll &cast(ll_impl &li);
};
inline class ll *ll::cast(ll_impl *li) { return (class ll *)li; }
inline class ll &ll::cast(ll_impl &li) { return (class ll &)li; }
}

#endif
