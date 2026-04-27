/*
 * dctran.c - Transient Analysis
 */
#include "analysis.h"
#include "device.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Initialize transient analysis */
static int dctran_init(Analysis *analysis, Circuit *ckt)
{
    printf("\n**** Transient Analysis ****\n");
    printf("Time: %g to %g, step %g\n",
           analysis->params.tstart,
           analysis->params.tstop,
           analysis->params.tstep);
    return OK;
}

/* Update capacitor/inductor models for transient */
static int transient_update_reactives(Circuit *ckt, SparseMatrix *mat, double dt)
{
    /* For capacitors: use trapezoidal integration
     * i = C * dv/dt ≈ C * (v(t) - v(t-dt)) / dt
     * Equivalent circuit: resistor in parallel with current source
     * G_eq = C/dt, I_hist = G_eq * v(t-dt)
     * 
     * For inductors: similar approach
     */
    
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        int eq1 = dev->n1;
        int eq2 = dev->n2;
        
        if (dev->type == DEV_CAPACITOR) {
            double c = dev->value;
            double g = c / dt;  /* Equivalent conductance */
            
            /* Load capacitor as conductance */
            if (eq1 >= 0 && eq2 >= 0) {
                sparse_add_element(mat, eq1, eq1, g);
                sparse_add_element(mat, eq1, eq2, -g);
                sparse_add_element(mat, eq2, eq1, -g);
                sparse_add_element(mat, eq2, eq2, g);
                
                /* History current: I_hist = g * (v1_old - v2_old)
                 * For first step, assume v(t-dt) = 0 (initially uncharged) */
                double v_old = 0.0;
                if (dev->params)
                    v_old = *(double *)dev->params;
                
                double i_hist = g * v_old;
                sparse_add_rhs(mat, eq1, -i_hist);
                sparse_add_rhs(mat, eq2, i_hist);
            } else if (eq1 >= 0) {
                sparse_add_element(mat, eq1, eq1, g);
                
                double v_old = 0.0;
                if (dev->params)
                    v_old = *(double *)dev->params;
                
                double i_hist = g * v_old;
                sparse_add_rhs(mat, eq1, -i_hist);
            } else if (eq2 >= 0) {
                sparse_add_element(mat, eq2, eq2, g);
                
                double v_old = 0.0;
                if (dev->params)
                    v_old = *(double *)dev->params;
                
                double i_hist = g * v_old;
                sparse_add_rhs(mat, eq2, i_hist);
            }
        }
        else if (dev->type == DEV_INDUCTOR) {
            double l = dev->value;
            double g = dt / (2.0 * l);  /* Trapezoidal conductance */
            
            if (eq1 >= 0 && eq2 >= 0) {
                sparse_add_element(mat, eq1, eq1, g);
                sparse_add_element(mat, eq1, eq2, -g);
                sparse_add_element(mat, eq2, eq1, -g);
                sparse_add_element(mat, eq2, eq2, g);
            } else if (eq1 >= 0) {
                sparse_add_element(mat, eq1, eq1, g);
            } else if (eq2 >= 0) {
                sparse_add_element(mat, eq2, eq2, g);
            }
        }
    }
    
    return OK;
}

/* Update capacitor history values after solution */
static int transient_update_cap_history(Circuit *ckt)
{
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (dev->type == DEV_CAPACITOR) {
            int eq1 = dev->n1;
            int eq2 = dev->n2;
            
            double v_new = 0.0;
            if (eq1 >= 0 && eq2 >= 0)
                v_new = ckt->voltage[eq1] - ckt->voltage[eq2];
            else if (eq1 >= 0)
                v_new = ckt->voltage[eq1];
            else if (eq2 >= 0)
                v_new = -ckt->voltage[eq2];
            
            /* Store voltage for next step */
            if (dev->params == NULL) {
                dev->params = malloc(sizeof(double));
            }
            if (dev->params) {
                *(double *)dev->params = v_new;
            }
        }
    }
    
    return OK;
}

/* Run transient analysis */
static int dctran_run(Analysis *analysis, Circuit *ckt)
{
    int iter;
    int converged;
    double t;
    double dt = analysis->params.tstep;
    double tstop = analysis->params.tstop;
    int num_points = 0;
    int max_points = 100000;
    double *v_data = NULL;
    double *t_data = NULL;
    SparseMatrix *mat;
    double *x, *b;
    
    /* Find a node to record */
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
    
    v_data = (double *)malloc(max_points * sizeof(double));
    t_data = (double *)malloc(max_points * sizeof(double));
    
    /* First, find DC operating point at t=0 */
    printf("\nFinding initial operating point...\n");
    
    /* For transient, set all capacitor voltages to 0 initially */
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (dev->type == DEV_CAPACITOR) {
            if (dev->params == NULL) {
                dev->params = calloc(1, sizeof(double));
            }
            /* Initial voltage = 0 (uncharged) */
            if (dev->params) {
                *(double *)dev->params = 0.0;
            }
        }
    }
    
    /* Reset node voltages to 0 for transient start */
    for (int i = 0; i < ckt->num_eqns; i++) {
        ckt->voltage[i] = 0.0;
    }
    
    /* Print header */
    printf("\n%-15s %-15s\n", "Time (s)", record_node ? record_node->name : "V(out)");
    printf("%-15s %-15s\n", "---------------", "---------------");
    
    /* Record initial point */
    if (record_node && record_node->eqnum >= 0) {
        v_data[num_points] = ckt->voltage[record_node->eqnum];
    } else {
        v_data[num_points] = 0.0;
    }
    t_data[num_points] = 0.0;
    num_points++;
    
    /* Time stepping */
    ckt->time = 0.0;
    for (t = dt; t <= tstop + dt * 0.001; t += dt) {
        ckt->time = t;
        converged = 0;
        
        /* Update capacitor history current ONCE per time step (not per iteration) */
        double *cap_hist_current = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));
        for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
            if (dev->type == DEV_CAPACITOR) {
                int eq1 = dev->n1;
                int eq2 = dev->n2;
                double c = dev->value;
                double g = c / dt;
                double v_old = 0.0;
                if (dev->params)
                    v_old = *(double *)dev->params;
                double i_hist = g * v_old;
                
                if (eq1 >= 0 && eq1 <= ckt->num_eqns + ckt->num_vsources)
                    cap_hist_current[eq1] += i_hist;
                if (eq2 >= 0 && eq2 <= ckt->num_eqns + ckt->num_vsources)
                    cap_hist_current[eq2] -= i_hist;
            }
        }
        
        /* Newton-Raphson at each time point */
        for (iter = 0; iter < ckt->maxiter; iter++) {
            sparse_clear(mat);
            
            /* Load all devices */
            for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
                const DeviceOps *ops = device_get_ops(dev->type);
                if (ops == NULL)
                    continue;
                
                if (dev->type == DEV_CAPACITOR || dev->type == DEV_INDUCTOR) {
                    /* Reactive devices: load conductance only (history handled separately) */
                    if (dev->type == DEV_CAPACITOR) {
                        int eq1 = dev->n1;
                        int eq2 = dev->n2;
                        double c = dev->value;
                        double g = c / dt;
                        if (eq1 >= 0 && eq2 >= 0) {
                            sparse_add_element(mat, eq1, eq1, g);
                            sparse_add_element(mat, eq1, eq2, -g);
                            sparse_add_element(mat, eq2, eq1, -g);
                            sparse_add_element(mat, eq2, eq2, g);
                        } else if (eq1 >= 0) {
                            sparse_add_element(mat, eq1, eq1, g);
                        } else if (eq2 >= 0) {
                            sparse_add_element(mat, eq2, eq2, g);
                        }
                    }
                } else if (dev->type == DEV_DIODE && ops->nonlinear) {
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
            
            /* Copy RHS and add capacitor history current */
            for (int i = 0; i < ckt->num_eqns + ckt->num_vsources; i++) {
                double *rhs = sparse_get_rhs(mat, i);
                b[i] = (rhs ? *rhs : 0.0) + cap_hist_current[i];
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
                /* Update capacitor history for next time step */
                transient_update_cap_history(ckt);
                break;
            }
        }
        
        /* Record result */
        if (record_node && record_node->eqnum >= 0) {
            v_data[num_points] = ckt->voltage[record_node->eqnum];
        } else {
            v_data[num_points] = 0.0;
        }
        t_data[num_points] = t;
        
        /* Free history current array */
        free(cap_hist_current);
        
        /* Print first few and last few points for debugging */
        if (num_points < 5 || num_points % 20 == 0 || num_points >= 96) {
            printf("%-15.6e %-15.6e\n", t, v_data[num_points]);
        }
        
        if (!converged) {
            fprintf(stderr, "Warning: transient did not converge at t = %g\n", t);
        }
        
        num_points++;
        if (num_points >= max_points)
            break;
    }
    
    printf("\nTotal points: %d\n", num_points);
    
    sparse_free(mat);
    free(x);
    free(b);
    free(v_data);
    free(t_data);
    
    return OK;
}

/* Cleanup */
static int dctran_cleanup(Analysis *analysis, Circuit *ckt)
{
    return OK;
}

/* Analysis operations */
static const AnalysisOps dctran_ops = {
    .name = "tran",
    .type = ANA_TRANSIENT,
    .init = dctran_init,
    .run = dctran_run,
    .cleanup = dctran_cleanup
};

const AnalysisOps *dctran_get_ops(void)
{
    return &dctran_ops;
}
