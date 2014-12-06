/* -*- c++ -*- */
/*
 * Copyright 2004,2010-2012 Free Software Foundation, Inc.
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

#include "clock_recovery_mm_ff_impl.h"
#include <gnuradio/io_signature.h>
#include <gnuradio/math.h>
#include <stdexcept>

namespace gr {
  namespace digital {

    clock_recovery_mm_ff::sptr 
    clock_recovery_mm_ff::make(float omega, float gain_omega,
			       float mu, float gain_mu,
			       float omega_relative_limit)
    {
      return gnuradio::get_initial_sptr
	(new clock_recovery_mm_ff_impl(omega, gain_omega, 
				       mu, gain_mu,
				       omega_relative_limit));
    }

    clock_recovery_mm_ff_impl::clock_recovery_mm_ff_impl(float omega, float gain_omega,
							 float mu, float gain_mu,
							 float omega_relative_limit)
      : block("clock_recovery_mm_ff",
		 io_signature::make(1, 2, sizeof(float)),
		 io_signature::make(1, 4, sizeof(float))),
	d_mu(mu), d_gain_mu(gain_mu), d_gain_omega(gain_omega),
	d_omega_relative_limit(omega_relative_limit),
	d_last_sample(0), d_interp(new filter::mmse_fir_interpolator_ff()),
	d_omega_orig(omega)
    {
      if(omega <  1)
	throw std::out_of_range("clock rate must be > 0");
      if(gain_mu <  0  || gain_omega < 0)
	throw std::out_of_range("Gains must be non-negative");

      set_omega(omega);			// also sets min and max omega
      set_relative_rate (1.0 / omega);
    }

    clock_recovery_mm_ff_impl::~clock_recovery_mm_ff_impl()
    {
      delete d_interp;
    }

    void
    clock_recovery_mm_ff_impl::forecast(int noutput_items,
					gr_vector_int &ninput_items_required)
    {
      unsigned ninputs = ninput_items_required.size();
      for(unsigned i=0; i < ninputs; i++)
	ninput_items_required[i] =
	  (int) ceil((noutput_items * d_omega) + d_interp->ntaps());
    }

    static inline float
    slice(float x)
    {
      return x < 0 ? -1.0F : 1.0F;
    }

    int
    clock_recovery_mm_ff_impl::general_work(int noutput_items,
					    gr_vector_int &ninput_items,
					    gr_vector_const_void_star &input_items,
					    gr_vector_void_star &output_items)
    {
      const float *in = (const float *)input_items[0];
      const float *thru_in = NULL;
      if (input_items.size() > 1)
	thru_in = (const float *) input_items[1];
      float *out = (float *)output_items[0];
      float *thru_out = NULL;
      if (output_items.size() > 1)
	thru_out = (float *) output_items[1];
      float *omega_out = NULL;
      if (output_items.size() > 2)
	omega_out = (float *) output_items[2];
      float *mu_out = NULL;
      if (output_items.size() > 3)
	mu_out = (float *) output_items[3];

      int ii = 0; // input index
      int oo = 0; // output index
      int ni = ninput_items[0] - d_interp->ntaps(); // don't use more input than this
      int thru_count = 0;
      float mm_val;

      while(oo < noutput_items && ii < ni ) {
	// produce output sample
	out[oo] = d_interp->interpolate(&in[ii], d_mu);
	mm_val = slice(d_last_sample) * out[oo] - slice(out[oo]) * d_last_sample;
	d_last_sample = out[oo];

	d_omega = d_omega + d_gain_omega * mm_val;
	d_omega = d_omega_mid + gr::branchless_clip(d_omega-d_omega_mid, d_omega_relative_limit);
	d_mu = d_mu + d_omega + d_gain_mu * mm_val;
	
	int i_omega = (int)floor(d_omega_orig);
	
	if (thru_out != NULL){
	  if (thru_in != NULL) {
	    //thru_out[oo] = thru_in[ii];
	    memcpy(thru_out + thru_count, /*in*/thru_in + ii/* - (int)(d_mu - floor(d_mu))*/, sizeof(float)*i_omega);
	    thru_count += i_omega;
	  }
	  else
	    thru_out[oo] = in[ii];
	}
	if (omega_out != NULL)
	  omega_out[oo] = d_omega;
	if (mu_out != NULL)
	  mu_out[oo] = d_mu;

	ii += (int)floor(d_mu);
	d_mu = d_mu - floor(d_mu);
	oo++;
      }

      consume_each(ii);
      //return oo;
      produce(0, oo);
      if (thru_out != NULL){
	  if (thru_in != NULL)
	    produce(1, thru_count);
	  else
	    produce(1, oo);
      }
      if (omega_out != NULL)
	produce(2, oo);
      if (mu_out != NULL)
	produce(3, oo);
      return WORK_CALLED_PRODUCE;
    }

  } /* namespace digital */
} /* namespace gr */
