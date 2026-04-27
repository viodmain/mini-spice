/*
 * dcsweep.c - DC Sweep Analysis
 */
#include "analysis.h"
#include "device.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Initialize DC sweep analysis */
static int dcsweep_init(Analysis *analysis, Circuit *ckt)
{
    printf("\n**** DC Sweep Analysis ****\n");
    printf("Sweeping: %s from %g to %g, step %g\n",
           analysis->params.src_name,
           analysis->params.start,
           analysis->params.stop,
           analysis->params.step);
    return OK;
}

/* Run DC sweep analysis */
static int dcsweep_run(Analysis *analysis, Circuit *ckt)
{
    int iter;
    int converged;
    double val;
    int num_points = 0;
    double *v_data = NULL;
    int v_idx = -1;
    SparseMatrix *mat;
    double *x, *b;
    
    /* Find the source to sweep */
    Device *sweep_dev = NULL;
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (strcmp(dev->name, analysis->params.src_name) == 0) {
            sweep_dev = dev;
            break;
        }
    }
    
    if (sweep_dev == NULL) {
        fprintf(stderr, "Error: source %s not found\n", analysis->params.src_name);
        return E_NOTFOUND;
    }
    
    /* Find a node to record (first non-ground node) */
    Node *record_node = NULL;
    for (Node *node = ckt->nodes; node != NULL; node = node->next) {
        if (!node->is_ground && node->eqnum >= 0) {
            record_node = node;
            break;
        }
    }
    
    /* Create sparse matrix */
    mat = sparse_create(ckt->num_eqns + ckt->num_vsources);
    x = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));
    b = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));
    
    if (!mat || !x || !b) {
        fprintf(stderr, "Error: out of memory\n");
        sparse_free(mat);
        free(x);
        free(b);
        return E_NOMEM;
    }
    
    /* Count number of points */
    int max_points = 10000;
    v_data = (double *)malloc(max_points * sizeof(double));
    
    /* Print header */
    printf("\n%-15s %-15s\n", analysis->params.src_name, 
           record_node ? record_node->name : "V(out)");
    printf("%-15s %-15s\n", "---------------", "---------------");
    
    /* Sweep */
    for (val = analysis->params.start; 
         val <= analysis->params.stop + 1e-15; 
         val += analysis->params.step) {
        
        /* Update source value */
        sweep_dev->value = val;
        
        /* DC sweep: Newton-Raphson at each point */
        converged = 0;
        for (iter = 0; iter < ckt->maxiter; iter++) {
            sparse_clear(mat);
            
            /* Load all devices */
            for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
                const DeviceOps *ops = device_get_ops(dev->type);
                if (ops == NULL)
                    continue;
                
                if (dev->type == DEV_DIODE && ops->nonlinear) {
                    ops->nonlinear(dev, ckt, mat);
                } else if (ops->load) {
                    ops->load(dev, ckt, mat);
                }
            }
            
            /* Add GMIN */
            double gmin = 1e-12;
            for (Node *node = ckt->nodes; node != NULL; node = node->next) {
                if (!node->is_ground && node->eqnum >= 0) {
                    sparse_add_element(mat, node->eqnum, node->eqnum, gmin);
                }
            }
            
            /* Copy RHS */
            for (int i = 0; i < ckt->num_eqns + ckt->num_vsources; i++) {
                double *rhs = sparse_get_rhs(mat, i);
                b[i] = rhs ? *rhs : 0.0;
            }
            
            /* Factor and solve */
            if (sparse_factor(mat, 1e-13) != OK)
                break;
            if (sparse_solve(mat, x, b) != OK)
                break;
            
            /* Check convergence */
            double max_change = 0.0;
            for (int i = 0; i < ckt->num_eqns; i++) {
                double change = fabs(x[i] - ckt->voltage[i]);
                if (change > max_change)
                    max_change = change;
                ckt->voltage[i] = x[i];
            }
            
            if (max_change < ckt->vntol) {
                converged = 1;
                break;
            }
        }
        
        /* Record result */
        if (record_node && record_node->eqnum >= 0) {
            v_data[num_points] = ckt->voltage[record_node->eqnum];
        } else {
            v_data[num_points] = 0.0;
        }
        
        printf("%-15.6e %-15.6e\n", val, v_data[num_points]);
        
        num_points++;
        if (num_points >= max_points)
            break;
    }
    
    printf("\nTotal points: %d\n", num_points);
    
    sparse_free(mat);
    free(x);
    free(b);
    free(v_data);
    
    return OK;
}

/* Cleanup */
static int dcsweep_cleanup(Analysis *analysis, Circuit *ckt)
{
    return OK;
}

/* Analysis operations */
static const AnalysisOps dcsweep_ops = {
    .name = "dc sweep",
    .type = ANA_DC_SWEEP,
    .init = dcsweep_init,
    .run = dcsweep_run,
    .cleanup = dcsweep_cleanup
};

const AnalysisOps *dcsweep_get_ops(void)
{
    return &dcsweep_ops;
}
