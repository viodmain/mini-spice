/*
 * ind.c - Inductor device model
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>

static int ind_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Inductor load: for DC, inductor is short circuit */
static int ind_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    int eq1 = dev->n1;
    int eq2 = dev->n2;
    
    /* For DC, inductor is a short circuit (zero resistance) */
    /* We need to add a small resistance to avoid singular matrix */
    double r = 1e-9;  /* 1 nOhm */
    double g = 1.0 / r;
    
    if (eq1 < 0 && eq2 < 0)
        return OK;
    
    if (eq1 >= 0 && eq2 >= 0) {
        sparse_add_element(mat, eq1, eq1, g);
        sparse_add_element(mat, eq1, eq2, -g);
        sparse_add_element(mat, eq2, eq1, -g);
        sparse_add_element(mat, eq2, eq2, g);
    } else if (eq1 >= 0) {
        sparse_add_element(mat, eq1, eq1, g);
    } else {
        sparse_add_element(mat, eq2, eq2, g);
    }
    
    return OK;
}

/* Inductor AC load: impedance = j*omega*L */
static int ind_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    int eq1 = dev->n1;
    int eq2 = dev->n2;
    double l = dev->value;
    double b;  /* Susceptance = 1/(omega*L) */
    
    if (eq1 < 0 && eq2 < 0)
        return OK;
    
    if (omega == 0.0 || l == 0.0)
        return ind_load(dev, ckt, mat);  /* DC: short circuit */
    
    b = 1.0 / (omega * l);
    
    if (eq1 >= 0 && eq2 >= 0) {
        sparse_add_element(mat, eq1, eq1, b);
        sparse_add_element(mat, eq1, eq2, -b);
        sparse_add_element(mat, eq2, eq1, -b);
        sparse_add_element(mat, eq2, eq2, b);
    } else if (eq1 >= 0) {
        sparse_add_element(mat, eq1, eq1, b);
    } else {
        sparse_add_element(mat, eq2, eq2, b);
    }
    
    return OK;
}

static int ind_update(Device *dev, Circuit *ckt)
{
    return OK;
}

static const DeviceOps ind_ops = {
    .name = "L",
    .type = DEV_INDUCTOR,
    .setup = ind_setup,
    .load = ind_load,
    .ac_load = ind_ac_load,
    .update = ind_update,
    .nonlinear = NULL
};

const DeviceOps *ind_get_ops(void)
{
    return &ind_ops;
}
