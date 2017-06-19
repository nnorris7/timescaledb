#include <postgres.h>

#include "dimension.h"
#include "dimension_slice.h"
#include "subspace_store.h"

typedef struct SubspaceStore {
  int16 num_dimensions;
  DimensionVec *origin; //origin of the tree
} SubspaceStore;

static DimensionVec *
subspace_store_dimension_create() 
{
	/* TODO remove type from vec */
	return dimension_vec_create(10);
}

SubspaceStore *
subspace_store_init(int16 num_dimensions) 
{
	SubspaceStore *sst = palloc(sizeof(SubspaceStore));

	sst->origin = subspace_store_dimension_create();
	sst->num_dimensions = num_dimensions;
	return sst;
}

static void 
subspace_store_free_internal_node(void * node) 
{
	dimension_vec_free((DimensionVec *)node);
}

void subspace_store_add(SubspaceStore *cache, Hypercube *hc,
								   void *end_store, void (*end_store_free)(void *))
{
	DimensionVec *vec = cache->origin;
	DimensionSlice *last = NULL;
	int i;
	
	Assert(hc->num_slices == cache->num_dimensions);

	for (i = 0; i < hc->num_slices; i++)
	{
		DimensionSlice *target = hc->slices[i];
		DimensionSlice *match;

		Assert(target->storage == NULL);

		if (vec == NULL)
		{
			last->storage = subspace_store_dimension_create();
			last->storage_free = subspace_store_free_internal_node;
			vec = last->storage;
		}
		if(vec->num_slices > 0) 
		{
			Assert(vec->slices[0]->fd.dimension_id = target->fd.dimension_id);
		}
		
		match = dimension_vec_find_slice(vec, target->fd.range_start);
		
		if (match == NULL) 
		{
			dimension_vec_add_slice_sort(&vec, target);
			match = target;
		}

		last = match;
		vec = last->storage; /* Internal nodes point to the next Dimension's Vec */ 
	}

	Assert(last->storage == NULL);
	last->storage = end_store; /* at the end we store the object */
	last->storage_free = end_store_free;
}

void *
subspace_store_get(SubspaceStore *cache, Point *target)
{
	int16 i;
	DimensionVec *vec = cache->origin;
	DimensionSlice *match = NULL;
	
	Assert(target->cardinality == cache->num_dimensions);

	for (i = 0; i < target->cardinality; i++)
	{
		match = dimension_vec_find_slice(vec, target->coordinates[i]);

		if (NULL == match) 
			return NULL;

		vec = match->storage;
	}
	return match->storage;
}

static bool
subspace_store_match_first(SubspaceStore *cache, Point *target)
{
	Assert(target->cardinality == cache->num_dimensions);
	return (dimension_vec_find_slice(cache->origin, target->coordinates[0]) != NULL);
}

void
subspace_store_free(SubspaceStore *cache)
{
	dimension_vec_free(cache->origin);
	pfree(cache);
}


