/*
 * isrc.c - Independent current source device model
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>

static int isrc_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Current source load: inject current into RHS */
static int isrc_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    int eq1 = dev->n1;  /* Positive node (current flows out of) */
    int eq2 = dev->n2;  /* Negative node (current flows into) */
    double i = dev->value;  /* Current value */
    
    /* Current source stamp:
     * Current flows from n1 to n2
     * 
     * RHS contribution:
     * n1: -I
     * n2: +I
     */
    
    if (eq1 >= 0)
        sparse_add_rhs(mat, eq1, -i);
    if (eq2 >= 0)
        sparse_add_rhs(mat, eq2, i);
    
    return OK;
}

/* Current source AC load */
static int isrc_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    int eq1 = dev->n1;
    int eq2 = dev->n2;
    double iac = dev->value2;  /* AC current magnitude */
    
    if (eq1 >= 0)
        sparse_add_rhs(mat, eq1, -iac);
    if (eq2 >= 0)
        sparse_add_rhs(mat, eq2, iac);
    
    return OK;
}

static int isrc_update(Device *dev, Circuit *ckt)
{
    return OK;
}

static const DeviceOps isrc_ops = {
    .name = "I",
    .type = DEV_ISRC,
    .setup = isrc_setup,
    .load = isrc_load,
    .ac_load = isrc_ac_load,
    .update = isrc_update,
    .nonlinear = NULL
};

const DeviceOps *isrc_get_ops(void)
{
    return &isrc_ops;
}
