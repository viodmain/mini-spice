/*
 * mos.c - MOSFET device model (Level 1: Shichman-Hodges)
 * Implements the simple square-law model for NMOS and PMOS transistors
 * Supports DC and AC analysis with body effect and channel length modulation
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* MOSFET node mapping: 1=Drain, 2=Gate, 3=Source, 4=Body (optional) */

/* Create default MOSFET model parameters */
static void *mos_create_params(void)
{
    mos_model_t *params = (mos_model_t *)calloc(1, sizeof(mos_model_t));
    if (params == NULL)
        return NULL;

    /* Default values (typical small-signal NMOS) */
    params->polarity = 1;       /* NMOS */
    params->kp = 100e-6;        /* Transconductance (100 uA/V^2) */
    params->vto = 0.7;          /* Threshold voltage */
    params->gamma = 0.5;        /* Body effect parameter */
    params->lambda = 0.01;      /* Channel length modulation */
    params->phi = 0.7;          /* Surface potential */
    params->w = 10e-6;          /* Channel width (10 um) */
    params->l = 1e-6;           /* Channel length (1 um) */
    params->cj = 0.0;           /* Bottom junction capacitance */
    params->mj = 0.5;           /* Bottom junction grading coefficient */
    params->cjsw = 0.0;         /* Side junction capacitance */
    params->mjs = 0.33;         /* Side junction grading coefficient */
    params->cjo = 0.0;          /* Zero-bias depletion capacitance */
    params->rd = 0.0;           /* Drain resistance */
    params->rs = 0.0;           /* Source resistance */
    params->rb = 0.0;           /* Gate resistance */
    params->u0 = 0;             /* Surface mobility */
    params->vmax = 0;           /* Maximum lateral velocity */
    params->eg = 1.11;          /* Activation energy */
    params->xti = 3.0;          /* Temperature exponent */

    return params;
}

/* Set MOSFET model parameter */
static int mos_set_param(Model *model, const char *param, double value)
{
    mos_model_t *p = (mos_model_t *)model->params;
    if (p == NULL)
        return E_NOTFOUND;

    if (strcmp(param, "kp") == 0) p->kp = value;
    else if (strcmp(param, "vto") == 0) p->vto = value;
    else if (strcmp(param, "gamma") == 0) p->gamma = value;
    else if (strcmp(param, "lambda") == 0) p->lambda = value;
    else if (strcmp(param, "phi") == 0) p->phi = value;
    else if (strcmp(param, "w") == 0) p->w = value;
    else if (strcmp(param, "l") == 0) p->l = value;
    else if (strcmp(param, "cj") == 0) p->cj = value;
    else if (strcmp(param, "mj") == 0) p->mj = value;
    else if (strcmp(param, "cjsw") == 0) p->cjsw = value;
    else if (strcmp(param, "mjs") == 0) p->mjs = value;
    else if (strcmp(param, "cjo") == 0) p->cjo = value;
    else if (strcmp(param, "rd") == 0) p->rd = value;
    else if (strcmp(param, "rs") == 0) p->rs = value;
    else if (strcmp(param, "rb") == 0) p->rb = value;
    else if (strcmp(param, "u0") == 0) p->u0 = value;
    else if (strcmp(param, "vmax") == 0) p->vmax = value;
    else if (strcmp(param, "eg") == 0) p->eg = value;
    else if (strcmp(param, "xti") == 0) p->xti = value;
    else return E_NOTFOUND;

    return OK;
}

/* Free MOSFET model parameters */
static void mos_free_params(void *params)
{
    free(params);
}

/* Setup: no special setup needed */
static int mos_setup(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Load: linear devices don't contribute to DC load */
static int mos_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    return OK;
}

/* AC load: small-signal model */
static int mos_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    mos_model_t *params = dev->model ? (mos_model_t *)dev->model->params : NULL;
    if (params == NULL)
        return OK;

    int nd = dev->n1;   /* Drain */
    int ng = dev->n2;   /* Gate */
    int ns = dev->n3;   /* Source */
    int nb = dev->n4;   /* Body (optional, default = source) */
    if (nb < 0) nb = ns;

    /* Get DC operating point voltages */
    double vgs = 0.0, vds = 0.0, vbs = 0.0;
    if (ng >= 0 && ns >= 0)
        vgs = ckt->voltage[ng] - ckt->voltage[ns];
    if (nd >= 0 && ns >= 0)
        vds = ckt->voltage[nd] - ckt->voltage[ns];
    if (nb >= 0 && ns >= 0)
        vbs = ckt->voltage[nb] - ckt->voltage[ns];

    /* Get instance W/L (can override model) */
    double w = params->w;
    double l = params->l;
    if (dev->value > 0) w = dev->value;       /* W from netlist */
    if (dev->value2 > 0) l = dev->value2;      /* L from netlist */

    /* Calculate effective kp = kp_model * W/L */
    double kp_eff = params->kp * w / l;

    /* Threshold voltage with body effect */
    double vth = params->vto + params->gamma * (sqrt(params->phi + vbs) - sqrt(params->phi));

    /* Overdrive voltage */
    double vov = vgs - vth;

    /* Calculate drain current and transconductance based on region */
    double id = 0.0;
    double gm = 0.0;   /* Transconductance */
    double gds = 0.0;  /* Output conductance */

    if (vov <= 0) {
        /* Cutoff region */
        id = 0.0;
        gm = 0.0;
        gds = 0.0;
    } else if (vds < vov) {
        /* Triode/Linear region */
        id = kp_eff * ((vov * vds) - 0.5 * vds * vds) * (1.0 + params->lambda * vds);
        gm = kp_eff * vds * (1.0 + params->lambda * vds);
        gds = kp_eff * (vov - vds) * (1.0 + params->lambda * vds) + kp_eff * ((vov * vds) - 0.5 * vds * vds) * params->lambda;
    } else {
        /* Saturation region */
        id = 0.5 * kp_eff * vov * vov * (1.0 + params->lambda * vds);
        gm = kp_eff * vov * (1.0 + params->lambda * vds);
        gds = 0.5 * kp_eff * vov * vov * params->lambda;
    }

    /* Apply polarity for PMOS */
    int pol = params->polarity;

    /* MNA stamp for simplified MOSFET model:
     * Id = gm * Vgs + gds * Vds
     * Ig = 0 (gate current = 0)
     */

    /* Gate: open circuit (no conductance) */
    /* No stamp needed for gate */

    /* Drain-Source: output conductance gds */
    if (nd >= 0 && ns >= 0) {
        sparse_add_element(mat, nd, nd, gds);
        sparse_add_element(mat, nd, ns, -gds);
        sparse_add_element(mat, ns, nd, -gds);
        sparse_add_element(mat, ns, ns, gds);
    } else if (nd >= 0) {
        sparse_add_element(mat, nd, nd, gds);
    } else if (ns >= 0) {
        sparse_add_element(mat, ns, ns, gds);
    }

    /* Transconductance: current from drain to source controlled by Vgs */
    /* Id = gm * (Vg - Vs) */
    if (nd >= 0 && ng >= 0) {
        sparse_add_element(mat, nd, ng, pol * gm);
    }
    if (nd >= 0 && ns >= 0) {
        sparse_add_element(mat, nd, ns, -pol * gm);
    }

    /* Body effect (back-gate transconductance gmb) */
    if (vov > 0 && params->gamma > 0) {
        double gmb = gm * params->gamma / (2.0 * sqrt(params->phi + vbs));
        if (nd >= 0 && nb >= 0) {
            sparse_add_element(mat, nd, nb, -pol * gmb);
        }
    }

    return OK;
}

/* Update: no state to update */
static int mos_update(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Nonlinear: Newton-Raphson iteration for MOSFET */
static int mos_nonlinear(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    mos_model_t *params = dev->model ? (mos_model_t *)dev->model->params : NULL;
    if (params == NULL) {
        /* Use default parameters if no model specified */
        static mos_model_t defaults;
        static int initialized = 0;
        if (!initialized) {
            mos_model_t *tmp = (mos_model_t *)mos_create_params();
            if (tmp) {
                defaults = *tmp;
                free(tmp);
            }
            initialized = 1;
        }
        params = &defaults;
    }

    int nd = dev->n1;   /* Drain */
    int ng = dev->n2;   /* Gate */
    int ns = dev->n3;   /* Source */
    int nb = dev->n4;   /* Body (optional) */
    if (nb < 0) nb = ns;

    /* Get node voltages */
    double vd = (nd >= 0) ? ckt->voltage[nd] : 0.0;
    double vg = (ng >= 0) ? ckt->voltage[ng] : 0.0;
    double vs = (ns >= 0) ? ckt->voltage[ns] : 0.0;
    double vb = (nb >= 0) ? ckt->voltage[nb] : 0.0;

    /* Terminal voltages */
    double vgs = vg - vs;
    double vds = vd - vs;
    double vbs = vb - vs;

    /* Get instance W/L */
    double w = params->w;
    double l = params->l;
    if (dev->value > 0) w = dev->value;
    if (dev->value2 > 0) l = dev->value2;

    /* Effective transconductance parameter */
    double kp_eff = params->kp * w / l;

    /* Threshold voltage with body effect */
    double phi = params->phi;
    double vth = params->vto;
    if (params->gamma > 0 && (phi + vbs) > 0) {
        vth = params->vto + params->gamma * (sqrt(phi + vbs) - sqrt(phi));
    }

    /* Overdrive voltage */
    double vov = vgs - vth;

    /* Calculate drain current and transconductances based on region */
    double id = 0.0;
    double gm = 0.0;   /* dId/dVgs */
    double gds = 0.0;  /* dId/dVds */
    double gmb = 0.0;  /* dId/dVbs (body effect) */

    if (vov <= 0) {
        /* Cutoff region: very small current */
        id = 1e-15;
        gm = 1e-12;
        gds = 1e-12;
    } else if (vds < vov) {
        /* Triode/Linear region */
        id = kp_eff * ((vov * vds) - 0.5 * vds * vds) * (1.0 + params->lambda * vds);
        gm = kp_eff * vds * (1.0 + params->lambda * vds);
        gds = kp_eff * (vov - vds) * (1.0 + params->lambda * vds) +
              kp_eff * ((vov * vds) - 0.5 * vds * vds) * params->lambda;

        /* Body effect in triode region */
        if (params->gamma > 0 && (phi + vbs) > 0) {
            gmb = gm * params->gamma / (2.0 * sqrt(phi + vbs));
        }
    } else {
        /* Saturation region */
        id = 0.5 * kp_eff * vov * vov * (1.0 + params->lambda * vds);
        gm = kp_eff * vov * (1.0 + params->lambda * vds);
        gds = 0.5 * kp_eff * vov * vov * params->lambda;

        /* Body effect in saturation */
        if (params->gamma > 0 && (phi + vbs) > 0) {
            gmb = gm * params->gamma / (2.0 * sqrt(phi + vbs));
        }
    }

    /* Apply polarity for PMOS */
    int pol = params->polarity;

    /* MNA stamp for MOSFET (4-terminal device) */
    /* Currents: Id (drain), Ig=0 (gate), Is=-Id (source), Ib=0 (body) */

    /* Drain node */
    if (nd >= 0) {
        /* dId/dVd = gds */
        sparse_add_element(mat, nd, nd, gds);
        /* dId/dVg = gm */
        if (ng >= 0)
            sparse_add_element(mat, nd, ng, pol * gm);
        /* dId/dVs = -gm - gds */
        if (ns >= 0)
            sparse_add_element(mat, nd, ns, -pol * gm - gds);
        /* dId/dVb = -gmb */
        if (nb >= 0)
            sparse_add_element(mat, nd, nb, -pol * gmb);
    }

    /* Source node: Is = -Id */
    if (ns >= 0) {
        /* dIs/dVd = -gds */
        sparse_add_element(mat, ns, nd, -gds);
        /* dIs/dVg = -gm */
        if (ng >= 0)
            sparse_add_element(mat, ns, ng, -pol * gm);
        /* dIs/dVs = gm + gds */
        sparse_add_element(mat, ns, ns, pol * gm + gds);
        /* dIs/dVb = gmb */
        if (nb >= 0)
            sparse_add_element(mat, ns, nb, pol * gmb);
    }

    /* Current source terms for Newton-Raphson */
    /* RHS: add -(I_device - G*V) for each node */
    if (nd >= 0)
        sparse_add_rhs(mat, nd, -pol * id);
    if (ns >= 0)
        sparse_add_rhs(mat, ns, pol * id);

    return OK;
}

/* Device operations table */
static const DeviceOps mos_ops = {
    .name = "M",
    .type = DEV_NMOS,
    .setup = mos_setup,
    .load = mos_load,
    .ac_load = mos_ac_load,
    .update = mos_update,
    .nonlinear = mos_nonlinear
};

const DeviceOps *mos_get_ops(void)
{
    return &mos_ops;
}

/* Model operations for MOSFET */
void *mos_create_model_params(void)
{
    return mos_create_params();
}

int mos_set_model_param(Model *model, const char *param, double value)
{
    return mos_set_param(model, param, value);
}

void mos_free_model_params(void *params)
{
    mos_free_params(params);
}
