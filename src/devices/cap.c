/*
 * cap.c - Capacitor device model
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>

static int cap_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Capacitor load: for DC, capacitor is open circuit (no contribution) */
static int cap_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    /* In DC analysis, capacitor is open circuit - no contribution */
    return OK;
}

/* Capacitor AC load: impedance = 1/(j*omega*C) */
static int cap_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    int eq1 = dev->n1;
    int eq2 = dev->n2;
    double c = dev->value;
    double b;  /* Susceptance = omega * C */
    
    if (eq1 < 0 && eq2 < 0)
        return OK;
    
    b = omega * c;
    
    /* AC stamp (imaginary part only for capacitive susceptance):
     *   [ eq1    eq2   ]
     * eq1 [ jb   -jb   ]
     * eq2 [ -jb  jb    ]
     * For complex matrix, we need real and imaginary parts.
     * For simplicity, we store imaginary in a separate structure.
     * Here we use a simplified approach: store jb in the matrix.
     */
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

static int cap_update(Device *dev, Circuit *ckt)
{
    return OK;
}

static const DeviceOps cap_ops = {
    .name = "C",
    .type = DEV_CAPACITOR,
    .setup = cap_setup,
    .load = cap_load,
    .ac_load = cap_ac_load,
    .update = cap_update,
    .nonlinear = NULL
};

const DeviceOps *cap_get_ops(void)
{
    return &cap_ops;
}
