/*****************************************************************************

        DistoSimple.cpp
        Author: Laurent de Soras, 2016

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if defined (_MSC_VER)
	#pragma warning (1 : 4130 4223 4705 4706)
	#pragma warning (4 : 4355 4786 4800)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "fstb/ToolsSimd.h"
#include "mfx/pi/DistoSimple.h"
#include "mfx/piapi/EventTs.h"

#include <cassert>



namespace mfx
{
namespace pi
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



DistoSimple::DistoSimple ()
:	_state (State_CONSTRUCTED)
,	_param_state_gain ()
,	_param_desc_gain (
		1, 1000,
		"Distortion Gain\nGain",
		"dB",
		0,
		"%+5.1f"
	)
,	_gain (1)
{
	_param_desc_gain.use_disp_num ().set_preset (param::HelperDispNum::Preset_DB);
	_param_state_gain.set_desc (_param_desc_gain);
	_param_state_gain.set_ramp_time (0.02);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



piapi::PluginInterface::State	DistoSimple::do_get_state () const
{
	return _state;
}



int	DistoSimple::do_init ()
{
	_state = State_INITIALISED;

	return Err_OK;
}



int	DistoSimple::do_restore ()
{
	_state = State_CONSTRUCTED;

	return Err_OK;
}



bool	DistoSimple::do_has_sidechain () const
{
	return false;
}



bool	DistoSimple::do_prefer_stereo () const
{
	return false;
}



int	DistoSimple::do_get_nbr_param (piapi::ParamCateg categ) const
{
	return (categ == piapi::ParamCateg_GLOBAL) ? 1 : 0;
}



const piapi::ParamDescInterface &	DistoSimple::do_get_param_info (piapi::ParamCateg categ, int index) const
{
	return _param_desc_gain;
}



double	DistoSimple::do_get_param_val (piapi::ParamCateg categ, int index, int note_id) const
{
	return _param_state_gain.get_val_tgt ();
}



int	DistoSimple::do_reset (double sample_freq, int max_buf_len, int &latency)
{
	latency = 0;

	_state = State_ACTIVE;

	return Err_OK;
}



void	DistoSimple::do_process_block (ProcInfo &proc)
{
	// Ignores timestamps for simplicity
	const int      nbr_evt = proc._nbr_evt;
	for (int index = 0; index < nbr_evt; ++index)
	{
		const piapi::EventTs &  evt = *(proc._evt_arr [index]);
		if (evt._type == piapi::EventType_PARAM)
		{
			const double   val_nrm = evt._evt._param._val;
			_param_state_gain.set_val (val_nrm);

			/*** To do: care about the ramping ***/
			_gain = float (_param_desc_gain.conv_nrm_to_nat (val_nrm));
		}
	}

	const int      nbr_spl = proc._nbr_spl;

	_param_state_gain.tick (nbr_spl);

	const auto     g     = fstb::ToolsSimd::set1_f32 (_gain);
	const auto     mi    = fstb::ToolsSimd::set1_f32 (-1);
	const auto     ma    = fstb::ToolsSimd::set1_f32 ( 1);
	const auto     zero  = fstb::ToolsSimd::set_f32_zero ();
	const auto     c_1_9 = fstb::ToolsSimd::set1_f32 ( 1.0f / 9);
	const auto     c_1_2 = fstb::ToolsSimd::set1_f32 ( 1.0f / 2);
	const auto     bias  = fstb::ToolsSimd::set1_f32 ( 0.2f);

	int            chn_src      = 0;
	int            chn_src_step = 1;
	if (proc._nbr_chn_arr [Dir_IN] == 1 && proc._nbr_chn_arr [Dir_OUT] > 1)
	{
		chn_src_step = 0;
	}

	for (int chn_dst = 0; chn_dst < proc._nbr_chn_arr [Dir_OUT]; ++chn_dst)
	{
		const float *  src_ptr = proc._src_arr [chn_dst];
		float *        dst_ptr = proc._dst_arr [chn_dst];
		for (int pos = 0; pos < nbr_spl; pos += 4)
		{
			auto           x = fstb::ToolsSimd::load_f32 (src_ptr + pos);
			x *= g;

			x -= bias;

			x = fstb::ToolsSimd::min_f32 (x, ma);
			x = fstb::ToolsSimd::max_f32 (x, mi);

			// x -> { x - x^9/9 if x >  0
			//      { x + x^2/2 if x <= 0
			// x * (1 - x^8/9)
			const auto     x2  = x * x;
			const auto     x4  = x2 * x2;
			const auto     x8  = x4 * x4;
			const auto     x9  = x8 * x;
			const auto     x_n = x + x2 * c_1_2;
			const auto     x_p = x - x9 * c_1_9;
			const auto     t_0 = fstb::ToolsSimd::cmp_gt_f32 (x, zero);
			x = fstb::ToolsSimd::select (t_0, x_p, x_n);

			x += bias;

			fstb::ToolsSimd::store_f32 (dst_ptr + pos, x);
		}

		chn_src += chn_src_step;
	}
}



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



}  // namespace pi
}  // namespace mfx



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
