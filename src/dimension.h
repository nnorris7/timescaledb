#ifndef TIMESCALEDB_DIMENSION_H
#define TIMESCALEDB_DIMENSION_H

#include <postgres.h>

#include "catalog.h"

typedef struct PartitioningInfo PartitioningInfo;

typedef enum DimensionType
{
	DIMENSION_TYPE_OPEN,
	DIMENSION_TYPE_CLOSED,
} DimensionType;
	
typedef struct Dimension
{
	FormData_dimension fd;
	DimensionType type;
	/* num_slices is the number of slices in the cached Dimension for a
	 * particular time interval, which might differ from the num_slices in the
	 * FormData in case partitioning has changed. */
	int16 num_slices;
	PartitioningInfo *partitioning;
} Dimension;


#define IS_OPEN_DIMENSION(d) \
	((d)->type == DIMENSION_TYPE_OPEN)

#define IS_CLOSED_DIMENSION(d) \
	((d)->type == DIMENSION_TYPE_CLOSED)

/* We currently support only one open dimension and one closed dimension */
#define MAX_OPEN_DIMENSIONS 1
#define MAX_CLOSED_DIMENSIONS 1
#define MAX_DIMENSIONS (MAX_OPEN_DIMENSIONS + MAX_CLOSED_DIMENSIONS)

/*
 * Hyperspace defines the current partitioning in a N-dimensional space.
 */
typedef struct Hyperspace
{
	int16 num_open_dimensions;
	int16 num_closed_dimensions;
	Dimension *open_dimensions[MAX_OPEN_DIMENSIONS];
	Dimension *closed_dimensions[MAX_CLOSED_DIMENSIONS];
} Hyperspace;

#define HYPERSPACE_NUM_DIMENSIONS(hs) \
	((hs)->num_open_dimensions + (hs)->num_closed_dimensions)

extern Hyperspace *dimension_scan(int32 hypertable_id);

#endif /* TIMESCALEDB_DIMENSION_H */
