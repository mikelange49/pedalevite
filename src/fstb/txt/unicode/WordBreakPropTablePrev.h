/*****************************************************************************

        WordBreakPropTablePrev.h
        Author: Laurent de Soras, 2008

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (fstb_txt_unicode_WordBreakPropTablePrev_HEADER_INCLUDED)
#define	fstb_txt_unicode_WordBreakPropTablePrev_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"fstb/txt/unicode/WordBreakProp.h"



namespace fstb
{
namespace txt
{
namespace unicode
{



enum WordBreakPropTablePrev
{

	WordBreakPropTablePrev_ALETTER_MIDLETTER = WordBreakProp_NBR_ELT,
	WordBreakPropTablePrev_ALETTER_MIDNUMLET,
	WordBreakPropTablePrev_NUMERIC_MIDNUM,
	WordBreakPropTablePrev_NUMERIC_MIDNUMLET,

	WordBreakPropTablePrev_NBR_ELT

};	// enum WordBreakPropTablePrev



}	// namespace unicode
}	// namespace txt
}	// namespace fstb



//#include	"fstb/txt/unicode/WordBreakPropTablePrev.hpp"



#endif	// fstb_txt_unicode_WordBreakPropTablePrev_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
