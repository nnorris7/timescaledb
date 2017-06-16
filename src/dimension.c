#include <postgres.h>
#include <access/relscan.h>
#include <utils/lsyscache.h>

#include "catalog.h"
#include "dimension.h"
#include "hypertable.h"
#include "scanner.h"
#include "partitioning.h"
#include "utils.h"

#define PARTITIONING_MODULO (USHRT_MAX)


static Dimension *
dimension_from_tuple(HeapTuple tuple, Oid main_table_relid)
{
	Dimension *d;

	d = palloc0(sizeof(Dimension));
	memcpy(&d->fd, GETSTRUCT(tuple), sizeof(FormData_dimension));

	/* If there is no partitioning func set we assume open dimension */
	d->type = heap_attisnull(tuple, Anum_dimension_partitioning_func) ?
		DIMENSION_TYPE_OPEN : DIMENSION_TYPE_CLOSED;

	if (d->type == DIMENSION_TYPE_CLOSED)
	{
		d->partitioning = partitioning_info_create(d->fd.num_slices,
												   NameStr(d->fd.partitioning_func_schema),
												   NameStr(d->fd.partitioning_func),
												   NameStr(d->fd.column_name),
												   PARTITIONING_MODULO,
												   main_table_relid);
	}

	d->column_attno = get_attnum(main_table_relid, NameStr(d->fd.column_name));

	return d;
}

static Hyperspace *
hyperspace_create(int32 hypertable_id, Oid main_table_relid)
{
	Hyperspace *hs = palloc0(sizeof(Hyperspace));
	hs->hypertable_id = hypertable_id;
	hs->main_table_relid = main_table_relid;
	hs->num_closed_dimensions = hs->num_closed_dimensions = 0;
	return hs;
}

static bool
dimension_tuple_found(TupleInfo *ti, void *data)
{
	Hyperspace *hs = data;
	Dimension *d = dimension_from_tuple(ti->tuple, hs->main_table_relid);

	if (IS_OPEN_DIMENSION(d))
		hs->open_dimensions[hs->num_open_dimensions++] = d;
	else
		hs->closed_dimensions[hs->num_closed_dimensions++] = d;

	return true;
}

Hyperspace *
dimension_scan(int32 hypertable_id, Oid main_table_relid)
{
	Catalog    *catalog = catalog_get();
	Hyperspace *space = hyperspace_create(hypertable_id, main_table_relid);
	ScanKeyData scankey[1];
	ScannerCtx	scanCtx = {
		.table = catalog->tables[DIMENSION].id,
		.index = catalog->tables[DIMENSION].index_ids[DIMENSION_HYPERTABLE_ID_IDX],
		.scantype = ScannerTypeIndex,
		.nkeys = 1,
		.scankey = scankey,
		.data = space,
		.tuple_found = dimension_tuple_found,
		.lockmode = AccessShareLock,
		.scandirection = ForwardScanDirection,
	};

	/* Perform an index scan on schema and table. */
	ScanKeyInit(&scankey[0], Anum_dimension_hypertable_id_idx_hypertable_id,
				BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(hypertable_id));

	scanner_scan(&scanCtx);

	return space;
}

static Point *
point_create(int16 num_dimensions)
{
	Point *p = palloc0(sizeof(Point) + (sizeof(int64) * num_dimensions));
	p->cardinality = num_dimensions;
	p->num_closed = p->num_open = 0;
	return p;
}
	
Point *
hyperspace_calculate_point(Hyperspace *hs, HeapTuple tuple, TupleDesc tupdesc)
{
	Point *p = point_create(HYPERSPACE_NUM_DIMENSIONS(hs));
	int i;

	for (i = 0; i < hs->num_open_dimensions; i++)
	{
		Dimension *d = hs->open_dimensions[i];
		Datum		datum;
		bool		isnull;

		datum = heap_getattr(tuple, d->column_attno, tupdesc, &isnull);

		if (isnull)
		{
			elog(ERROR, "Time attribute not found in tuple");
		}

		p->coordinates[p->num_open++] = time_value_to_internal(datum, d->fd.column_type);
	}

	for (i = 0; i < hs->num_closed_dimensions; i++)
	{
		Dimension *d = hs->closed_dimensions[i];
		p->coordinates[p->num_open + p->num_closed++] = partitioning_func_apply_tuple(d->partitioning, tuple, tupdesc);
	}

	return p;
}
