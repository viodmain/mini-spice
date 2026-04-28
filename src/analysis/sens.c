/*
 * sens.c - DC sensitivity analysis
 * Calculates sensitivity of DC operating point to component values
 * Syntax: .SENS V(out) or .SENS I(V1)
 * Output: dV(out)/dR, dV(out)/dC, etc. for all components
 */
#include "analysis.h"
#include "device.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Initialize sensitivity analysis */
static int sens_init(Analysis *analysis, Circuit *ckt)
{
    printf("\n**** DC Sensitivity Analysis ****\n");
    if (analysis->params.sens_output) {
        printf("Output: %s\n", analysis->params.sens_output);
    }
    return OK;
}

/* Run sensitivity analysis */
static int sens_run(Analysis *analysis, Circuit *ckt)
{
    /* First, run DC operating point analysis */
    printf("Computing DC operating point...\n");

    SparseMatrix *mat = sparse_create(ckt->num_eqns + ckt->num_vsources);
    double *x = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));
    double *b = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));

    if (!mat || !x || !b) {
        fprintf(stderr, "Error: out of memory\n");
        sparse_free(mat);
        free(x);
        free(b);
        return E_NOMEM;
    }

    /* Reset voltages */
    for (int i = 0; i < ckt->num_eqns; i++) {
        ckt->voltage[i] = 0.0;
    }

    /* Newton-Raphson iteration for DC OP */
    int converged = 0;
    for (int iter = 0; iter < ckt->maxiter; iter++) {
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
        for (Node *node = ckt->nodes; node != NULL; node = node->next) {
            if (!node->is_ground && node->eqnum >= 0) {
                sparse_add_element(mat, node->eqnum, node->eqnum, ckt->gmin);
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

    if (!converged) {
        fprintf(stderr, "Warning: DC sensitivity analysis did not converge\n");
    }

    /* Find output node */
    Node *out_node = NULL;
    if (analysis->params.sens_output) {
        out_node = circuit_find_node(ckt, analysis->params.sens_output);
    }
    if (out_node == NULL) {
        /* Use first non-ground node */
        for (Node *node = ckt->nodes; node != NULL; node = node->next) {
            if (!node->is_ground && node->eqnum >= 0) {
                out_node = node;
                break;
            }
        }
    }

    if (out_node == NULL) {
        fprintf(stderr, "Error: no output node for sensitivity analysis\n");
        sparse_free(mat);
        free(x);
        free(b);
        return E_NOTFOUND;
    }

    double vout_orig = (out_node->eqnum >= 0) ? ckt->voltage[out_node->eqnum] : 0.0;

    printf("\n%-15s %-15s %-20s\n", "Device", "Value", "Sensitivity");
    printf("%-15s %-15s %-20s\n", "------", "-----", "-----------");

    /* Calculate sensitivity for each resistor */
    double delta = 0.01;  /* 1% perturbation */

    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (dev->type == DEV_RESISTOR) {
            double r_orig = dev->value;
            double r_perturbed = r_orig * (1.0 + delta);

            /* Perturb resistor value */
            dev->value = r_perturbed;

            /* Re-run DC OP with perturbed value */
            for (int i = 0; i < ckt->num_eqns; i++) {
                ckt->voltage[i] = 0.0;
            }

            /* Quick solve (single iteration for sensitivity) */
            sparse_clear(mat);
            for (Device *d = ckt->devices; d != NULL; d = d->next) {
                const DeviceOps *ops = device_get_ops(d->type);
                if (ops == NULL)
                    continue;
                if (ops->load)
                    ops->load(d, ckt, mat);
            }

            for (Node *node = ckt->nodes; node != NULL; node = node->next) {
                if (!node->is_ground && node->eqnum >= 0) {
                    sparse_add_element(mat, node->eqnum, node->eqnum, ckt->gmin);
                }
            }

            for (int i = 0; i < ckt->num_eqns + ckt->num_vsources; i++) {
                double *rhs = sparse_get_rhs(mat, i);
                b[i] = rhs ? *rhs : 0.0;
            }

            if (sparse_factor(mat, 1e-13) == OK) {
                sparse_solve(mat, x, b);

                double vout_pert = (out_node->eqnum >= 0) ? x[out_node->eqnum] : 0.0;

                /* Sensitivity: dV/dR * R/V */
                double sens = (vout_pert - vout_orig) / (r_perturbed - r_orig);
                double rel_sens = sens * r_orig / (vout_orig != 0 ? vout_orig : 1.0);

                printf("%-15s %-15.6g %-20.6e (rel: %.6e)\n",
                       dev->name, r_orig, sens, rel_sens);
            }

            /* Restore original value */
            dev->value = r_orig;
        }
    }

    /* Sensitivity for voltage sources */
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (dev->type == DEV_VSRC) {
            double v_orig = dev->value;
            double v_perturbed = v_orig * (1.0 + delta) + delta * 0.01;

            dev->value = v_perturbed;

            for (int i = 0; i < ckt->num_eqns; i++) {
                ckt->voltage[i] = 0.0;
            }

            sparse_clear(mat);
            for (Device *d = ckt->devices; d != NULL; d = d->next) {
                const DeviceOps *ops = device_get_ops(d->type);
                if (ops == NULL)
                    continue;
                if (ops->load)
                    ops->load(d, ckt, mat);
            }

            for (Node *node = ckt->nodes; node != NULL; node = node->next) {
                if (!node->is_ground && node->eqnum >= 0) {
                    sparse_add_element(mat, node->eqnum, node->eqnum, ckt->gmin);
                }
            }

            for (int i = 0; i < ckt->num_eqns + ckt->num_vsources; i++) {
                double *rhs = sparse_get_rhs(mat, i);
                b[i] = rhs ? *rhs : 0.0;
            }

            if (sparse_factor(mat, 1e-13) == OK) {
                sparse_solve(mat, x, b);

                double vout_pert = (out_node->eqnum >= 0) ? x[out_node->eqnum] : 0.0;

                double sens = (vout_pert - vout_orig) / (v_perturbed - v_orig);
                double rel_sens = sens * v_orig / (vout_orig != 0 ? vout_orig : 1.0);

                printf("%-15s %-15.6g %-20.6e (rel: %.6e)\n",
                       dev->name, v_orig, sens, rel_sens);
            }

            dev->value = v_orig;
        }
    }

    sparse_free(mat);
    free(x);
    free(b);

    return OK;
}

/* Cleanup */
static int sens_cleanup(Analysis *analysis, Circuit *ckt)
{
    return OK;
}

/* Analysis operations */
static const AnalysisOps sens_ops = {
    .name = "sens",
    .type = ANA_SENSITIVITY,
    .init = sens_init,
    .run = sens_run,
    .cleanup = sens_cleanup
};

const AnalysisOps *sens_get_ops(void)
{
    return &sens_ops;
}
