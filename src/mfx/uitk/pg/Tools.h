/*****************************************************************************

        Tools.h
        Author: Laurent de Soras, 2016

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#pragma once
#if ! defined (mfx_uitk_pg_Tools_HEADER_INCLUDED)
#define mfx_uitk_pg_Tools_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include "mfx/uitk/MsgHandlerInterface.h"
#include "mfx/pi/PluginModel.h"
#include "mfx/PiType.h"

#include <set>
#include <string>
#include <vector>



namespace mfx
{

class ControlSource;
class Model;
class View;

namespace doc
{
	class Preset;
}

namespace uitk
{

class NText;

namespace pg
{



class CtrlSrcNamed;

class Tools
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	static void    set_param_text (const View &view, int width, int index, float val, int slot_index, PiType type, NText *param_name_ptr, NText &param_val, NText *param_unit_ptr, NText *fx_name_ptr, bool group_unit_val_flag);
	static MsgHandlerInterface::EvtProp
	               change_param (Model &model, const View &view, int slot_index, PiType type, int index, float step, int dir);
	static double  change_param (double val_nrm, const View &view, int slot_index, PiType type, int index, float step, int dir);
	static std::set <float>
	               create_beat_notches ();
	static int     find_ctrl_index (const ControlSource &src, const std::vector <CtrlSrcNamed> &ctrl_list);
	static std::string
	               find_ctrl_name (const ControlSource &src, const std::vector <CtrlSrcNamed> &ctrl_list);
	static void    change_plugin (Model &model, const View &view, int slot_index, int dir, const std::vector <pi::PluginModel> &fx_list);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	static void    print_param_with_pres (std::string &val_s, std::string &unit, const doc::Preset &preset, int slot_index, PiType type, int index, float val, const piapi::ParamDescInterface &desc);



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	               Tools ()                               = delete;
	               Tools (const Tools &other)             = delete;
	virtual        ~Tools () = default;
	Tools &        operator = (const Tools &other)        = delete;
	bool           operator == (const Tools &other) const = delete;
	bool           operator != (const Tools &other) const = delete;

}; // class Tools



}  // namespace pg
}  // namespace uitk
}  // namespace mfx



//#include "mfx/uitk/pg/Tools.hpp"



#endif   // mfx_uitk_pg_Tools_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/