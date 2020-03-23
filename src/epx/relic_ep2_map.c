/*
 * RELIC is an Efficient LIbrary for Cryptography
 * Copyright (C) 2007-2019 RELIC Authors
 *
 * This file is part of RELIC. RELIC is legal property of its developers,
 * whose names are not listed here. Please refer to the COPYRIGHT file
 * for contact information.
 *
 * RELIC is free software; you can redistribute it and/or modify it under the
 * terms of the version 2.1 (or later) of the GNU Lesser General Public License
 * as published by the Free Software Foundation; or version 2.0 of the Apache
 * License as published by the Apache Software Foundation. See the LICENSE files
 * for more details.
 *
 * RELIC is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the LICENSE files for more details.
 *
 * You should have received a copy of the GNU Lesser General Public or the
 * Apache License along with RELIC. If not, see <https://www.gnu.org/licenses/>
 * or <https://www.apache.org/licenses/>.
 */

/**
 * @file
 *
 * Implementation of hashing to a prime elliptic curve over a quadratic
 * extension.
 *
 * @ingroup epx
 */

#include "relic_core.h"
#include "relic_md.h"

/*============================================================================*/
/* Private definitions                                                        */
/*============================================================================*/

#ifdef EP_CTMAP
/**
 * Evaluate a polynomial represented by its coefficients using Horner's rule.
 *
 * @param[out] c			- the result.
 * @param[in] a				- the input value.
 * @param[in] coeffs		- the vector of coefficients in the polynomial.
 * @param[in] len			- the degree of the polynomial.
 */
static void fp2_eval(fp2_t c, fp2_t a, fp2_t *coeffs, int deg) {
	fp2_copy(c, coeffs[deg]);
	for (int i = deg; i > 0; --i) {
		fp2_mul(c, c, a);
		fp2_add(c, c, coeffs[i - 1]);
	}
}

/**
 * Generic isogeny map evaluation for use with SSWU map.
 */
/* declaring this function inline suppresses "unused function" warnings */
static inline void ep2_iso(ep2_t q, ep2_t p) {
	fp2_t t0, t1, t2, t3;

	if (!ep2_curve_is_ctmap()) {
		ep2_copy(q, p);
		return;
	}
	/* XXX add real support for input points in projective form */
	if (!p->norm) {
		ep2_norm(p, p);
	}

	fp2_null(t0);
	fp2_null(t1);
	fp2_null(t2);
	fp2_null(t3);

	TRY {
		fp2_new(t0);
		fp2_new(t1);
		fp2_new(t2);
		fp2_new(t3);

		iso2_t coeffs = ep2_curve_get_iso();

		/* numerators */
		fp2_eval(t0, p->x, coeffs->xn, coeffs->deg_xn);
		fp2_eval(t1, p->x, coeffs->yn, coeffs->deg_yn);
		/* denominators */
		fp2_eval(t2, p->x, coeffs->yd, coeffs->deg_yd);
		fp2_eval(t3, p->x, coeffs->xd, coeffs->deg_xd);

		/* Y = Ny * Dx * Z^2. */
		fp2_mul(q->y, p->y, t1);
		fp2_mul(q->y, q->y, t3);
		/* Z = Dx * Dy, t1 = Z^2. */
		fp2_mul(q->z, t2, t3);
		fp2_sqr(t1, q->z);
		fp2_mul(q->y, q->y, t1);
		/* X = Nx * Dy * Z. */
		fp2_mul(q->x, t0, t2);
		fp2_mul(q->x, q->x, q->z);
		q->norm = 0;
	}
	CATCH_ANY {
		THROW(ERR_CAUGHT);
	}
	FINALLY {
		fp2_free(t0);
		fp2_free(t1);
		fp2_free(t2);
		fp2_free(t3);
	}
}
#endif /* EP_CTMAP */

/**
 * Multiplies a point by the cofactor in a Barreto-Naehrig curve.
 *
 * @param[out] r			- the result.
 * @param[in] p				- the point to multiply.
 */
void ep2_mul_cof_bn(ep2_t r, ep2_t p) {
	bn_t x;
	ep2_t t0, t1, t2;

	ep2_null(t0);
	ep2_null(t1);
	ep2_null(t2);
	bn_null(x);

	TRY {
		ep2_new(t0);
		ep2_new(t1);
		ep2_new(t2);
		bn_new(x);

		fp_prime_get_par(x);

		/* Compute t0 = xP. */
		ep2_mul_basic(t0, p, x);

		/* Compute t1 = \psi(3xP). */
		ep2_dbl(t1, t0);
		ep2_add(t1, t1, t0);
		ep2_norm(t1, t1);
		ep2_frb(t1, t1, 1);

		/* Compute t2 = \psi^3(P) + t0 + t1 + \psi^2(xP). */
		ep2_frb(t2, p, 2);
		ep2_frb(t2, t2, 1);
		ep2_add(t2, t2, t0);
		ep2_add(t2, t2, t1);
		ep2_frb(t1, t0, 2);
		ep2_add(t2, t2, t1);

		ep2_norm(r, t2);
	}
	CATCH_ANY {
		THROW(ERR_CAUGHT);
	}
	FINALLY {
		ep2_free(t0);
		ep2_free(t1);
		ep2_free(t2);
		bn_free(x);
	}
}

/**
 * Multiplies a point by the cofactor in a Barreto-Lynn-Scott curve.
 *
 * @param[out] r			- the result.
 * @param[in] p				- the point to multiply.
 */
void ep2_mul_cof_b12(ep2_t r, ep2_t p) {
	bn_t x;
	ep2_t t0, t1, t2, t3;

	ep2_null(t0);
	ep2_null(t1);
	ep2_null(t2);
	ep2_null(t3);
	bn_null(x);

	TRY {
		ep2_new(t0);
		ep2_new(t1);
		ep2_new(t2);
		ep2_new(t3);
		bn_new(x);

		fp_prime_get_par(x);

		/* Compute t0 = xP. */
		ep2_mul_basic(t0, p, x);
		/* Compute t1 = [x^2]P. */
		ep2_mul_basic(t1, t0, x);

		/* t2 = (x^2 - x - 1)P = x^2P - x*P - P. */
		ep2_sub(t2, t1, t0);
		ep2_sub(t2, t2, p);
		/* t3 = \psi(x - 1)P. */
		ep2_sub(t3, t0, p);
		ep2_frb(t3, t3, 1);
		ep2_add(t2, t2, t3);
		/* t3 = \psi^2(2P). */
		ep2_dbl(t3, p);
		ep2_frb(t3, t3, 2);
		ep2_add(t2, t2, t3);
		ep2_norm(r, t2);
	}
	CATCH_ANY {
		THROW(ERR_CAUGHT);
	}
	FINALLY {
		ep2_free(t0);
		ep2_free(t1);
		ep2_free(t2);
		ep2_free(t3);
		bn_free(x);
	}
}

/*============================================================================*/
/* Public definitions                                                         */
/*============================================================================*/

void ep2_map(ep2_t p, const uint8_t *msg, int len) {
	bn_t x;
	fp2_t t0;
	uint8_t digest[RLC_MD_LEN];

	bn_null(x);
	fp2_null(t0);

	TRY {
		bn_new(x);
		fp2_new(t0);

		md_map(digest, msg, len);
		bn_read_bin(x, digest, RLC_MIN(RLC_FP_BYTES, RLC_MD_LEN));

		fp_prime_conv(p->x[0], x);
		fp_zero(p->x[1]);
		fp_set_dig(p->z[0], 1);
		fp_zero(p->z[1]);

		while (1) {
			ep2_rhs(t0, p);

			if (fp2_srt(p->y, t0)) {
				p->norm = 1;
				break;
			}

			fp_add_dig(p->x[0], p->x[0], 1);
		}

		switch (ep_curve_is_pairf()) {
			case EP_BN:
				ep2_mul_cof_bn(p, p);
				break;
			case EP_B12:
				ep2_mul_cof_b12(p, p);
				break;
			default:
				/* Now, multiply by cofactor to get the correct group. */
				ep2_curve_get_cof(x);
				if (bn_bits(x) < RLC_DIG) {
					ep2_mul_dig(p, p, x->dp[0]);
				} else {
					ep2_mul_basic(p, p, x);
				}
				break;
		}
	}
	CATCH_ANY {
		THROW(ERR_CAUGHT);
	}
	FINALLY {
		bn_free(x);
		fp2_free(t0);
	}
}
