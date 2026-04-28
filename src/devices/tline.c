/*
 * tline.c - Lossless transmission line model
 * T: Lossless transmission line (nodes: n1+, n1-, n2+, n2-, delay, Z0)
 * Implements the lossless transmission line using the method of characteristics
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* Maximum history buffer size */
#define MAX_TLINE_HISTORY 10000

/* Transmission line history data */
typedef struct {
    double *i_hist1;    /* History current at port 1 */
    double *i_hist2;    /* History current at port 2 */
    double *v_hist1;    /* History voltage at port 1 */
    double *v_hist2;    /* History voltage at port 2 */
    int head;           /* Circular buffer head */
    int size;           /* Buffer size */
} tline_history_t;

/* Create default transmission line parameters */
static void *tline_create_params(void)
{
    transmission_line_t *params = (transmission_line_t *)calloc(1, sizeof(transmission_line_t));
    if (params == NULL)
        return NULL;

    params->td = 1e-9;    /* 1 ns delay */
    params->z0 = 50.0;    /* 50 ohm characteristic impedance */
    params->f = 0.0;      /* Not used for lossless */
    params->n = 1;        /* Number of segments */

    return params;
}

/* Set transmission line parameter */
static int tline_set_param(Model *model, const char *param, double value)
{
    transmission_line_t *p = (transmission_line_t *)model->params;
    if (p == NULL)
        return E_NOTFOUND;

    if (strcmp(param, "td") == 0) p->td = value;
    else if (strcmp(param, "z0") == 0) p->z0 = value;
    else if (strcmp(param, "f") == 0) p->f = value;
    else if (strcmp(param, "n") == 0) p->n = value;
    else return E_NOTFOUND;

    return OK;
}

/* Free transmission line parameters */
static void tline_free_params(void *params)
{
    free(params);
}

/* Setup: allocate history buffer */
static int tline_setup(Device *dev, Circuit *ckt)
{
    /* Allocate history buffer for transient analysis */
    tline_history_t *hist = (tline_history_t *)calloc(1, sizeof(tline_history_t));
    if (hist == NULL)
        return E_NOMEM;

    hist->size = MAX_TLINE_HISTORY;
    hist->i_hist1 = (double *)calloc(hist->size, sizeof(double));
    hist->i_hist2 = (double *)calloc(hist->size, sizeof(double));
    hist->v_hist1 = (double *)calloc(hist->size, sizeof(double));
    hist->v_hist2 = (double *)calloc(hist->size, sizeof(double));
    hist->head = 0;

    dev->params = hist;
    return OK;
}

/* Load: transmission line in MNA */
static int tline_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    transmission_line_t *params = dev->model ? (transmission_line_t *)dev->model->params : NULL;
    if (params == NULL) {
        /* Use defaults */
        static transmission_line_t defaults = { 1e-9, 50.0, 0.0, 1 };
        params = &defaults;
    }

    tline_history_t *hist = (tline_history_t *)dev->params;
    if (hist == NULL)
        return OK;

    int n1p = dev->n1;   /* Port 1 + */
    int n1m = dev->n2;   /* Port 1 - */
    int n2p = dev->n3;   /* Port 2 + */
    int n2m = dev->n4;   /* Port 2 - */

    double z0 = params->z0;
    double td = params->td;

    /* Characteristic admittance */
    double g0 = 1.0 / z0;

    /* Calculate number of delay steps based on current time step */
    double dt = ckt->time > 0 ? ckt->time : 1e-12;
    int ndelay = (int)(td / dt + 0.5);
    if (ndelay < 1) ndelay = 1;
    if (ndelay >= hist->size) ndelay = hist->size - 1;

    /* Get history values */
    int hist_idx = (hist->head - ndelay + hist->size) % hist->size;
    double i1_hist = hist->i_hist1[hist_idx];
    double i2_hist = hist->i_hist2[hist_idx];

    /* Method of characteristics:
     * At port 1: i1(t) = g0 * v1(t) - i1_hist(t - td)
     * At port 2: i2(t) = g0 * v2(t) - i2_hist(t - td)
     *
     * Where:
     * i1_hist(t - td) = g0 * v2(t - td) + i2(t - td)
     * i2_hist(t - td) = g0 * v1(t - td) + i1(t - td)
     */

    /* MNA stamp for transmission line (equivalent to two current sources) */
    /* Current source at port 1: I = g0 * V1 - i1_hist */
    if (n1p >= 0 && n1m >= 0) {
        sparse_add_element(mat, n1p, n1p, g0);
        sparse_add_element(mat, n1p, n1m, -g0);
        sparse_add_element(mat, n1m, n1p, -g0);
        sparse_add_element(mat, n1m, n1m, g0);
    } else if (n1p >= 0) {
        sparse_add_element(mat, n1p, n1p, g0);
    } else if (n1m >= 0) {
        sparse_add_element(mat, n1m, n1m, g0);
    }

    /* Current source at port 2 */
    if (n2p >= 0 && n2m >= 0) {
        sparse_add_element(mat, n2p, n2p, g0);
        sparse_add_element(mat, n2p, n2m, -g0);
        sparse_add_element(mat, n2m, n2p, -g0);
        sparse_add_element(mat, n2m, n2m, g0);
    } else if (n2p >= 0) {
        sparse_add_element(mat, n2p, n2p, g0);
    } else if (n2m >= 0) {
        sparse_add_element(mat, n2m, n2m, g0);
    }

    /* RHS: history current terms */
    if (n1p >= 0)
        sparse_add_rhs(mat, n1p, -i1_hist);
    if (n1m >= 0)
        sparse_add_rhs(mat, n1m, i1_hist);
    if (n2p >= 0)
        sparse_add_rhs(mat, n2p, -i2_hist);
    if (n2m >= 0)
        sparse_add_rhs(mat, n2m, i2_hist);

    return OK;
}

/* AC load: not supported for transmission lines in AC analysis */
static int tline_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    /* Transmission lines are not typically used in AC small-signal analysis */
    return OK;
}

/* Update: store history values for next time step */
static int tline_update(Device *dev, Circuit *ckt)
{
    tline_history_t *hist = (tline_history_t *)dev->params;
    if (hist == NULL)
        return OK;

    int n1p = dev->n1;
    int n1m = dev->n2;
    int n2p = dev->n3;
    int n2m = dev->n4;

    /* Calculate port voltages and currents */
    double v1 = 0.0, v2 = 0.0;
    if (n1p >= 0 && n1m >= 0)
        v1 = ckt->voltage[n1p] - ckt->voltage[n1m];
    else if (n1p >= 0)
        v1 = ckt->voltage[n1p];
    else if (n1m >= 0)
        v1 = -ckt->voltage[n1m];

    if (n2p >= 0 && n2m >= 0)
        v2 = ckt->voltage[n2p] - ckt->voltage[n2m];
    else if (n2p >= 0)
        v2 = ckt->voltage[n2p];
    else if (n2m >= 0)
        v2 = -ckt->voltage[n2m];

    /* Calculate currents (from adjacent devices or assume from voltages) */
    /* For simplicity, store voltages as history */
    if (hist->head < hist->size) {
        hist->v_hist1[hist->head] = v1;
        hist->v_hist2[hist->head] = v2;
        /* Currents will be calculated from voltages and Z0 */
        transmission_line_t *params = dev->model ? (transmission_line_t *)dev->model->params : NULL;
        double z0 = params ? params->z0 : 50.0;
        hist->i_hist1[hist->head] = v1 / z0;
        hist->i_hist2[hist->head] = v2 / z0;
    }

    hist->head = (hist->head + 1) % hist->size;

    return OK;
}

/* Nonlinear: transmission lines are linear */
static int tline_nonlinear(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    return tline_load(dev, ckt, mat);
}

/* Device operations table */
static const DeviceOps tline_ops = {
    .name = "T",
    .type = DEV_TRANSMISSION_LINE,
    .setup = tline_setup,
    .load = tline_load,
    .ac_load = tline_ac_load,
    .update = tline_update,
    .nonlinear = tline_nonlinear
};

const DeviceOps *tline_get_ops(void)
{
    return &tline_ops;
}

/* Model operations */
void *tline_create_model_params(void)
{
    return tline_create_params();
}

int tline_set_model_param(Model *model, const char *param, double value)
{
    return tline_set_param(model, param, value);
}

void tline_free_model_params(void *params)
{
    tline_free_params(params);
}
