#include <postgres.h>

#include <access/relscan.h>

#include "catalog.h"
#include "dimension_slice.h"
#include "hypertable.h"
#include "scanner.h"


static inline DimensionSlice *
dimension_slice_from_form_data(Form_dimension_slice fd)
{
	DimensionSlice *ds;
	ds = palloc0(sizeof(DimensionSlice));
	memcpy(&ds->fd, fd, sizeof(FormData_dimension_slice));
	return ds;
}

static inline DimensionSlice *
dimension_slice_from_tuple(HeapTuple tuple)
{
	return dimension_slice_from_form_data((Form_dimension_slice ) GETSTRUCT(tuple));
}

static inline Hypercube *
hypercube_alloc(void)
{
	return palloc0(sizeof(Hypercube));
}

static inline void
hypercube_free(Hypercube *hc)
{
	int i;

	for (i = 0; i < hc->num_closed_slices; i++)
		pfree(hc->closed_slices[i]);

	for (i = 0; i < hc->num_open_slices; i++)
		pfree(hc->open_slices[i]);

	pfree(hc);
}

static bool
dimension_slice_tuple_found(TupleInfo *ti, void *data)
{
	DimensionSlice **ds = data;
	*ds = dimension_slice_from_tuple(ti->tuple);
	return false;
}

DimensionSlice *
dimension_slice_scan(int32 dimension_id, int64 coordinate)
{
	Catalog    *catalog = catalog_get();
	DimensionSlice *slice = NULL;
	ScanKeyData scankey[3];
	ScannerCtx	scanCtx = {
		.table = catalog->tables[DIMENSION_SLICE].id,
		.index = catalog->tables[DIMENSION_SLICE].index_ids[DIMENSION_SLICE_DIMENSION_ID_RANGE_START_RANGE_END_IDX],
		.scantype = ScannerTypeIndex,
		.nkeys = 3,
		.scankey = scankey,
		.data = &slice,
		.tuple_found = dimension_slice_tuple_found,
		.lockmode = AccessShareLock,
		.scandirection = ForwardScanDirection,
	};

	/* Perform an index scan for slice matching the dimension's ID and which
	 * encloses the coordinate */
	ScanKeyInit(&scankey[0], Anum_dimension_slice_dimension_id_range_start_range_end_idx_dimension_id,
				BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(dimension_id));
	ScanKeyInit(&scankey[1], Anum_dimension_slice_dimension_id_range_start_range_end_idx_range_start,
				BTGreaterEqualStrategyNumber, F_INT8EQ, Int64GetDatum(coordinate));
	ScanKeyInit(&scankey[2], Anum_dimension_slice_dimension_id_range_start_range_end_idx_range_end,
				BTLessStrategyNumber, F_INT8EQ, Int64GetDatum(coordinate));

	scanner_scan(&scanCtx);

	return slice;
}

static inline bool
scan_dimensions(Hypercube *hc, Dimension *dimensions[], int16 num_dimensions, int64 point[])
{
	int i;

	for (i = 0; i < num_dimensions; i++)
	{
		Dimension *d = dimensions[i];
		DimensionSlice *slice = dimension_slice_scan(d->fd.id, point[i]);

		if (slice == NULL)
			return false;

		if (IS_CLOSED_DIMENSION(d))
			hc->closed_slices[hc->num_closed_slices++] = slice;
		else
			hc->open_slices[hc->num_open_slices++] = slice;
	}
	return true;
}

Hypercube *
dimension_slice_point_scan(Hyperspace *space, int64 point[])
{
	Hypercube *cube = hypercube_alloc();

	if (!scan_dimensions(cube, space->closed_dimensions, space->num_closed_dimensions, point)) {
		hypercube_free(cube);
		return NULL;
	}

	if (!scan_dimensions(cube, space->open_dimensions, space->num_open_dimensions, point)) {
		hypercube_free(cube);
		return NULL;
	}

	return cube;
}

typedef struct PointScanCtx
{
	Hyperspace *hs;
	Hypercube *hc;
	int64 *point;
} PointScanCtx;

static inline bool
point_in_slice(const Form_dimension_slice slice, const int64 coordinate)
{
	return coordinate >= slice->range_start && coordinate < slice->range_end;
}

static inline
DimensionSlice *match_dimension_slice(Form_dimension_slice slice, int64 point[],
									  Dimension *dimensions[], int16 num_dimensions)
{
	int i;

	for (i = 0; i < num_dimensions; i++)
	{
		int32 dimension_id = dimensions[i]->fd.id;
		int64 coordinate = point[i];

		if (slice->dimension_id == dimension_id && point_in_slice(slice, coordinate))
			return dimension_slice_from_form_data(slice);
	}

	return NULL;
}

static bool
point_filter(TupleInfo *ti, void *data)
{
	PointScanCtx *ctx = data;
	Hyperspace *hs = ctx->hs;
	Hypercube *hc = ctx->hc;
	DimensionSlice *slice;

	/* Match closed dimension */
	slice = match_dimension_slice((Form_dimension_slice) GETSTRUCT(ti->tuple), ctx->point,
								  hs->closed_dimensions, hs->num_closed_dimensions);

	if (slice != NULL)
	{
		hc->closed_slices[hc->num_closed_slices++] = slice;
		return hs->num_closed_dimensions != hc->num_closed_slices &&
			hs->num_open_dimensions != hc->num_closed_slices;
	}

	/* Match open dimension */
	slice = match_dimension_slice((Form_dimension_slice) GETSTRUCT(ti->tuple), ctx->point,
								  hs->open_dimensions, hs->num_open_dimensions);

	if (slice != NULL)
	{
		hc->open_slices[hc->num_open_slices++] = slice;
		return hs->num_closed_dimensions != hc->num_closed_slices &&
			hs->num_open_dimensions != hc->num_closed_slices;
	}

	return true;
}

/*
 * Given a N-dimensional point, scan for the hypercube that encloses it.
 *
 * NOTE: This assumes non-overlapping slices.
 */
Hypercube *
dimension_slice_point_scan_heap(Hyperspace *space, int64 point[])
{
	Catalog    *catalog = catalog_get();
	Hypercube *cube = palloc0(sizeof(Hypercube));
	PointScanCtx ctx = {
		.hs = space,
		.hc = cube,
		.point = point,
	};
	ScannerCtx	scanCtx = {
		.table = catalog->tables[DIMENSION_SLICE].id,
		.scantype = ScannerTypeHeap,
		.nkeys = 0,
		.data = &ctx,
		.filter = point_filter,
		.tuple_found = dimension_slice_tuple_found,
		.lockmode = AccessShareLock,
		.scandirection = ForwardScanDirection,
	};

	cube->num_open_slices = cube->num_closed_slices = 0;

	scanner_scan(&scanCtx);

	if (cube->num_open_slices != space->num_open_dimensions ||
		cube->num_closed_slices != space->num_closed_dimensions)
		return NULL;

	return cube;
}
