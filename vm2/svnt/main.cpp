#include "svnt.h"
#include "world.h"
#include "object.h"
#include "connector.h"

using namespace pfm;

int main(int argc, char *argv[])
{
	int r;
	connector_factory cf;
	object_factory of;
	world_factory wf;
	wf.set_cf(&cf);
	fiber_factory<svnt::fiber> ff(of, wf);
	pfms d(ff,cf);
	if ((r = d.init(argc,argv)) < 0) {
		return r;
	}
	return d.run();
}
