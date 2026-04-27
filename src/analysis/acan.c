/*
 * acan.c - AC Analysis
 */
#include "analysis.h"
#include "device.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Initialize AC analysis */
static int acan_init(Analysis *analysis, Circuit *ckt)
{
    printf("\n**** AC Analysis ****\n");
    printf("Frequency sweep: ");
    switch (analysis->params.ac_sweep_type) {
        case SRC_DECADE: printf("decade "); break;
        case SRC_OCTAVE: printf("octave "); break;
        default: printf("linear "); break;
    }
    printf("from %g Hz to %g Hz, %g points/decade\n",
           analysis->params.ac_start,
           analysis->params.ac_stop,
           analysis->params.ac_points);
    return OK;
}

/* Run AC analysis */
static int acan_run(Analysis *analysis, Circuit *ckt)
{
    int i;
    double freq;
    int num_points = 0;
    int max_points = 10000;
    double *mag = NULL;
    double *phase = NULL;
    SparseMatrix *mat;
    double *x, *b;
    
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
    
    mag = (double *)malloc(max_points * sizeof(double));
    phase = (double *)malloc(max_points * sizeof(double));
    
    /* First, find DC operating point */
    printf("\nFinding DC operating point...\n");
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        const DeviceOps *ops = device_get_ops(dev->type);
        if (ops && ops->load) {
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
    for (i = 0; i < ckt->num_eqns + ckt->num_vsources; i++) {
        double *rhs = sparse_get_rhs(mat, i);
        b[i] = rhs ? *rhs : 0.0;
    }
    
    if (sparse_factor(mat, 1e-13) == OK) {
        sparse_solve(mat, x, b);
        for (i = 0; i < ckt->num_eqns; i++)
            ckt->voltage[i] = x[i];
    }
    
    /* Find a node to record */
    Node *record_node = NULL;
    for (Node *node = ckt->nodes; node != NULL; node = node->next) {
        if (!node->is_ground && node->eqnum >= 0) {
            record_node = node;
            break;
        }
    }
    
    /* Print header */
    printf("\n%-15s %-15s %-15s\n", "Frequency (Hz)", "Magnitude (V)", "Phase (deg)");
    printf("%-15s %-15s %-15s\n", "-------------", "-------------", "-----------");
    
    /* Frequency sweep */
    double start = analysis->params.ac_start;
    double stop = analysis->params.ac_stop;
    double points = analysis->params.ac_points;
    
    if (analysis->params.ac_sweep_type == SRC_DECADE) {
        /* Decade sweep */
        double log_start = log10(start);
        double log_stop = log10(stop);
        double log_step = (log_stop - log_start) / (points - 1);
        
        for (int p = 0; p < points; p++) {
            freq = pow(10.0, log_start + p * log_step);
            double omega = 2.0 * M_PI * freq;
            
            /* Clear matrix */
            sparse_clear(mat);
            
            /* Load AC contributions */
            for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
                const DeviceOps *ops = device_get_ops(dev->type);
                if (ops && ops->ac_load) {
                    ops->ac_load(dev, ckt, mat, omega);
                }
            }
            
            /* Copy RHS */
            for (i = 0; i < ckt->num_eqns + ckt->num_vsources; i++) {
                double *rhs = sparse_get_rhs(mat, i);
                b[i] = rhs ? *rhs : 0.0;
            }
            
            /* Solve */
            if (sparse_factor(mat, 1e-13) == OK) {
                sparse_solve(mat, x, b);
                
                /* Record result */
                if (record_node && record_node->eqnum >= 0) {
                    mag[num_points] = fabs(x[record_node->eqnum]);
                    phase[num_points] = 0.0;  /* Simplified: no phase tracking */
                } else {
                    mag[num_points] = 0.0;
                    phase[num_points] = 0.0;
                }
            } else {
                mag[num_points] = 0.0;
                phase[num_points] = 0.0;
            }
            
            printf("%-15.6e %-15.6e %-15.2f\n", freq, mag[num_points], phase[num_points]);
            
            num_points++;
            if (num_points >= max_points)
                break;
        }
    } else {
        /* Linear sweep */
        double step = (stop - start) / (points - 1);
        
        for (int p = 0; p < points; p++) {
            freq = start + p * step;
            double omega = 2.0 * M_PI * freq;
            
            /* Clear matrix */
            sparse_clear(mat);
            
            /* Load AC contributions */
            for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
                const DeviceOps *ops = device_get_ops(dev->type);
                if (ops && ops->ac_load) {
                    ops->ac_load(dev, ckt, mat, omega);
                }
            }
            
            /* Copy RHS */
            for (i = 0; i < ckt->num_eqns + ckt->num_vsources; i++) {
                double *rhs = sparse_get_rhs(mat, i);
                b[i] = rhs ? *rhs : 0.0;
            }
            
            /* Solve */
            if (sparse_factor(mat, 1e-13) == OK) {
                sparse_solve(mat, x, b);
                
                /* Record result */
                if (record_node && record_node->eqnum >= 0) {
                    mag[num_points] = fabs(x[record_node->eqnum]);
                    phase[num_points] = 0.0;
                } else {
                    mag[num_points] = 0.0;
                    phase[num_points] = 0.0;
                }
            } else {
                mag[num_points] = 0.0;
                phase[num_points] = 0.0;
            }
            
            printf("%-15.6e %-15.6e %-15.2f\n", freq, mag[num_points], phase[num_points]);
            
            num_points++;
            if (num_points >= max_points)
                break;
        }
    }
    
    printf("\nTotal points: %d\n", num_points);
    
    sparse_free(mat);
    free(x);
    free(b);
    free(mag);
    free(phase);
    
    return OK;
}

/* Cleanup */
static int acan_cleanup(Analysis *analysis, Circuit *ckt)
{
    return OK;
}

/* Analysis operations */
static const AnalysisOps acan_ops = {
    .name = "ac",
    .type = ANA_AC,
    .init = acan_init,
    .run = acan_run,
    .cleanup = acan_cleanup
};

const AnalysisOps *acan_get_ops(void)
{
    return &acan_ops;
}
