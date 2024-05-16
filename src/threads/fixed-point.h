#define F (1 << 14)

#define int_to_fp(n) (n * F)

#define fp_to_int(x) (x / F)

#define fp_to_int_nearest(x) (x >= 0 ? (x + F / 2) / F : (x - F / 2) / F)

#define add_fp(x, y) (x + y)

#define add_fp_by_int(x, n) (x + n * F)

#define sub_fp(x, y) (x - y)

#define sub_fp_by_int(x, n) (x - n * F)

#define mul_fp(x, y) (((int64_t) x) * y / F)

#define mul_fp_by_int(x, n) (x * n)

#define div_fp(x, y) (((int64_t) x) * F / y)

#define div_fp_by_int(x, n) (x / n)
