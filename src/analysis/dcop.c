/*
 * dcop.c - DC Operating Point Analysis
 */
#include "analysis.h"
#include "device.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Initialize DC OP analysis */
static int dcop_init(Analysis *analysis, Circuit *ckt)
{
    printf("\n**** DC Operating Point Analysis ****\n");
    return OK;
}

/* Run DC OP analysis */
static int dcop_run(Analysis *analysis, Circuit *ckt)
{
    int i, iter;
    int converged = 0;
    SparseMatrix *mat;
    double *x, *b;
    double max_volt_change;
    
    /* Create sparse matrix */
    mat = sparse_create(ckt->num_eqns + ckt->num_vsources);
    if (mat == NULL) {
        fprintf(stderr, "Error: cannot create sparse matrix\n");
        return E_NOMEM;
    }
    
    x = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));
    b = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));
    
    if (!x || !b) {
        fprintf(stderr, "Error: out of memory\n");
        sparse_free(mat);
        free(x);
        free(b);
        return E_NOMEM;
    }
    
    /* Newton-Raphson iteration */
    for (iter = 0; iter < ckt->maxiter; iter++) {
        /* Clear matrix */
        sparse_clear(mat);
        
        /* Load all devices */
        for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
            const DeviceOps *ops = device_get_ops(dev->type);
            if (ops == NULL)
                continue;
            
            if (dev->type == DEV_DIODE && ops->nonlinear) {
                /* Nonlinear device */
                ops->nonlinear(dev, ckt, mat);
            } else if (ops->load) {
                /* Linear device */
                ops->load(dev, ckt, mat);
            }
        }
        
        /* Add GMIN (minimum conductance to ground) to prevent floating nodes */
        double gmin = 1e-12;
        for (Node *node = ckt->nodes; node != NULL; node = node->next) {
            if (!node->is_ground && node->eqnum >= 0) {
                sparse_add_element(mat, node->eqnum, node->eqnum, gmin);
            }
        }
        
        /* Copy RHS */
        for (i = 0; i < ckt->num_eqns + ckt->num_vsources; i++) {
            double *rhs = sparse_get_rhs(mat, i);
            b[i] = rhs ? *rhs : 0.0;
        }
        
        /* Factor and solve */
        if (sparse_factor(mat, 1e-13) != OK) {
            fprintf(stderr, "Warning: matrix factorization failed at iteration %d\n", iter);
            break;
        }
        
        if (sparse_solve(mat, x, b) != OK) {
            fprintf(stderr, "Warning: matrix solve failed at iteration %d\n", iter);
            break;
        }
        
        /* Check convergence */
        max_volt_change = 0.0;
        for (i = 0; i < ckt->num_eqns; i++) {
            double change = fabs(x[i] - ckt->voltage[i]);
            if (change > max_volt_change)
                max_volt_change = change;
            ckt->voltage[i] = x[i];
        }
        
        if (max_volt_change < ckt->vntol) {
            converged = 1;
            break;
        }
    }
    
    /* Print results */
    if (converged) {
        printf("Converged after %d iterations\n", iter + 1);
    } else {
        printf("WARNING: DC analysis did not converge after %d iterations\n", ckt->maxiter);
        printf("Max voltage change: %e\n", max_volt_change);
    }
    
    printf("\nNode Voltages:\n");
    printf("%-15s %15s\n", "Node", "Voltage (V)");
    printf("%-15s %15s\n", "----", "-----------");
    
    for (Node *node = ckt->nodes; node != NULL; node = node->next) {
        if (node->is_ground) {
            printf("%-15s %15.6e\n", node->name, 0.0);
        } else if (node->eqnum >= 0) {
            printf("%-15s %15.6e\n", node->name, ckt->voltage[node->eqnum]);
        }
    }
    
    /* Print voltage source currents */
    printf("\nVoltage Source Currents:\n");
    printf("%-15s %15s\n", "Source", "Current (A)");
    printf("%-15s %15s\n", "------", "-----------");
    
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (dev->type == DEV_VSRC) {
            int ib = dev->n3;
            double current = x[ib];
            printf("%-15s %15.6e\n", dev->name, current);
        }
    }
    
    sparse_free(mat);
    free(x);
    free(b);
    
    return converged ? OK : E_CONVERGE;
}

/* Cleanup */
static int dcop_cleanup(Analysis *analysis, Circuit *ckt)
{
    return OK;
}

/* Analysis operations */
static const AnalysisOps dcop_ops = {
    .name = "dc op",
    .type = ANA_DC_OP,
    .init = dcop_init,
    .run = dcop_run,
    .cleanup = dcop_cleanup
};

const AnalysisOps *dcop_get_ops(void)
{
    return &dcop_ops;
}
