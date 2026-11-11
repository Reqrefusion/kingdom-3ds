/* FlashParallel_split on the shared engine's job slot
 * (../source/engine/platform/jobs_backend.h, the same worker Lume uses).
 * The worker takes the first half of the rows, the caller the second half.
 * When no worker exists (Old 3DS, LUME_DISABLE_JOBS) or it is busy, everything
 * runs on the caller -- the result is the same either way. */
#include "../../flash/filters.h"
#include "engine/platform/jobs_backend.h"

static LumeMovieJobs *jobs;

typedef struct {
	void (*fn)(void *ctx, int32_t y0, int32_t y1);
	void *ctx;
	int32_t y0, y1;
} Half;

static void run_half(void *arg)
{
	Half *h = arg;
	h->fn(h->ctx, h->y0, h->y1);
}

static void split(void (*fn)(void *ctx, int32_t y0, int32_t y1), void *ctx, int32_t rows)
{
	Half first = {fn, ctx, 0, rows / 2};
	if (lume_jobs_start(jobs, run_half, &first)) {
		fn(ctx, rows / 2, rows);
		lume_jobs_wait(jobs);
	} else
		fn(ctx, 0, rows);
}

/* returns true when a worker core is available */
bool Parallel_init(void)
{
	jobs = lume_jobs_create();
	FlashParallel_split = jobs ? split : NULL;
	return jobs != NULL;
}

void Parallel_close(void)
{
	if (jobs)
		lume_jobs_close(jobs);
	jobs = NULL;
	FlashParallel_split = NULL;
}
