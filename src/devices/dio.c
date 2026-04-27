/*
 * dio.c - Diode device model
 * Nonlinear device requiring Newton-Raphson iteration
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Diode model parameters */
typedef struct {
    double is;    /* Saturation current (A) */
    double n;     /* Emission coefficient */
    double rs;    /* Ohmic resistance (Ohm) */
    double cjo;   /* Zero-bias junction capacitance (F) */
    double vj;    /* Junction potential (V) */
    double m;     /* Grading coefficient */
    double tt;    /* Transit time (s) */
    double eg;    /* Activation energy (eV) */
    double xti;   /* Temperature exponent */
    double kf;    /* Flicker noise coefficient */
    double af;    /* Flicker noise exponent */
    double fc;    /* Forward bias coefficient */
} diode_params_t;

/* Diode setup */
static int dio_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Diode load: linearized model for Newton-Raphson */
static int dio_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    /* Diode is nonlinear, handled in dio_nonlinear */
    return OK;
}

/* Diode AC load */
static int dio_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    /* For AC, diode is linearized around operating point */
    /* Use small-signal conductance and capacitance */
    diode_params_t *params = dev->model ? (diode_params_t *)dev->model->params : NULL;
    if (params == NULL)
        return OK;
    
    /* Get voltage across diode */
    int n1 = dev->n1;
    int n2 = dev->n2;
    double vd = 0.0;
    if (n1 >= 0 && n2 >= 0)
        vd = ckt->voltage[n1] - ckt->voltage[n2];
    else if (n1 >= 0)
        vd = ckt->voltage[n1];
    else if (n2 >= 0)
        vd = -ckt->voltage[n2];
    
    /* Small-signal conductance: g_d = dI/dV = Is/(n*Vt) * exp(Vd/(n*Vt)) */
    double vt = 8.617e-5 * ckt->temp;  /* Thermal voltage */
    double gd;
    if (vd < 0)
        gd = params->is / (params->n * vt);  /* Reverse bias */
    else
        gd = params->is / (params->n * vt) * exp(vd / (params->n * vt));
    
    /* Junction capacitance */
    double cj = params->cjo / pow(1.0 - vd / params->vj, params->m);
    double b = omega * cj;
    
    /* AC stamp (conductance + susceptance in parallel) */
    if (n1 >= 0 && n2 >= 0) {
        sparse_add_element(mat, n1, n1, gd);
        sparse_add_element(mat, n1, n2, -gd);
        sparse_add_element(mat, n2, n1, -gd);
        sparse_add_element(mat, n2, n2, gd);
        /* Capacitive part */
        sparse_add_element(mat, n1, n1, b);
        sparse_add_element(mat, n1, n2, -b);
        sparse_add_element(mat, n2, n1, -b);
        sparse_add_element(mat, n2, n2, b);
    } else if (n1 >= 0) {
        sparse_add_element(mat, n1, n1, gd + b);
    } else if (n2 >= 0) {
        sparse_add_element(mat, n2, n2, gd + b);
    }
    
    return OK;
}

static int dio_update(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Diode nonlinear: Newton-Raphson iteration */
static int dio_nonlinear(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    diode_params_t *params = dev->model ? (diode_params_t *)dev->model->params : NULL;
    if (params == NULL) {
        /* Use default parameters */
        static diode_params_t defaults = {
            .is = 1e-14,
            .n = 1.0,
            .rs = 0.0,
            .cjo = 0.0,
            .vj = 0.7,
            .m = 0.5,
            .tt = 0.0,
            .eg = 1.11,
            .xti = 3.0,
            .kf = 0.0,
            .af = 1.0,
            .fc = 0.5
        };
        params = &defaults;
    }
    
    int n1 = dev->n1;
    int n2 = dev->n2;
    
    /* Get voltage across diode */
    double vd = 0.0;
    if (n1 >= 0 && n2 >= 0)
        vd = ckt->voltage[n1] - ckt->voltage[n2];
    else if (n1 >= 0)
        vd = ckt->voltage[n1];
    else if (n2 >= 0)
        vd = -ckt->voltage[n2];
    
    /* Limit voltage to prevent overflow */
    double vt = 8.617e-5 * ckt->temp;  /* Thermal voltage at T */
    double vlimit = 20.0 * vt;
    if (vd > vlimit) vd = vlimit;
    if (vd < -vlimit) vd = -vlimit;
    
    /* Diode current: I = Is * (exp(Vd/(n*Vt)) - 1) */
    double id;
    double g;  /* Dynamic conductance */
    
    if (vd >= 0) {
        double exp_arg = vd / (params->n * vt);
        if (exp_arg > 50) exp_arg = 50;  /* Prevent overflow */
        id = params->is * (exp(exp_arg) - 1.0);
        g = params->is / (params->n * vt) * exp(exp_arg);
    } else {
        id = -params->is;  /* Reverse saturation current */
        g = params->is / (params->n * vt);
    }
    
    /* Series resistance effect */
    if (params->rs > 0.0) {
        double rs_eff = params->rs / (1.0 + params->rs * g);
        g = g / (1.0 + params->rs * g);
        
        /* Modified stamp with series resistance */
        if (n1 >= 0 && n2 >= 0) {
            sparse_add_element(mat, n1, n1, g);
            sparse_add_element(mat, n1, n2, -g);
            sparse_add_element(mat, n2, n1, -g);
            sparse_add_element(mat, n2, n2, g);
        } else if (n1 >= 0) {
            sparse_add_element(mat, n1, n1, g);
        } else if (n2 >= 0) {
            sparse_add_element(mat, n2, n2, g);
        }
        
        /* Current source term for Newton-Raphson */
        double irs = id - g * vd;
        if (n1 >= 0)
            sparse_add_rhs(mat, n1, -irs);
        if (n2 >= 0)
            sparse_add_rhs(mat, n2, irs);
    } else {
        /* No series resistance */
        if (n1 >= 0 && n2 >= 0) {
            sparse_add_element(mat, n1, n1, g);
            sparse_add_element(mat, n1, n2, -g);
            sparse_add_element(mat, n2, n1, -g);
            sparse_add_element(mat, n2, n2, g);
        } else if (n1 >= 0) {
            sparse_add_element(mat, n1, n1, g);
        } else if (n2 >= 0) {
            sparse_add_element(mat, n2, n2, g);
        }
        
        /* Current source term */
        double irs = id - g * vd;
        if (n1 >= 0)
            sparse_add_rhs(mat, n1, -irs);
        if (n2 >= 0)
            sparse_add_rhs(mat, n2, irs);
    }
    
    return OK;
}

/* Device operations */
static const DeviceOps dio_ops = {
    .name = "D",
    .type = DEV_DIODE,
    .setup = dio_setup,
    .load = dio_load,
    .ac_load = dio_ac_load,
    .update = dio_update,
    .nonlinear = dio_nonlinear
};

const DeviceOps *dio_get_ops(void)
{
    return &dio_ops;
}

/* Model parameter handling */
void *dio_create_params(void)
{
    diode_params_t *params = (diode_params_t *)calloc(1, sizeof(diode_params_t));
    if (params == NULL)
        return NULL;
    
    /* Default values */
    params->is = 1e-14;
    params->n = 1.0;
    params->rs = 0.0;
    params->cjo = 0.0;
    params->vj = 0.7;
    params->m = 0.5;
    params->tt = 0.0;
    params->eg = 1.11;
    params->xti = 3.0;
    params->kf = 0.0;
    params->af = 1.0;
    params->fc = 0.5;
    
    return params;
}

int dio_set_param(Model *model, const char *param, double value)
{
    diode_params_t *params = (diode_params_t *)model->params;
    if (params == NULL)
        return E_NOTFOUND;
    
    if (strcmp(param, "is") == 0) params->is = value;
    else if (strcmp(param, "n") == 0) params->n = value;
    else if (strcmp(param, "rs") == 0) params->rs = value;
    else if (strcmp(param, "cjo") == 0) params->cjo = value;
    else if (strcmp(param, "vj") == 0) params->vj = value;
    else if (strcmp(param, "m") == 0) params->m = value;
    else if (strcmp(param, "tt") == 0) params->tt = value;
    else if (strcmp(param, "eg") == 0) params->eg = value;
    else if (strcmp(param, "xti") == 0) params->xti = value;
    else if (strcmp(param, "kf") == 0) params->kf = value;
    else if (strcmp(param, "af") == 0) params->af = value;
    else if (strcmp(param, "fc") == 0) params->fc = value;
    else return E_NOTFOUND;
    
    return OK;
}

void dio_free_params(void *params)
{
    free(params);
}
