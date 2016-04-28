/*****************************************************************************

        CategClass.h
        Author: Laurent de Soras, 2008

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/



#if ! defined (fstb_txt_unicode_CategClass_HEADER_INCLUDED)
#define	fstb_txt_unicode_CategClass_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



namespace fstb
{
namespace txt
{
namespace unicode
{



enum CategClass
{

	CategClass_LETTER = 0,
	CategClass_MARK,
	CategClass_NUMBER,
	CategClass_PUNCTUATION,
	CategClass_SYMBOL,
	CategClass_SEPARATOR,
	CategClass_OTHER,

	CategClass_NBR_ELT

};	// enum CategClass



}	// namespace unicode
}	// namespace txt
}	// namespace fstb



//#include	"fstb/txt/unicode/CategClass.hpp"



#endif	// fstb_txt_unicode_CategClass_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
