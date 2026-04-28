/*
 * bjt.c - BJT device model (NPN/PNP)
 * Implements the simple Ebers-Moll model for DC and AC analysis
 * Supports both NPN (Q prefix) and PNP transistors
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* BJT node mapping: 1=Collector, 2=Base, 3=Emitter, 4=Substrate (optional) */

/* Create default BJT model parameters */
static void *bjt_create_params(void)
{
    bjt_model_t *params = (bjt_model_t *)calloc(1, sizeof(bjt_model_t));
    if (params == NULL)
        return NULL;

    /* Default values (typical small-signal NPN) */
    params->polarity = 1;         /* NPN */
    params->is = 1e-16;           /* Saturation current */
    params->bf = 100.0;           /* Forward beta */
    params->nf = 1.0;             /* Forward emission coefficient */
    params->br = 1.0;             /* Reverse beta */
    params->nr = 1.0;             /* Reverse emission coefficient */
    params->rb = 0.0;             /* Base resistance */
    params->re = 0.0;             /* Emitter resistance */
    params->rc = 0.0;             /* Collector resistance */
    params->cje = 0.0;            /* C-B capacitance */
    params->vje = 0.75;           /* C-B built-in potential */
    params->me = 0.5;             /* C-B grading coefficient */
    params->cjcs = 0.0;           /* C-S capacitance */
    params->vjc = 0.75;           /* C-S built-in potential */
    params->mc = 0.5;             /* C-S grading coefficient */
    params->tf = 0.0;             /* Forward transit time */
    params->tr = 0.0;             /* Reverse transit time */
    params->bvc = 1e10;           /* C-B breakdown voltage */
    params->bve = 1e10;           /* C-E breakdown voltage */
    params->ibvc = 0.0;           /* Current at C-B breakdown */
    params->ibve = 0.0;           /* Current at C-E breakdown */
    params->eg = 1.11;            /* Activation energy */
    params->xti = 3.0;            /* Temperature exponent */

    return params;
}

/* Set BJT model parameter */
static int bjt_set_param(Model *model, const char *param, double value)
{
    bjt_model_t *p = (bjt_model_t *)model->params;
    if (p == NULL)
        return E_NOTFOUND;

    if (strcmp(param, "is") == 0) p->is = value;
    else if (strcmp(param, "bf") == 0) p->bf = value;
    else if (strcmp(param, "nf") == 0) p->nf = value;
    else if (strcmp(param, "br") == 0) p->br = value;
    else if (strcmp(param, "nr") == 0) p->nr = value;
    else if (strcmp(param, "rb") == 0) p->rb = value;
    else if (strcmp(param, "re") == 0) p->re = value;
    else if (strcmp(param, "rc") == 0) p->rc = value;
    else if (strcmp(param, "cje") == 0) p->cje = value;
    else if (strcmp(param, "vje") == 0) p->vje = value;
    else if (strcmp(param, "me") == 0) p->me = value;
    else if (strcmp(param, "cjcs") == 0) p->cjcs = value;
    else if (strcmp(param, "vjc") == 0) p->vjc = value;
    else if (strcmp(param, "mc") == 0) p->mc = value;
    else if (strcmp(param, "tf") == 0) p->tf = value;
    else if (strcmp(param, "tr") == 0) p->tr = value;
    else if (strcmp(param, "bvc") == 0) p->bvc = value;
    else if (strcmp(param, "bve") == 0) p->bve = value;
    else if (strcmp(param, "ibvc") == 0) p->ibvc = value;
    else if (strcmp(param, "ibve") == 0) p->ibve = value;
    else if (strcmp(param, "eg") == 0) p->eg = value;
    else if (strcmp(param, "xti") == 0) p->xti = value;
    else return E_NOTFOUND;

    return OK;
}

/* Free BJT model parameters */
static void bjt_free_params(void *params)
{
    free(params);
}

/* Setup: no special setup needed */
static int bjt_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Load: linear devices don't contribute to DC load */
static int bjt_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    return OK;
}

/* AC load: small-signal model */
static int bjt_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    /* For AC analysis, BJT is linearized around the operating point */
    /* This simplified model adds conductance terms based on gm */
    bjt_model_t *params = dev->model ? (bjt_model_t *)dev->model->params : NULL;
    if (params == NULL)
        return OK;

    int nc = dev->n1;   /* Collector */
    int nb = dev->n2;   /* Base */
    int ne = dev->n3;   /* Emitter */

    /* Get DC operating point voltages */
    double vbe = 0.0, vce = 0.0;
    if (nb >= 0 && ne >= 0)
        vbe = ckt->voltage[nb] - ckt->voltage[ne];
    else if (nb >= 0)
        vbe = ckt->voltage[nb];
    else if (ne >= 0)
        vbe = -ckt->voltage[ne];

    if (nc >= 0 && ne >= 0)
        vce = ckt->voltage[nc] - ckt->voltage[ne];
    else if (nc >= 0)
        vce = ckt->voltage[nc];
    else if (ne >= 0)
        vce = -ckt->voltage[ne];

    /* Calculate thermal voltage */
    double vt = KB * ckt->temp / QE;

    /* Calculate collector current (simplified Ebers-Moll) */
    double ic = params->is * (exp(vbe / (params->nf * vt)) - 1.0);
    if (ic < 0) ic = 0;

    /* Transconductance: gm = Ic / Vt */
    double gm = ic / vt;

    /* Base-emitter conductance: gpi = gm / beta */
    double gpi = gm / params->bf;

    /* Output conductance (simplified) */
    double go = 1e-6 * ic;  /* Small output conductance */

    /* Apply polarity for PNP */
    int pol = params->polarity;

    /* MNA stamp for simplified hybrid-pi model:
     * Collector: ic = gm * vbe + go * vce
     * Base: ib = gpi * vbe
     */

    /* Base-Emitter conductance (gpi) */
    if (nb >= 0 && ne >= 0) {
        sparse_add_element(mat, nb, nb, gpi);
        sparse_add_element(mat, nb, ne, -gpi);
        sparse_add_element(mat, ne, nb, -gpi);
        sparse_add_element(mat, ne, ne, gpi);
    } else if (nb >= 0) {
        sparse_add_element(mat, nb, nb, gpi);
    } else if (ne >= 0) {
        sparse_add_element(mat, ne, ne, gpi);
    }

    /* Transconductance (gm): current from collector to emitter controlled by vbe */
    if (nc >= 0 && nb >= 0) {
        sparse_add_element(mat, nc, nb, pol * gm);
    }
    if (nc >= 0 && ne >= 0) {
        sparse_add_element(mat, nc, ne, -pol * gm);
    }

    /* Output conductance (go) */
    if (nc >= 0 && ne >= 0) {
        sparse_add_element(mat, nc, nc, go);
        sparse_add_element(mat, nc, ne, -go);
        sparse_add_element(mat, ne, nc, -go);
        sparse_add_element(mat, ne, ne, go);
    } else if (nc >= 0) {
        sparse_add_element(mat, nc, nc, go);
    } else if (ne >= 0) {
        sparse_add_element(mat, ne, ne, go);
    }

    return OK;
}

/* Update: no state to update */
static int bjt_update(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Nonlinear: Newton-Raphson iteration for BJT */
static int bjt_nonlinear(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    bjt_model_t *params = dev->model ? (bjt_model_t *)dev->model->params : NULL;
    if (params == NULL) {
        /* Use default parameters if no model specified */
        static bjt_model_t defaults;
        static int initialized = 0;
        if (!initialized) {
            bjt_model_t *tmp = (bjt_model_t *)bjt_create_params();
            if (tmp) {
                defaults = *tmp;
                free(tmp);
            }
            initialized = 1;
        }
        params = &defaults;
    }

    int nc = dev->n1;   /* Collector */
    int nb = dev->n2;   /* Base */
    int ne = dev->n3;   /* Emitter */

    /* Get node voltages */
    double vc = (nc >= 0) ? ckt->voltage[nc] : 0.0;
    double vb = (nb >= 0) ? ckt->voltage[nb] : 0.0;
    double ve = (ne >= 0) ? ckt->voltage[ne] : 0.0;

    /* Vbe and Vbc */
    double vbe = vb - ve;
    double vbc = vb - vc;

    /* Thermal voltage */
    double vt = KB * ckt->temp / QE;
    double vlimit = 20.0 * vt;

    /* Limit voltages to prevent overflow */
    if (vbe > vlimit) vbe = vlimit;
    if (vbe < -vlimit) vbe = -vlimit;
    if (vbc > vlimit) vbc = vlimit;
    if (vbc < -vlimit) vbc = -vlimit;

    /* Ebers-Moll model */
    double is = params->is;
    double bf = params->bf;
    double nf = params->nf;
    double br = params->br;
    double nr = params->nr;

    /* Forward and reverse currents */
    double af = bf / (bf + 1.0);    /* Forward alpha */
    double ar = br / (br + 1.0);    /* Reverse alpha */

    /* Exponential terms */
    double exp_be, exp_bc;
    double g_be, g_bc;  /* Conductances */

    /* Forward current (BE junction) */
    double arg_f = vbe / (nf * vt);
    if (arg_f > 50) arg_f = 50;
    if (arg_f < -50) arg_f = -50;
    exp_be = exp(arg_f);
    g_be = is / (nf * vt) * exp_be;
    double if_current = is * (exp_be - 1.0);

    /* Reverse current (BC junction) */
    double arg_r = vbc / (nr * vt);
    if (arg_r > 50) arg_r = 50;
    if (arg_r < -50) arg_r = -50;
    exp_bc = exp(arg_r);
    g_bc = is / (nr * vt) * exp_bc;
    double ir_current = is * (exp_bc - 1.0);

    /* Terminal currents (Ebers-Moll) */
    /* Ic = af*If - Ir + (1-ar)*Ir ≈ af*If - Ir (simplified) */
    /* Ib = (1-af)*If + (1-ar)*Ir */
    /* Ie = -If + ar*Ir */

    double ic = af * if_current - ir_current;
    double ib = (1.0 - af) * if_current + (1.0 - ar) * ir_current;
    double ie = -(if_current - ar * ir_current);

    /* Conductance matrix (Jacobian) */
    /* dIc/dVbe, dIc/dVbc, etc. */
    double gmf = af * g_be;       /* Forward transconductance */
    double gmr = g_bc;             /* Reverse transconductance */
    double gbf = (1.0 - af) * g_be;  /* Base conductance forward */
    double gbr = (1.0 - ar) * g_bc;  /* Base conductance reverse */

    /* Apply polarity for PNP */
    int pol = params->polarity;

    /* MNA stamp for BJT (3-terminal device) */
    /* Currents: Ic, Ib, Ie */
    /* Voltages: Vc, Vb, Ve */

    /* Collector node */
    if (nc >= 0) {
        /* dIc/dVc = -dIc/dVbc = -gmr */
        sparse_add_element(mat, nc, nc, gmr);
        /* dIc/dVb = dIc/dVbc = gmr + gmf */
        if (nb >= 0)
            sparse_add_element(mat, nc, nb, gmr + gmf);
        /* dIc/dVe = -dIc/dVbe = -gmf */
        if (ne >= 0)
            sparse_add_element(mat, nc, ne, -gmf);
    }

    /* Base node */
    if (nb >= 0) {
        /* dIb/dVc = -dIb/dVbc = -gbr */
        sparse_add_element(mat, nb, nc, gbr);
        /* dIb/dVb = gbf + gbr */
        sparse_add_element(mat, nb, nb, gbf + gbr);
        /* dIb/dVe = -dbf */
        sparse_add_element(mat, nb, ne, -gbf);
    }

    /* Emitter node */
    if (ne >= 0) {
        /* dIe/dVc = ar * dIr/dVbc = ar * g_bc */
        sparse_add_element(mat, ne, nc, ar * g_bc);
        /* dIe/dVb = -ar * g_bc */
        sparse_add_element(mat, ne, nb, -ar * g_bc);
        /* dIe/dVe = g_be + ar * g_bc */
        sparse_add_element(mat, ne, ne, g_be + ar * g_bc);
    }

    /* Current source terms for Newton-Raphson: I_hist = I - G*V */
    /* For each node: add (I_device - sum(G_ij * V_j)) to RHS */
    double ic_total = ic;
    double ib_total = ib;
    double ie_total = ie;

    /* RHS contributions */
    if (nc >= 0)
        sparse_add_rhs(mat, nc, -pol * ic_total);
    if (nb >= 0)
        sparse_add_rhs(mat, nb, -pol * ib_total);
    if (ne >= 0)
        sparse_add_rhs(mat, ne, pol * ie_total);

    return OK;
}

/* Device operations table */
static const DeviceOps bjt_ops = {
    .name = "Q",
    .type = DEV_NPN,
    .setup = bjt_setup,
    .load = bjt_load,
    .ac_load = bjt_ac_load,
    .update = bjt_update,
    .nonlinear = bjt_nonlinear
};

const DeviceOps *bjt_get_ops(void)
{
    return &bjt_ops;
}

/* Model operations for BJT */
void *bjt_create_model_params(void)
{
    return bjt_create_params();
}

int bjt_set_model_param(Model *model, const char *param, double value)
{
    return bjt_set_param(model, param, value);
}

void bjt_free_model_params(void *params)
{
    bjt_free_params(params);
}
