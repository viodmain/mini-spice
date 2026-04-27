/*
 * res.c - Resistor device model
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Resistor setup: allocate matrix entries */
static int res_setup(Device *dev, Circuit *ckt)
{
    /* Resistor connects two nodes, adds 4 entries to matrix */
    /* No special setup needed - matrix entries allocated on demand */
    return OK;
}

/* Resistor load: load contributions into matrix */
static int res_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    Node *n1, *n2;
    int eq1, eq2;
    double g;  /* Conductance = 1/R */
    
    /* Find nodes */
    n1 = NULL;
    n2 = NULL;
    for (Node *node = ckt->nodes; node != NULL; node = node->next) {
        /* Match by node number stored in device */
        /* For now, use device n1, n2 as node indices */
    }
    
    /* Get equation numbers */
    eq1 = dev->n1;  /* Already converted to eqnum during parsing */
    eq2 = dev->n2;
    
    /* Check for ground */
    if (eq1 < 0 && eq2 < 0) {
        fprintf(stderr, "Warning: resistor %s connected to ground on both ends\n", dev->name);
        return OK;
    }
    
    /* Conductance */
    g = 1.0 / dev->value;
    
    /* Load stamp:
     *   [ eq1  eq2 ]
     * eq1 [  g  -g ]
     * eq2 [ -g   g ]
     */
    if (eq1 >= 0 && eq2 >= 0) {
        sparse_add_element(mat, eq1, eq1, g);
        sparse_add_element(mat, eq1, eq2, -g);
        sparse_add_element(mat, eq2, eq1, -g);
        sparse_add_element(mat, eq2, eq2, g);
    } else if (eq1 >= 0) {
        /* n2 is ground */
        sparse_add_element(mat, eq1, eq1, g);
    } else {
        /* n1 is ground */
        sparse_add_element(mat, eq2, eq2, g);
    }
    
    return OK;
}

/* Resistor AC load */
static int res_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    /* AC analysis uses same conductance as DC */
    return res_load(dev, ckt, mat);
}

/* Resistor update */
static int res_update(Device *dev, Circuit *ckt)
{
    /* Nothing to update for linear resistor */
    return OK;
}

/* Device operations */
static const DeviceOps res_ops = {
    .name = "R",
    .type = DEV_RESISTOR,
    .setup = res_setup,
    .load = res_load,
    .ac_load = res_ac_load,
    .update = res_update,
    .nonlinear = NULL
};

/* Register resistor device */
const DeviceOps *res_get_ops(void)
{
    return &res_ops;
}
