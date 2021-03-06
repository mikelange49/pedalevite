/*****************************************************************************

        Bigverb.h
        Author: Laurent de Soras, 2020

Template parameters:

- T: audio data type (floating point)

Ported from a code by Paul Batchelor
https://pbat.ch/sndkit/bigverb/

Original source (found later):
ReverbSC by Sean Costello (1999)
ported in C to CSound by Istvan Varga (2005, LGPL)
https://github.com/csound/csound/blob/develop/Opcodes/reverbsc.c

Notes from the CSound code:

8 delay line FDN reverb, with feedback matrix based upon
physical modeling scattering junction of 8 lossless waveguides
of equal characteristic impedance. Based on Julius O. Smith III,
"A New Approach to Digital Reverberation using Closed Waveguide
Networks," Proceedings of the International Computer Music
Conference 1985, p. 47-53 (also available as a seperate
publication from CCRMA), as well as some more recent papers by
Smith and others.

Notes from Paul Batchelor:

Every delay line has a bit of jitter applied to the delay time, which causes a
little of pitch modulation. Several schroeder and FDN-style reverb designs use
this kind of modulation, such as the Mutable instruments clouds reverb, and
the tank reverb described in the Dattorro paper "Effect design, part 1:
reverberator and other filters". Such modulation creates the illusion of the
reverb being a higher-order system than it actual (aka: a more complex reverb
algorithm with more delay lines). While these designs tend to use a sinusoidal
LFO on only a few delay lines, the Costello topology applies non-correlated
modulation to every single delay line. As a result, you get a very dense-
sounding reverb output.

[...]

The credit for these parameter tunings (and this topology) go to Sean
Costello, who also originally designed this reverb algorithm in 1999.

[...]

Normally, this family of reverbs is a combination of allpass filters and comb
filters in various series/parallel combinations. This implementation has a far
simpler topology: it is essentially (but not quite) 8 feedback delay lines in
parallel with a 1-pole lowpass IIR filter for tone control. If it weren't for
the massive amount delay modulation, this would be a pretty metallic sounding
reverb!

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://www.wtfpl.net/ for more details.

*Tab=3***********************************************************************/



#pragma once
#if ! defined (mfx_dsp_spat_Bigverb_HEADER_INCLUDED)
#define mfx_dsp_spat_Bigverb_HEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include <vector>



namespace mfx
{
namespace dsp
{
namespace spat
{



template <typename T>
class Bigverb
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	void           set_sample_freq (double sample_freq);

	void           set_size (float size);
	void           set_cutoff (float cutoff);

	void           process_sample (T *outL, T *outR, T inL, T inR);

	void           clear_buffers ();



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	static constexpr T  _output_gain = T (0.35);
	static constexpr T  _jp_scale    = T (0.25);

	struct ParamSet
	{
		int            get_delay_size (float sr) const;

		float          _delay;     // delay time, s
		float          _drift;     // random variation in delay time, s
		float          _randfreq;  // random variation frequency, Hz
		int            _seed;      // random seed (0 - 32767)
	};

	class Delay
	{
	public:

		static constexpr int FRACNBITS = 28;
		static constexpr int FRACSCALE = 1 << FRACNBITS;
		static constexpr int FRACMASK  = FRACSCALE - 1;

		static constexpr int _rng_bits  = 16;
		static constexpr int _rng_range = 1 << _rng_bits;
		static constexpr int _rng_mask  = _rng_range - 1;
		static constexpr int _rng_scale = _rng_range >> 1;

		void           init (const ParamSet &p, int sz, float sr);
		T              compute (T in, T fdbk, T filt, float sr);
		void           generate_next_line (float sr);
		float          calculate_drift () const;
		void           clear_buffers ();

		std::vector <T>
		               _buf;
		int            _sz       = 0;
		int            _wpos     = 0;
		int            _irpos    = 0;
		int            _frpos    = 0;
		int            _seed     = 0;
		int            _rng      = 0;
		int            _inc      = 0;
		int            _counter  = 0;
		int            _maxcount = 0; // Length of the linear segments, samples
		float          _dels     = 0;
		float          _drift    = 0;
		T              _y        = T (0);
	};

	float          _sr      = 0;
	float          _size    = 0.93f;
	float          _cutoff  = 10000.f;
	float          _pcutoff = -1.f;
	T              _filt    = T (1.f);
	Delay          _delay [8];

	static constexpr ParamSet _params [8] =
	{
		//    delay        drift  randfreq  seed
		{ 2473 / 44100.f, 1.0e-3f, 3.100f,  1966 },
		{ 2767 / 44100.f, 1.1e-3f, 3.500f, 29491 },
		{ 3217 / 44100.f, 1.7e-3f, 1.110f, 22937 },
		{ 3557 / 44100.f, 0.6e-3f, 3.973f,  9830 },
		{ 3907 / 44100.f, 1.0e-3f, 2.341f, 20643 },
		{ 4127 / 44100.f, 1.1e-3f, 1.897f, 22937 },
		{ 2143 / 44100.f, 1.7e-3f, 0.891f, 29491 },
		{ 1933 / 44100.f, 0.6e-3f, 3.221f, 14417 }
	};





/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	bool           operator == (const Bigverb &other) const = delete;
	bool           operator != (const Bigverb &other) const = delete;

}; // class Bigverb



}  // namespace spat
}  // namespace dsp
}  // namespace mfx



#include "mfx/dsp/spat/Bigverb.hpp"



#endif   // mfx_dsp_spat_Bigverb_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
