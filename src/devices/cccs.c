/*
 * cccs.c - Current-Controlled Current Source (F) device model
 * Uses MNA - controlled by current through a voltage source
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>

static int cccs_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* CCCS load: I_out = beta * I_control */
static int cccs_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    int n1 = dev->n1;  /* Positive output node */
    int n2 = dev->n2;  /* Negative output node */
    int vctrl = dev->n3;  /* Controlling voltage source branch current eq */
    double beta = dev->value;  /* Current gain */
    
    /* CCCS stamp:
     * 
     * Output current: I_out = beta * I_control
     * 
     * Matrix contributions:
     * n1, vctrl: +beta
     * n2, vctrl: -beta
     */
    
    if (n1 >= 0)
        sparse_add_element(mat, n1, vctrl, beta);
    if (n2 >= 0)
        sparse_add_element(mat, n2, vctrl, -beta);
    
    return OK;
}

static int cccs_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    return cccs_load(dev, ckt, mat);
}

static int cccs_update(Device *dev, Circuit *ckt)
{
    return OK;
}

static const DeviceOps cccs_ops = {
    .name = "F",
    .type = DEV_CCCS,
    .setup = cccs_setup,
    .load = cccs_load,
    .ac_load = cccs_ac_load,
    .update = cccs_update,
    .nonlinear = NULL
};

const DeviceOps *cccs_get_ops(void)
{
    return &cccs_ops;
}
