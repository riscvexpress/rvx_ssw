#include "dca_scalar.h"
#include "ervp_variable_allocation.h"
#include "ervp_malloc_system.h"

#define NUM_SCALAR_MATRIX 16

static UNKNOWN_TYPE scalar_matrix_value[NUM_SCALAR_MATRIX] NOTCACHED_DATA;
static ErvpMatrixInfo scalar_matrix_info[NUM_SCALAR_MATRIX] NOTCACHED_DATA;
static int scalar_matrix_index NOTCACHED_DATA;

static void __attribute__((constructor)) construct_dca_scalar()
{
	for (int i = 0; i < NUM_SCALAR_MATRIX; i++)
		matrix_init_scalar_info(MATRIX_DATATYPE_SINT32, &scalar_matrix_value[i], &scalar_matrix_info[i]);
	scalar_matrix_index = 0;
}

const ErvpMatrixInfo *dca_scalar_malloc(int value)
{
	ErvpMatrixInfo *result = NULL;
	_acquire_lock_for_malloc();
	scalar_matrix_value[scalar_matrix_index].value_signed = value;
	scalar_matrix_info[scalar_matrix_index].is_binary = (value == 0) || (value == 1);
	result = &scalar_matrix_info[scalar_matrix_index++];
	if (scalar_matrix_index == NUM_SCALAR_MATRIX)
		scalar_matrix_index = 0;
	_release_lock_for_malloc();
	return result;
}