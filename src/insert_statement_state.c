#include <postgres.h>
#include <utils/lsyscache.h>

#include "insert_statement_state.h"
#include "insert_chunk_state.h"
#include "chunk_cache.h"
#include "chunk.h"
#include "cache.h"
#include "hypertable_cache.h"
#include "partitioning.h"
#include "dimension.h"
#include "hypertable.h"

InsertStatementState *
insert_statement_state_new(Oid relid)
{
	MemoryContext oldctx;
	MemoryContext mctx = AllocSetContextCreate(CacheMemoryContext,
											   "Insert context",
											   ALLOCSET_DEFAULT_SIZES);
	InsertStatementState *state;
	Hypertable *ht;
	Cache *hypertable_cache;

	oldctx = MemoryContextSwitchTo(mctx);

	hypertable_cache = hypertable_cache_pin();
	ht = hypertable_cache_get_entry(hypertable_cache, relid);

	state = palloc(sizeof(InsertStatementState) + sizeof(DimensionSlice *) * ht->space->num_open_dimensions);
	state->mctx = mctx;
	state->chunk_cache = chunk_cache_pin();
	state->hypertable_cache = hypertable_cache;

	/* Find hypertable and the time field column */
	state->num_open_dimensions = ht->space->num_open_dimensions;
	state->num_partitions = 0;

	MemoryContextSwitchTo(oldctx);
	return state;
}

void
insert_statement_state_destroy(InsertStatementState *state)
{
	int			i;

	for (i = 0; i < state->num_partitions; i++)
	{
		if (state->cstates[i] != NULL)
		{
			insert_chunk_state_destroy(state->cstates[i]);
		}
	}

	cache_release(state->chunk_cache);
	cache_release(state->hypertable_cache);

	MemoryContextDelete(state->mctx);
}

static void
set_or_update_new_entry(InsertStatementState *state, Hyperspace *hs, Point *point)
{
	/*
	Chunk	   *chunk;

	if (state->cstates[partition->index] != NULL)
	{
		insert_chunk_state_destroy(state->cstates[partition->index]);
	}

	chunk = chunk_cache_get(state->chunk_cache, partition, timepoint);
	state->cstates[partition->index] = insert_chunk_state_new(chunk);
	*/
}

/*
 * Get an insert context to the chunk corresponding to the partition and
 * timepoint of a tuple.
 */
extern InsertChunkState *
insert_statement_state_get_insert_chunk_state(InsertStatementState *state, Hyperspace *hs, Point *point)
{

#if 0
	/* First call, set up mem */
	if (state->num_partitions == 0)
	{
		state->num_partitions = epoch->num_partitions;
		state->cstates = palloc(sizeof(InsertChunkState) * state->num_partitions);
		memset(state->cstates, 0, sizeof(InsertChunkState) * state->num_partitions);
	}

	/*
	 * Currently we only support one epoch. To support multiple epochs here,
	 * we could either do a realloc to a larger array if we see a larger
	 * num_partitions or keep track of max partitions among epochs and
	 * configure the state using that at initialization.
	 */
	if (state->num_partitions != epoch->num_partitions)
	{
		elog(ERROR, "multiple epochs not supported");
	}

	/*
	 * Check if first insert to partition or if the tuple should go to another
	 * chunk in the partition
	 */
	if (NULL == state->cstates[partition->index] ||
		!chunk_timepoint_is_member(state->cstates[partition->index]->chunk, timepoint))
	{
		set_or_update_new_entry(state, partition, timepoint);
	}

	return state->cstates[partition->index];
#endif
	return NULL;
}
