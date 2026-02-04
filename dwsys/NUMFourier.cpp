/* NUMFourier.cpp
 *
 * Copyright (C) 1997-2011,2025 David Weenink, Paul Boersma 2016-2018,2020
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This code is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this work. If not, see <http://www.gnu.org/licenses/>.
 */

#include "NUMFourier.h"

#include "oo_DESTROY.h"
#include "NUMFourierTable_def.h"
#include "oo_COPY.h"
#include "NUMFourierTable_def.h"
#include "oo_EQUAL.h"
#include "NUMFourierTable_def.h"
#include "oo_CAN_WRITE_AS_ENCODING.h"
#include "NUMFourierTable_def.h"
#include "oo_WRITE_TEXT.h"
#include "NUMFourierTable_def.h"
#include "oo_WRITE_BINARY.h"
#include "NUMFourierTable_def.h"
#include "oo_READ_TEXT.h"
#include "NUMFourierTable_def.h"
#include "oo_READ_BINARY.h"
#include "NUMFourierTable_def.h"
#include "oo_DESCRIPTION.h"
#include "NUMFourierTable_def.h"

#define POCKETFFT_NO_MULTITHREADING
#define POCKETFFT_CACHE_SIZE 16
#include "pocketfft_hdronly.h"

Thing_implement (NUMFourierTable, Daata, 0);

autoNUMFourierTable NUMFourierTable_create (integer n) {
	try {
		autoNUMFourierTable me = Thing_new (NUMFourierTable);
		my n = n;
		/*
			pocketfft manages its own plans internally, so trigcache/splitcache
			are no longer needed. Keep minimal allocations for struct compatibility.
		*/
		my trigcacheSize = 0;
		my splitcacheSize = 0;
		return me;
	} catch (MelderError) {
		Melder_throw (U"Cannot create NUMFourierTable.");
	}
}

void NUMforwardRealFastFourierTransform (VEC data) {
	autoNUMFourierTable table = NUMFourierTable_create (data.size);
	NUMfft_forward (table.get(), data);
	if (data.size > 1) {
		/*
			To be compatible with old behaviour.
		*/
		double tmp = data [data.size];
		for (integer i = data.size; i > 2; i --)
			data [i] = data [i - 1];
		data [2] = tmp;
	}
}

void NUMreverseRealFastFourierTransform (VEC data) {
	if (data.size > 1) {
		/*
			To be compatible with old behaviour.
		*/
		double tmp = data [2];
		for (integer i = 2; i < data.size; i ++)
			data [i] = data [i + 1];
		data [data.size] = tmp;
	}
	autoNUMFourierTable table = NUMFourierTable_create (data.size);
	NUMfft_backward (table.get(), data);
}

void NUMfft_forward (NUMFourierTable me, VEC data) {
	if (my n == 1)
		return;
	Melder_assert (my n == data.size);
	size_t n = (size_t) my n;
	pocketfft::shape_t shape{n};
	pocketfft::stride_t stride{(ptrdiff_t)sizeof(double)};
	pocketfft::shape_t axes{0};
	double *ptr = data.asArgumentToFunctionThatExpectsZeroBasedArray();
	pocketfft::r2r_fftpack(shape, stride, stride, axes,
		true, true, ptr, ptr, 1.0);
}

void NUMfft_backward (NUMFourierTable me, VEC data) {
	if (my n == 1)
		return;
	Melder_assert (my n == data.size);
	size_t n = (size_t) my n;
	pocketfft::shape_t shape{n};
	pocketfft::stride_t stride{(ptrdiff_t)sizeof(double)};
	pocketfft::shape_t axes{0};
	double *ptr = data.asArgumentToFunctionThatExpectsZeroBasedArray();
	pocketfft::r2r_fftpack(shape, stride, stride, axes,
		false, false, ptr, ptr, 1.0);
}

void NUMrealft (VEC data, integer isign) {
	if (isign == 1)
		NUMforwardRealFastFourierTransform (data);
	else
		NUMreverseRealFastFourierTransform (data);
}

/* End of file NUMFourier.cpp */
