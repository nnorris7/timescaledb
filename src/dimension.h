#ifndef TIMESCALEDB_DIMENSION_H
#define TIMESCALEDB_DIMENSION_H

#include <postgres.h>

#include "catalog.h"

typedef struct PartitioningInfo PartitioningInfo;

typedef struct Dimension
{
	FormData_dimension fd;
	/* num_slices is the number of slices in the cached Dimension for a
	 * particular time interval, which might differ from the num_slices in the
	 * FormData in case partitioning has changed. */
	int16 num_slices;
	PartitioningInfo *partitioning;
} Dimension;

typedef struct DimensionSlice
{
	int32 id;
	int32 dimension_id;
	int64 range_start;
	int64 range_end;
} DimensionSlice;

#endif /* TIMESCALEDB_DIMENSION_H */
