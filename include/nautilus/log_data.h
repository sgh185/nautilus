/*
 * Copyright (c) 2018, Arm Limited.
 * SPDX-License-Identifier: MIT
 *
 * Modified by Michael Cuevas <cuevas@u.northwestern.edu>
 *
 */

#ifndef __LOG_DATA_H
#define __LOG_DATA_H

#define __MUSL_LOG_TABLE_BITS 7
#define __MUSL_LOG_POLY_ORDER 6
#define __MUSL_LOG_POLY1_ORDER 12
extern const struct log_data {
	double ln2hi;
	double ln2lo;
	double poly[__MUSL_LOG_POLY_ORDER - 1]; /* First coefficient is 1.  */
	double poly1[__MUSL_LOG_POLY1_ORDER - 1];
	struct {
		double invc, logc;
	} tab[1 << __MUSL_LOG_TABLE_BITS];
	struct {
		double chi, clo;
	} tab2[1 << __MUSL_LOG_TABLE_BITS];
} __log_data;

#endif
