/*
 * vcvs.c - Voltage-Controlled Voltage Source (E) device model
 * Uses MNA - adds branch current variable
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>

static int vcvs_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* VCVS load: V_out = mu * (V_n1 - V_n2) */
static int vcvs_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    int n1 = dev->n1;  /* Positive input node */
    int n2 = dev->n2;  /* Negative input node */
    int n3 = dev->n3;  /* Positive output node */
    int n4 = dev->n4;  /* Negative output node */
    int ib = dev->n5;  /* Branch current equation (stored in n5) */
    double mu = dev->value;  /* Voltage gain */
    
    /* VCVS MNA stamp:
     * 
     *     [ n1  n2  n3  n4  ib ]
     * n3  [  0   0   0   0   1 ]
     * n4  [  0   0   0   0  -1 ]
     * ib  [ mu -mu   1  -1   0 ]
     * 
     * Constraint: V_n3 - V_n4 = mu * (V_n1 - V_n2)
     */
    
    if (n3 >= 0) {
        sparse_add_element(mat, n3, ib, 1.0);
        sparse_add_element(mat, ib, n3, 1.0);
    }
    if (n4 >= 0) {
        sparse_add_element(mat, n4, ib, -1.0);
        sparse_add_element(mat, ib, n4, -1.0);
    }
    if (n1 >= 0)
        sparse_add_element(mat, ib, n1, mu);
    if (n2 >= 0)
        sparse_add_element(mat, ib, n2, -mu);
    
    return OK;
}

static int vcvs_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    return vcvs_load(dev, ckt, mat);
}

static int vcvs_update(Device *dev, Circuit *ckt)
{
    return OK;
}

static const DeviceOps vcvs_ops = {
    .name = "E",
    .type = DEV_VCVS,
    .setup = vcvs_setup,
    .load = vcvs_load,
    .ac_load = vcvs_ac_load,
    .update = vcvs_update,
    .nonlinear = NULL
};

const DeviceOps *vcvs_get_ops(void)
{
    return &vcvs_ops;
}
