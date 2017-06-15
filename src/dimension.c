#include <postgres.h>
#include <access/relscan.h>

#include "dimension.h"
#include "scanner.h"

static Dimension *
dimension_from_tuple(HeapTuple tuple)
{
	Dimension *d;
	d = palloc0(sizeof(Dimension));
	memcpy(&d->fd, GETSTRUCT(tuple), sizeof(FormData_dimension));   
	return d;
}

static bool
dimension_tuple_found(TupleInfo *ti, void *data)
{
	Dimension **h = data;
	*h = dimension_from_tuple(ti->tuple);
	return false;
}

#if 0
static void *
dimension_table_scan(Cache *cache, CacheQuery *query)
{
	HypertableCacheQuery *hq = (HypertableCacheQuery *) query;
	Catalog    *catalog = catalog_get();
	HypertableNameCacheEntry *cache_entry = query->result;
	int			number_found;
	ScanKeyData scankey[2];
	ScannerCtx	scanCtx = {
		.table = catalog->tables[HYPERTABLE].id,
		.index = catalog->tables[HYPERTABLE].index_ids[HYPERTABLE_NAME_INDEX],
		.scantype = ScannerTypeIndex,
		.nkeys = 2,
		.scankey = scankey,
		.data = query->result,
		.tuple_found = hypertable_tuple_found,
		.lockmode = AccessShareLock,
		.scandirection = ForwardScanDirection,
	};

	/* Perform an index scan on schema and table. */
	ScanKeyInit(&scankey[0], Anum_dimension_hypertable_id_idx_hypertable_id,
				BTEqualStrategyNumber, F_INT4EQ, );

	number_found = scanner_scan(&scanCtx);

	switch (number_found)
	{
		case 0:
			/* Negative cache entry: table is not a hypertable */
			cache_entry->hypertable = NULL;
			break;
		case 1:
			Assert(strncmp(cache_entry->hypertable->fd.schema_name, hq->schema, NAMEDATALEN) == 0);
			Assert(strncmp(cache_entry->hypertable->fd.table_name, hq->table, NAMEDATALEN) == 0);
			break;
		default:
			elog(ERROR, "Got an unexpected number of records: %d", number_found);
			break;

	}
	return query->result;
}
#endif

#if defined(__MAIN__)
#include <stdio.h>

int main(int argc, char **argv)
{

	printf("Sizeof(Dimension)=%zu\n", sizeof(Dimension));
	return 0;
}
#endif
