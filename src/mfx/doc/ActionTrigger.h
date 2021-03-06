/*****************************************************************************

        ActionTrigger.h
        Author: Laurent de Soras, 2016

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#pragma once
#if ! defined (mfx_doc_ActionTrigger_HEADER_INCLUDED)
#define mfx_doc_ActionTrigger_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma warning (4 : 4250)
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



namespace mfx
{
namespace doc
{



enum ActionTrigger
{
	ActionTrigger_INVALID = -1,

	ActionTrigger_PRESS   =  0,
	ActionTrigger_HOLD,
	ActionTrigger_RELEASE,

	ActionTrigger_NBR_ELT

}; // enum ActionTrigger



const char *  ActionTrigger_get_name (ActionTrigger trig);



}  // namespace doc
}  // namespace mfx



#endif   // mfx_doc_ActionTrigger_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
