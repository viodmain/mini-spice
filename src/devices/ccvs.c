/*
 * ccvs.c - Current-Controlled Voltage Source (H) device model
 * Uses MNA - controlled by current through a voltage source
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>

static int ccvs_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* CCVS load: V_out = r_m * I_control */
static int ccvs_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    int n1 = dev->n1;  /* Positive output node */
    int n2 = dev->n2;  /* Negative output node */
    int ib_out = dev->n3;  /* Output branch current eq */
    int vctrl = dev->n4;  /* Controlling voltage source branch current eq */
    double rm = dev->value;  /* Transresistance */
    
    /* CCVS MNA stamp:
     * 
     *     [ n1  n2  ib_out  vctrl ]
     * n1  [  0   0    1      0   ]
     * n2  [  0   0   -1      0   ]
     * ib  [  1  -1    0     -rm  ]
     * 
     * Constraint: V_n1 - V_n2 = rm * I_control
     */
    
    if (n1 >= 0) {
        sparse_add_element(mat, n1, ib_out, 1.0);
        sparse_add_element(mat, ib_out, n1, 1.0);
    }
    if (n2 >= 0) {
        sparse_add_element(mat, n2, ib_out, -1.0);
        sparse_add_element(mat, ib_out, n2, -1.0);
    }
    sparse_add_element(mat, ib_out, vctrl, -rm);
    
    return OK;
}

static int ccvs_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    return ccvs_load(dev, ckt, mat);
}

static int ccvs_update(Device *dev, Circuit *ckt)
{
    return OK;
}

static const DeviceOps ccvs_ops = {
    .name = "H",
    .type = DEV_CCVS,
    .setup = ccvs_setup,
    .load = ccvs_load,
    .ac_load = ccvs_ac_load,
    .update = ccvs_update,
    .nonlinear = NULL
};

const DeviceOps *ccvs_get_ops(void)
{
    return &ccvs_ops;
}
