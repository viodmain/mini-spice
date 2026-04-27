/*
 * vccs.c - Voltage-Controlled Current Source (G) device model
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>

static int vccs_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* VCCS load: I_out = g_m * (V_n1 - V_n2) */
static int vccs_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    int n1 = dev->n1;  /* Positive input node */
    int n2 = dev->n2;  /* Negative input node */
    int n3 = dev->n3;  /* Positive output node */
    int n4 = dev->n4;  /* Negative output node */
    double gm = dev->value;  /* Transconductance */
    
    /* VCCS stamp:
     * 
     * Output current: I = gm * (V_n1 - V_n2)
     * 
     * Matrix contributions:
     * n3,n1: +gm    n3,n2: -gm
     * n4,n1: -gm    n4,n2: +gm
     */
    
    if (n3 >= 0 && n1 >= 0)
        sparse_add_element(mat, n3, n1, gm);
    if (n3 >= 0 && n2 >= 0)
        sparse_add_element(mat, n3, n2, -gm);
    if (n4 >= 0 && n1 >= 0)
        sparse_add_element(mat, n4, n1, -gm);
    if (n4 >= 0 && n2 >= 0)
        sparse_add_element(mat, n4, n2, gm);
    
    return OK;
}

static int vccs_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    return vccs_load(dev, ckt, mat);
}

static int vccs_update(Device *dev, Circuit *ckt)
{
    return OK;
}

static const DeviceOps vccs_ops = {
    .name = "G",
    .type = DEV_VCCS,
    .setup = vccs_setup,
    .load = vccs_load,
    .ac_load = vccs_ac_load,
    .update = vccs_update,
    .nonlinear = NULL
};

const DeviceOps *vccs_get_ops(void)
{
    return &vccs_ops;
}
