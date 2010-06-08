#include "mstr.h"
#include "world.h"
#include "object.h"
#include "connector.h"

using namespace pfm;

int main(int argc, char *argv[])
{
	int r;
	connector_factory cf;
	object_factory of;
	world_factory wf(&cf);
	fiber_factory<mstr::fiber> ff(of, wf);
	pfmm d(ff);
	if ((r = d.init(argc,argv)) < 0) {
		return r;
	}
	return d.run();
}
