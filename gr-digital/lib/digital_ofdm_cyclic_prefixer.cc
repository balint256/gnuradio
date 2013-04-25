/* -*- c++ -*- */
/*
 * Copyright 2004,2006,2010,2011 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <digital_ofdm_cyclic_prefixer.h>
#include <gr_io_signature.h>
#include "stdio.h"

digital_ofdm_cyclic_prefixer_sptr
digital_make_ofdm_cyclic_prefixer (size_t input_size, size_t output_size, const std::vector<float> &window)
{
  return gnuradio::get_initial_sptr(new digital_ofdm_cyclic_prefixer (input_size,
								      output_size, window));
}

digital_ofdm_cyclic_prefixer::digital_ofdm_cyclic_prefixer (size_t input_size,
							    size_t output_size, const std::vector<float> &window)
  : gr_sync_interpolator ("ofdm_cyclic_prefixer",
			  gr_make_io_signature (1, 1, input_size*sizeof(gr_complex)),
			  gr_make_io_signature (1, 1, sizeof(gr_complex)),
			  output_size), 
    d_input_size(input_size),
    d_output_size(output_size),
    d_window(window),
    d_buffer(NULL)
{
fprintf(stderr, "[%s<%i>] Input: %i, Output: %i, Window length: %i\n", name().c_str(), unique_id(), input_size, output_size, window.size());
	//set_history(1 + (window.size() / 2));
	if (window.size() > 0)
	{
		const int buffer_size = window.size() / 2;
		d_buffer = new gr_complex[buffer_size];
		memset(d_buffer, 0x00, sizeof(gr_complex) * buffer_size);
	}
}

digital_ofdm_cyclic_prefixer::~digital_ofdm_cyclic_prefixer()
{
	if (d_buffer)
		delete [] d_buffer;
}

//static unsigned char _b[1024];	// 128
//static int z = 0;

int
digital_ofdm_cyclic_prefixer::work (int noutput_items,
				    gr_vector_const_void_star &input_items,
				    gr_vector_void_star &output_items)
{
	assert(noutput_items == input_size);

  gr_complex *in = (gr_complex *) input_items[0];
  gr_complex *out = (gr_complex *) output_items[0];
  const size_t cp_size = d_output_size - d_input_size;
  unsigned int i=0, j=0;

	if (d_window.size() == 0)
	{
  j = cp_size;
  for(i=0; i < d_input_size; i++,j++) {
    out[j] = in[i];
  }

  j = d_input_size - cp_size;
  for(i=0; i < cp_size; i++, j++) {
    out[i] = in[j];
  }
	}
	else
	{
	//if (memcmp((void*)_b, (void*)in, d_window.size()/2 * sizeof(gr_complex)) != 0)
	//	fprintf(stderr, "! ");

	//const int history_offset = d_window.size() / 2;
	const int offset = d_window.size() / 2;
	//const int offset = d_window.size() % 2;
	
	// Combine end of last FFT with a bit more (earlier) part of this CP)
/*	for (i = 0, j = d_window.size()/2; j < d_window.size(); i++, j++)
	{
		out[i] = (in[i] * (z ? 0 : d_window[j])) + ((z ? d_window[i] : 0) * in[(history_offset + d_input_size) - (cp_size + d_window.size()/2) + i]);
		if (z == 0)
			out[i] = gr_complex(out[i].imag(), out[i].real());
		//out[i] = in[i];
		//out[i] = gr_complex(0,0);
	}
*/	
/*	for (i = 0, j = d_window.size()/2; j < d_window.size(); i++, j++)
	{
		//out[i] = (d_buffer[i] * (z ? 0 : d_window[j])) + ((z ? d_window[i] : 0) * in[d_input_size - (cp_size + d_window.size()/2) + i]);
		out[i] = (d_buffer[i] * d_window[j]) + (d_window[i] * in[d_input_size - (cp_size + d_window.size()/2) + i]);
		//if (z == 0)
		//	out[i] = gr_complex(out[i].imag(), out[i].real());
	}
*/
	for (i = 0; i < d_window.size()/2; i++)
	{
		out[i] = d_window[i] * in[d_input_size - cp_size + i] + d_window[offset + i] * d_buffer[i];
	}

	// Copy this CP
/*	for (j = d_input_size - cp_size; j < d_input_size; i++, j++)
	{
		out[i] = (z ? in[history_offset + j] : 0);
		//out[i] = gr_complex(0.2,0.2);
	}
*//*	for (j = d_input_size - cp_size; j < d_input_size; i++, j++)
	{
		out[i] = in[j];
	}
*/	for (j = d_input_size - cp_size + offset; j < d_input_size; j++, i++)
	{
		out[i] = in[j];
	}
	/*for (j = d_input_size - cp_size; j < d_input_size - cp_size + d_window.size()/2; i++, j++)
	{
		out[i] = in[history_offset + j] * d_window[j - (d_input_size - cp_size)];
		//out[i] = gr_complex(0.2,0.2);
	}
	for (; j < d_input_size; i++, j++)
	{
		out[i] = in[history_offset + j];
		//out[i] = gr_complex(0.2,0.2);
	}*/
	
	// Copy most of the FFT (rest will be in history for next iteration)
/*	for (j=0; i < d_output_size; ++i, ++j)
	{
		out[i] = (z ? -in[history_offset + j] : 0);
	}
*//*	for (j=0; i < d_output_size; ++i, ++j)
	{
		out[i] = in[j];
	}
*/	for (j = 0; i < d_output_size; ++i, ++j)
	{
		out[i] = in[j];
	}
	assert(j == d_input_size);
	//memcpy(d_buffer, &in[d_input_size - d_window.size()/2], d_window.size()/2 * sizeof(gr_complex));
	memcpy(d_buffer, in, d_window.size()/2 * sizeof(gr_complex));
//memcpy(_b, (void*)&in[history_offset + d_input_size - d_window.size()/2], d_window.size()/2 * sizeof(gr_complex));
	/*for (j=0; i < d_output_size - (d_window.size()/2); ++i, ++j)
	{
		out[i] = in[history_offset + j];
		//out[i] = gr_complex(0.7,0.7);
	}
	for (int n = 0; i < d_output_size; ++i, ++j, ++n)
	{
		out[i] = in[history_offset + j] * d_window[(d_window.size() / 2) + n];
		out[i] = in[history_offset + j];
		//out[i] = gr_complex(1,1);
	}*/
	}
	//z = (z ? 0 : 1);
  return d_output_size;
}
