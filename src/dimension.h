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

/* We currently support only one time dimension and one space dimension */
#define MAX_TIME_DIMENSIONS 1
#define MAX_SPACE_DIMENSIONS 1

/*
 * Hyperspace defines the current partitioning in a N-dimensional space.
 */
typedef struct Hyperspace
{
	int16 num_time_dimensions;
	int16 num_space_dimensions;
	Dimension *time_dimensions[MAX_TIME_DIMENSIONS];
	Dimension *space_dimensions[MAX_SPACE_DIMENSIONS];
} Hyperspace;

typedef struct DimensionSlice
{
	int32 id;
	int32 dimension_id;
	int64 range_start;
	int64 range_end;
} DimensionSlice;

extern Hyperspace *dimension_scan(int32 hypertable_id);

#endif /* TIMESCALEDB_DIMENSION_H */
