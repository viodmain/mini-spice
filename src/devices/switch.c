/*
 * switch.c - Voltage and current controlled switch models
 * S: Voltage-controlled switch (nodes: +ctrl, -ctrl, +, -)
 * W: Current-controlled switch (nodes: +ctrl, -ctrl, +, -)
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Create default switch model parameters */
static void *switch_create_params(void)
{
    switch_model_t *params = (switch_model_t *)calloc(1, sizeof(switch_model_t));
    if (params == NULL)
        return NULL;

    params->ron = 1.0;        /* On resistance: 1 ohm */
    params->roff = 1e12;      /* Off resistance: 1 Tera-ohm */
    params->vt = 0.5;         /* Threshold voltage */
    params->vh = 0.0;         /* Hysteresis voltage */

    return params;
}

/* Set switch model parameter */
static int switch_set_param(Model *model, const char *param, double value)
{
    switch_model_t *p = (switch_model_t *)model->params;
    if (p == NULL)
        return E_NOTFOUND;

    if (strcmp(param, "ron") == 0) p->ron = value;
    else if (strcmp(param, "roff") == 0) p->roff = value;
    else if (strcmp(param, "vt") == 0) p->vt = value;
    else if (strcmp(param, "vh") == 0) p->vh = value;
    else return E_NOTFOUND;

    return OK;
}

/* Free switch model parameters */
static void switch_free_params(void *params)
{
    free(params);
}

/* Setup: no special setup needed */
static int switch_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Load: linear (switch is either on or off) */
static int switch_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    switch_model_t *params = dev->model ? (switch_model_t *)dev->model->params : NULL;
    if (params == NULL) {
        /* Use defaults */
        static switch_model_t defaults = { 1.0, 1e12, 0.5, 0.0 };
        params = &defaults;
    }

    int nc1 = dev->n1;   /* Control + */
    int nc2 = dev->n2;   /* Control - */
    int n1 = dev->n3;    /* Switch + */
    int n2 = dev->n4;    /* Switch - */

    /* Get control voltage */
    double vctrl = 0.0;
    if (nc1 >= 0 && nc2 >= 0)
        vctrl = ckt->voltage[nc1] - ckt->voltage[nc2];
    else if (nc1 >= 0)
        vctrl = ckt->voltage[nc1];
    else if (nc2 >= 0)
        vctrl = -ckt->voltage[nc2];

    /* Determine switch state */
    double ron = params->ron;
    double roff = params->roff;
    double vt = params->vt;
    double vh = params->vh;

    /* Hysteresis: different thresholds for turning on/off */
    double vth_on = vt + vh / 2.0;
    double vth_off = vt - vh / 2.0;

    double g;  /* Conductance */
    if (vctrl >= vth_on) {
        g = 1.0 / ron;  /* On */
    } else if (vctrl <= vth_off) {
        g = 1.0 / roff;  /* Off */
    } else {
        /* Keep previous state (default to off) */
        g = 1.0 / roff;
    }

    /* MNA stamp for resistor between n1 and n2 */
    if (n1 >= 0 && n2 >= 0) {
        sparse_add_element(mat, n1, n1, g);
        sparse_add_element(mat, n1, n2, -g);
        sparse_add_element(mat, n2, n1, -g);
        sparse_add_element(mat, n2, n2, g);
    } else if (n1 >= 0) {
        sparse_add_element(mat, n1, n1, g);
    } else if (n2 >= 0) {
        sparse_add_element(mat, n2, n2, g);
    }

    return OK;
}

/* AC load: switch is linearized around operating point */
static int switch_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    /* Same as DC load for small-signal */
    return switch_load(dev, ckt, mat);
}

/* Update: no state to update */
static int switch_update(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Nonlinear: same as load (switch is piecewise-linear) */
static int switch_nonlinear(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    return switch_load(dev, ckt, mat);
}

/* Device operations table */
static const DeviceOps switch_ops = {
    .name = "S",
    .type = DEV_SWITCH_VOLTAGE,
    .setup = switch_setup,
    .load = switch_load,
    .ac_load = switch_ac_load,
    .update = switch_update,
    .nonlinear = switch_nonlinear
};

const DeviceOps *switch_get_ops(void)
{
    return &switch_ops;
}

/* Current-controlled switch operations */
static const DeviceOps cswitch_ops = {
    .name = "W",
    .type = DEV_SWITCH_CURRENT,
    .setup = switch_setup,
    .load = switch_load,
    .ac_load = switch_ac_load,
    .update = switch_update,
    .nonlinear = switch_nonlinear
};

const DeviceOps *cswitch_get_ops(void)
{
    return &cswitch_ops;
}

/* Model operations */
void *switch_create_model_params(void)
{
    return switch_create_params();
}

int switch_set_model_param(Model *model, const char *param, double value)
{
    return switch_set_param(model, param, value);
}

void switch_free_model_params(void *params)
{
    switch_free_params(params);
}
