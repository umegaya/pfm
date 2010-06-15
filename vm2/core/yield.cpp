#include "common.h"
#include "yield.h"
#include "fiber.h"

using namespace pfm;

THREAD yield::get_current_thrd(fiber *fb) {
	return fb->ff().curr();
}
