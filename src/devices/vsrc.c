/*
 * vsrc.c - Independent voltage source device model
 * Uses Modified Nodal Analysis (MNA) - adds branch current variable
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Voltage source setup: allocate branch current equation */
static int vsrc_setup(Device *dev, Circuit *ckt)
{
    /* Voltage source adds a branch current variable */
    /* The branch current equation number is stored in device */
    /* For now, we'll handle this in the parser */
    return OK;
}

/* Voltage source load: MNA stamp */
static int vsrc_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    int eq1 = dev->n1;  /* Positive node */
    int eq2 = dev->n2;  /* Negative node */
    int ib = dev->n3;   /* Branch current equation number */
    double v = dev->value;  /* Voltage value */
    
    /* MNA stamp for voltage source:
     * 
     *     [ n1  n2  ib ]
     * n1  [  0   0   1 ] [v_n1]   [  0  ]
     * n2  [  0   0  -1 ] [v_n2] = [  0  ]
     * ib  [  1  -1   0 ] [i_ib]   [  V  ]
     * 
     * For time-varying sources, V depends on time
     */
    
    if (eq1 >= 0) {
        sparse_add_element(mat, eq1, ib, 1.0);
        sparse_add_element(mat, ib, eq1, 1.0);
    }
    if (eq2 >= 0) {
        sparse_add_element(mat, eq2, ib, -1.0);
        sparse_add_element(mat, ib, eq2, -1.0);
    }
    
    /* RHS: voltage value */
    sparse_set_rhs(mat, ib, v);
    
    return OK;
}

/* Voltage source AC load */
static int vsrc_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    int eq1 = dev->n1;
    int eq2 = dev->n2;
    int ib = dev->n3;
    double vac = dev->value2;  /* AC magnitude */
    
    /* AC analysis: voltage source is AC magnitude */
    if (eq1 >= 0) {
        sparse_add_element(mat, eq1, ib, 1.0);
        sparse_add_element(mat, ib, eq1, 1.0);
    }
    if (eq2 >= 0) {
        sparse_add_element(mat, eq2, ib, -1.0);
        sparse_add_element(mat, ib, eq2, -1.0);
    }
    
    /* RHS: AC voltage */
    sparse_set_rhs(mat, ib, vac);
    
    return OK;
}

static int vsrc_update(Device *dev, Circuit *ckt)
{
    /* Update voltage for time-varying sources */
    /* For DC, value is constant */
    return OK;
}

static const DeviceOps vsrc_ops = {
    .name = "V",
    .type = DEV_VSRC,
    .setup = vsrc_setup,
    .load = vsrc_load,
    .ac_load = vsrc_ac_load,
    .update = vsrc_update,
    .nonlinear = NULL
};

const DeviceOps *vsrc_get_ops(void)
{
    return &vsrc_ops;
}
