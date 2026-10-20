/**
 * @file test_c_api.c
 * @brief Pure C (C99/C11) test suite for CPP-BigInt C-ABI wrapper.
 */

#include "numeric/c_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_assert_total = 0;
static int g_assert_passed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    g_assert_total++; \
    if (cond) { \
        g_assert_passed++; \
    } else { \
        printf("[FAIL] %s:%d: %s (%s)\n", __FILE__, __LINE__, msg, #cond); \
    } \
} while (0)

#define ASSERT_EQ_INT(actual, expected, msg) do { \
    g_assert_total++; \
    if ((actual) == (expected)) { \
        g_assert_passed++; \
    } else { \
        printf("[FAIL] %s:%d: %s (expected %d, got %d)\n", __FILE__, __LINE__, msg, (int)(expected), (int)(actual)); \
    } \
} while (0)

#define ASSERT_EQ_STR(actual, expected, msg) do { \
    g_assert_total++; \
    if ((actual) && (expected) && strcmp((actual), (expected)) == 0) { \
        g_assert_passed++; \
    } else { \
        printf("[FAIL] %s:%d: %s (expected \"%s\", got \"%s\")\n", __FILE__, __LINE__, msg, (expected) ? (expected) : "NULL", (actual) ? (actual) : "NULL"); \
    } \
} while (0)

/* ------------------------------------------------------------------------- */
/* 1. Lifecycle & Creation                                                   */
/* ------------------------------------------------------------------------- */
static void test_lifecycle(void) {
    printf(">> Running test_lifecycle...\n");

    bigint_t* a = bigint_create();
    ASSERT_TRUE(a != NULL, "bigint_create returns non-null");
    ASSERT_EQ_INT(bigint_is_zero(a), 1, "default bigint is zero");

    bigint_t* b = bigint_create_i64(-42);
    ASSERT_TRUE(b != NULL, "bigint_create_i64 returns non-null");
    ASSERT_EQ_INT(bigint_is_negative(b), 1, "-42 is negative");
    ASSERT_EQ_INT(bigint_sign(b), -1, "-42 sign is -1");

    bigint_status_t st;
    int64_t val_i64 = bigint_to_i64(b, &st);
    ASSERT_EQ_INT(st, BIGINT_OK, "bigint_to_i64 succeeds");
    ASSERT_TRUE(val_i64 == -42, "bigint_to_i64 matches -42");

    bigint_t* c = bigint_clone(b);
    ASSERT_TRUE(c != NULL, "bigint_clone returns non-null");
    ASSERT_EQ_INT(bigint_cmp(b, c), 0, "clone equals original");

    bigint_destroy(c);
    bigint_destroy(b);
    bigint_destroy(a);
}

/* ------------------------------------------------------------------------- */
/* 2. Zero-Heap Stack SBO (bigint_sbo256_t and bigint_sbo512_t)             */
/* ------------------------------------------------------------------------- */
static void test_stack_sbo(void) {
    printf(">> Running test_stack_sbo...\n");

    bigint_sbo256_t sbo_a, sbo_b, sbo_c;
    bigint_t* a = bigint_init_sbo256(&sbo_a, 1000);
    bigint_t* b = bigint_init_sbo256(&sbo_b, 250);
    bigint_t* c = bigint_init_sbo256(&sbo_c, 0);

    ASSERT_TRUE(a != NULL && b != NULL && c != NULL, "init_sbo256 returns valid pointers");
    ASSERT_EQ_INT(bigint_is_sbo(a), 1, "a is in SBO mode");
    ASSERT_EQ_INT(bigint_is_sbo(b), 1, "b is in SBO mode");

    ASSERT_EQ_INT(bigint_add(c, a, b), BIGINT_OK, "sbo add succeeds");
    ASSERT_EQ_INT(bigint_is_sbo(c), 1, "c remains in SBO mode");

    bigint_status_t st;
    ASSERT_TRUE(bigint_to_i64(c, &st) == 1250, "1000 + 250 == 1250");
    ASSERT_EQ_INT(st, BIGINT_OK, "status is OK");

    bigint_destroy_sbo256(&sbo_c);
    bigint_destroy_sbo256(&sbo_b);
    bigint_destroy_sbo256(&sbo_a);

    /* Test 512-bit SBO */
    bigint_sbo512_t sbo512;
    bigint_t* d = bigint_init_sbo512_str(&sbo512, "123456789012345678901234567890", 10);
    ASSERT_TRUE(d != NULL, "init_sbo512_str succeeds");
    ASSERT_EQ_INT(bigint_is_sbo(d), 1, "d is in SBO mode");

    char buf[128];
    ASSERT_EQ_INT(bigint_to_str(d, buf, sizeof(buf), 10), BIGINT_OK, "to_str succeeds");
    ASSERT_EQ_STR(buf, "123456789012345678901234567890", "sbo512 str round-trip");

    bigint_destroy_sbo512(&sbo512);
}

/* ------------------------------------------------------------------------- */
/* 3. Basic Arithmetic & In-Place Operations                                 */
/* ------------------------------------------------------------------------- */
static void test_arithmetic(void) {
    printf(">> Running test_arithmetic...\n");

    bigint_t* a = bigint_create_i64(100);
    bigint_t* b = bigint_create_i64(30);
    bigint_t* r = bigint_create();

    /* Add */
    bigint_add(r, a, b);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 130, "100 + 30 == 130");

    /* Sub */
    bigint_sub(r, a, b);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 70, "100 - 30 == 70");

    /* Mul */
    bigint_mul(r, a, b);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 3000, "100 * 30 == 3000");

    /* Div & Mod */
    bigint_div(r, a, b);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 3, "100 / 30 == 3");

    bigint_mod(r, a, b);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 10, "100 % 30 == 10");

    /* Div_Rem */
    bigint_t* q = bigint_create();
    bigint_t* rem = bigint_create();
    bigint_div_rem(q, rem, a, b);
    ASSERT_TRUE(bigint_to_i64(q, NULL) == 3, "div_rem q == 3");
    ASSERT_TRUE(bigint_to_i64(rem, NULL) == 10, "div_rem rem == 10");

    /* In-place arithmetic */
    bigint_add_inplace(a, b); /* a = 130 */
    ASSERT_TRUE(bigint_to_i64(a, NULL) == 130, "100 += 30 -> 130");

    bigint_sub_inplace(a, b); /* a = 100 */
    ASSERT_TRUE(bigint_to_i64(a, NULL) == 100, "130 -= 30 -> 100");

    bigint_mul_inplace(a, b); /* a = 3000 */
    ASSERT_TRUE(bigint_to_i64(a, NULL) == 3000, "100 *= 30 -> 3000");

    /* Neg & Abs */
    bigint_neg(r, a);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == -3000, "neg(3000) == -3000");

    bigint_abs(r, r);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 3000, "abs(-3000) == 3000");

    bigint_destroy(rem);
    bigint_destroy(q);
    bigint_destroy(r);
    bigint_destroy(b);
    bigint_destroy(a);
}

/* ------------------------------------------------------------------------- */
/* 4. Bitwise & Shift Operations                                             */
/* ------------------------------------------------------------------------- */
static void test_bitwise(void) {
    printf(">> Running test_bitwise...\n");

    bigint_t* a = bigint_create_i64(0b1100);
    bigint_t* b = bigint_create_i64(0b1010);
    bigint_t* r = bigint_create();

    bigint_and(r, a, b);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 0b1000, "1100 & 1010 == 1000");

    bigint_or(r, a, b);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 0b1110, "1100 | 1010 == 1110");

    bigint_xor(r, a, b);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 0b0110, "1100 ^ 1010 == 0110");

    bigint_t* one = bigint_create_i64(1);
    bigint_shl(r, one, 10);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 1024, "1 << 10 == 1024");

    bigint_shr(r, r, 5);
    ASSERT_TRUE(bigint_to_i64(r, NULL) == 32, "1024 >> 5 == 32");

    bigint_destroy(one);
    bigint_destroy(r);
    bigint_destroy(b);
    bigint_destroy(a);
}

/* ------------------------------------------------------------------------- */
/* 5. Comparisons                                                            */
/* ------------------------------------------------------------------------- */
static void test_comparisons(void) {
    printf(">> Running test_comparisons...\n");

    bigint_t* a = bigint_create_i64(42);
    bigint_t* b = bigint_create_i64(100);
    bigint_t* c = bigint_create_i64(-5);

    ASSERT_EQ_INT(bigint_cmp(a, b), -1, "42 < 100");
    ASSERT_EQ_INT(bigint_cmp(b, a), 1, "100 > 42");
    ASSERT_EQ_INT(bigint_cmp(a, a), 0, "42 == 42");

    ASSERT_EQ_INT(bigint_cmp(a, c), 1, "42 > -5");
    ASSERT_EQ_INT(bigint_cmp(c, a), -1, "-5 < 42");

    ASSERT_EQ_INT(bigint_cmp_i64(a, 42), 0, "cmp_i64 42 == 42");
    ASSERT_EQ_INT(bigint_cmp_i64(a, 50), -1, "cmp_i64 42 < 50");
    ASSERT_EQ_INT(bigint_cmp_i64(a, 30), 1, "cmp_i64 42 > 30");

    bigint_destroy(c);
    bigint_destroy(b);
    bigint_destroy(a);
}

/* ------------------------------------------------------------------------- */
/* 6. Math Functions (pow, gcd, lcm, isqrt)                                  */
/* ------------------------------------------------------------------------- */
static void test_math(void) {
    printf(">> Running test_math...\n");

    bigint_t* base = bigint_create_i64(2);
    bigint_t* res = bigint_create();

    /* 2^10 = 1024 */
    bigint_pow(res, base, 10);
    ASSERT_TRUE(bigint_to_i64(res, NULL) == 1024, "2^10 == 1024");

    /* gcd(48, 18) = 6 */
    bigint_t* n48 = bigint_create_i64(48);
    bigint_t* n18 = bigint_create_i64(18);
    bigint_gcd(res, n48, n18);
    ASSERT_TRUE(bigint_to_i64(res, NULL) == 6, "gcd(48, 18) == 6");

    /* lcm(48, 18) = 144 */
    bigint_lcm(res, n48, n18);
    ASSERT_TRUE(bigint_to_i64(res, NULL) == 144, "lcm(48, 18) == 144");

    /* isqrt(100) = 10, isqrt(105) = 10 */
    bigint_t* n105 = bigint_create_i64(105);
    bigint_isqrt(res, n105);
    ASSERT_TRUE(bigint_to_i64(res, NULL) == 10, "isqrt(105) == 10");

    bigint_destroy(n105);
    bigint_destroy(n18);
    bigint_destroy(n48);
    bigint_destroy(res);
    bigint_destroy(base);
}

/* ------------------------------------------------------------------------- */
/* 7. String Conversions & Radix Parsing                                     */
/* ------------------------------------------------------------------------- */
static void test_strings(void) {
    printf(">> Running test_strings...\n");

    /* Decimal */
    bigint_t* a = bigint_create_str("9876543210987654321", 10);
    ASSERT_TRUE(a != NULL, "create_str decimal non-null");

    size_t req_len = bigint_to_str_len(a, 10);
    ASSERT_TRUE(req_len == 20, "req_len includes null term (19+1)");

    char buf[64];
    ASSERT_EQ_INT(bigint_to_str(a, buf, sizeof(buf), 10), BIGINT_OK, "to_str succeeds");
    ASSERT_EQ_STR(buf, "9876543210987654321", "decimal string match");

    /* Hex */
    char* hex_str = bigint_to_str_alloc(a, 16);
    ASSERT_TRUE(hex_str != NULL, "hex_str non-null");

    bigint_t* b = bigint_create_str(hex_str, 16);
    ASSERT_TRUE(b != NULL, "create_str from hex non-null");
    ASSERT_EQ_INT(bigint_cmp(a, b), 0, "hex round-trip equals decimal");

    bigint_free_str(hex_str);
    bigint_destroy(b);

    /* Buffer too small test */
    char small_buf[5];
    ASSERT_EQ_INT(bigint_to_str(a, small_buf, sizeof(small_buf), 10), BIGINT_ERR_BUFFER_TOO_SMALL, "buffer too small detected");

    bigint_destroy(a);
}

/* ------------------------------------------------------------------------- */
/* 8. Error Handling                                                         */
/* ------------------------------------------------------------------------- */
static void test_errors(void) {
    printf(">> Running test_errors...\n");

    /* Null pointer */
    ASSERT_EQ_INT(bigint_add(NULL, NULL, NULL), BIGINT_ERR_NULL_PTR, "null ptr returns ERR_NULL_PTR");

    /* Division by zero */
    bigint_t* a = bigint_create_i64(10);
    bigint_t* zero = bigint_create_i64(0);
    bigint_t* r = bigint_create();

    ASSERT_EQ_INT(bigint_div(r, a, zero), BIGINT_ERR_DIV_BY_ZERO, "div by zero returns ERR_DIV_BY_ZERO");
    ASSERT_EQ_INT(bigint_mod(r, a, zero), BIGINT_ERR_DIV_BY_ZERO, "mod by zero returns ERR_DIV_BY_ZERO");

    /* Invalid string */
    bigint_t* invalid = bigint_create_str("123abcXYZ", 10);
    ASSERT_TRUE(invalid == NULL, "invalid decimal string returns NULL");

    bigint_destroy(r);
    bigint_destroy(zero);
    bigint_destroy(a);
}

int main(void) {
    printf("==========================================================\n");
    printf("   CPP-BigInt Pure C-ABI Test Suite (c_api_test)\n");
    printf("   Version: %s\n", bigint_version());
    printf("==========================================================\n");

    test_lifecycle();
    test_stack_sbo();
    test_arithmetic();
    test_bitwise();
    test_comparisons();
    test_math();
    test_strings();
    test_errors();

    printf("\n----------------------------------------------------------\n");
    printf("Results: %d / %d assertions passed.\n", g_assert_passed, g_assert_total);
    printf("----------------------------------------------------------\n");

    if (g_assert_passed == g_assert_total && g_assert_total > 0) {
        printf("[ALL TESTS PASSED]\n");
        return 0;
    } else {
        printf("[TESTS FAILED]\n");
        return 1;
    }
}
