#include "mstr.h"
#include "world.h"
#include "object.h"
#include "connector.h"
#include "cp.h"

using namespace pfm;

int main(int argc, char *argv[])
{
	int r;
	connector_factory cf;
	object_factory of;
	world_factory wf;
	wf.set_cf(&cf);
	fiber_factory<mstr::fiber> ff(of, wf);
	pfmm d(ff,cf);
	if ((r = d.init(argc,argv)) < 0) {
		return r;
	}
	return d.run();
}
