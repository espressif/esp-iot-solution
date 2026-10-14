/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "esp_heap_caps.h"
#include "unity.h"
#include "kinematic.h"

#define TEST_MEMORY_LEAK_THRESHOLD (-500)

// Reference posture used by the grasp application as IK seed
static const Joint GRASP_SEED(0.0, DEG2RAD(30.0), DEG2RAD(-36.0), DEG2RAD(65.0), 0.0, 0.0);

static size_t before_free_8bit;
static size_t before_free_32bit;

static float frand(float lo, float hi)
{
    return lo + (hi - lo) * (float)rand() / (float)RAND_MAX;
}

TEST_CASE("forward kinematics matches reference poses", "[kinematic]")
{
    Kinematic kinematic;

    AxisAngle identity = TransformMatrix().to_axis_angle();
    TEST_ASSERT_FALSE(isnan(identity.x));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, identity.theta);

    // Reference values computed with the same DH table on host
    TransformMatrix t_zero = kinematic.forward(Joint());
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.264000f, t_zero(0, 3));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f,      t_zero(1, 3));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.157497f, t_zero(2, 3));

    TransformMatrix t_seed = kinematic.forward(GRASP_SEED);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.153139f, t_seed(0, 3));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f,      t_seed(1, 3));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.039709f, t_seed(2, 3));

    // Bottom row of a homogeneous transform is always [0 0 0 1]
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, t_seed(3, 0));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, t_seed(3, 3));
}

TEST_CASE("IK solves grasp workspace targets", "[kinematic]")
{
    Kinematic kinematic;
    TransformMatrix base = kinematic.forward(GRASP_SEED);

    // Positions the application actually commands (grasp and release heights)
    const float targets[][3] = {
        {0.32f,  0.00f, 0.045f},
        {0.25f,  0.10f, 0.015f},
        {0.25f, -0.10f, 0.015f},
        {0.20f,  0.00f, 0.015f},
    };

    for (const auto &p : targets) {
        TransformMatrix target = base;
        target(0, 3) = p[0];
        target(1, 3) = p[1];
        target(2, 3) = p[2];

        IKResult ik = kinematic.solve_ik(target, GRASP_SEED);
        printf("target (%.3f, %.3f, %.3f): status=%s pos_err=%.5f rot_err=%.5f iter=%d\n",
               p[0], p[1], p[2], ik_status_to_string(ik.status), ik.pos_error, ik.rot_error, ik.iterations);
        TEST_ASSERT_TRUE_MESSAGE((bool)ik, "IK did not converge on an in-workspace grasp target");

        // Reported error must match an actual FK round trip
        TransformMatrix reached = kinematic.forward(ik.joints);
        float dx = reached(0, 3) - p[0];
        float dy = reached(1, 3) - p[1];
        float dz = reached(2, 3) - p[2];
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, ik.pos_error, sqrtf(dx * dx + dy * dy + dz * dz));
        TEST_ASSERT_TRUE(ik.pos_error < 5e-3f);
    }
}

TEST_CASE("IK round trip on random reachable poses", "[kinematic]")
{
    Kinematic kinematic;
    srand(7);

    const int total = 200;
    int ok = 0;
    for (int n = 0; n < total; n++) {
        Joint q;
        for (int i = 0; i < Kinematic::NUM_JOINTS; i++) {
            q[i] = frand(-M_PI / 2, M_PI / 2);
        }
        TransformMatrix target = kinematic.forward(q);

        // Perturbed seed: reachable by construction, seed near the true solution
        Joint seed = q;
        for (int i = 0; i < Kinematic::NUM_JOINTS; i++) {
            seed[i] += frand(-0.3f, 0.3f);
        }

        IKResult ik = kinematic.solve_ik(target, seed);
        if (ik) {
            ok++;
            TEST_ASSERT_TRUE(ik.pos_error < 5e-3f);
        }
    }
    printf("round trip: %d/%d converged\n", ok, total);
    TEST_ASSERT_GREATER_OR_EQUAL(total - 4, ok);
}

TEST_CASE("IK fails cleanly on unreachable target", "[kinematic]")
{
    Kinematic kinematic;
    TransformMatrix target = kinematic.forward(GRASP_SEED);
    target(0, 3) = 1.0f;   // far outside the ~0.63 m reach
    target(1, 3) = 0.0f;
    target(2, 3) = 0.0f;

    IKResult ik = kinematic.solve_ik(target, GRASP_SEED);
    TEST_ASSERT_FALSE((bool)ik);
    TEST_ASSERT_EQUAL(IKStatus::NOT_CONVERGED, ik.status);
    TEST_ASSERT_FALSE(isnan(ik.pos_error));
    TEST_ASSERT_FALSE(isnan(ik.rot_error));
    for (int i = 0; i < Kinematic::NUM_JOINTS; i++) {
        TEST_ASSERT_FALSE(isnan(ik.joints[i]));
    }
}

static void check_leak(size_t before_free, size_t after_free, const char *type)
{
    ssize_t delta = after_free - before_free;
    printf("MALLOC_CAP_%s: Before %u bytes free, After %u bytes free (delta %d)\n", type, before_free, after_free, delta);
    TEST_ASSERT_MESSAGE(delta >= TEST_MEMORY_LEAK_THRESHOLD, "memory leak");
}

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    check_leak(before_free_8bit, after_free_8bit, "8BIT");
    check_leak(before_free_32bit, after_free_32bit, "32BIT");
}

extern "C" void app_main(void)
{
    printf("KINEMATIC TEST\n");
    unity_run_menu();
}
