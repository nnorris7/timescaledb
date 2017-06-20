#include <postgres.h>
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/catcache.h"
#include "utils/builtins.h"
#include "executor/spi.h"
#include "access/xact.h"

#include "metadata_queries.h"
#include "utils.h"
#include "partitioning.h"
#include "chunk.h"

/* Utility function to prepare an SPI plan */
static SPIPlanPtr
prepare_plan(const char *src, int nargs, Oid *argtypes)
{
	SPIPlanPtr	plan;

	if (SPI_connect() < 0)
	{
		elog(ERROR, "Could not connect for prepare");
	}
	plan = SPI_prepare(src, nargs, argtypes);
	if (NULL == plan)
	{
		elog(ERROR, "Could not prepare plan");
	}
	if (SPI_keepplan(plan) != 0)
	{
		elog(ERROR, "Could not keep plan");
	}
	if (SPI_finish() < 0)
	{
		elog(ERROR, "Could not finish for prepare");
	}

	return plan;
}

#define DEFINE_PLAN(fnname, query, num, args)	\
	static SPIPlanPtr fnname() {				\
		static SPIPlanPtr plan = NULL;			\
		if(plan != NULL)						\
			return plan;						\
		plan = prepare_plan(query, num, args);	\
		return plan;							\
	}

/*
 * Retrieving chunks:
 *
 * Locked chunk retrieval has to occur on every row. So we have a fast and slowpath.
 * The fastpath retrieves and locks the chunk only if it already exists locally. The
 * fastpath is faster since it does not call a plpgsql function but calls sql directly.
 * This was found to make a performance difference in tests.
 *
 * The slowpath calls  get_or_create_chunk(), and is called only if the fastpath returned no rows.
 *
 */
#define INT8ARRAYOID 1016

#define CHUNK_QUERY_ARGS (Oid[]) {INT4ARRAYOID, INT8ARRAYOID}
#define CHUNK_QUERY "SELECT * \
				FROM _timescaledb_internal.chunk_get_or_create($1, $2)"

/* plan for getting a chunk via get_or_create_chunk(). */
DEFINE_PLAN(get_chunk_plan, CHUNK_QUERY, 4, CHUNK_QUERY_ARGS)

#define CHUNK_CREATE_ARGS (Oid[]) {INT4ARRAYOID, INT8ARRAYOID} /* 1016 is int 8 array */
#define CHUNK_CREATE "SELECT * \
				FROM _timescaledb_internal.chunk_create($1, $2)"

/* plan for creating a chunk via create_chunk(). */
DEFINE_PLAN(create_chunk_plan, CHUNK_CREATE, 2, CHUNK_CREATE_ARGS)

static HeapTuple
chunk_tuple_create_spi_connected(int32 time_dimension_id, int64 time_value,
								int32 space_dimension_id, int64 space_value,
								TupleDesc *desc, SPIPlanPtr plan)
{
	int			ret;
	Datum		dimension_ids[2] = {
		Int32GetDatum(time_dimension_id), Int32GetDatum(space_dimension_id),
	};

	Datum		dimension_values[2] = {
		Int64GetDatum(time_value), Int64GetDatum(space_value)
	};

	int num_dimensions = 2;
	if (space_dimension_id == 0) {
		num_dimensions = 1;
	}


	Datum		args[2] = {
         PointerGetDatum(construct_array(dimension_ids, num_dimensions, INT4OID, 4, true, 'i')), 
         PointerGetDatum(construct_array(dimension_values, num_dimensions, INT8OID, 8, true, 'd')), 
	};

	HeapTuple	tuple;

	ret = SPI_execute_plan(plan, args, NULL, false, 4);

	if (ret <= 0)
	{
		elog(ERROR, "Got an SPI error %d", ret);
	}

	if (SPI_processed != 1)
	{
		elog(ERROR, "Got not 1 row but %lu", SPI_processed);
	}

	if (desc != NULL)
		*desc = SPI_tuptable->tupdesc;

	tuple = SPI_tuptable->vals[0];

	return tuple;
}

Chunk *
spi_chunk_get_or_create(int32 time_dimension_id, int64 time_value,
						int32 space_dimension_id, int64 space_value,
						int16 num_constraints)
{
	HeapTuple	tuple;
	TupleDesc	desc;
	Chunk	   *chunk;
	MemoryContext old, top = CurrentMemoryContext;
	SPIPlanPtr	plan = get_chunk_plan();

	if (SPI_connect() < 0)
		elog(ERROR, "Got an SPI connect error");

	tuple = chunk_tuple_create_spi_connected(time_dimension_id, time_value,
											 space_dimension_id, space_value,
											 &desc, plan);

	old = MemoryContextSwitchTo(top);
	chunk = chunk_create_from_tuple(tuple, num_constraints);
	MemoryContextSwitchTo(old);

	SPI_finish();

	return chunk;
}


Chunk *
spi_chunk_create(int32 time_dimension_id, int64 time_value,
				 int32 space_dimension_id, int64 space_value,
				 int16 num_constraints)
{
	HeapTuple	tuple;
	TupleDesc	desc;
	Chunk	   *chunk;
	MemoryContext old, top = CurrentMemoryContext;
	SPIPlanPtr	plan = create_chunk_plan();

	if (SPI_connect() < 0)
		elog(ERROR, "Got an SPI connect error");

	tuple = chunk_tuple_create_spi_connected(time_dimension_id, time_value,
											 space_dimension_id, space_value,
											 &desc, plan);

	old = MemoryContextSwitchTo(top);
	chunk = chunk_create_from_tuple(tuple, num_constraints);
	MemoryContextSwitchTo(old);

	SPI_finish();

	return chunk;
}
