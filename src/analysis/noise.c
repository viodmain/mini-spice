/*
 * noise.c - Noise analysis
 * Calculates noise spectral density at output node
 * Syntax: .NOISE V(out) src <start> <stop> <points>
 * Output: output noise density, input-referred noise density
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

/* Initialize noise analysis */
static int noise_init(Analysis *analysis, Circuit *ckt)
{
    printf("\n**** Noise Analysis ****\n");
    printf("Output: %s, Source: %s\n",
           analysis->params.noise_output ? analysis->params.noise_output : "V(out)",
           analysis->params.noise_src ? analysis->params.noise_src : "V1");
    return OK;
}

/* Calculate thermal noise of a resistor: 4kT/R (A^2/Hz) */
static double thermal_noise_current(double r, double temp)
{
    double k = 1.380649e-23;  /* Boltzmann constant */
    return 4.0 * k * temp / r;  /* A^2/Hz */
}

/* Calculate shot noise of a diode: 2qI (A^2/Hz) */
static double shot_noise_current(double i, double temp)
{
    double q = 1.602176634e-19;  /* Electron charge */
    return 2.0 * q * i;  /* A^2/Hz */
}

/* Run noise analysis */
static int noise_run(Analysis *analysis, Circuit *ckt)
{
    /* Noise analysis is performed at each frequency point */
    /* For simplicity, we calculate noise at DC operating point */

    double fstart = analysis->params.noise_start;
    double fstop = analysis->params.noise_stop;
    double npoints = analysis->params.noise_points;
    sweep_type_t sweep = analysis->params.noise_sweep_type;

    if (npoints < 1) npoints = 10;

    /* Find output node */
    Node *out_node = NULL;
    if (analysis->params.noise_output) {
        out_node = circuit_find_node(ckt, analysis->params.noise_output);
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
        fprintf(stderr, "Error: no output node for noise analysis\n");
        return E_NOTFOUND;
    }

    printf("\n%-15s %-20s %-20s\n", "Frequency (Hz)", "Output Noise (V/sqrt(Hz))", "Input Noise (V/sqrt(Hz))");
    printf("%-15s %-20s %-20s\n", "-------------", "-----------------------", "-----------------------");

    /* For each frequency point */
    for (int i = 0; i < npoints; i++) {
        double freq;

        /* Calculate frequency based on sweep type */
        switch (sweep) {
        case SRC_DECADE:
            freq = fstart * pow(10.0, (double)i / (npoints - 1) * log10(fstop / fstart));
            break;
        case SRC_OCTAVE:
            freq = fstart * pow(2.0, (double)i / (npoints - 1) * log2(fstop / fstart));
            break;
        default:
            freq = fstart + (double)i / (npoints - 1) * (fstop - fstart);
            break;
        }

        /* Calculate total output noise */
        double output_noise = 0.0;  /* V^2/Hz */

        /* Sum noise contributions from all resistors */
        for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
            if (dev->type == DEV_RESISTOR) {
                double r = dev->value;
                if (r > 0) {
                    /* Thermal noise: 4kT/R current noise */
                    double inoise = thermal_noise_current(r, ckt->temp);

                    /* Transfer function from resistor to output (simplified) */
                    /* For a resistor between nodes n1 and n2, assume unity gain */
                    output_noise += inoise * r * r;  /* V^2/Hz */
                }
            }
        }

        /* Sum noise from diodes */
        for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
            if (dev->type == DEV_DIODE && dev->model) {
                /* Get diode current from operating point */
                int n1 = dev->n1;
                int n2 = dev->n2;
                double vd = 0.0;
                if (n1 >= 0 && n2 >= 0)
                    vd = ckt->voltage[n1] - ckt->voltage[n2];
                else if (n1 >= 0)
                    vd = ckt->voltage[n1];

                /* Diode current (simplified) */
                diode_model_t *dparams = (diode_model_t *)dev->model->params;
                if (dparams) {
                    double vt = 8.617e-5 * ckt->temp;
                    double id = dparams->is * (exp(vd / (dparams->n * vt)) - 1.0);
                    if (id < 0) id = 0;

                    /* Shot noise */
                    double inoise = shot_noise_current(id, ckt->temp);
                    output_noise += inoise * 1000.0;  /* Assume 1k ohm load */
                }
            }
        }

        /* Output noise density (V/sqrt(Hz)) */
        double out_noise_density = sqrt(output_noise);

        /* Input noise density (referred to input source) */
        /* Simplified: assume unity gain */
        double in_noise_density = out_noise_density;

        printf("%-15.6g %-20.6e %-20.6e\n", freq, out_noise_density, in_noise_density);
    }

    return OK;
}

/* Cleanup */
static int noise_cleanup(Analysis *analysis, Circuit *ckt)
{
    return OK;
}

/* Analysis operations */
static const AnalysisOps noise_ops = {
    .name = "noise",
    .type = ANA_NOISE,
    .init = noise_init,
    .run = noise_run,
    .cleanup = noise_cleanup
};

const AnalysisOps *noise_get_ops(void)
{
    return &noise_ops;
}
