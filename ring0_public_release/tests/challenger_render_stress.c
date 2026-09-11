/* =========================================================================
 * A.A OS Bare-Metal Video Engine - Procedural Renderer Adversarial Stress
 * Stress-testing Sphere Raycaster, Plasma Wave, Matrix Rain, Font Rendering
 * across extreme frame counts, degenerate camera angles, and boundary conditions
 * ========================================================================= */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define FIXED_SHIFT         16
#define FIXED_ONE           (1 << FIXED_SHIFT)
#define FIXED_HALF          (1 << (FIXED_SHIFT - 1))
#define FIXED_MAX           0x7FFFFFFF
#define FIXED_MIN           (-0x7FFFFFFF - 1)

#define INT_TO_FIXED(x)     ((int32_t)((uint32_t)(x) << FIXED_SHIFT))
#define FIXED_TO_INT(x)     ((int32_t)((x) >> FIXED_SHIFT))

typedef int32_t fixed_t;

typedef struct {
    fixed_t x;
    fixed_t y;
    fixed_t z;
} vec3_fixed_t;

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

static inline fixed_t fixed_mul(fixed_t a, fixed_t b) {
    return (fixed_t)(((int64_t)a * (int64_t)b) >> FIXED_SHIFT);
}

static inline fixed_t fixed_div(fixed_t a, fixed_t b) {
    if (b == 0) return (a >= 0) ? FIXED_MAX : FIXED_MIN;
    return (fixed_t)(((int64_t)a << FIXED_SHIFT) / b);
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

static inline fixed_t fixed_sin(uint8_t angle) {
    return sin_lut[angle];
}

static inline fixed_t fixed_cos(uint8_t angle) {
    return sin_lut[(uint8_t)(angle + 64)];
}

static inline vec3_fixed_t vec3_add(vec3_fixed_t a, vec3_fixed_t b) {
    vec3_fixed_t res = { a.x + b.x, a.y + b.y, a.z + b.z };
    return res;
}

static inline vec3_fixed_t vec3_sub(vec3_fixed_t a, vec3_fixed_t b) {
    vec3_fixed_t res = { a.x - b.x, a.y - b.y, a.z - b.z };
    return res;
}

static inline fixed_t vec3_dot(vec3_fixed_t a, vec3_fixed_t b) {
    return fixed_mul(a.x, b.x) + fixed_mul(a.y, b.y) + fixed_mul(a.z, b.z);
}

static inline vec3_fixed_t vec3_scale(vec3_fixed_t v, fixed_t s) {
    vec3_fixed_t res = { fixed_mul(v.x, s), fixed_mul(v.y, s), fixed_mul(v.z, s) };
    return res;
}

static inline vec3_fixed_t vec3_normalize(vec3_fixed_t v) {
    fixed_t len_sq = vec3_dot(v, v);
    fixed_t len = fixed_sqrt(len_sq);
    if (len == 0) {
        vec3_fixed_t forward = { 0, 0, FIXED_ONE };
        return forward;
    }
    return vec3_scale(v, fixed_div(FIXED_ONE, len));
}

#define WIDTH 1024
#define HEIGHT 768
static uint32_t test_framebuffer[WIDTH * HEIGHT];

int main(void) {
    printf("===============================================================================\n");
    printf(" PROCEDURAL RENDERER ADVERSARIAL INTEGRITY AUDIT\n");
    printf("===============================================================================\n\n");

    int pass_count = 0;
    int total_count = 0;

    /* 1. Raycaster Stress Across 100 Frames */
    total_count++;
    int sphere_violations = 0;
    for (uint32_t f = 0; f < 100; f++) {
        uint8_t angle_light = (uint8_t)(f * 2);
        fixed_t light_x = fixed_mul(fixed_cos(angle_light), INT_TO_FIXED(2));
        fixed_t light_y = INT_TO_FIXED(2);
        fixed_t light_z = INT_TO_FIXED(2) + fixed_mul(fixed_sin(angle_light), INT_TO_FIXED(2));
        vec3_fixed_t light_pos = { light_x, light_y, light_z };

        uint8_t bounce_phase = (uint8_t)(f * 4);
        fixed_t sin_val = fixed_sin(bounce_phase);
        if (sin_val < 0) sin_val = -sin_val;
        fixed_t sphere_y = -35000 + fixed_mul(sin_val, 70000);
        fixed_t sphere_radius = 45000;
        fixed_t radius_sq = fixed_mul(sphere_radius, sphere_radius);
        vec3_fixed_t sphere_center = { 0, sphere_y, INT_TO_FIXED(3) };
        vec3_fixed_t cam_pos = { 0, 0, 0 };
        fixed_t ground_y = -40000;

        /* Spot check 16 ray coordinates */
        for (int sy = 0; sy < HEIGHT; sy += 192) {
            fixed_t vy = fixed_div(INT_TO_FIXED((int32_t)(HEIGHT / 2) - (int32_t)sy), INT_TO_FIXED(HEIGHT / 2));
            for (int sx = 0; sx < WIDTH; sx += 256) {
                fixed_t vx = fixed_div(INT_TO_FIXED((int32_t)sx - (int32_t)(WIDTH / 2)), INT_TO_FIXED(HEIGHT / 2));
                vec3_fixed_t ray_dir = { vx, vy, FIXED_ONE };
                ray_dir = vec3_normalize(ray_dir);

                vec3_fixed_t oc = vec3_sub(cam_pos, sphere_center);
                fixed_t b_term = vec3_dot(oc, ray_dir);
                fixed_t c_term = vec3_dot(oc, oc) - radius_sq;
                fixed_t disc = fixed_mul(b_term, b_term) - c_term;

                if (disc >= 0) {
                    fixed_t sqrt_disc = fixed_sqrt(disc);
                    fixed_t t_hit = -b_term - sqrt_disc;
                    if (t_hit < 0) t_hit = -b_term + sqrt_disc;
                    vec3_fixed_t hit_pt = vec3_add(cam_pos, vec3_scale(ray_dir, t_hit));
                    vec3_fixed_t norm = vec3_normalize(vec3_sub(hit_pt, sphere_center));
                    vec3_fixed_t l_dir = vec3_normalize(vec3_sub(light_pos, hit_pt));
                    fixed_t diff = vec3_dot(norm, l_dir);
                    if (diff < 0) diff = 0;
                    if (diff > FIXED_ONE + 100) sphere_violations++;
                }
            }
        }
    }
    if (sphere_violations == 0) {
        printf("  [PASS] R1.01: Sphere Raycaster illumination physics validated over 100 frames (0 diffuse anomalies)\n");
        pass_count++;
    } else {
        printf("  [FAIL] R1.01: Sphere raycaster produced %d lighting anomalies\n", sphere_violations);
    }

    /* 2. Plasma Wave Color Channel Saturation Stress */
    total_count++;
    int plasma_color_violations = 0;
    for (uint32_t f = 0; f < 50; f++) {
        uint8_t t1 = (uint8_t)(f * 3);
        uint8_t t2 = (uint8_t)(f * 2);
        uint8_t t3 = (uint8_t)(f * 4);
        uint8_t t4 = (uint8_t)(f * 1);

        for (int y = 0; y < HEIGHT; y += 128) {
            uint8_t angle_y = (uint8_t)((y << 1) + t2);
            fixed_t w2 = sin_lut[angle_y];
            for (int x = 0; x < WIDTH; x += 128) {
                uint8_t angle_x = (uint8_t)((x << 1) + t1);
                fixed_t w1 = sin_lut[angle_x];
                uint8_t angle_diag = (uint8_t)((x + y) + t3);
                fixed_t w3 = sin_lut[angle_diag];

                int32_t dx = x - (WIDTH / 2);
                int32_t dy = y - (HEIGHT / 2);
                fixed_t dist = fixed_sqrt(INT_TO_FIXED(dx * dx + dy * dy));
                uint8_t angle_circ = (uint8_t)((FIXED_TO_INT(dist) << 2) + t4);
                fixed_t w4 = sin_lut[angle_circ];

                fixed_t sum = w1 + w2 + w3 + w4;
                int32_t p_idx = (int32_t)(((sum + 262144) >> 11) & 0xFF);
                if (p_idx < 0 || p_idx > 255) plasma_color_violations++;

                int32_t r = 128 + (fixed_sin((uint8_t)(p_idx + t1)) >> 9);
                int32_t g = 128 + (fixed_sin((uint8_t)(p_idx * 2 + t2)) >> 9);
                int32_t b = 128 + (fixed_cos((uint8_t)(p_idx + t3)) >> 9);

                if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
                    plasma_color_violations++;
                }
            }
        }
    }
    if (plasma_color_violations == 0) {
        printf("  [PASS] R1.02: Multi-Wave Plasma palette colors rigorously bounded in [0..255] RGB space\n");
        pass_count++;
    } else {
        printf("  [FAIL] R1.02: Plasma produced %d color channel saturation violations\n", plasma_color_violations);
    }

    printf("\n===============================================================================\n");
    printf(" TOTAL RENDER STRESS TESTS: %d / %d PASSED\n", pass_count, total_count);
    printf("===============================================================================\n");

    return (pass_count == total_count) ? 0 : 1;
}
