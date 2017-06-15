#ifndef TIMESCALEDB_HYPERTABLE_H
#define TIMESCALEDB_HYPERTABLE_H

#include <postgres.h>

#include "catalog.h"

typedef struct PartitionEpoch PartitionEpoch;
typedef struct Dimension Dimension;

#define MAX_EPOCHS_PER_HYPERTABLE 20
#define MAX_DIMENSIONS_PER_HYPERTABLE 10

typedef struct Hypertable
{
	FormData_hypertable fd;
	Oid         main_table;
	int16       num_dimensions;	
	int			num_epochs;
	int16 time_dim_index;
	int16 space_dim_index;
	/* Array of PartitionEpoch. Order by start_time */
	PartitionEpoch *epochs[MAX_EPOCHS_PER_HYPERTABLE];
	Dimension *dimensions[MAX_DIMENSIONS_PER_HYPERTABLE];
} Hypertable;

typedef struct HeapTupleData *HeapTuple;

extern Hypertable *hypertable_from_tuple(HeapTuple tuple);
extern Dimension *hypertable_time_dimension(Hypertable *h);
extern Dimension *hypertable_space_dimension(Hypertable *h);

#endif /* TIMESCALEDB_HYPERTABLE_H */
