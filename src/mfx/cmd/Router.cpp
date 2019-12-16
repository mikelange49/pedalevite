/*****************************************************************************

        Router.cpp
        Author: Laurent de Soras, 2019

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://www.wtfpl.net/ for more details.

*Tab=3***********************************************************************/



#if defined (_MSC_VER)
	#pragma warning (1 : 4130 4223 4705 4706)
	#pragma warning (4 : 4355 4786 4800)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "mfx/cmd/BufAlloc.h"
#include "mfx/cmd/Document.h"
#include "mfx/cmd/Router.h"
#include "mfx/piapi/ParamDescInterface.h"
#include "mfx/piapi/PluginDescInterface.h"
#include "mfx/Cst.h"
#include "mfx/PluginPool.h"
#include "mfx/ProcessingContext.h"

#include <algorithm>
#include <map>
#include <vector>

#include <cassert>



namespace mfx
{
namespace cmd
{



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



void	Router::set_process_info (double sample_freq, int max_block_size)
{
	assert (sample_freq > 0);
	assert (max_block_size > 0);

	_sample_freq    = sample_freq;
	_max_block_size = max_block_size;
}



void	Router::create_routing (Document &doc, PluginPool &plugin_pool)
{
#if 0
	create_routing_chain (doc, plugin_pool);
#else
	create_routing_graph (doc, plugin_pool);
#endif
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



void	Router::create_routing_chain (Document &doc, PluginPool &plugin_pool)
{
	ProcessingContext &  ctx = *doc._ctx_sptr;

	// Final number of channels
	int            nbr_chn_cur   =
		ChnMode_get_nbr_chn (doc._chn_mode, piapi::Dir_IN );
	const int      nbr_chn_final =
		ChnMode_get_nbr_chn (doc._chn_mode, piapi::Dir_OUT);
	ctx._nbr_chn_out = nbr_chn_final;

	// Buffers
	BufAlloc       buf_alloc (Cst::BufSpecial_NBR_ELT);

	std::array <int, piapi::PluginInterface::_max_nbr_chn>   cur_buf_arr;
	for (auto &b : cur_buf_arr)
	{
		b = -1;
	}

	// Input
	ProcessingContextNode::Side & audio_i =
		ctx._interface_ctx._side_arr [Dir_IN ];
	audio_i._nbr_chn     = Cst::_nbr_chn_in;
	audio_i._nbr_chn_tot = audio_i._nbr_chn;
	for (int i = 0; i < audio_i._nbr_chn; ++i)
	{
		const int      buf = buf_alloc.alloc ();
		audio_i._buf_arr [i] = buf;
	}
	assert (nbr_chn_cur <= audio_i._nbr_chn);
	for (int i = 0; i < nbr_chn_cur; ++i)
	{
		cur_buf_arr [i] = audio_i._buf_arr [i];
	}

	// Plug-ins
	for (const Slot & slot : doc._slot_list)
	{
		const int      pi_id_main = slot._component_arr [PiType_MAIN]._pi_id;
		if (pi_id_main >= 0)
		{
			int            nbr_chn_in      = nbr_chn_cur;
			const piapi::PluginDescInterface &   desc_main =
				*plugin_pool.use_plugin (pi_id_main)._desc_ptr;
			const bool     out_st_flag     = desc_main.prefer_stereo ();
			const bool     final_mono_flag = (nbr_chn_final == 1);
			int            nbr_chn_out     =
				  (out_st_flag && ! (slot._force_mono_flag || final_mono_flag))
				? 2
				: nbr_chn_in;

			const int      latency   = slot._component_arr [PiType_MAIN]._latency;

			const int      pi_id_mix = slot._component_arr [PiType_MIX]._pi_id;

			// Processing context
			ctx._context_arr.resize (ctx._context_arr.size () + 1);
			ProcessingContext::PluginContext &  pi_ctx = ctx._context_arr.back ();
			pi_ctx._mixer_flag = (pi_id_mix >= 0);
			ProcessingContextNode & ctx_node_main = pi_ctx._node_arr [PiType_MAIN];

			// Main plug-in
			ctx_node_main._pi_id = pi_id_main;

			pi_ctx._mix_in_arr.clear ();

			ProcessingContextNode::Side & main_side_i =
				ctx_node_main._side_arr [Dir_IN ];
			ProcessingContextNode::Side & main_side_o =
				ctx_node_main._side_arr [Dir_OUT];
			int            main_nbr_i = 1;
			int            main_nbr_o = 1;
			int            main_nbr_s = 0;

			// Input
			desc_main.get_nbr_io (main_nbr_i, main_nbr_o, main_nbr_s);
			main_side_i._nbr_chn     = (main_nbr_i > 0) ? nbr_chn_in : 0;
			main_side_i._nbr_chn_tot = nbr_chn_in * main_nbr_i;
			for (int chn = 0; chn < main_side_i._nbr_chn_tot; ++chn)
			{
				if (chn < nbr_chn_in)
				{
					main_side_i._buf_arr [chn] = cur_buf_arr [chn];
				}
				else
				{
					main_side_i._buf_arr [chn] = Cst::BufSpecial_SILENCE;
				}
			}

			// Output
			std::array <int, piapi::PluginInterface::_max_nbr_chn>   nxt_buf_arr;
			main_side_o._nbr_chn     = (main_nbr_o > 0) ? nbr_chn_out : 0;
			main_side_o._nbr_chn_tot = nbr_chn_out * main_nbr_o;
			for (int chn = 0; chn < main_side_o._nbr_chn_tot; ++chn)
			{
				if (chn < nbr_chn_out && slot._gen_audio_flag)
				{
					const int      buf = buf_alloc.alloc ();
					nxt_buf_arr [chn]          = buf;
					main_side_o._buf_arr [chn] = buf;
				}
				else
				{
					main_side_o._buf_arr [chn] = Cst::BufSpecial_TRASH;
				}
			}

			// Signals
			ctx_node_main._nbr_sig = main_nbr_s;
			const int      nbr_reg_sig =
				int (slot._component_arr [PiType_MAIN]._sig_port_list.size ());
			for (int sig = 0; sig < main_nbr_s; ++sig)
			{
				ProcessingContextNode::SigInfo & sig_info =
					ctx_node_main._sig_buf_arr [sig];
				sig_info._buf_index  = Cst::BufSpecial_TRASH;
				sig_info._port_index = -1;

				if (sig < nbr_reg_sig)
				{
					const int      port_index =
						slot._component_arr [PiType_MAIN]._sig_port_list [sig];
					if (port_index >= 0)
					{
						sig_info._buf_index  = buf_alloc.alloc ();
						sig_info._port_index = port_index;
					}
				}
			}

			// With dry/wet mixer
			if (pi_id_mix >= 0)
			{
				assert (slot._gen_audio_flag);

				ProcessingContextNode & ctx_node_mix = pi_ctx._node_arr [PiType_MIX];

				ctx_node_mix._aux_param_flag = true;
				ctx_node_mix._comp_delay     = latency;
				ctx_node_mix._pin_mult       = 1;

				ctx_node_mix._pi_id = pi_id_mix;
				ProcessingContextNode::Side & mix_side_i =
					ctx_node_mix._side_arr [Dir_IN ];
				ProcessingContextNode::Side & mix_side_o =
					ctx_node_mix._side_arr [Dir_OUT];

				ctx_node_mix._nbr_sig = 0;

				// Bypass output for the main plug-in
				for (int chn = 0; chn < nbr_chn_out * main_nbr_o; ++chn)
				{
					pi_ctx._bypass_buf_arr [chn] = buf_alloc.alloc ();
				}

				// Dry/wet input
				mix_side_i._nbr_chn     = nbr_chn_out;
				mix_side_i._nbr_chn_tot = nbr_chn_out * 2;
				for (int chn = 0; chn < nbr_chn_out; ++chn)
				{
					// 1st pin: main output
					mix_side_i._buf_arr [              chn] = nxt_buf_arr [chn];

					// 2nd pin: main input as default bypass
					const int       chn_in = std::min (chn, nbr_chn_in - 1);
					const int       buf    = cur_buf_arr [chn_in];
					mix_side_i._buf_arr [nbr_chn_out + chn] = buf;
				}

				// Dry/wet output
				std::array <int, piapi::PluginInterface::_max_nbr_chn>   mix_buf_arr;
				mix_side_o._nbr_chn     = nbr_chn_out;
				mix_side_o._nbr_chn_tot = nbr_chn_out;
				for (int chn = 0; chn < nbr_chn_out; ++chn)
				{
					const int      buf = buf_alloc.alloc ();
					mix_buf_arr [chn]         = buf;
					mix_side_o._buf_arr [chn] = buf;
				}

				// Shift buffers
				for (int chn = 0; chn < nbr_chn_out * main_nbr_o; ++chn)
				{
					buf_alloc.ret (pi_ctx._bypass_buf_arr [chn]);
				}
				for (int chn = 0; chn < nbr_chn_out; ++chn)
				{
					buf_alloc.ret (nxt_buf_arr [chn]);
					nxt_buf_arr [chn] = mix_buf_arr [chn];
				}
			}

			// Output buffers become the next input buffers
			if (slot._gen_audio_flag)
			{
				for (int chn = 0; chn < nbr_chn_out; ++chn)
				{
					if (chn < nbr_chn_in)
					{
						buf_alloc.ret (cur_buf_arr [chn]);
					}
					cur_buf_arr [chn] = nxt_buf_arr [chn];
				}
				nbr_chn_cur = nbr_chn_out;
			}
		}
	}

	// Output
	ProcessingContextNode::Side & audio_o =
		ctx._interface_ctx._side_arr [Dir_OUT];
	audio_o._nbr_chn     = Cst::_nbr_chn_out;
	audio_o._nbr_chn_tot = audio_o._nbr_chn;
	for (int i = 0; i < audio_o._nbr_chn; ++i)
	{
		const int      chn_src = std::min (i, nbr_chn_cur - 1);
		audio_o._buf_arr [i] = cur_buf_arr [chn_src];
	}

	for (int chn = 0; chn < nbr_chn_cur; ++chn)
	{
		buf_alloc.ret (cur_buf_arr [chn]);
	}
}



void	Router::create_routing_graph (Document &doc, PluginPool &plugin_pool)
{
	/*** To do: temporary ***/
	make_graph_from_chain (doc);
	/*** To do: temporary ***/

	doc._ctx_sptr->_nbr_chn_out =
		ChnMode_get_nbr_chn (doc._chn_mode, piapi::Dir_OUT);

	// Finds the number of audio inputs and outputs
	_nbr_a_src = 1;
	_nbr_a_dst = 1;
	switch (doc._chn_mode)
	{
	case ChnMode_1M_1M:
		// Nothing
		break;
	case ChnMode_1M_1S:
		// Nothing
		break;
	case ChnMode_1S_1S:
		// Nothing
		break;
	default:
		assert (false);
		break;
	}

	add_aux_plugins (doc, plugin_pool);
	create_graph_context (doc, plugin_pool);
}



// Completes the Document structure to turn the implicit slot chain into
// a true graph.
void	Router::make_graph_from_chain (Document &doc)
{
	doc._cnx_list.clear ();

	const int         nbr_slots = int (doc._slot_list.size ());
	int               slot_src  = 0;
	CnxEnd::SlotType  slot_type = CnxEnd::SlotType_IO;
	for (int slot_pos = 0; slot_pos < nbr_slots; ++slot_pos)
	{
		const Slot &   slot = doc._slot_list [slot_pos];
		const int      pi_id_main = slot._component_arr [PiType_MAIN]._pi_id;
		if (pi_id_main >= 0)
		{
			// Connects the plug-in to the source
			Cnx            cnx;
			cnx._src._slot_type = slot_type;
			cnx._src._slot_pos  = slot_src;
			cnx._src._pin       = 0;
			cnx._dst._slot_type = CnxEnd::SlotType_NORMAL;
			cnx._dst._slot_pos  = slot_pos;
			cnx._dst._pin       = 0;
			doc._cnx_list.push_back (cnx);

			// The plug-in becomes the new source only if it generates audio
			if (slot._gen_audio_flag)
			{
				slot_type = CnxEnd::SlotType_NORMAL;
				slot_src  = slot_pos;
			}
		}
	}

	// Last connection to the audio output
	Cnx            cnx;
	cnx._src._slot_type = slot_type;
	cnx._src._slot_pos  = slot_src;
	cnx._src._pin       = 0;
	cnx._dst._slot_type = CnxEnd::SlotType_IO;
	cnx._dst._slot_pos  = 0;
	cnx._dst._pin       = 0;
	doc._cnx_list.push_back (cnx);
}



void	Router::add_aux_plugins (Document &doc, PluginPool &plugin_pool)
{
	// Computes the plug-in latencies across the graph to evaluate where
	// to insert compensation delay plug-ins
	prepare_graph_for_latency_analysis (doc);
	_lat_algo.run ();

	// Finds multiple connections to single input pins
	count_nbr_cnx_per_input_pin (_cnx_per_pin_in, doc._cnx_list);

	// Inserts required plug-ins: delays for latency compensation
	add_aux_plugins_delays (doc, plugin_pool);

	// Connects the plug-ins to the graph
	_cnx_list = doc._cnx_list;
	connect_delays (doc);
}



void	Router::prepare_graph_for_latency_analysis (const Document &doc)
{
	_lat_algo.reset ();
	const int      nbr_cnx   = int (doc._cnx_list.size ());
	_nbr_slots = int (doc._slot_list.size ());
	const int      nbr_nodes = _nbr_slots + _nbr_a_src + _nbr_a_dst;
	_lat_algo.set_nbr_elt (nbr_nodes, nbr_cnx);

	// Builds the connection list
	for (int cnx_pos = 0; cnx_pos < nbr_cnx; ++cnx_pos)
	{
		const Cnx &    cnx_doc = doc._cnx_list [cnx_pos];
		lat::Cnx &     cnx_lat = _lat_algo.use_cnx (cnx_pos);

		int            node_idx_src =
			conv_doc_slot_to_lat_node_index (piapi::Dir_IN , cnx_doc);
		cnx_lat.set_node (piapi::Dir_OUT, node_idx_src);

		int            node_idx_dst =
			conv_doc_slot_to_lat_node_index (piapi::Dir_OUT, cnx_doc);
		cnx_lat.set_node (piapi::Dir_IN , node_idx_dst);

		// Reports the connection on the nodes
		lat::Node &    node_src = _lat_algo.use_node (node_idx_src);
		node_src.add_cnx (piapi::Dir_OUT, cnx_pos);

		lat::Node &    node_dst = _lat_algo.use_node (node_idx_dst);
		node_dst.add_cnx (piapi::Dir_IN , cnx_pos);
	}

	// Builds the node list
	for (int slot_pos = 0; slot_pos < _nbr_slots; ++slot_pos)
	{
		lat::Node &    node = _lat_algo.use_node (slot_pos);
		node.set_nature (lat::Node::Nature_NORMAL);

		const Slot &   slot    = doc._slot_list [slot_pos];
		const int      latency = slot._component_arr [PiType_MAIN]._latency;
		node.set_latency (latency);
	}

	// Source nodes (audio inputs)
	for (int pin_cnt = 0; pin_cnt < _nbr_a_src; ++pin_cnt)
	{
		const int      node_idx =
			conv_io_pin_to_lat_node_index (piapi::Dir_IN , pin_cnt);
		lat::Node &    node = _lat_algo.use_node (node_idx);
		node.set_nature (lat::Node::Nature_SOURCE);
		node.set_latency (0);
		assert (node.is_pure (piapi::Dir_OUT));
	}

	// Destination nodes (audio outputs)
	for (int pin_cnt = 0; pin_cnt < _nbr_a_dst; ++pin_cnt)
	{
		const int      node_idx =
			conv_io_pin_to_lat_node_index (piapi::Dir_OUT, pin_cnt);
		lat::Node &    node = _lat_algo.use_node (node_idx);
		node.set_nature (lat::Node::Nature_SINK);
		node.set_latency (0);
		assert (node.is_pure (piapi::Dir_IN ));
	}
}



/*
Maps the document slot indexes to lat::Algo node indexes.
In this order:
- Standard plug-in slots
- Audio inputs
- Audio outputs
*/

int	Router::conv_doc_slot_to_lat_node_index (piapi::Dir dir, const Cnx &cnx) const
{
	const CnxEnd & end = (dir != piapi::Dir_IN) ? cnx._dst : cnx._src;
	int            node_index = end._slot_pos;

	switch (end._slot_type)
	{
	case CnxEnd::SlotType_NORMAL:
		// Nothing
		break;
	case CnxEnd::SlotType_IO:
		node_index += _nbr_slots;
		if (dir != piapi::Dir_IN)
		{
			node_index += _nbr_a_src;
		}
		break;
	case CnxEnd::SlotType_DLY:
		// These kinds of slot shouldn't appear here.
		assert (false);
		break;
	default:
		assert (false);
		break;
	}

	return node_index;
}



int	Router::conv_io_pin_to_lat_node_index (piapi::Dir dir, int pin) const
{
	int            node_index = _nbr_slots + pin;
	if (dir != piapi::Dir_IN)
	{
		node_index += _nbr_a_src;
	}

	return node_index;
}



void	Router::add_aux_plugins_delays (Document &doc, PluginPool &plugin_pool)
{
	// Lists the connections requiring a compensation delay.
	std::map <Cnx, CnxInfo>  cnx_map; // [cnx] = info
	const int            nbr_cnx = _lat_algo.get_nbr_cnx ();
	for (int cnx_idx = 0; cnx_idx < nbr_cnx; ++cnx_idx)
	{
		const lat::Cnx &  cnx_lat  = _lat_algo.use_cnx (cnx_idx);
		const int         delay    = cnx_lat.get_comp_delay ();
		if (delay > 0)
		{
			const Cnx &    cnx_doc  = doc._cnx_list [cnx_idx];
			CnxInfo &      cnx_info = cnx_map [cnx_doc];
			cnx_info._delay   = delay;
			cnx_info._cnx_idx = cnx_idx;
		}
	}

	// Checks the existing doc._plugin_dly_list to find delays and their
	// connections. Updates the connection list by erasing fulfilled
	// associations.
	const int      nbr_dly = int (doc._plugin_dly_list.size ());
	for (int dly_idx = 0; dly_idx < nbr_dly; ++dly_idx)
	{
		PluginAux &    plug_dly = doc._plugin_dly_list [dly_idx];
		const auto     it       = cnx_map.find (plug_dly._cnx);
		const CnxInfo& cnx_info = it->second;
		if (it != cnx_map.end ())
		{
			plug_dly._cnx_index  = cnx_info._cnx_idx;

			// Updates the delay
			plug_dly._comp_delay = cnx_info._delay;

			// This connection works, we don't need it anymore.
			cnx_map.erase (it);
		}
		else
		{
			// Not found, this plug-in may be removed or replaced.
			doc._map_model_id [plug_dly._model] [plug_dly._pi_id] = false;
			plug_dly._cnx_index = -1;
			plug_dly._cnx.invalidate ();
			plug_dly._param_list.clear ();
		}
	}

	// Completes the plug-in by reusing the slots scheduled for removal.
	int            scan_pos = 0;
	auto           it       = cnx_map.begin ();
	while (scan_pos < nbr_dly && it != cnx_map.end ())
	{
		PluginAux &    plug_dly = doc._plugin_dly_list [scan_pos];
		if (! plug_dly._cnx.is_valid ())
		{
			assert (plug_dly._pi_id >= 0);
			const CnxInfo& cnx_info = it->second;

			// Reuses this plug-in
			doc._map_model_id [plug_dly._model] [plug_dly._pi_id] = true;
			plug_dly._cnx        = it->first;

			plug_dly._cnx_index  = cnx_info._cnx_idx;

			// Sets the delay
			plug_dly._comp_delay = cnx_info._delay;

			// Next connection
			it = cnx_map.erase (it);
		}
		else
		{
			++ scan_pos;
		}
	}

	// Schedules the unused plug-ins for removal
	while (scan_pos < nbr_dly)
	{
		PluginAux &    plug_dly = doc._plugin_dly_list [scan_pos];
		if (! plug_dly._cnx.is_valid ())
		{
			doc._map_model_id [plug_dly._model] [plug_dly._pi_id] = false;
			plug_dly._cnx_index = -1;
			plug_dly._cnx.invalidate ();
			plug_dly._param_list.clear ();
		}
	}

	// Then add more plug-ins if required
	// From here, nbr_dly is not valid any more.
	while (it != cnx_map.end ())
	{
		const CnxInfo& cnx_info = it->second;

		// Creates a new delay plug-in
		PluginAux &    plug_dly = create_plugin_aux (
			doc, plugin_pool, doc._plugin_dly_list, Cst::_plugin_dly
		);
		plug_dly._cnx        = it->first;
		plug_dly._cnx_index  = cnx_info._cnx_idx;

		// Sets the delay
		plug_dly._comp_delay = cnx_info._delay;

		++ it;
	}
}



// The plug-in is inserted at the back of the list
// It is registred in the Document object as "in use".
PluginAux &	Router::create_plugin_aux (Document &doc, PluginPool &plugin_pool, Document::PluginAuxList &aux_list, std::string model)
{
	aux_list.resize (aux_list.size () + 1);
	PluginAux &    plug = aux_list.back ();

	plug._model = model;
	plug._pi_id = plugin_pool.create (plug._model);
	assert (plug._pi_id >= 0);

	if (_sample_freq > 0)
	{
		PluginPool::PluginDetails &   details =
			plugin_pool.use_plugin (plug._pi_id);
		int         latency = 0;
		details._pi_uptr->reset (_sample_freq, _max_block_size, latency);
	}

	doc._map_model_id [plug._model] [plug._pi_id] = true;

	return plug;
}



void	Router::connect_delays (Document &doc)
{
	const int      nbr_plug = int (doc._plugin_dly_list.size ());
	for (int plug_idx = 0; plug_idx < nbr_plug; ++plug_idx)
	{
		const auto &   plug_dly = doc._plugin_dly_list [plug_idx];
		if (plug_dly._cnx.is_valid ())
		{
			_cnx_list.resize (_cnx_list.size () + 1);
			Cnx            cnx_ins = _cnx_list.back ();
			Cnx &          cnx_old = _cnx_list [plug_dly._cnx_index];

			cnx_ins._src = cnx_old._src;
			cnx_ins._dst._slot_type = CnxEnd::SlotType_DLY;
			cnx_ins._dst._slot_pos  = plug_idx;
			cnx_ins._dst._pin       = 0;

			cnx_old._src._slot_type = CnxEnd::SlotType_DLY;
			cnx_old._src._slot_pos  = plug_idx;
			cnx_old._src._pin       = 0;
		}
	}
}



void	Router::create_graph_context (Document &doc, PluginPool &plugin_pool)
{
	NodeCategList  categ_list;
	init_node_categ_list (doc, categ_list);

	BufAlloc       buf_alloc (Cst::BufSpecial_NBR_ELT);
	allocate_buf_audio_i (doc, buf_alloc);

	for (auto &cnx_pin : categ_list [CnxEnd::SlotType_IO] [0]._cnx_src_list)
	{
		for (auto &cnx : cnx_pin)
		{
			if (cnx._src._slot_type != CnxEnd::SlotType_IO)
			{
				visit_node (doc, plugin_pool, buf_alloc, categ_list, cnx);
			}
		}
	}

	allocate_buf_audio_o (doc, buf_alloc, categ_list);

	// Deallocates audio input and output buffers (for sanity checks)
	free_buf_audio_i (doc, buf_alloc);
	free_buf_audio_o (doc, buf_alloc);
	assert (buf_alloc.get_nbr_alloc_buf () == 0);
}



void	Router::init_node_categ_list (const Document &doc, NodeCategList &categ_list) const
{
	for (auto &categ : categ_list)
	{
		categ.clear ();
	}

	categ_list [CnxEnd::SlotType_NORMAL].resize (doc._slot_list.size ());
	categ_list [CnxEnd::SlotType_DLY   ].resize (doc._plugin_dly_list.size ());
	categ_list [CnxEnd::SlotType_IO    ].resize (1); // For the audio output node

	// For each category, counts the number of input pins fed by each output pin
	for (auto &cnx : _cnx_list)
	{
		// Does not count the audio input node
		if (cnx._src._slot_type != CnxEnd::SlotType_IO)
		{
			NodeCateg &    categ = categ_list [cnx._src._slot_type];
			assert (cnx._src._slot_pos < int (categ.size ()));
			NodeInfo &     node  = categ [cnx._src._slot_pos];
			if (int (node._pin_dst_list.size ()) <= cnx._src._pin)
			{
				node._pin_dst_list.resize (cnx._src._pin + 1);
			}
			PinOutInfo &   pin   = node._pin_dst_list [cnx._src._pin];

			++ pin._dst_count;
		}

		// Registers the connection on the destination node
		{
			NodeCateg &    categ = categ_list [cnx._dst._slot_type];
			assert (cnx._dst._slot_pos < int (categ.size ()));
			NodeInfo &     node  = categ [cnx._dst._slot_pos];
			if (int (node._cnx_src_list.size ()) <= cnx._dst._pin)
			{
				node._cnx_src_list.resize (cnx._dst._pin + 1);
			}
			Document::CnxList &  pin = node._cnx_src_list [cnx._dst._pin];
			pin.push_back (cnx);
		}
	}
}



void	Router::allocate_buf_audio_i (Document &doc, BufAlloc &buf_alloc)
{
	ProcessingContext &  ctx = *doc._ctx_sptr;
	ProcessingContextNode::Side & audio_i =
		ctx._interface_ctx._side_arr [Dir_IN ];
	const int      nbr_pins = _nbr_a_src;
	audio_i._nbr_chn     = ChnMode_get_nbr_chn (doc._chn_mode, piapi::Dir_IN);
	audio_i._nbr_chn_tot = nbr_pins * audio_i._nbr_chn;
	for (int chn_cnt = 0; chn_cnt < audio_i._nbr_chn_tot; ++chn_cnt)
	{
		const int      buf = buf_alloc.alloc ();
		audio_i._buf_arr [chn_cnt] = buf;
	}
}



void	Router::allocate_buf_audio_o (Document &doc, BufAlloc &buf_alloc, const NodeCategList &categ_list)
{
	const NodeInfo &  node_info = categ_list [CnxEnd::SlotType_IO] [0];

	ProcessingContext &  ctx = *doc._ctx_sptr;
	ProcessingContextNode::Side & audio_o =
		ctx._interface_ctx._side_arr [Dir_OUT];
	const int      nbr_pins = _nbr_a_dst;
	audio_o._nbr_chn     = ctx._nbr_chn_out;
	audio_o._nbr_chn_tot = nbr_pins * audio_o._nbr_chn;
	assert (audio_o._nbr_chn_tot <= Cst::_nbr_chn_out);
	assert (node_info._cnx_src_list.size () == nbr_pins);

	const bool     mix_flag = collects_mix_source_buffers (
		ctx, buf_alloc, categ_list, node_info,
		audio_o, nbr_pins, audio_o._nbr_chn,
		ctx._interface_mix
	);

	// Deallocates mix buffers
	if (mix_flag)
	{
		for (auto &chn : ctx._interface_mix)
		{
			for (auto buf : chn)
			{
				buf_alloc.ret_if_std (buf);
			}
		}
	}
}



void	Router::free_buf_audio_i (Document &doc, BufAlloc &buf_alloc)
{
	ProcessingContext &  ctx = *doc._ctx_sptr;
	ProcessingContextNode::Side & audio_i =
		ctx._interface_ctx._side_arr [Dir_IN ];

	for (int chn_cnt = 0; chn_cnt < audio_i._nbr_chn_tot; ++chn_cnt)
	{
		buf_alloc.ret (audio_i._buf_arr [chn_cnt]);
	}
}



void	Router::free_buf_audio_o (Document &doc, BufAlloc &buf_alloc)
{
	ProcessingContext &  ctx = *doc._ctx_sptr;
	ProcessingContextNode::Side & audio_o =
		ctx._interface_ctx._side_arr [Dir_OUT];

	for (int chn_cnt = 0; chn_cnt < audio_o._nbr_chn_tot; ++chn_cnt)
	{
		buf_alloc.ret (audio_o._buf_arr [chn_cnt]);
	}
}



// Visited node is the source of the connection
void	Router::visit_node (Document &doc, const PluginPool &plugin_pool, BufAlloc &buf_alloc, NodeCategList &categ_list, const Cnx &cnx)
{
	assert (   cnx._src._slot_type == CnxEnd::SlotType_NORMAL
	        || cnx._src._slot_type == CnxEnd::SlotType_DLY);

	NodeInfo &     node_info =
		categ_list [cnx._src._slot_type] [cnx._src._slot_pos];

	// Node not visited yet ?
	if (! node_info._visit_flag)
	{
		// This will update the channel count, for the input
		check_source_nodes (doc, plugin_pool, buf_alloc, categ_list, node_info);

		ProcessingContext &  ctx = *doc._ctx_sptr;

		// Pins that are actually connected in the graph.
		// May be different (< or >) of the plug-in's real number of i/o pins.
		const int      nbr_pins_o = int (node_info._pin_dst_list.size ());
		const int      nbr_pins_i = int (node_info._cnx_src_list.size ());

		// -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		// Collects some information depending on the plug-in type

		int            pi_id_main = -1;
		int            pi_id_mix  = -1;
		int            latency    =  0;
		int            nbr_chn_i  = node_info._nbr_chn;
		int            nbr_chn_o  = nbr_chn_i;
		int            main_nbr_i = 1;
		int            main_nbr_o = 1;
		int            main_nbr_s = 0;
		bool           gen_audio_flag = false;
		if (cnx._src._slot_type == CnxEnd::SlotType_DLY)
		{
			// Delay plug-in
			assert (nbr_pins_i == nbr_pins_o);
			PluginAux &    dly = doc._plugin_dly_list [cnx._src._slot_pos];
			pi_id_main = dly._pi_id;
			latency    = dly._comp_delay;
			dly._comp_delay = 0;
			gen_audio_flag  = true;
		}
		else
		{
			// Standard plug-in with its mixer
			assert (cnx._src._slot_type == CnxEnd::SlotType_NORMAL);
			const Slot &   slot = doc._slot_list [cnx._src._slot_pos];
			pi_id_main = slot._component_arr [PiType_MAIN]._pi_id;
			if (pi_id_main >= 0)
			{
				gen_audio_flag = slot._gen_audio_flag;

				const piapi::PluginDescInterface &   desc_main =
					*plugin_pool.use_plugin (pi_id_main)._desc_ptr;
				const bool     out_st_flag = desc_main.prefer_stereo ();
				desc_main.get_nbr_io (main_nbr_i, main_nbr_o, main_nbr_s);

				pi_id_mix  = slot._component_arr [PiType_MIX ]._pi_id;
				latency    = slot._component_arr [PiType_MAIN]._latency;
				nbr_chn_o  = nbr_chn_i;
				if (out_st_flag && ! slot._force_mono_flag)
				{
					nbr_chn_o = ChnMode_get_nbr_chn (doc._chn_mode, piapi::Dir_OUT);
				}
			}
		}
		node_info._nbr_chn = nbr_chn_o;

		const int   nbr_pins_ctx_i = std::max (nbr_pins_i, main_nbr_i);
		const int   nbr_pins_ctx_o = std::max (nbr_pins_o, main_nbr_o);

		// -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		// Creates a processing context node

		node_info._ctx_index = int (ctx._context_arr.size ());
		ctx._context_arr.resize (node_info._ctx_index + 1);
		ProcessingContext::PluginContext &  pi_ctx = ctx._context_arr.back ();
		pi_ctx._mixer_flag = (pi_id_mix >= 0);

		ProcessingContextNode & ctx_node_main = pi_ctx._node_arr [PiType_MAIN];
		ctx_node_main._pi_id = pi_id_main;
		pi_ctx._mix_in_arr.clear ();

		ProcessingContextNode::Side & main_side_i =
			ctx_node_main._side_arr [Dir_IN ];
		ProcessingContextNode::Side & main_side_o =
			ctx_node_main._side_arr [Dir_OUT];

		// -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		// Allocates buffers

		// Inputs
		main_side_i._nbr_chn     = (main_nbr_i > 0) ? nbr_chn_i : 0;
		main_side_i._nbr_chn_tot = nbr_chn_i * nbr_pins_ctx_i;
		pi_ctx._mix_in_arr.resize (main_side_i._nbr_chn_tot);

		const bool     mix_flag = collects_mix_source_buffers (
			ctx, buf_alloc, categ_list, node_info,
			main_side_i, nbr_pins_ctx_i, nbr_chn_i,
			pi_ctx._mix_in_arr
		);

		// Outputs
		main_side_o._nbr_chn     = (main_nbr_o > 0) ? nbr_chn_o : 0;
		main_side_o._nbr_chn_tot = nbr_chn_o * nbr_pins_ctx_o;
		for (int pin_cnt = 0; pin_cnt < nbr_pins_ctx_o; ++pin_cnt)
		{
			const int      chn_ofs = pin_cnt * nbr_chn_o;
			int            nbr_dst = 0;
			if (pi_ctx._mixer_flag)
			{
				nbr_dst = 1;
			}
			else if (pin_cnt < nbr_pins_o)
			{
				nbr_dst = node_info._pin_dst_list [pin_cnt]._dst_count;
			}

			for (int chn_cnt = 0; chn_cnt < nbr_chn_o; ++chn_cnt)
			{
				int            buf = Cst::BufSpecial_TRASH;
				if (nbr_dst > 0)
				{
					if (pi_id_main < 0)
					{
						// No plug-in: acts like a bypass on the first input pin
						// by reporting the buffers (no copy needed)
						if (main_side_i._nbr_chn == 0)
						{
							buf = Cst::BufSpecial_SILENCE;
						}
						else
						{
							const int      chn_idx =
								std::min (chn_cnt, main_side_i._nbr_chn - 1);
							buf = main_side_i._buf_arr [chn_idx];
						}
						buf_alloc.use_more_if_std (nbr_dst);
					}
					else if (pin_cnt < main_nbr_o)
					{
						buf = buf_alloc.alloc (nbr_dst);
					}
				}
				main_side_o._buf_arr [chn_ofs + chn_cnt] = buf;
			}
		} // for pin_cnt

		// Signals
		ctx_node_main._nbr_sig = main_nbr_s;
		if (   cnx._src._slot_type == CnxEnd::SlotType_NORMAL
		    && pi_id_main >= 0)
		{
			const Slot &   slot = doc._slot_list [cnx._src._slot_pos];
			const int      nbr_reg_sig =
				int (slot._component_arr [PiType_MAIN]._sig_port_list.size ());
			for (int sig = 0; sig < main_nbr_s; ++sig)
			{
				ProcessingContextNode::SigInfo & sig_info =
					ctx_node_main._sig_buf_arr [sig];
				sig_info._buf_index  = Cst::BufSpecial_TRASH;
				sig_info._port_index = -1;

				if (sig < nbr_reg_sig)
				{
					const int      port_index =
						slot._component_arr [PiType_MAIN]._sig_port_list [sig];
					if (port_index >= 0)
					{
						sig_info._buf_index  = buf_alloc.alloc ();
						sig_info._port_index = port_index;
					}
				}
			}
		}

		// -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		// Dry/wet mixer

		if (pi_id_mix >= 0)
		{
			assert (gen_audio_flag);
			ProcessingContextNode & ctx_node_mix = pi_ctx._node_arr [PiType_MIX];

			ctx_node_mix._aux_param_flag = true;
			ctx_node_mix._comp_delay     = latency;
			ctx_node_mix._pin_mult       = nbr_pins_o;

			ctx_node_mix._pi_id = pi_id_mix;
			ProcessingContextNode::Side & mix_side_i =
				ctx_node_mix._side_arr [Dir_IN ];
			ProcessingContextNode::Side & mix_side_o =
				ctx_node_mix._side_arr [Dir_OUT];

			ctx_node_mix._nbr_sig = 0;

			// Bypass output for the main plug-in
			for (int chn = 0; chn < main_side_o._nbr_chn_tot; ++chn)
			{
				pi_ctx._bypass_buf_arr [chn] = buf_alloc.alloc ();
			}

			// Dry/wet input
			mix_side_i._nbr_chn     =                  nbr_chn_o;
			mix_side_i._nbr_chn_tot = nbr_pins_ctx_o * 2 * nbr_chn_o;
			for (int pin_cnt = 0; pin_cnt < nbr_pins_ctx_o; ++pin_cnt)
			{
				const int   ofs_chn_i = pin_cnt * nbr_chn_i;
				const int   ofs_chn_o = pin_cnt * nbr_chn_o;
				const int   ofs_chn_w = pin_cnt * 2 * nbr_chn_o;
				const int   ofs_chn_b = ofs_chn_w + nbr_chn_o;

				for (int chn_cnt = 0; chn_cnt < nbr_chn_o; ++chn_cnt)
				{
					// 1st pin of the pair: wet, main output
					mix_side_i._buf_arr [ofs_chn_w + chn_cnt] =
						main_side_o._buf_arr [ofs_chn_o + chn_cnt];

					// 2nd pin of the pair: dry, main input as default bypass
					int               buf = Cst::BufSpecial_SILENCE;
					if (pin_cnt < nbr_pins_ctx_i)
					{
						const int    chn_idx = std::min (chn_cnt, nbr_chn_i - 1);
						buf = main_side_i._buf_arr [ofs_chn_i + chn_idx];
					}
					mix_side_i._buf_arr [ofs_chn_b + chn_cnt] = buf;
				}
			}

			// Dry/wet output
			mix_side_o._nbr_chn     =                  nbr_chn_o;
			mix_side_o._nbr_chn_tot = nbr_pins_ctx_o * nbr_chn_o;
			for (int pin_cnt = 0; pin_cnt < nbr_pins_ctx_o; ++pin_cnt)
			{
				const int      ofs_chn = pin_cnt * nbr_chn_o;
				int            nbr_dst = 0;
				if (pin_cnt < nbr_pins_o)
				{
					nbr_dst = node_info._pin_dst_list [pin_cnt]._dst_count;
				}

				for (int chn_cnt = 0; chn_cnt < nbr_chn_o; ++chn_cnt)
				{
					int            buf = Cst::BufSpecial_TRASH;
					if (nbr_dst > 0)
					{
						buf = buf_alloc.alloc (nbr_dst);
					}
					mix_side_o._buf_arr [ofs_chn + chn_cnt] = buf;
				}
			}
		}

		// -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		// At this point, the plug-in is being processed

		if (cnx._src._slot_type == CnxEnd::SlotType_DLY)
		{
			ctx_node_main._aux_param_flag = true;
			ctx_node_main._comp_delay     = latency;
			ctx_node_main._pin_mult       = nbr_pins_ctx_i;
		}

		// -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -  -
		// Frees input buffers

		// Main inputs
		for (int chn_cnt = 0; chn_cnt < main_side_i._nbr_chn_tot; ++chn_cnt)
		{
			buf_alloc.ret_if_std (main_side_i._buf_arr [chn_cnt]);
		}

		// Mixed inputs
		if (mix_flag)
		{
			for (auto &chn : pi_ctx._mix_in_arr)
			{
				for (auto buf : chn)
				{
					buf_alloc.ret_if_std (buf);
				}
			}
		}

		// Done.
		node_info._visit_flag = true;
	}
}



void	Router::check_source_nodes (Document &doc, const PluginPool &plugin_pool, BufAlloc &buf_alloc, NodeCategList &categ_list, NodeInfo &node_info)
{
	int         nbr_chn = 1;

	// Visits all source nodes
	for (auto &cnx_src_pin : node_info._cnx_src_list)
	{
		for (auto &cnx_src : cnx_src_pin)
		{
			const NodeInfo &  node_src =
				categ_list [cnx_src._src._slot_type] [cnx_src._src._slot_pos];
			int            nbr_chn_src = 1;

			// Source is an audio input
			if (cnx_src._src._slot_type == CnxEnd::SlotType_IO)
			{
				nbr_chn_src = ChnMode_get_nbr_chn (doc._chn_mode, piapi::Dir_IN);
			}

			// Source is a plug-in: process it
			else
			{
				visit_node (doc, plugin_pool, buf_alloc, categ_list, cnx_src);
				nbr_chn_src = node_src._nbr_chn;
			}

			nbr_chn = std::max (nbr_chn, nbr_chn_src);
		}
	}

	node_info._nbr_chn = nbr_chn;
}



bool	Router::collects_mix_source_buffers (ProcessingContext &ctx, BufAlloc &buf_alloc, const NodeCategList &categ_list, const NodeInfo &node_info, ProcessingContextNode::Side &side, int nbr_pins_ctx, int nbr_chn, ProcessingContext::PluginContext::MixInputArray &mix_in_arr) const
{
	bool           mix_flag = false;
	const int      nbr_pins_cnx = int (node_info._cnx_src_list.size ());
	mix_in_arr.resize (side._nbr_chn_tot);
	for (int pin_cnt = 0; pin_cnt < nbr_pins_ctx; ++pin_cnt)
	{
		const int      chn_ofs = pin_cnt * nbr_chn;

		// Default: silence
		int            nbr_real_src = 0;
		for (int chn_cnt = 0; chn_cnt < nbr_chn; ++chn_cnt)
		{
			side._buf_arr [chn_ofs + chn_cnt] = Cst::BufSpecial_SILENCE;
		}

		// Pins in the graph
		if (pin_cnt < nbr_pins_cnx)
		{
			const Document::CnxList &  pin_src_arr =
				node_info._cnx_src_list [pin_cnt];
			const int      nbr_src = int (pin_src_arr.size ());

			// Scans each source and add their buffers to the mixing lists
			for (int src_cnt = 0; src_cnt < nbr_src; ++src_cnt)
			{
				const Cnx &       cnx_src  = pin_src_arr [src_cnt];
				const NodeInfo &  node_src =
					categ_list [cnx_src._src._slot_type] [cnx_src._src._slot_pos];
				const ProcessingContextNode::Side * src_side_o_ptr = 0;
				if (cnx_src._src._slot_type == CnxEnd::SlotType_IO)
				{
					src_side_o_ptr = &ctx._interface_ctx._side_arr [Dir_IN];
				}
				else
				{
					assert (node_src._ctx_index >= 0);
					const ProcessingContext::PluginContext & pi_ctx_src =
						ctx._context_arr [node_src._ctx_index];
					const ProcessingContextNode & ctx_src =
						  (pi_ctx_src._mixer_flag)
						? pi_ctx_src._node_arr [PiType_MIX ]
						: pi_ctx_src._node_arr [PiType_MAIN];
					src_side_o_ptr = &ctx_src._side_arr [Dir_OUT];
				}
				const int      nbr_chn_src = src_side_o_ptr->_nbr_chn;

				if (nbr_chn_src > 0)
				{
					// Collects the input channels
					for (int chn_cnt = 0; chn_cnt < nbr_chn; ++chn_cnt)
					{
						ProcessingContext::PluginContext::MixInChn & mix_in =
							mix_in_arr [chn_ofs + chn_cnt];

						const int      chn_idx = std::min (chn_cnt, nbr_chn_src - 1);
						const int      buf     =
							src_side_o_ptr->use_buf (cnx_src._src._pin, chn_idx);
						mix_in.push_back (buf);
						buf_alloc.use_more_if_std (buf);

						nbr_real_src = std::max (nbr_real_src, int (mix_in.size ()));
					}
				}
			} // for src_cnt

			// Input buffers for the plug-in
			for (int chn_cnt = 0; chn_cnt < nbr_chn; ++chn_cnt)
			{
				int            buf = Cst::BufSpecial_SILENCE;
				if (nbr_real_src > 1)
				{
					// Allocates new buffers for the mix output
					buf = buf_alloc.alloc ();
					mix_flag = true;
				}
				else if (nbr_real_src == 1)
				{
					// Only one source, moves the mixing list content to the
					// ProcessingContextNode::Side
					ProcessingContext::PluginContext::MixInChn & mix_in =
						mix_in_arr [chn_ofs + chn_cnt];
					assert (mix_in.size () == 1);
					buf = mix_in [0];
					mix_in.clear ();
				}
				side._buf_arr [chn_ofs + chn_cnt] = buf;
			} // for chn_cnt
		} // if pin_cnt < nbr_pins_cnx
	} // for pin_cnt

	// Removes the mixing data if we found out there is nothing to mix
	if (! mix_flag)
	{
		mix_in_arr.clear ();
	}

	return mix_flag;
}



void	Router::count_nbr_cnx_per_input_pin (MapCnxPerPin &res_map, const Document::CnxList &graph)
{
	res_map.clear ();

	// Counts all connections
	for (auto &cnx : graph)
	{
		const CnxEnd & pin = cnx._dst;
		auto           p   = res_map.insert (std::make_pair (pin, 0));
		++ p.first->second;
	}

	// Now removes single connections
	MapCnxPerPin::iterator  it = res_map.begin ();
	while (it != res_map.end ())
	{
		if (it->second <= 1)
		{
			it = res_map.erase (it);
		}
		else
		{
			++ it;
		}
	}
}



}  // namespace cmd
}  // namespace mfx



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/