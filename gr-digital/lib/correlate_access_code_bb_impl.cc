/* -*- c++ -*- */
/*
 * Copyright 2004,2006,2010-2012 Free Software Foundation, Inc.
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

#include "correlate_access_code_bb_impl.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/blocks/count_bits.h>
#include <stdexcept>
#include <cstdio>

namespace gr {
  namespace digital {

#define VERBOSE 0

    correlate_access_code_bb::sptr
    correlate_access_code_bb::make(const std::string &access_code, int threshold, bool detect_inversion/* = false*/)
    {
      return gnuradio::get_initial_sptr
	(new correlate_access_code_bb_impl(access_code, threshold, detect_inversion));
    }

    correlate_access_code_bb_impl::correlate_access_code_bb_impl(
        const std::string &access_code, int threshold, bool detect_inversion/* = false*/)
      : sync_block("correlate_access_code_bb",
		      io_signature::make(1, 1, sizeof(char)),
		      io_signature::make(1, 1, sizeof(char))),
	d_data_reg(0), d_flag_reg(0), d_flag_bit(0), d_mask(0),
	d_threshold(threshold), d_invert(false), d_invert_reg(0), d_invert_bit(0), d_detect_inversion(detect_inversion)
    {
      if(!set_access_code(access_code)) {
	throw std::out_of_range ("access_code is > 64 bits");
      }
    }

    correlate_access_code_bb_impl::~correlate_access_code_bb_impl()
    {
    }

    bool
    correlate_access_code_bb_impl::set_access_code(
        const std::string &access_code)
    {
      unsigned len = access_code.length();	// # of bytes in string
      if(len > 64)
	return false;

      // set len top bits to 1.
      d_mask = ((~0ULL) >> (64 - len)) << (64 - len);

      d_flag_bit = 1LL << (64 - len);	// Where we or-in new flag values.
                                        // new data always goes in 0x0000000000000001
      d_invert_bit = 1LL << (64 - len);

      d_access_code = 0;
      d_inverted_access_code = 0;
      for(unsigned i=0; i < 64; i++){
	d_access_code <<= 1;
  d_inverted_access_code <<= 1;
	if(i < len)
	  d_access_code |= access_code[i] & 1;	// look at LSB only
    d_inverted_access_code |= (~access_code[i]) & 1;
      }
fprintf(stderr, "[%s<%ld>] Access code: 0x%llx\n", name().c_str(), unique_id(), d_access_code);
if (d_detect_inversion) fprintf(stderr, "[%s<%ld>] Inverted:    0x%llx\n", name().c_str(), unique_id(), d_inverted_access_code);
      return true;
    }

    int
    correlate_access_code_bb_impl::work(int noutput_items,
					gr_vector_const_void_star &input_items,
					gr_vector_void_star &output_items)
    {
      const unsigned char *in = (const unsigned char*)input_items[0];
      unsigned char *out = (unsigned char*)output_items[0];

      for(int i = 0; i < noutput_items; i++) {
	// compute output value
	unsigned int t = 0;

	t |= ((d_data_reg >> 63) & 0x1) << 0;
	t |= ((d_flag_reg >> 63) & 0x1) << 1;	// flag bit
  t |= ((d_invert_reg >> 63) & 0x1) << 2;
	out[i] = t;

	// compute hamming distance between desired access code and current data
	unsigned long long wrong_bits = 0;
	unsigned int nwrong = d_threshold+1;
	int new_flag = 0;

	wrong_bits  = (d_data_reg ^ (d_invert ? d_inverted_access_code : d_access_code)) & d_mask;
	nwrong = gr::blocks::count_bits64(wrong_bits);

  unsigned long long wrong_inverted_bits = (d_data_reg ^ (d_invert ? d_access_code : d_inverted_access_code)) & d_mask;
  unsigned int nwrong_inverted = gr::blocks::count_bits64(wrong_inverted_bits);

	// test for access code with up to threshold errors
	new_flag = (nwrong <= d_threshold);
  unsigned int new_inverted = (nwrong_inverted <= d_threshold) & d_detect_inversion;

  if (new_inverted)
  {
    //fprintf(stderr, "%llu: found inverted access code\n", nitems_read(0));
    //if (new_flag)
    //  fprintf(stderr, "%llu: found normal access code\n", nitems_read(0));
  }

  if (new_flag && new_inverted)
  {
    fprintf(stderr, "Both normal and inverted flag found! (items read: %llu)\n", nitems_read(0));
  }
  else if (new_flag && d_invert)
  {
    fprintf(stderr, "Switching to normal mode (items read: %llu)\n", nitems_read(0));
    d_invert = false;
  }
  else if (new_inverted && (d_invert == false))
  {
    fprintf(stderr, "Switching to inverted mode (items read: %llu)\n", nitems_read(0));
    d_invert = true;
  }

#if VERBOSE
	if(new_flag) {
	  fprintf(stderr, "access code found: %llx\n", d_access_code);
	}
	else {
	  fprintf(stderr, "%llx  ==>  %llx\n", d_access_code, d_data_reg);
	}
#endif

	// shift in new data and new flag
	d_data_reg = (d_data_reg << 1) | ((d_invert ? (~in[i]) : in[i]) & 0x1);
	d_flag_reg = (d_flag_reg << 1);
  d_invert_reg = (d_invert_reg << 1);
	if(new_flag || new_inverted) {
	  d_flag_reg |= d_flag_bit;
	}
  if (new_inverted) {
    d_invert_reg |= d_invert_bit;
  }
      }

      return noutput_items;
    }

  } /* namespace digital */
} /* namespace gr */
