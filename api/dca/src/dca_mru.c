#include "ervp_mmiox1.h"
#include "ervp_printf.h"
#include "ervp_smart_flush.h"
#include "ervp_delay.h"

#include "dca_matrix_info.h"
#include "dca_mru.h"

typedef struct
{
	dca_matrix_info_t mi;
	dca_matrix_info_t mo;
	uint32_t opcode;
} dca_mru_inst_t;

void dca_mru_hwinfo_elaborate(dca_mru_hwpara_t *hwpara, dca_mru_hwinfo_t *hwinfo)
{
	static int id_to_issue = 0;
	// hwinfo->num_col = hwpara->matrix_size_para % 10000;
	// hwinfo->num_row = (hwpara->matrix_size_para / 10000) % 10000;
	hwinfo->id = id_to_issue++;
}

ervp_hwtask_busy_fx_t dca_mru_copy_part(ervp_mop_mapping_t *mop_mapping, const dca_mru_hwinfo_t *const hwinfo, const ErvpMatrixInfo *mi_info, ErvpMatrixInfo *mo_info, int num_row, int num_col, unsigned int option_value)
{
	ervp_hwtask_busy_fx_t hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
	if (mop_option_has_postprocess(mop_option_set(option_value)))
	{
		matrix_copy_part_sw(mi_info, mo_info, num_row, num_col, option_value);
	}
	else
	{
		// printf_function();
		_matrix_check_copy_part(mi_info, mo_info, num_row, num_col, option_value);

		dca_mru_inst_t inst;
		assert((mo_info->stride_ls3 & 7) == 0);

		inst.opcode = DCA_MRU_COPY;
		dca_matrix_info_generate(mi_info, &(inst.mi));
		dca_matrix_info_generate(mo_info, &(inst.mo));
		inst.mi.br.num_row_m1 = num_row - 1;
		inst.mi.br.num_col_m1 = num_col - 1;
		inst.mo.br.num_row_m1 = num_row - 1;
		inst.mo.br.num_col_m1 = num_col - 1;
		trackedvar_smart_flush(2, mi_info->addr, mo_info->addr);

		// mmiox1_inst_wait_vacant(hwinfo->mmiox_info);
		mmiox1_inst_push(hwinfo->mmiox_info, &inst, 1, 0);
		hwtask_busy_fx = dca_mru_busy_fx(hwinfo);
	}
	return hwtask_busy_fx;
}

ervp_hwtask_busy_fx_t dca_mru_transpose_part(ervp_mop_mapping_t *mop_mapping, const dca_mru_hwinfo_t *const hwinfo, const ErvpMatrixInfo *mi_info, ErvpMatrixInfo *mo_info, int num_row, int num_col, unsigned int option_value)
{
	ervp_hwtask_busy_fx_t hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
	if (mop_option_has_postprocess(mop_option_set(option_value)))
	{
		matrix_transpose_part_sw(mi_info, mo_info, num_row, num_col, option_value);
	}
	else
	{
		// printf_function();
		_matrix_check_transpose_part(mi_info, mo_info, num_row, num_col, option_value);

		dca_mru_inst_t inst;
		assert((mo_info->stride_ls3 & 7) == 0);

		inst.opcode = DCA_MRU_TRANSPOSE;
		dca_matrix_info_generate(mi_info, &(inst.mi));
		dca_matrix_info_generate(mo_info, &(inst.mo));
		inst.mi.br.num_row_m1 = num_row - 1;
		inst.mi.br.num_col_m1 = num_col - 1;
		inst.mo.br.num_row_m1 = num_col - 1;
		inst.mo.br.num_col_m1 = num_row - 1;
		trackedvar_smart_flush(2, mi_info->addr, mo_info->addr);

		// mmiox1_inst_wait_vacant(hwinfo->mmiox_info);
		mmiox1_inst_push(hwinfo->mmiox_info, &inst, 1, 0);
		hwtask_busy_fx = dca_mru_busy_fx(hwinfo);
	}
	return hwtask_busy_fx;
}

ervp_hwtask_busy_fx_t dca_mru_fill_fixed(ervp_mop_mapping_t *mop_mapping, const dca_mru_hwinfo_t *const hwinfo, ErvpMatrixInfo *result, int32_t value)
{
	dca_mru_inst_t inst;
	ErvpMatrixInfo temp;
	ErvpMatrixInfo *result_aligned;
	if ((result->stride_ls3 & 7) == 0)
		result_aligned = result;
	else
	{
		assert(matrix_datatype_is_subbyte(result->datatype));
		assert(matrix_has_contiguous_layout(result));

		int size = matrix_num_bytes(result);
		result_aligned = &temp;
		matrix_init_info(MATRIX_DATATYPE_SINT08, 1, size, result->addr, result_aligned);

		switch (matrix_datatype_get_num_bits(result->datatype))
		{
		case 4:
			value = value * 0x11;
			break;
		case 2:
			value = value * 0x55;
			break;
		case 1:
			if (value != 0)
				value = -1;
			break;
		default:
			assert(0);
		}
	}

	inst.opcode = DCA_MRU_FILL;
	dca_matrix_info_init(&(inst.mi));
	dca_matrix_info_generate(result_aligned, &(inst.mo));

	trackedvar_smart_flush(1, result_aligned->addr);
	// mmiox1_inst_wait_vacant(hwinfo->mmiox_info);
	mmiox1_input_push(hwinfo->mmiox_info, &value, 1);
	mmiox1_inst_push(hwinfo->mmiox_info, &inst, 1, 0);
	return dca_mru_busy_fx(hwinfo);
}