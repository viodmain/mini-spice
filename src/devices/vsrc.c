/*
 * vsrc.c - Voltage source with waveform support
 * Supports DC, SIN, PULSE, PWL, EXP waveforms for transient analysis
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Voltage source setup: allocate MNA row/column for branch current */
static int vsrc_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Voltage source load: DC or time-varying */
static int vsrc_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    int n1 = dev->n1;   /* Positive node */
    int n2 = dev->n2;   /* Negative node */
    int ib = dev->n3;   /* Branch current equation */

    /* Get voltage value: use waveform if in transient, else DC value */
    double v = dev->value;  /* DC value */

    /* If transient analysis and waveform is defined, evaluate at current time */
    if (ckt->time > 0 && dev->waveform) {
        v = waveform_eval(dev->waveform, ckt->time);
    }

    /* MNA stamp for voltage source:
     * [ 0 ... 1 ... ] [ V ]   [ v ]
     * [ 1 ... 0 ... ] [ I ] = [   ]
     */
    if (n1 >= 0) {
        sparse_add_element(mat, n1, ib, 1.0);
        sparse_add_element(mat, ib, n1, 1.0);
    }
    if (n2 >= 0) {
        sparse_add_element(mat, n2, ib, -1.0);
        sparse_add_element(mat, ib, n2, -1.0);
    }

    /* RHS: voltage value */
    sparse_add_rhs(mat, ib, v);

    return OK;
}

/* AC load: small-signal AC analysis */
static int vsrc_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    int n1 = dev->n1;
    int n2 = dev->n2;
    int ib = dev->n3;

    /* Use AC magnitude from waveform or value2 */
    double ac_mag = 1.0;
    if (dev->waveform) {
        ac_mag = dev->waveform->ac_mag;
    } else {
        ac_mag = dev->value2;
    }

    /* MNA stamp (same structure as DC, but with AC magnitude) */
    if (n1 >= 0) {
        sparse_add_element(mat, n1, ib, 1.0);
        sparse_add_element(mat, ib, n1, 1.0);
    }
    if (n2 >= 0) {
        sparse_add_element(mat, n2, ib, -1.0);
        sparse_add_element(mat, ib, n2, -1.0);
    }

    /* AC RHS */
    sparse_add_rhs(mat, ib, ac_mag);

    return OK;
}

/* Update: no state to update for voltage source */
static int vsrc_update(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Nonlinear: voltage sources are linear */
static int vsrc_nonlinear(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    return OK;
}

/* Device operations table */
static const DeviceOps vsrc_ops = {
    .name = "V",
    .type = DEV_VSRC,
    .setup = vsrc_setup,
    .load = vsrc_load,
    .ac_load = vsrc_ac_load,
    .update = vsrc_update,
    .nonlinear = vsrc_nonlinear
};

const DeviceOps *vsrc_get_ops(void)
{
    return &vsrc_ops;
}
