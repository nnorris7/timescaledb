#include <postgres.h>
#include <access/htup_details.h>
#include <utils/lsyscache.h>
#include <catalog/namespace.h>

#include "hypertable.h"
#include "dimension.h"

Hypertable *
hypertable_from_tuple(HeapTuple tuple)
{
	Hypertable *h;
	Oid namespace_oid;
	
	h = palloc0(sizeof(Hypertable));
	memcpy(&h->fd, GETSTRUCT(tuple), sizeof(FormData_hypertable));
	namespace_oid = get_namespace_oid(NameStr(h->fd.schema_name), false);
	h->main_table = get_relname_relid(NameStr(h->fd.table_name), namespace_oid);
	h->time_dim_index = -1;
	h->space_dim_index = -1;
	
	return h;
}

/*
  Find the indices to the hypertable's space and time dimensions. This currently
  assumes one time and one space dimension.
 */
static void
find_dimensions(Hypertable *h)
{
	int i;

	if (h->time_dim_index >= 0 && h->space_dim_index >= 0)
		return;
	
	for (i = 0; i < h->num_dimensions; i++)
	{
		if (OidIsValid(h->dimensions[i]->fd.time_type))
			h->time_dim_index = i;
		else
			h->space_dim_index = i;
	}

	Assert(h->time_dim_index >= 0);
	Assert(h->space_dim_index >= 0);
}

Dimension *
hypertable_time_dimension(Hypertable *h)
{
	if (h->time_dim_index < 0)
		find_dimensions(h);
		
	return h->dimensions[h->time_dim_index];
}

Dimension *
hypertable_space_dimension(Hypertable *h)
{
	if (h->space_dim_index < 0)
		find_dimensions(h);
		
	return h->dimensions[h->space_dim_index];
}
