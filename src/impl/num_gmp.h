#ifndef _SECP256K1_NUM_REPR_IMPL_H_
#define _SECP256K1_NUM_REPR_IMPL_H_

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <gmp.h>

#include "num.h"

#ifdef VERIFY
void static secp256k1_num_sanity(const secp256k1_num_t *a) {
    assert(a->limbs == 1 || (a->limbs > 1 && a->data[a->limbs-1] != 0));
}
#else
#define secp256k1_num_sanity(a) do { } while(0)
#endif

void static secp256k1_num_start(void) {
}

void static secp256k1_num_stop(void) {
}

void static secp256k1_num_init(secp256k1_num_t *r) {
    r->neg = 0;
    r->limbs = 1;
    r->data[0] = 0;
}

void static secp256k1_num_free(secp256k1_num_t *r) {
}

void static secp256k1_num_copy(secp256k1_num_t *r, const secp256k1_num_t *a) {
    *r = *a;
}

int static secp256k1_num_bits(const secp256k1_num_t *a) {
    int ret=(a->limbs-1)*GMP_NUMB_BITS;
    mp_limb_t x=a->data[a->limbs-1];
    while (x) {
        x >>= 1;
        ret++;
    }
    return ret;
}


void static secp256k1_num_get_bin(unsigned char *r, unsigned int rlen, const secp256k1_num_t *a) {
    unsigned char tmp[65];
    int len = mpn_get_str(tmp, 256, (mp_limb_t*)a->data, a->limbs);
    assert(len <= rlen);
    memset(r, 0, rlen - len);
    memcpy(r + rlen - len, tmp, len);
}

void static secp256k1_num_set_bin(secp256k1_num_t *r, const unsigned char *a, unsigned int alen) {
    assert(alen > 0);
    assert(alen <= 64);
    int len = mpn_set_str(r->data, a, alen, 256);
    assert(len <= NUM_LIMBS*2);
    r->limbs = len;
    r->neg = 0;
    while (r->limbs > 1 && r->data[r->limbs-1]==0) r->limbs--;
}

void static secp256k1_num_set_int(secp256k1_num_t *r, int a) {
    r->limbs = 1;
    r->neg = (a < 0);
    r->data[0] = (a < 0) ? -a : a;
}

void static secp256k1_num_mod(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);

    r->neg = a->neg;
    if (a->limbs >= b->limbs) {
        mp_limb_t q[2*NUM_LIMBS+1];
        mp_limb_t t[2*NUM_LIMBS+1];
        mpn_tdiv_qr(t, q, 0, a->data, a->limbs, b->data, b->limbs);
        r->limbs = b->limbs;
        while (r->limbs > 1 && q[r->limbs-1]==0) r->limbs--;
        mpn_copyi(r->data, q, r->limbs);
    } else {
        *r = *a;
    }
}

void static secp256k1_num_mod_inverse(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *m) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(m);

#ifdef VERIFY
    secp256k1_num_t a2 = *a;
    secp256k1_num_t m2 = *m;
#endif

    // mpn_gcdext computes: (G,S) = gcdext(U,V), where
    // * G = gcd(U,V)
    // * G = U*S + V*T
    // * U has equal or more limbs than V, and V has no padding
    // If we set U to be (a padded version of) a, and V = m:
    //   G = a*S + m*T
    //   G = a*S mod m
    // Assuming G=1:
    //   S = 1/a mod m
    assert(m->limbs <= NUM_LIMBS);
    assert(m->data[m->limbs-1] != 0);
    mp_limb_t g[NUM_LIMBS+1];
    mp_limb_t u[NUM_LIMBS+1];
    mp_limb_t v[NUM_LIMBS+1];
    for (int i=0; i < m->limbs; i++) {
        u[i] = (i < a->limbs) ? a->data[i] : 0;
        v[i] = m->data[i];
    }
    mp_size_t sn = NUM_LIMBS+1;
    mp_size_t gn = mpn_gcdext(g, r->data, &sn, u, m->limbs, v, m->limbs);
    assert(gn == 1);
    assert(g[0] == 1);
    r->neg = a->neg ^ m->neg;
    if (sn < 0) {
        mpn_sub(r->data, m->data, m->limbs, r->data, -sn);
        r->limbs = m->limbs;
        while (r->limbs > 1 && r->data[r->limbs-1]==0) r->limbs--;
    } else {
        r->limbs = sn;
    }

#ifdef VERIFY
    secp256k1_num_t c;
    secp256k1_num_mod_mul(&c, &a2, r, m);
    assert(c.limbs == 1 && c.data[0] == 1);
#endif
}

int static secp256k1_num_is_zero(const secp256k1_num_t *a) {
    return (a->limbs == 1 && a->data[0] == 0);
}

int static secp256k1_num_is_odd(const secp256k1_num_t *a) {
    return a->data[0] & 1;
}

int static secp256k1_num_is_neg(const secp256k1_num_t *a) {
    return (a->limbs > 1 || a->data[0] != 0) && a->neg;
}

int static secp256k1_num_cmp(const secp256k1_num_t *a, const secp256k1_num_t *b) {
    if (a->limbs > b->limbs) return 1;
    if (a->limbs < b->limbs) return -1;
    return mpn_cmp(a->data, b->data, a->limbs);
}

void static secp256k1_num_add_abs(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    mp_limb_t c = mpn_add(r->data, a->data, a->limbs, b->data, b->limbs);
    r->limbs = a->limbs;
    if (c != 0) {
        assert(r->limbs < 2*NUM_LIMBS);
        r->data[r->limbs++] = c;
    }
}

void static secp256k1_num_sub_abs(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    mp_limb_t c = mpn_sub(r->data, a->data, a->limbs, b->data, b->limbs);
    assert(c == 0);
    r->limbs = a->limbs;
    while (r->limbs > 1 && r->data[r->limbs-1]==0) r->limbs--;
}

void static secp256k1_num_subadd(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b, int bneg) {
    if (!(b->neg ^ bneg ^ a->neg)) { // a and b have the same sign
        r->neg = a->neg;
        if (a->limbs >= b->limbs) {
            secp256k1_num_add_abs(r, a, b);
        } else {
            secp256k1_num_add_abs(r, b, a);
        }
    } else {
        if (secp256k1_num_cmp(a, b) > 0) {
            r->neg = a->neg;
            secp256k1_num_sub_abs(r, a, b);
        } else {
            r->neg = b->neg ^ bneg;
            secp256k1_num_sub_abs(r, b, a);
        }
    }
}

void static secp256k1_num_add(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);

#ifdef VERIFY
    secp256k1_num_t a2 = *a;
    secp256k1_num_t b2 = *b;
#endif

    secp256k1_num_subadd(r, a, b, 0);

#ifdef VERIFY
    secp256k1_num_t c = *r;
    secp256k1_num_subadd(&c, &c, &b2, 1);
    assert(secp256k1_num_cmp(&c, &a2) == 0);
#endif
}

void static secp256k1_num_sub(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
#ifdef VERIFY
    secp256k1_num_t a2 = *a;
    secp256k1_num_t b2 = *b;
#endif
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);

    secp256k1_num_subadd(r, a, b, 1);

#ifdef VERIFY
    secp256k1_num_t c;
    secp256k1_num_subadd(&c, r, &b2, 0);
    assert(secp256k1_num_cmp(&c, &a2) == 0);
#endif
}

void static secp256k1_num_mul(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
#ifdef VERIFY
    secp256k1_num_t a2 = *a;
    secp256k1_num_t b2 = *b;
#endif

    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);

    mp_limb_t tmp[2*NUM_LIMBS+1];
    assert(a->limbs + b->limbs <= 2*NUM_LIMBS+1);
    if ((a->limbs==1 && a->data[0]==0) || (b->limbs==1 && b->data[0]==0)) {
        r->limbs = 1;
        r->neg = 0;
        r->data[0] = 0;
        return;
    }
    if (a->limbs >= b->limbs)
        mpn_mul(tmp, a->data, a->limbs, b->data, b->limbs);
    else
        mpn_mul(tmp, b->data, b->limbs, a->data, a->limbs);
    r->limbs = a->limbs + b->limbs;
    if (r->limbs > 1 && tmp[r->limbs - 1]==0) r->limbs--;
    assert(r->limbs <= 2*NUM_LIMBS);
    mpn_copyi(r->data, tmp, r->limbs);
    r->neg = a->neg ^ b->neg;

    secp256k1_num_sanity(&a2);
    secp256k1_num_sanity(&b2);

#ifdef VERIFY
    secp256k1_num_t c;
    secp256k1_num_div(&c, r, &b2);
    assert(secp256k1_num_cmp(&a2, &c) == 0);
    secp256k1_num_div(&c, r, &a2);
    assert(secp256k1_num_cmp(&b2, &c) == 0);
#endif
}

void static secp256k1_num_div(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b) {
    secp256k1_num_sanity(a);
    secp256k1_num_sanity(b);
    if (b->limbs > a->limbs) {
        r->limbs = 1;
        r->data[0] = 0;
        r->neg = 0;
        return;
    }

    mp_limb_t quo[2*NUM_LIMBS+1];
    mp_limb_t rem[2*NUM_LIMBS+1];
    mpn_tdiv_qr(quo, rem, 0, a->data, a->limbs, b->data, b->limbs);
    mpn_copyi(r->data, quo, a->limbs - b->limbs + 1);
    r->limbs = a->limbs - b->limbs + 1;
    while (r->limbs > 1 && r->data[r->limbs - 1]==0) r->limbs--;
    r->neg = a->neg ^ b->neg;
}

void static secp256k1_num_mod_mul(secp256k1_num_t *r, const secp256k1_num_t *a, const secp256k1_num_t *b, const secp256k1_num_t *m) {
    secp256k1_num_t tmp;
    secp256k1_num_mul(&tmp, a, b);
    secp256k1_num_mod(r, &tmp, m);
}


int static secp256k1_num_shift(secp256k1_num_t *r, int bits) {
    assert(bits <= GMP_NUMB_BITS);
    mp_limb_t ret = mpn_rshift(r->data, r->data, r->limbs, bits);
    if (r->limbs>1 && r->data[r->limbs-1]==0) r->limbs--;
    ret >>= (GMP_NUMB_BITS - bits);
    return ret;
}

int static secp256k1_num_get_bit(const secp256k1_num_t *a, int pos) {
    return (a->limbs*GMP_NUMB_BITS > pos) && ((a->data[pos/GMP_NUMB_BITS] >> (pos % GMP_NUMB_BITS)) & 1);
}

void static secp256k1_num_inc(secp256k1_num_t *r) {
    mp_limb_t ret = mpn_add_1(r->data, r->data, r->limbs, (mp_limb_t)1);
    if (ret) {
        assert(r->limbs < 2*NUM_LIMBS);
        r->data[r->limbs++] = ret;
    }
}

void static secp256k1_num_set_hex(secp256k1_num_t *r, const char *a, int alen) {
    static const unsigned char cvt[256] = {
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 1, 2, 3, 4, 5, 6,7,8,9,0,0,0,0,0,0,
        0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0,
        0, 0, 0, 0, 0, 0, 0,0,0,0,0,0,0,0,0,0
    };
    char num[257] = {};
    for (int i=0; i<alen; i++) {
        num[i] = cvt[a[i]];
    }
    r->limbs = mpn_set_str(r->data, num, alen, 16);
    while (r->limbs > 1 && r->data[r->limbs-1] == 0) r->limbs--;
}

void static secp256k1_num_get_hex(char *r, int rlen, const secp256k1_num_t *a) {
    static const unsigned char cvt[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    unsigned char *tmp = malloc(257);
    mp_size_t len = mpn_get_str(tmp, 16, (mp_limb_t*)a->data, a->limbs);
    assert(len <= rlen);
    for (int i=0; i<len; i++) {
        assert(rlen-len+i >= 0);
        assert(rlen-len+i < rlen);
        assert(tmp[i] >= 0);
        assert(tmp[i] < 16);
        r[rlen-len+i] = cvt[tmp[i]];
    }
    for (int i=0; i<rlen-len; i++) {
        assert(i >= 0);
        assert(i < rlen);
        r[i] = cvt[0];
    }
    free(tmp);
}

void static secp256k1_num_split(secp256k1_num_t *rl, secp256k1_num_t *rh, const secp256k1_num_t *a, int bits) {
    assert(bits > 0);
    rh->neg = a->neg;
    if (bits >= a->limbs * GMP_NUMB_BITS) {
        *rl = *a;
        rh->limbs = 1;
        rh->data[0] = 0;
        return;
    }
    rl->limbs = 0;
    rl->neg = a->neg;
    int left = bits;
    while (left >= GMP_NUMB_BITS) {
        rl->data[rl->limbs] = a->data[rl->limbs];
        rl->limbs++;
        left -= GMP_NUMB_BITS;
    }
    if (left == 0) {
        mpn_copyi(rh->data, a->data + rl->limbs, a->limbs - rl->limbs);
        rh->limbs = a->limbs - rl->limbs;
    } else {
        mpn_rshift(rh->data, a->data + rl->limbs, a->limbs - rl->limbs, left);
        rh->limbs = a->limbs - rl->limbs;
        while (rh->limbs>1 && rh->data[rh->limbs-1]==0) rh->limbs--;
    }
    if (left > 0) {
        rl->data[rl->limbs] = a->data[rl->limbs] & ((((mp_limb_t)1) << left) - 1);
        rl->limbs++;
    }
    while (rl->limbs>1 && rl->data[rl->limbs-1]==0) rl->limbs--;
}

void static secp256k1_num_negate(secp256k1_num_t *r) {
    r->neg ^= 1;
}

void static secp256k1_num_set_rand(secp256k1_num_t *r, const secp256k1_num_t *a) {
    mpn_random(r->data, a->limbs);
    r->limbs = a->limbs;
    r->neg = 0;
    secp256k1_num_mod(r, r, a);
}

#endif
