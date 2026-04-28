/*
 * fourier.c - Fourier analysis
 * Performs discrete Fourier transform on transient analysis results
 * Syntax: .FOUR <fundamental_freq> <output_variable>
 * Output: DC component, harmonics (magnitude and phase), THD
 */
#include "analysis.h"
#include "device.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Initialize Fourier analysis */
static int fourier_init(Analysis *analysis, Circuit *ckt)
{
    printf("\n**** Fourier Analysis ****\n");
    printf("Fundamental frequency: %.6g Hz\n", analysis->params.four_freq);
    printf("Number of harmonics: %d\n", analysis->params.four_harmonics);
    return OK;
}

/* Run Fourier analysis on transient data */
static int fourier_run(Analysis *analysis, Circuit *ckt)
{
    /* This analysis requires transient data to be available */
    /* For simplicity, we run a transient analysis first, then compute FFT */

    double f0 = analysis->params.four_freq;  /* Fundamental frequency */
    int nharm = analysis->params.four_harmonics;  /* Number of harmonics */
    if (nharm < 1) nharm = 9;  /* Default: 9 harmonics */

    double T0 = 1.0 / f0;  /* Fundamental period */

    /* Find a node to analyze */
    char *output_name = analysis->params.sens_output;  /* Reuse sens_output field */
    Node *target_node = NULL;
    if (output_name) {
        target_node = circuit_find_node(ckt, output_name);
    }

    if (target_node == NULL) {
        /* Use first non-ground node */
        for (Node *node = ckt->nodes; node != NULL; node = node->next) {
            if (!node->is_ground && node->eqnum >= 0) {
                target_node = node;
                break;
            }
        }
    }

    if (target_node == NULL) {
        fprintf(stderr, "Error: no output node for Fourier analysis\n");
        return E_NOTFOUND;
    }

    /* Run transient analysis to collect one period of data */
    double tstep = T0 / 1000;  /* 1000 points per period */
    int npoints = 1000;

    printf("\nRunning transient analysis for Fourier transform...\n");
    printf("Collecting %d points over one period (%.6g s)\n", npoints, T0);

    /* Allocate storage for transient data */
    double *t_data = (double *)malloc(npoints * sizeof(double));
    double *v_data = (double *)malloc(npoints * sizeof(double));

    if (!t_data || !v_data) {
        fprintf(stderr, "Error: out of memory\n");
        free(t_data);
        free(v_data);
        return E_NOMEM;
    }

    /* Create sparse matrix for transient analysis */
    SparseMatrix *mat = sparse_create(ckt->num_eqns + ckt->num_vsources);
    double *x = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));
    double *b = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));

    if (!mat || !x || !b) {
        fprintf(stderr, "Error: out of memory\n");
        free(t_data);
        free(v_data);
        return E_NOMEM;
    }

    /* Reset capacitor history for fresh transient start */
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (dev->type == DEV_CAPACITOR) {
            if (dev->params == NULL) {
                dev->params = calloc(1, sizeof(double));
            }
            *(double *)dev->params = 0.0;
        }
    }

    /* Reset node voltages */
    for (int i = 0; i < ckt->num_eqns; i++) {
        ckt->voltage[i] = 0.0;
    }

    /* Run transient analysis for one period */
    ckt->time = 0.0;
    for (int i = 0; i < npoints; i++) {
        ckt->time = i * tstep;
        t_data[i] = ckt->time;

        /* Update capacitor history current */
        double *cap_hist = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));
        for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
            if (dev->type == DEV_CAPACITOR) {
                int eq1 = dev->n1;
                int eq2 = dev->n2;
                double c = dev->value;
                double g = c / tstep;
                double v_old = 0.0;
                if (dev->params)
                    v_old = *(double *)dev->params;
                double i_hist = g * v_old;

                if (eq1 >= 0 && eq1 <= ckt->num_eqns + ckt->num_vsources)
                    cap_hist[eq1] += i_hist;
                if (eq2 >= 0 && eq2 <= ckt->num_eqns + ckt->num_vsources)
                    cap_hist[eq2] -= i_hist;
            }
        }

        /* Newton-Raphson iteration */
        int converged = 0;
        for (int iter = 0; iter < ckt->maxiter; iter++) {
            sparse_clear(mat);

            /* Load all devices */
            for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
                const DeviceOps *ops = device_get_ops(dev->type);
                if (ops == NULL)
                    continue;

                if (dev->type == DEV_CAPACITOR) {
                    int eq1 = dev->n1;
                    int eq2 = dev->n2;
                    double c = dev->value;
                    double g = c / tstep;
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
                } else if (dev->type == DEV_DIODE && ops->nonlinear) {
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
            for (int j = 0; j < ckt->num_eqns + ckt->num_vsources; j++) {
                double *rhs = sparse_get_rhs(mat, j);
                b[j] = (rhs ? *rhs : 0.0) + cap_hist[j];
            }

            /* Factor and solve */
            if (sparse_factor(mat, 1e-13) != OK)
                break;
            if (sparse_solve(mat, x, b) != OK)
                break;

            /* Check convergence */
            double max_change = 0.0;
            for (int j = 0; j < ckt->num_eqns; j++) {
                double change = fabs(x[j] - ckt->voltage[j]);
                if (change > max_change)
                    max_change = change;
                ckt->voltage[j] = x[j];
            }

            if (max_change < ckt->vntol) {
                converged = 1;
                /* Update capacitor history */
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
                        if (dev->params)
                            *(double *)dev->params = v_new;
                    }
                }
                break;
            }
        }

        /* Record voltage */
        if (target_node->eqnum >= 0) {
            v_data[i] = ckt->voltage[target_node->eqnum];
        } else {
            v_data[i] = 0.0;
        }

        free(cap_hist);
    }

    /* Compute Discrete Fourier Transform */
    printf("\n**** Fourier Components ****\n");
    printf("%-10s %-15s %-15s %-15s\n", "Harmonic", "Frequency (Hz)", "Magnitude", "Phase (deg)");
    printf("%-10s %-15s %-15s %-15s\n", "--------", "--------------", "--------", "-----------");

    double *mag = (double *)calloc(nharm + 1, sizeof(double));
    double *phase = (double *)calloc(nharm + 1, sizeof(double));

    /* DC component (harmonic 0) */
    double dc_avg = 0.0;
    for (int i = 0; i < npoints; i++) {
        dc_avg += v_data[i];
    }
    dc_avg /= npoints;
    mag[0] = dc_avg;
    phase[0] = 0.0;

    printf("%-10d %-15.6g %-15.6e %-15.6f\n", 0, 0.0, mag[0], phase[0]);

    /* Harmonics */
    double total_harmonic_power = 0.0;
    double fundamental_power = 0.0;

    for (int h = 1; h <= nharm; h++) {
        double f = h * f0;
        double sum_cos = 0.0;
        double sum_sin = 0.0;

        for (int i = 0; i < npoints; i++) {
            double t = t_data[i];
            double angle = 2.0 * M_PI * f * t;
            sum_cos += v_data[i] * cos(angle);
            sum_sin += v_data[i] * sin(angle);
        }

        /* Normalize */
        sum_cos *= 2.0 / npoints;
        sum_sin *= 2.0 / npoints;

        /* Magnitude and phase */
        mag[h] = sqrt(sum_cos * sum_cos + sum_sin * sum_sin);
        phase[h] = atan2(-sum_sin, sum_cos) * 180.0 / M_PI;

        printf("%-10d %-15.6g %-15.6e %-15.6f\n", h, f, mag[h], phase[h]);

        /* Track harmonic power for THD */
        if (h == 1) {
            fundamental_power = mag[h] * mag[h];
        } else {
            total_harmonic_power += mag[h] * mag[h];
        }
    }

    /* Total Harmonic Distortion */
    double thd = 0.0;
    if (fundamental_power > 0) {
        thd = sqrt(total_harmonic_power / fundamental_power) * 100.0;
    }

    printf("\nTotal Harmonic Distortion: %.2f%%\n", thd);

    /* Cleanup */
    sparse_free(mat);
    free(x);
    free(b);
    free(t_data);
    free(v_data);
    free(mag);
    free(phase);

    return OK;
}

/* Cleanup */
static int fourier_cleanup(Analysis *analysis, Circuit *ckt)
{
    return OK;
}

/* Analysis operations */
static const AnalysisOps fourier_ops = {
    .name = "fourier",
    .type = ANA_FOURIER,
    .init = fourier_init,
    .run = fourier_run,
    .cleanup = fourier_cleanup
};

const AnalysisOps *fourier_get_ops(void)
{
    return &fourier_ops;
}
