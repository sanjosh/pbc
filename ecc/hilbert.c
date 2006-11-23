#include <stdio.h>
#include <stdlib.h> //for malloc, free
#include <gmp.h>
#include <math.h>
#include "pbc_field.h"
#include "pbc_darray.h"
#include "pbc_poly.h"
#include "pbc_hilbert.h"
#include "pbc_mpc.h"

static mpf_t pi, eulere, recipeulere, epsilon, negepsilon;

static darray_t fact;

static void mpf_exp(mpf_t res, mpf_t pwr)
{
    mpf_t a;
    mpf_t f0;
    int i;

    mpf_init(a); mpf_set(a, pwr);

    mpf_init(f0);

    mpf_set(f0, a);
    mpf_add_ui(res, a, 1);

    for (i=2;;i++) {
	mpf_mul(f0, f0, a);
	mpf_div_ui(f0, f0, i);
	if (mpf_sgn(f0) > 0) {
	    if (mpf_cmp(f0, epsilon) < 0) break;
	} else {
	    if (mpf_cmp(f0, negepsilon) > 0) break;
	}
	mpf_add(res, res, f0);
    }

    mpf_clear(f0);
    mpf_clear(a);
}

static void mpc_cis(mpc_t res, mpf_t theta)
{
    mpf_t a;

    mpf_init(a); mpf_set(a, theta);
    //res = exp(i a)
    //    = cos a + i sin a
    //converges quickly near the origin
    mpf_t f0;
    mpf_ptr rx = mpc_re(res), ry = mpc_im(res);
    int i;
    int toggle = 1;

    mpf_init(f0);

    mpf_set(f0, a);
    mpf_set_ui(rx, 1);
    mpf_set(ry, f0);
    i = 1;
    for(;;) {
	toggle = !toggle;
	i++;
	mpf_div_ui(f0, f0, i);
	mpf_mul(f0, f0, a);
	if (toggle) {
	    mpf_add(rx, rx, f0);
	} else {
	    mpf_sub(rx, rx, f0);
	}

	i++;
	mpf_div_ui(f0, f0, i);
	mpf_mul(f0, f0, a);

	if (toggle) {
	    mpf_add(ry, ry, f0);
	} else {
	    mpf_sub(ry, ry, f0);
	}

	if (mpf_sgn(f0) > 0) {
	    if (mpf_cmp(f0, epsilon) < 0) break;
	} else {
	    if (mpf_cmp(f0, negepsilon) > 0) break;
	}
    }

    mpf_clear(f0);
    mpf_clear(a);
}

static void compute_q(mpc_t q, mpc_t tau)
    //q = exp(2 pi i tau)
{
    mpc_t z0;
    mpf_t f0, f1;
    mpf_ptr fp0;
    unsigned long pwr;

    mpc_init(z0);
    mpf_init(f0);
    mpf_init(f1);

    //compute z0 = 2 pi i tau
    mpc_set(z0, tau);
    //first remove integral part of Re(tau)
    //since exp(2 pi i)  = 1
    //it seems |Re(tau)| < 1 anyway?
    fp0 = mpc_re(z0);
    mpf_trunc(f1, fp0);
    mpf_sub(fp0, fp0, f1);

    mpc_mul_mpf(z0, z0, pi);
    mpc_mul_ui(z0, z0, 2);
    mpc_muli(z0, z0);

    //compute q = exp(z0);
    //first write z0 = A + a + b i
    //where A is a (negative) integer
    //and a, b are in [-1, 1]
    //compute e^A separately
    fp0 = mpc_re(z0);
    pwr = mpf_get_ui(fp0);
    mpf_pow_ui(f0, recipeulere, pwr);
    mpf_add_ui(fp0, fp0, pwr);

    mpf_exp(f1, mpc_re(z0));
    mpf_mul(f0, f1, f0);
    mpc_cis(q, mpc_im(z0));

    /*
    old_mpc_exp(q, z0);
    */
    mpc_mul_mpf(q, q, f0);

    mpc_clear(z0);
    mpf_clear(f0);
    mpf_clear(f1);
}

static void compute_Delta(mpc_t z, mpc_t q)
    //z = Delta(q) (see Cohen)
{
    int d;
    int n;
    int power;
    mpc_t z0, z1, z2;

    mpc_init(z0);
    mpc_init(z1);
    mpc_init(z2);

    mpc_set_ui(z0, 1);
    d = -1;
    for(n=1; n<100; n++) {
	power = n *(3 * n - 1) / 2;
	mpc_pow_ui(z1, q, power);
	mpc_pow_ui(z2, q, n);
	mpc_mul(z2, z2, z1);
	mpc_add(z1, z1, z2);
	if (d) {
	    mpc_sub(z0, z0, z1);
	    d = 0;
	} else {
	    mpc_add(z0, z0, z1);
	    d = 1;
	}
    }

    mpc_pow_ui(z0, z0, 24);
    mpc_mul(z, z0, q);

    mpc_clear(z0);
    mpc_clear(z1);
    mpc_clear(z2);
}

static void compute_h(mpc_t z, mpc_t tau)
    //z = h(tau)
    //(called h() by Blake et al, f() by Cohen)
{
    mpc_t z0, z1, q;
    mpc_init(q);
    mpc_init(z0);
    mpc_init(z1);
    compute_q(q, tau);
    mpc_mul(z0, q, q);
    compute_Delta(z0, z0);
    compute_Delta(z1, q);
    mpc_div(z, z0, z1);
    mpc_clear(q);
    mpc_clear(z0);
    mpc_clear(z1);
}

static void compute_j(mpc_t j, mpc_t tau)
    //j = j(tau)
{
    mpc_t h;
    mpc_t z0;
    mpc_init(h);
    mpc_init(z0);
    compute_h(h, tau);
    //mpc_mul_ui(z0, h, 256);
    mpc_mul_2exp(z0, h, 8);
    mpc_add_ui(z0, z0, 1);
    mpc_pow_ui(z0, z0, 3);
    mpc_div(j, z0, h);
    mpc_clear(z0);
    mpc_clear(h);
}

static void compute_pi(int prec)
{
    //Chudnovsky brothers' Ramanujan formula
    //http://www.cs.uwaterloo.ca/~alopez-o/math-faq/mathtext/node12.html
    mpz_t k1, k2, k4, k5, d;
    unsigned int k3 = 640320;
    unsigned int k6 = 53360;
    mpz_t z0, z1, z2;
    mpq_t p, q;
    mpf_t f1;
    int toggle = 1;
    int n;
    //converges fast: each term gives over 47 bits
    int nlimit = prec / 47 + 1;

    mpz_init(k1);
    mpz_init(k2);
    mpz_init(k4);
    mpz_init(k5);
    mpz_init(d);
    mpz_init(z0);
    mpz_init(z1);
    mpz_init(z2);
    mpq_init(q);
    mpq_init(p);
    mpf_init(f1);

    mpz_set_str(k1, "545140134", 10);
    mpz_set_str(k2, "13591409", 10);
    mpz_set_str(k4, "100100025", 10);
    mpz_set_str(k5, "327843840", 10);

    mpz_mul(d, k4, k5);
    mpz_mul_2exp(d, d, 3);
    mpq_set_ui(p, 0, 1);

    for (n=0; n<nlimit; n++) {
	mpz_fac_ui(z0, 6*n);
	mpz_mul_ui(z1, k1, n);
	mpz_add(z1, z1, k2);
	mpz_mul(z0, z0, z1);

	mpz_fac_ui(z1, 3*n);
	mpz_fac_ui(z2, n);
	mpz_pow_ui(z2, z2, 3);
	mpz_mul(z1, z1, z2);
	mpz_pow_ui(z2, d, n);
	mpz_mul(z1, z1, z2);

	mpz_set(mpq_numref(q), z0);
	mpz_set(mpq_denref(q), z1);
	mpq_canonicalize(q);
	if (toggle) {
	    mpq_add(p, p, q);
	} else {
	    mpq_sub(p, p, q);
	}
	toggle = !toggle;
    }
    mpq_inv(q, p);
    mpz_mul_ui(mpq_numref(q), mpq_numref(q), k6);
    mpq_canonicalize(q);
    mpf_set_q(pi, q);
    mpf_sqrt_ui(f1, k3);
    mpf_mul(pi, pi, f1);
    //mpf_out_str(stdout, 0, 14 * nlimit, pi);
    //printf("\n");

    mpz_clear(k1);
    mpz_clear(k2);
    mpz_clear(k4);
    mpz_clear(k5);
    mpz_clear(d);
    mpz_clear(z0);
    mpz_clear(z1);
    mpz_clear(z2);
    mpq_clear(q);
    mpq_clear(p);
    mpf_clear(f1);
}

static void precision_init(int prec)
{
    int i;
    mpf_t f0;

    mpf_set_default_prec(prec);
    mpf_init2(epsilon, 2);
    mpf_init2(negepsilon, 2);
    mpf_init(recipeulere);
    mpf_init(pi);
    mpf_init(eulere);
    darray_init(fact);

    mpf_set_ui(epsilon, 1);
    mpf_div_2exp(epsilon, epsilon, prec);
    mpf_neg(negepsilon, epsilon);

    mpf_init(f0);
    mpf_set_ui(eulere, 1);
    mpf_set_ui(f0, 1);
    for (i=1;; i++) {
	mpf_div_ui(f0, f0, i);
	if (mpf_cmp(f0, epsilon) < 0) {
	    break;
	}
	mpf_add(eulere, eulere, f0);
    }
    mpf_clear(f0);

    mpf_ui_div(recipeulere, 1, eulere);

    compute_pi(prec);
}

static void precision_clear(void)
{
    mpf_clear(eulere);
    mpf_clear(recipeulere);
    mpf_clear(pi);
    mpf_clear(epsilon);
    mpf_clear(negepsilon);
    darray_clear(fact);
}

void hilbert_poly(darray_t P, int D)
    //returns darray of mpz's
    //that are coefficients of H_D(x)
    //(see Cohen: note my D is -D in his notation)
{
    int a, b;
    int t;
    int B = floor(sqrt((double) D / 3.0));
    mpc_t alpha;
    mpc_t j;
    mpf_t sqrtD;
    mpf_t f0;
    darray_t Pz;
    mpc_t z0, z1, z2;
    double d = 1.0;
    int h = 1;
    int jcount = 1;

    //first compute required precision
    b = D % 2;
    for (;;) {
	t = (b*b + D) / 4;
	a = b;
	if (a <= 1) {
	    a = 1;
	    goto step535_4;
	}
step535_3:
	if (!(t % a)) {
	    jcount++;
	    if ((a == b) || (a*a == t) || !b) {
		d += 1.0 / ((double) a);
		h++;
	    } else {
		d += 2.0 / ((double) a);
		h+=2;
	    }
	}
step535_4:
	a++;
	if (a * a <= t) {
	    goto step535_3;
	} else {
	    b += 2;
	    if (b > B) break;
	}
    }

    //printf("modulus: %f\n", exp(3.14159265358979 * sqrt(D)) * d * 0.5);
    d *= sqrt(D) * 3.14159265358979 / log(2);
    precision_init(d + 34);
    fprintf(stderr, "class number %d, %d bit precision\n", h, (int) d + 34);

    darray_init(Pz);
    mpc_init(alpha);
    mpc_init(j);
    mpc_init(z0);
    mpc_init(z1);
    mpc_init(z2);
    mpf_init(sqrtD);
    mpf_init(f0);

    mpf_sqrt_ui(sqrtD, D);
    b = D % 2;
    h = 0;
    for (;;) {
	t = (b*b + D) / 4;
	if (b > 1) {
	    a = b;
	} else {
	    a = 1;
	}
step3:
	if (t % a) {
step4:
	    a++;
	    if (a * a <= t) goto step3;
	} else {
	    //a, b, t/a are coeffs of an appropriate
	    //primitive reduced positive definite form
	    //compute j((-b + sqrt{-D})/(2a))
	    h++;
	    fprintf(stderr, "[%d/%d] a b c = %d %d %d\n", h, jcount, a, b, t/a);
	    mpf_set_ui(f0, 1);
	    mpf_div_ui(f0, f0, 2 * a);
	    mpf_mul(mpc_im(alpha), sqrtD, f0);
	    mpf_mul_ui(f0, f0, b);
	    mpf_neg(mpc_re(alpha), f0);

	    compute_j(j, alpha);
if (0) {
    int i;
    for (i=Pz->count - 1; i>=0; i--) {
	printf("P %d = ", i);
	mpc_out_str(stdout, 10, 4, Pz->item[i]);
	printf("\n");
    }
}
	    if (a == b || a * a == t || !b) {
		//P *= X - j
		int i, n;
		mpc_ptr p0;
		p0 = (mpc_ptr) malloc(sizeof(mpc_t));
		mpc_init(p0);
		mpc_neg(p0, j);
		n = Pz->count;
		if (n) {
		    mpc_set(z1, Pz->item[0]);
		    mpc_add(Pz->item[0], z1, p0);
		    for (i=1; i<n; i++) {
			mpc_mul(z0, z1, p0);
			mpc_set(z1, Pz->item[i]);
			mpc_add(Pz->item[i], z1, z0);
		    }
		    mpc_mul(p0, p0, z1);
		}
		darray_append(Pz, p0);
	    } else {
		//P *= X^2 - 2 Re(j) X + |j|^2
		int i, n;
		mpc_ptr p0, p1;
		p0 = (mpc_ptr) malloc(sizeof(mpc_t));
		p1 = (mpc_ptr) malloc(sizeof(mpc_t));
		mpc_init(p0);
		mpc_init(p1);
		//p1 = - 2 Re(j)
		mpf_mul_ui(f0, mpc_re(j), 2);
		mpf_neg(f0, f0);
		mpf_set(mpc_re(p1), f0);
		//p0 = |j|^2
		mpf_mul(f0, mpc_re(j), mpc_re(j));
		mpf_mul(mpc_re(p0), mpc_im(j), mpc_im(j));
		mpf_add(mpc_re(p0), mpc_re(p0), f0);
		n = Pz->count;
		if (!n) {
		} else if (n == 1) {
		    mpc_set(z1, Pz->item[0]);
		    mpc_add(Pz->item[0], z1, p1);
		    mpc_mul(p1, z1, p1);
		    mpc_add(p1, p1, p0);
		    mpc_mul(p0, p0, z1);
		} else {
		    mpc_set(z2, Pz->item[0]);
		    mpc_set(z1, Pz->item[1]);
		    mpc_add(Pz->item[0], z2, p1);
		    mpc_mul(z0, z2, p1);
		    mpc_add(Pz->item[1], z1, z0);
		    mpc_add(Pz->item[1], Pz->item[1], p0);
		    for (i=2; i<n; i++) {
			mpc_mul(z0, z1, p1);
			mpc_mul(alpha, z2, p0);
			mpc_set(z2, z1);
			mpc_set(z1, Pz->item[i]);
			mpc_add(alpha, alpha, z0);
			mpc_add(Pz->item[i], z1, alpha);
		    }
		    mpc_mul(z0, z2, p0);
		    mpc_mul(p1, p1, z1);
		    mpc_add(p1, p1, z0);
		    mpc_mul(p0, p0, z1);
		}
		darray_append(Pz, p1);
		darray_append(Pz, p0);
	    }
	    goto step4;
	}
	b+=2;
	if (b > B) break;
    }

    //round polynomial
    {
	int i;
	mpz_ptr coeff;
	for (i=Pz->count - 1; i>=0; i--) {
	    coeff = (mpz_ptr) malloc(sizeof(mpz_t));
	    mpz_init(coeff);
	    if (mpf_sgn(mpc_re(Pz->item[i])) < 0) {
		mpf_set_d(f0, -0.5);
	    } else {
		mpf_set_d(f0, 0.5);
	    }
	    mpf_add(f0, f0, mpc_re(Pz->item[i]));
	    mpz_set_f(coeff, f0);
	    darray_append(P, coeff);
	    mpc_clear(Pz->item[i]);
	    free(Pz->item[i]);
	}
	coeff = (mpz_ptr) malloc(sizeof(mpz_t));
	mpz_init(coeff);
	mpz_set_ui(coeff, 1);
	darray_append(P, coeff);
    }
    darray_clear(Pz);
    mpc_clear(z0);
    mpc_clear(z1);
    mpc_clear(z2);
    mpf_clear(f0);
    mpf_clear(sqrtD);
    mpc_clear(alpha);
    mpc_clear(j);

    precision_clear();
}

void hilbert_poly_clear(darray_t P)
{
    int i, n = P->count;

    for (i=0; i<n; i++) {
	mpz_ptr z = P->item[i];
	mpz_clear(z);
    }
}

int findroot(element_ptr root, element_ptr poly)
    //returns 0 if a root exists and sets root to one of the roots
    //otherwise return value is nonzero
{
    //compute gcd(x^q - x, poly)
    field_t fpxmod;
    element_t p, x, r, fac, g;
    mpz_t q;
    
    mpz_init(q);
    mpz_set(q, poly_base_field(poly)->order);

    field_init_polymod(fpxmod, poly);
    element_init(p, fpxmod);
    element_init(x, fpxmod);
    element_init(g, poly->field);
    element_set1(((element_t *) x->data)[1]);
    /*
    printf("q = ");
    mpz_out_str(stdout, 10, q);
    printf("\n");
    */
fprintf(stderr, "findroot: degree %d...\n", poly_degree(poly));
    element_pow_mpz(p, x, q);
    element_sub(p, p, x);

    element_polymod_to_poly(g, p);
    element_clear(p);
    poly_gcd(g, g, poly);
    poly_make_monic(g, g);
    element_clear(x);
    field_clear(fpxmod);

    if (!poly_degree(g)) {
	printf("no roots!\n");
	mpz_clear(q);
	element_clear(g);
	return -1;
    }
    //Use Cantor-Zassenhaus to find a root
    element_init(fac, g->field);
    element_init(x, g->field);
    element_set_si(x, 1);
    mpz_sub_ui(q, q, 1);
    mpz_divexact_ui(q, q, 2);
    element_init(r, g->field);
    for (;;) {
	if (poly_degree(g) == 1) {
	    //found a root!
	    break;
	}
step_random:
	poly_random_monic(r, 1);
	//TODO: evaluate at g instead of bothering with gcd
	poly_gcd(fac, r, g);

	if (poly_degree(fac) > 0) {
	    poly_make_monic(g, fac);
	} else {
	    field_init_polymod(fpxmod, g);
	    int n;
	    element_init(p, fpxmod);

	    element_poly_to_polymod_truncate(p, r);
fprintf(stderr, "findroot: degree %d...\n", poly_degree(g));
	    element_pow_mpz(p, p, q);

	    element_polymod_to_poly(r, p);
	    element_clear(p);

	    element_add(r, r, x);
	    poly_gcd(fac, r, g);
	    n = poly_degree(fac);
	    if (n > 0 && n < poly_degree(g)) {
		poly_make_monic(g, fac);
	    } else {
		goto step_random;
	    }
	    field_clear(fpxmod);
	}
    }
fprintf(stderr, "findroot: found root\n");
    element_neg(root, poly_coeff(g, 0));
    element_clear(r);
    mpz_clear(q);
    element_clear(x);
    element_clear(g);
    element_clear(fac);
    return 0;
}
