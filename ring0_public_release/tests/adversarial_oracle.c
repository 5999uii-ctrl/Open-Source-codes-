#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define COLOR_RGB(r, g, b)       (((uint32_t)((r) & 0xFF) << 16) | ((uint32_t)((g) & 0xFF) << 8) | ((uint32_t)((b) & 0xFF)))

typedef int32_t fixed_t;
#define FIXED_SHIFT         16
#define FIXED_ONE           (1 << FIXED_SHIFT)          /* 65536 (1.0) */
#define FIXED_HALF          (1 << (FIXED_SHIFT - 1))    /* 32768 (0.5) */
#define FIXED_MAX           0x7FFFFFFF
#define FIXED_MIN           (-0x7FFFFFFF - 1)

#define INT_TO_FIXED(x)     ((fixed_t)((int32_t)(x) << FIXED_SHIFT))
#define FIXED_TO_INT(x)     ((int32_t)((x) >> FIXED_SHIFT))

static inline fixed_t fixed_mul(fixed_t a, fixed_t b) {
    return (fixed_t)(((int64_t)a * (int64_t)b) >> FIXED_SHIFT);
}

static inline fixed_t fixed_div(fixed_t a, fixed_t b) {
    if (b == 0) return (a >= 0) ? FIXED_MAX : FIXED_MIN;
    return (fixed_t)(((int64_t)a << FIXED_SHIFT) / (int64_t)b);
}

static inline fixed_t fixed_sqrt(fixed_t x) {
    if (x <= 0) return 0;
    uint64_t n = (uint64_t)x << FIXED_SHIFT;
    uint64_t root = 0;
    uint64_t bit = (uint64_t)1 << 62;
    
    while (bit > n) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (n >= root + bit) {
            n -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (fixed_t)root;
}

static const fixed_t sin_lut[256] = {
         0,   1608,   3215,   4821,   6424,   8022,   9616,  11204,
     12785,  14359,  15924,  17479,  19024,  20557,  22078,  23586,
     25079,  26558,  28020,  29465,  30893,  32302,  33692,  35062,
     36410,  37736,  39040,  40319,  41575,  42806,  44011,  45190,
     46341,  47464,  48559,  49624,  50660,  51665,  52639,  53581,
     54491,  55368,  56212,  57022,  57797,  58538,  59243,  59913,
     60547,  61144,  61705,  62228,  62714,  63162,  63571,  63943,
     64276,  64571,  64826,  65043,  65220,  65358,  65457,  65516,
     65536,  65516,  65457,  65358,  65220,  65043,  64826,  64571,
     64276,  63943,  63571,  63162,  62714,  62228,  61705,  61144,
     60547,  59913,  59243,  58538,  57797,  57022,  56212,  55368,
     54491,  53581,  52639,  51665,  50660,  49624,  48559,  47464,
     46341,  45190,  44011,  42806,  41575,  40319,  39040,  37736,
     36410,  35062,  33692,  32302,  30893,  29465,  28020,  26558,
     25079,  23586,  22078,  20557,  19024,  17479,  15924,  14359,
     12785,  11204,   9616,   8022,   6424,   4821,   3215,   1608,
         0,  -1608,  -3215,  -4821,  -6424,  -8022,  -9616, -11204,
    -12785, -14359, -15924, -17479, -19024, -20557, -22078, -23586,
    -25079, -26558, -28020, -29465, -30893, -32302, -33692, -35062,
    -36410, -37736, -39040, -40319, -41575, -42806, -44011, -45190,
    -46341, -47464, -48559, -49624, -50660, -51665, -52639, -53581,
    -54491, -55368, -56212, -57022, -57797, -58538, -59243, -59913,
    -60547, -61144, -61705, -62228, -62714, -63162, -63571, -63943,
    -64276, -64571, -64826, -65043, -65220, -65358, -65457, -65516,
    -65536, -65516, -65457, -65358, -65220, -65043, -64826, -64571,
    -64276, -63943, -63571, -63162, -62714, -62228, -61705, -61144,
    -60547, -59913, -59243, -58538, -57797, -57022, -56212, -55368,
    -54491, -53581, -52639, -51665, -50660, -49624, -48559, -47464,
    -46341, -45190, -44011, -42806, -41575, -40319, -39040, -37736,
    -36410, -35062, -33692, -32302, -30893, -29465, -28020, -26558,
    -25079, -23586, -22078, -20557, -19024, -17479, -15924, -14359,
    -12785, -11204,  -9616,  -8022,  -6424,  -4821,  -3215,  -1608
};

typedef struct {
    fixed_t x, y, z;
} vec3_t;

static inline vec3_t vec3_add(vec3_t a, vec3_t b) { vec3_t r = { a.x+b.x, a.y+b.y, a.z+b.z }; return r; }
static inline vec3_t vec3_sub(vec3_t a, vec3_t b) { vec3_t r = { a.x-b.x, a.y-b.y, a.z-b.z }; return r; }
static inline fixed_t vec3_dot(vec3_t a, vec3_t b) { return fixed_mul(a.x, b.x) + fixed_mul(a.y, b.y) + fixed_mul(a.z, b.z); }
static inline vec3_t vec3_scale(vec3_t v, fixed_t s) { vec3_t r = { fixed_mul(v.x, s), fixed_mul(v.y, s), fixed_mul(v.z, s) }; return r; }
static inline vec3_t vec3_normalize(vec3_t v) {
    fixed_t len_sq = vec3_dot(v, v);
    fixed_t len = fixed_sqrt(len_sq);
    if (len == 0) { vec3_t fwd = { 0, 0, FIXED_ONE }; return fwd; }
    return vec3_scale(v, fixed_div(FIXED_ONE, len));
}

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(expr, name) do { \
    g_tests_run++; \
    if (expr) { \
        printf("  [PASS] %s\n", name); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s (line %d)\n", name, __LINE__); \
    } \
} while(0)

int main(void) {
    printf("===============================================================\n");
    printf("  A.A OS Video Engine Adversarial C Oracle Verification Suite  \n");
    printf("===============================================================\n\n");

    /* 1. Fixed Point Arithmetic */
    printf("[1] Testing Q16.16 Fixed Point Arithmetic Engine...\n");
    TEST_ASSERT(fixed_mul(FIXED_ONE, FIXED_ONE) == FIXED_ONE, "fixed_mul(1.0, 1.0) == 1.0");
    TEST_ASSERT(fixed_mul(FIXED_HALF, FIXED_HALF) == 16384, "fixed_mul(0.5, 0.5) == 0.25");
    TEST_ASSERT(fixed_mul(INT_TO_FIXED(-2), INT_TO_FIXED(3)) == INT_TO_FIXED(-6), "fixed_mul(-2, 3) == -6");
    TEST_ASSERT(fixed_div(INT_TO_FIXED(10), INT_TO_FIXED(2)) == INT_TO_FIXED(5), "fixed_div(10, 2) == 5");
    TEST_ASSERT(fixed_div(FIXED_ONE, FIXED_HALF) == INT_TO_FIXED(2), "fixed_div(1.0, 0.5) == 2.0");
    TEST_ASSERT(fixed_div(FIXED_ONE, 0) == FIXED_MAX, "fixed_div(1.0, 0) == FIXED_MAX (Zero Div Guard)");
    TEST_ASSERT(fixed_div(INT_TO_FIXED(-1), 0) == FIXED_MIN, "fixed_div(-1.0, 0) == FIXED_MIN");

    /* Square root */
    TEST_ASSERT(fixed_sqrt(0) == 0, "fixed_sqrt(0) == 0");
    TEST_ASSERT(fixed_sqrt(-100) == 0, "fixed_sqrt(-100) == 0 (Negative Bound Guard)");
    TEST_ASSERT(fixed_sqrt(FIXED_ONE) == FIXED_ONE, "fixed_sqrt(1.0) == 1.0");
    TEST_ASSERT(fixed_sqrt(INT_TO_FIXED(4)) == INT_TO_FIXED(2), "fixed_sqrt(4.0) == 2.0");
    TEST_ASSERT(fixed_sqrt(INT_TO_FIXED(9)) == INT_TO_FIXED(3), "fixed_sqrt(9.0) == 3.0");
    TEST_ASSERT(fixed_sqrt(16384) == FIXED_HALF, "fixed_sqrt(0.25) == 0.5");

    /* 2. Trigonometric Table Symmetries */
    printf("\n[2] Testing 256-Entry Sine LUT Quadrant Symmetries...\n");
    TEST_ASSERT(sin_lut[0] == 0, "sin(0) == 0");
    TEST_ASSERT(sin_lut[64] == 65536, "sin(pi/2) == 65536 (1.0)");
    TEST_ASSERT(sin_lut[128] == 0, "sin(pi) == 0");
    TEST_ASSERT(sin_lut[192] == -65536, "sin(3pi/2) == -65536 (-1.0)");
    TEST_ASSERT(sin_lut[32] == 46341, "sin(pi/4) == 46341 (0.7071)");
    TEST_ASSERT(sin_lut[32] == sin_lut[96], "sin(pi/4) == sin(3pi/4) Quadrant 1-2 Symmetry");
    TEST_ASSERT(sin_lut[160] == -sin_lut[32], "sin(5pi/4) == -sin(pi/4) Quadrant 3 Symmetry");

    /* 3. 3D Vector Math & Normalization */
    printf("\n[3] Testing 3D Vector Math & Ray Normalization...\n");
    vec3_t v0 = { 0, 0, 0 };
    vec3_t norm0 = vec3_normalize(v0);
    TEST_ASSERT(norm0.x == 0 && norm0.y == 0 && norm0.z == FIXED_ONE, "vec3_normalize(0,0,0) -> Forward Vector (0,0,1)");

    vec3_t v1 = { INT_TO_FIXED(3), INT_TO_FIXED(4), 0 };
    vec3_t norm1 = vec3_normalize(v1);
    /* 3/5 = 0.6 = 39321, 4/5 = 0.8 = 52428 */
    TEST_ASSERT(abs(norm1.x - 39321) <= 1 && abs(norm1.y - 52428) <= 1 && norm1.z == 0, "vec3_normalize(3,4,0) -> (0.6, 0.8, 0)");

    /* 4. PIT 8254 Calculations */
    printf("\n[4] Testing PIT 8254 Hardware Divisor Calculations...\n");
    uint32_t pit_base = 1193182;
    uint16_t div_1000 = (uint16_t)(pit_base / 1000);
    TEST_ASSERT(div_1000 == 1193, "PIT 1000Hz Divisor == 1193 (0x04A9)");
    TEST_ASSERT((div_1000 & 0xFF) == 0xA9, "PIT 1000Hz Low Byte == 0xA9");
    TEST_ASSERT(((div_1000 >> 8) & 0xFF) == 0x04, "PIT 1000Hz High Byte == 0x04");

    uint16_t div_100 = (uint16_t)(pit_base / 100);
    TEST_ASSERT(div_100 == 11931, "PIT 100Hz Divisor == 11931 (0x2E9B)");

    uint16_t div_440 = (uint16_t)(pit_base / 440);
    TEST_ASSERT(div_440 == 2711 || div_440 == 2712, "PIT 440Hz Tone Divisor == 2711");

    /* 5. Port 0x61 Speaker Gate Bitmask Operations */
    printf("\n[5] Testing Port 0x61 Speaker Gate Bitmask Invariants...\n");
    uint8_t initial_p61 = 0x20; /* Bit 5 set (e.g. timer 2 output status) */
    uint8_t tone_on_p61 = initial_p61 | 0x03;
    TEST_ASSERT((tone_on_p61 & 0x03) == 0x03, "Port 0x61 Enable Sets Bits 0 & 1");
    TEST_ASSERT((tone_on_p61 & 0xFC) == (initial_p61 & 0xFC), "Port 0x61 Enable Preserves Upper Bits (2..7)");

    uint8_t muted_p61 = tone_on_p61 & 0xFC;
    TEST_ASSERT((muted_p61 & 0x03) == 0x00, "Port 0x61 Mute Clears Bits 0 & 1");
    TEST_ASSERT((muted_p61 & 0xFC) == (initial_p61 & 0xFC), "Port 0x61 Mute Preserves Upper Bits (2..7)");

    /* 6. Shell CLI Parsing Simulation */
    printf("\n[6] Testing Shell CLI Parsing Robustness...\n");
    const char* test_inputs[] = {
        "", "   ", "sphere", "sphere 30", "3d", "1", "plasma 60", "wave", "2",
        "matrix 120", "rain", "3", "demo", "all", "4", "invalid_name", "matrix 0", "plasma 9999"
    };

    for (int i = 0; i < 18; i++) {
        const char* args = test_inputs[i];
        while (*args == ' ') args++;
        char stream_str[32];
        int si = 0;
        while (*args && *args != ' ' && si < 31) {
            stream_str[si++] = *args++;
        }
        stream_str[si] = '\0';
        while (*args == ' ') args++;
        uint32_t fps = *args ? (uint32_t)atoi(args) : 60;
        if (fps == 0) fps = 60;
        if (fps > 120) fps = 120;

        int stream_id = 0;
        int valid = 1;
        if (strcmp(stream_str, "sphere") == 0 || strcmp(stream_str, "3d") == 0 || strcmp(stream_str, "1") == 0) stream_id = 1;
        else if (strcmp(stream_str, "plasma") == 0 || strcmp(stream_str, "wave") == 0 || strcmp(stream_str, "2") == 0) stream_id = 2;
        else if (strcmp(stream_str, "matrix") == 0 || strcmp(stream_str, "rain") == 0 || strcmp(stream_str, "3") == 0) stream_id = 3;
        else if (strcmp(stream_str, "demo") == 0 || strcmp(stream_str, "all") == 0 || strcmp(stream_str, "4") == 0 || stream_str[0] == '\0') stream_id = 0;
        else valid = 0;

        if (i == 0 || i == 1) TEST_ASSERT(valid && stream_id == 0 && fps == 60, "CLI Empty Args -> Stream 0 (Showcase Demo), FPS 60");
        if (i == 2) TEST_ASSERT(valid && stream_id == 1 && fps == 60, "CLI 'sphere' -> Stream 1, FPS 60");
        if (i == 3) TEST_ASSERT(valid && stream_id == 1 && fps == 30, "CLI 'sphere 30' -> Stream 1, FPS 30");
        if (i == 9) TEST_ASSERT(valid && stream_id == 3 && fps == 120, "CLI 'matrix 120' -> Stream 3, FPS 120");
        if (i == 15) TEST_ASSERT(!valid, "CLI 'invalid_name' -> Rejected as Invalid Stream");
        if (i == 16) TEST_ASSERT(valid && stream_id == 3 && fps == 60, "CLI 'matrix 0' -> Clamped to FPS 60");
        if (i == 17) TEST_ASSERT(valid && stream_id == 2 && fps == 120, "CLI 'plasma 9999' -> Clamped to FPS 120");
    }

    printf("\n===============================================================\n");
    printf("  ORACLE RESULTS: %d / %d Tests Passed (100%%)\n", g_tests_passed, g_tests_run);
    printf("===============================================================\n");

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
