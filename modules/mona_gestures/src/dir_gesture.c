// SPDX-License-Identifier: MIT
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zmk/event_manager.h>
#include <zmk/behaviors.h>
#include <zmk/input/input_processor.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define DT_DRV_COMPAT mona_dir_gestures

#define BINDING_FROM_PROP(node_id, prop) \
    ZMK_BEHAVIOR_DT_SPEC_GET_BY_IDX(node_id, prop, 0)

struct dir_g_cfg {
    int32_t threshold;
    int32_t timeout_ms;
    struct zmk_behavior_binding up;
    struct zmk_behavior_binding down;
    struct zmk_behavior_binding left;
    struct zmk_behavior_binding right;
};

struct dir_g_data {
    int32_t sum_x;
    int32_t sum_y;
    int64_t started_ms;
    bool active;
};

static int dir_g_process(const struct device *dev, struct zmk_input_event *ev) {
    const struct dir_g_cfg *cfg = dev->config;
    struct dir_g_data *st = dev->data;

    if (ev->type != INPUT_EV_REL) return 0;

    const int64_t now = k_uptime_get();

    if (!st->active || (now - st->started_ms) > cfg->timeout_ms) {
        st->active = true;
        st->started_ms = now;
        st->sum_x = 0;
        st->sum_y = 0;
    }

    if (ev->code == INPUT_REL_X) st->sum_x += ev->value;
    if (ev->code == INPUT_REL_Y) st->sum_y += ev->value;

    const int32_t ax = ABS(st->sum_x);
    const int32_t ay = ABS(st->sum_y);

    if (ax < cfg->threshold && ay < cfg->threshold) return 0;

    const struct zmk_behavior_binding *b = NULL;
    if (ax >= ay) {
        b = (st->sum_x > 0) ? &cfg->right : &cfg->left;
    } else {
        b = (st->sum_y > 0) ? &cfg->down : &cfg->up;
    }

    if (b && b->behavior) {
        behavior_invoke_binding(*b, true,  0);
        behavior_invoke_binding(*b, false, 0);
    }

    st->active = false;
    return 0;
}

static const struct zmk_input_processor_driver_api api = {
    .process = dir_g_process,
};

#define DIR_G_INSTANCE(inst)                                                              \
    static const struct dir_g_cfg cfg_##inst = {                                          \
        .threshold = DT_INST_PROP(inst, threshold),                                       \
        .timeout_ms = DT_INST_PROP(inst, timeout_ms),                                     \
        .up    = BINDING_FROM_PROP(DT_DRV_INST(inst), bindings_up),                       \
        .down  = BINDING_FROM_PROP(DT_DRV_INST(inst), bindings_down),                     \
        .left  = BINDING_FROM_PROP(DT_DRV_INST(inst), bindings_left),                     \
        .right = BINDING_FROM_PROP(DT_DRV_INST(inst), bindings_right),                    \
    };                                                                                    \
    static struct dir_g_data data_##inst;                                                 \
    DEVICE_DT_INST_DEFINE(inst, NULL, NULL, &data_##inst, &cfg_##inst,                    \
                          APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(DIR_G_INSTANCE)
