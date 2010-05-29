#include "serializer.h"
#include "object.h"
#include "proto.h"
#include "ll.h"


int pfm::rpc::world_request::pack_object(
		pfm::serializer &sr, pfm::object &o)
{
	return pfm::ll::coroutine::pack_object(sr, o); 
}
