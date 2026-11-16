#include "ervp_mmiox1.h"
#include "ervp_printf.h"
#include "ervp_smart_flush.h"
#include "ervp_delay.h"
#include "ervp_matrix_op_transform.h"

#include "dca_matrix_info.h"
#include "dca_matrix_mac.h"

typedef struct
{
	dca_matrix_info_t ma;
	dca_matrix_info_t mb;
	dca_matrix_info_t mc;
	uint32_t opcode;
} dca_matrix_mac_inst_t;

void dca_matrix_mac_hwinfo_elaborate(dca_matrix_mac_hwpara_t *hwpara, dca_matrix_mac_hwinfo_t *hwinfo)
{
	static int id_to_issue = 0;
	hwinfo->num_col = hwpara->matrix_size_para % 10000;
	hwinfo->num_row = (hwpara->matrix_size_para / 10000) % 10000;
	if (hwinfo->num_row == 0)
		hwinfo->num_row = hwinfo->num_col;
	hwinfo->id = id_to_issue++;
	hwinfo->mmiox_info = NULL;
}

// only mult accumulates on the loaded output in the datapath
static inline int _dca_matrix_mac_opcode_has_hw_acc(unsigned int opcode)
{
	return (opcode & DCA_MATRIX_MAC_OPCODE_MULT_COND) != 0;
}

static void _dca_matrix_mac_request(const dca_matrix_mac_hwinfo_t *const hwinfo, unsigned int opcode, const ErvpMatrixInfo *ma_info, const ErvpMatrixInfo *mb_info, ErvpMatrixInfo *mc_info)
{
	if ((opcode & DCA_MATRIX_MAC_OPCODE_LOAD_ACC) != 0)
		assert(_dca_matrix_mac_opcode_has_hw_acc(opcode));

	dca_matrix_mac_inst_t inst;

	inst.opcode = opcode;
	dca_matrix_info_generate(ma_info, &(inst.ma));
	dca_matrix_info_generate(mb_info, &(inst.mb));
	dca_matrix_info_generate(mc_info, &(inst.mc));

	mmiox1_inst_push(hwinfo->mmiox_info, &inst, 1, 0);
}

ervp_hwtask_busy_fx_t dca_matrix_mac_start_nopost_noflush(const dca_matrix_mac_hwinfo_t *const hwinfo, unsigned int opcode, const ErvpMatrixInfo *ma_info, const ErvpMatrixInfo *mb_info, ErvpMatrixInfo *mc_info, unsigned int option_value)
{
	assert(!ma_info->is_scalar);
	assert(!mb_info->is_scalar);
	assert(!mc_info->is_scalar);
	assert(!mop_option_has_postprocess(mop_option_set(option_value)));

	ervp_mop_option_t mop_option;
	mop_option.value = option_value;

	unsigned int opcode_with_acc = opcode;
	if (mop_option.br.acc)
	{
		opcode_with_acc &= (~DCA_MATRIX_MAC_OPCODE_INIT_ACC);
		opcode_with_acc |= DCA_MATRIX_MAC_OPCODE_LOAD_ACC;
	}
	_dca_matrix_mac_request(hwinfo, opcode_with_acc, ma_info, mb_info, mc_info);
	return dca_matrix_mac_busy_fx(hwinfo);
}

ervp_hwtask_busy_fx_t dca_matrix_mac_start(ervp_mop_mapping_t *mop_mapping, const dca_matrix_mac_hwinfo_t *const hwinfo, unsigned int opcode, const ErvpMatrixInfo *ma_info, const ErvpMatrixInfo *mb_info, ErvpMatrixInfo *mc_info, unsigned int option_value)
{
	ervp_hwtask_busy_fx_t hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
	const ervp_mop_option_t mop_option = mop_option_set(option_value);
	if (mop_option_has_postprocess(mop_option) || (mop_option_is_acc(mop_option) && !_dca_matrix_mac_opcode_has_hw_acc(opcode)))
	{
		ervp_matrix_datatype_t datatype = matrix_datatype_is_float(mc_info->datatype) ? MATRIX_DATATYPE_FLOAT32 : MATRIX_DATATYPE_SINT32;
		ErvpMatrixInfo *temp = matrix_alloc(datatype, mc_info->num_row, mc_info->num_col);
		trackedvar_smart_flush(3, ma_info->addr, mb_info->addr, temp->addr);
		hwtask_busy_fx = dca_matrix_mac_start_nopost_noflush(hwinfo, opcode, ma_info, mb_info, temp, 0);
		hwtask_wait_complete(hwtask_busy_fx);
		hwtask_busy_fx = matrix_perform_postprocess_tf(mop_mapping, temp, mc_info, option_value);
		if (hwtask_busy_fx)
		{
			hwtask_wait_complete(hwtask_busy_fx);
			hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
			matrix_free(temp);
		}
		else
			matrix_free(temp);
	}
	else
	{
		trackedvar_smart_flush(3, ma_info->addr, mb_info->addr, mc_info->addr);
		hwtask_busy_fx = dca_matrix_mac_start_nopost_noflush(hwinfo, opcode, ma_info, mb_info, mc_info, option_value);
	}
	return hwtask_busy_fx;
}
