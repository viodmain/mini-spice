/*
 * circuit.c - Circuit data structures and management
 * Extended with waveform evaluation, parameter handling, subcircuits
 */
#include "circuit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Node management --- */

Node *circuit_find_node(Circuit *ckt, const char *name)
{
    Node *node;
    for (node = ckt->nodes; node != NULL; node = node->next) {
        if (strcmp(node->name, name) == 0)
            return node;
    }
    return NULL;
}

Node *circuit_get_or_create_node(Circuit *ckt, const char *name)
{
    Node *node = circuit_find_node(ckt, name);
    if (node != NULL)
        return node;

    /* Create new node */
    node = (Node *)malloc(sizeof(Node));
    if (node == NULL) {
        fprintf(stderr, "Error: out of memory creating node %s\n", name);
        return NULL;
    }

    node->name = strdup(name);
    node->number = ckt->num_nodes++;
    node->eqnum = -1;
    node->is_ground = (strcmp(name, "0") == 0 || strcmp(name, "gnd") == 0);
    node->is_vsource = 0;
    node->init_voltage = 0.0;
    node->has_init = 0;
    node->next = ckt->nodes;
    ckt->nodes = node;

    return node;
}

int circuit_get_eqnum(Circuit *ckt, Node *node)
{
    if (node->is_ground)
        return -1;  /* Ground has no equation */

    if (node->eqnum >= 0)
        return node->eqnum;

    /* Allocate new equation number */
    node->eqnum = ckt->num_eqns++;
    return node->eqnum;
}

/* --- Device management --- */

Device *circuit_add_device(Circuit *ckt, const char *name, device_type_t type,
                           int n1, int n2, double value)
{
    Device *dev = (Device *)malloc(sizeof(Device));
    if (dev == NULL) {
        fprintf(stderr, "Error: out of memory creating device %s\n", name);
        return NULL;
    }

    dev->name = strdup(name);
    dev->type = type;
    dev->n1 = n1;
    dev->n2 = n2;
    dev->n3 = -1;
    dev->n4 = -1;
    dev->n5 = -1;
    dev->value = value;
    dev->value2 = 0.0;
    dev->params = NULL;
    dev->model = NULL;
    dev->waveform = NULL;
    dev->subckt_def = NULL;
    dev->sub_devices = NULL;
    dev->port_map = NULL;
    dev->expr = NULL;
    dev->next = ckt->devices;
    ckt->devices = dev;

    return dev;
}

Device *circuit_add_device4(Circuit *ckt, const char *name, device_type_t type,
                            int n1, int n2, int n3, int n4, double value)
{
    Device *dev = circuit_add_device(ckt, name, type, n1, n2, value);
    if (dev != NULL) {
        dev->n3 = n3;
        dev->n4 = n4;
    }
    return dev;
}

Device *circuit_add_device5(Circuit *ckt, const char *name, device_type_t type,
                            int n1, int n2, int n3, int n4, int n5, double value)
{
    Device *dev = circuit_add_device4(ckt, name, type, n1, n2, n3, n4, value);
    if (dev != NULL) {
        dev->n5 = n5;
    }
    return dev;
}

/* --- Model management --- */

Model *circuit_find_model(Circuit *ckt, const char *name)
{
    Model *model;
    for (model = ckt->models; model != NULL; model = model->next) {
        if (strcmp(model->name, name) == 0)
            return model;
    }
    return NULL;
}

Model *circuit_add_model(Circuit *ckt, const char *name, device_type_t type)
{
    Model *model = circuit_find_model(ckt, name);
    if (model != NULL)
        return model;  /* Already exists */

    model = (Model *)malloc(sizeof(Model));
    if (model == NULL) {
        fprintf(stderr, "Error: out of memory creating model %s\n", name);
        return NULL;
    }

    model->name = strdup(name);
    model->type = type;
    model->params = NULL;
    model->next = ckt->models;
    ckt->models = model;

    return model;
}

/* --- Parameter management --- */

Param *circuit_find_param(Circuit *ckt, const char *name)
{
    Param *p;
    for (p = ckt->params; p != NULL; p = p->next) {
        if (strcmp(p->name, name) == 0)
            return p;
    }
    return NULL;
}

int circuit_set_param(Circuit *ckt, const char *name, double value)
{
    Param *p = circuit_find_param(ckt, name);
    if (p == NULL) {
        p = (Param *)malloc(sizeof(Param));
        if (p == NULL)
            return E_NOMEM;
        p->name = strdup(name);
        p->next = ckt->params;
        ckt->params = p;
    }
    p->value = value;
    return OK;
}

/* Evaluate a parameter expression (simple substitution for now) */
double circuit_eval_param(Circuit *ckt, const char *expr)
{
    /* For simple parameter references like {param_name} */
    if (expr[0] == '{' && expr[strlen(expr) - 1] == '}') {
        char name[256];
        strncpy(name, expr + 1, strlen(expr) - 2);
        name[strlen(expr) - 2] = '\0';

        Param *p = circuit_find_param(ckt, name);
        if (p)
            return p->value;
    }

    /* Try to parse as number */
    char *end;
    double val = strtod(expr, &end);
    if (end != expr)
        return val;

    return 0.0;
}

/* --- Subcircuit management --- */

SubcktDef *circuit_find_subckt(Circuit *ckt, const char *name)
{
    SubcktDef *s;
    for (s = ckt->subckts; s != NULL; s = s->next) {
        if (strcasecmp(s->name, name) == 0)
            return s;
    }
    return NULL;
}

SubcktDef *circuit_add_subckt(Circuit *ckt, const char *name)
{
    SubcktDef *s = circuit_find_subckt(ckt, name);
    if (s != NULL)
        return s;

    s = (SubcktDef *)calloc(1, sizeof(SubcktDef));
    if (s == NULL)
        return NULL;

    s->name = strdup(name);
    s->nports = 0;
    s->devices = NULL;
    s->nodes = NULL;
    s->next = ckt->subckts;
    ckt->subckts = s;

    return s;
}

/* --- Waveform evaluation --- */

/* Evaluate a waveform at time t */
double waveform_eval(waveform_params_t *wf, double t)
{
    if (wf == NULL)
        return 0.0;

    switch (wf->type) {
    case WAVEFORM_SIN: {
        /* SIN(Voffset Vamp Freq Td Theta Phi) */
        double td = wf->sin_td;
        if (t < td)
            return wf->sin_voffset;

        double tau = t - td;
        double phase = wf->sin_phi * M_PI / 180.0;
        double omega = 2.0 * M_PI * wf->sin_freq;
        double damping = wf->sin_theta;

        double envelope = 1.0;
        if (damping > 0)
            envelope = exp(-damping * tau);

        return wf->sin_voffset + envelope * wf->sin_vamp * sin(omega * tau + phase);
    }

    case WAVEFORM_PULSE: {
        /* PULSE(V1 V2 Td Tr Tf Pw Per) */
        double v1 = wf->pulse_v1;
        double v2 = wf->pulse_v2;
        double td = wf->pulse_td;
        double tr = wf->pulse_tr;
        double tf = wf->pulse_tf;
        double pw = wf->pulse_pw;
        double per = wf->pulse_per;

        if (t < td)
            return v1;

        /* Calculate position within period */
        double t_period = per > 0 ? fmod(t - td, per) : 0;

        /* Rising edge */
        if (t_period < tr)
            return v1 + (v2 - v1) * t_period / tr;

        /* Flat top */
        if (t_period < tr + pw)
            return v2;

        /* Falling edge */
        if (t_period < tr + pw + tf)
            return v2 - (v2 - v1) * (t_period - tr - pw) / tf;

        return v1;
    }

    case WAVEFORM_PWL: {
        /* PWL(t1 v1 t2 v2 ...) */
        int np = wf->pwl_npoints;
        if (np < 2)
            return 0.0;

        /* Before first point */
        if (t <= wf->pwl_time[0])
            return wf->pwl_value[0];

        /* After last point */
        if (t >= wf->pwl_time[np - 1])
            return wf->pwl_value[np - 1];

        /* Interpolate */
        for (int i = 0; i < np - 1; i++) {
            if (t >= wf->pwl_time[i] && t <= wf->pwl_time[i + 1]) {
                double frac = (t - wf->pwl_time[i]) / (wf->pwl_time[i + 1] - wf->pwl_time[i]);
                return wf->pwl_value[i] + frac * (wf->pwl_value[i + 1] - wf->pwl_value[i]);
            }
        }

        return wf->pwl_value[np - 1];
    }

    case WAVEFORM_EXP: {
        /* EXP(V1 V2 Td1 Tau1 Td2 Tau2) */
        double v1 = wf->exp_v1;
        double v2 = wf->exp_v2;
        double td1 = wf->exp_td1;
        double tau1 = wf->exp_tau1;
        double td2 = wf->exp_td2;
        double tau2 = wf->exp_tau2;

        if (t < td1)
            return v1;

        if (t < td2) {
            /* Rising exponential */
            double tau = t - td1;
            if (tau1 > 0)
                return v1 + (v2 - v1) * (1.0 - exp(-tau / tau1));
            return v2;
        }

        /* Falling exponential */
        double tau = t - td2;
        if (tau2 > 0)
            return v2 + (v1 - v2) * (1.0 - exp(-tau / tau2));
        return v1;
    }

    default:
        return 0.0;
    }
}

/* --- Circuit lifecycle --- */

Circuit *circuit_create(const char *title)
{
    Circuit *ckt = (Circuit *)malloc(sizeof(Circuit));
    if (ckt == NULL) {
        fprintf(stderr, "Error: out of memory creating circuit\n");
        return NULL;
    }

    ckt->title = title ? strdup(title) : strdup("Untitled");
    ckt->nodes = NULL;
    ckt->num_nodes = 0;
    ckt->num_eqns = 0;
    ckt->devices = NULL;
    ckt->models = NULL;
    ckt->analyses = NULL;
    ckt->params = NULL;
    ckt->subckts = NULL;
    ckt->voltage = NULL;
    ckt->current = NULL;
    ckt->time = 0.0;
    ckt->temp = 300.15;       /* Default 27°C in Kelvin */
    ckt->temp_celsius = 27.0;
    ckt->abstol = DEFAULT_ABSTOL;
    ckt->reltol = DEFAULT_RELTOL;
    ckt->vntol = DEFAULT_VNTOL;
    ckt->trtol = DEFAULT_TRTOL;
    ckt->maxiter = DEFAULT_MAXITER;
    ckt->trmaxiter = DEFAULT_MAXITER;
    ckt->gmin = DEFAULT_GMIN;
    ckt->num_vsources = 0;
    ckt->next_vsrc_branch = 0;
    ckt->print_all = 1;
    ckt->print_nodes = NULL;
    ckt->num_print_nodes = 0;

    /* Initialize default options */
    ckt->options.abstol = DEFAULT_ABSTOL;
    ckt->options.vntol = DEFAULT_VNTOL;
    ckt->options.reltol = DEFAULT_RELTOL;
    ckt->options.trtol = DEFAULT_TRTOL;
    ckt->options.maxiter = DEFAULT_MAXITER;
    ckt->options.trmaxiter = DEFAULT_MAXITER;
    ckt->options.gmin = DEFAULT_GMIN;
    ckt->options.gminsteps = 0;
    ckt->options.srcsteps = 0;
    ckt->options.numdgt = 6;
    ckt->options.method = INTEGR_TRAPEZOIDAL;
    ckt->options.ltol = 0;
    ckt->options.chgtol = 1.0e-14;

    /* Create ground node (node 0) */
    circuit_get_or_create_node(ckt, "0");
    ckt->nodes->is_ground = 1;

    return ckt;
}

void circuit_free(Circuit *ckt)
{
    if (ckt == NULL)
        return;

    /* Free nodes */
    Node *node = ckt->nodes;
    while (node != NULL) {
        Node *next = node->next;
        free(node->name);
        free(node);
        node = next;
    }

    /* Free devices */
    Device *dev = ckt->devices;
    while (dev != NULL) {
        Device *next = dev->next;
        free(dev->name);
        free(dev->params);
        if (dev->waveform) {
            free(dev->waveform->pwl_time);
            free(dev->waveform->pwl_value);
            free(dev->waveform);
        }
        free(dev->expr);
        free(dev->port_map);
        free(dev);
        dev = next;
    }

    /* Free models */
    Model *model = ckt->models;
    while (model != NULL) {
        Model *next = model->next;
        free(model->name);
        free(model->params);
        free(model);
        model = next;
    }

    /* Free analyses */
    Analysis *ana = ckt->analyses;
    while (ana != NULL) {
        Analysis *next = ana->next;
        free(ana->params.src_name);
        free(ana->params.noise_output);
        free(ana->params.noise_src);
        free(ana->params.sens_output);
        free(ana->params.pz_input);
        free(ana->params.pz_output);
        free(ana);
        ana = next;
    }

    /* Free subcircuits */
    SubcktDef *s = ckt->subckts;
    while (s != NULL) {
        SubcktDef *next = s->next;
        free(s->name);
        for (int i = 0; i < s->nports; i++)
            free(s->port_names[i]);
        /* Free subcircuit devices and nodes */
        Device *sd = s->devices;
        while (sd != NULL) {
            Device *snext = sd->next;
            free(sd->name);
            free(sd->params);
            free(sd);
            sd = snext;
        }
        Node *sn = s->nodes;
        while (sn != NULL) {
            Node *snext = sn->next;
            free(sn->name);
            free(sn);
            sn = snext;
        }
        free(s);
        s = next;
    }

    /* Free parameters */
    Param *p = ckt->params;
    while (p != NULL) {
        Param *next = p->next;
        free(p->name);
        free(p);
        p = next;
    }

    /* Free print nodes */
    for (int i = 0; i < ckt->num_print_nodes; i++)
        free(ckt->print_nodes[i]);
    free(ckt->print_nodes);

    /* Free results */
    free(ckt->voltage);
    free(ckt->current);
    free(ckt->title);
    free(ckt);
}

/* --- Circuit initialization --- */

int circuit_init(Circuit *ckt)
{
    int i;

    /* Allocate voltage and current arrays */
    int total_eqns = ckt->num_eqns + ckt->num_vsources;
    ckt->voltage = (double *)calloc(total_eqns + 1, sizeof(double));
    ckt->current = (double *)calloc(total_eqns + 1, sizeof(double));

    if (ckt->voltage == NULL || ckt->current == NULL) {
        fprintf(stderr, "Error: out of memory allocating result arrays\n");
        return E_NOMEM;
    }

    /* Initialize voltages to zero, or to initial conditions if specified */
    for (i = 0; i <= total_eqns; i++)
        ckt->voltage[i] = 0.0;
    for (i = 0; i <= total_eqns; i++)
        ckt->current[i] = 0.0;

    /* Apply initial conditions from nodes */
    for (Node *node = ckt->nodes; node != NULL; node = node->next) {
        if (node->has_init && node->eqnum >= 0 && node->eqnum < total_eqns) {
            ckt->voltage[node->eqnum] = node->init_voltage;
        }
    }

    return OK;
}

/* Allocate a voltage source branch current index */
int circuit_alloc_vsrc_branch(Circuit *ckt)
{
    ckt->num_vsources++;
    return ckt->next_vsrc_branch++;
}

/* Resolve branch current indices to equation numbers */
int circuit_resolve_branches(Circuit *ckt)
{
    /* Branch current equation numbers start after node equations */
    int branch_offset = ckt->num_eqns;

    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (dev->type == DEV_VSRC || dev->type == DEV_VCVS || dev->type == DEV_CCVS) {
            /* Convert branch index to equation number */
            if (dev->n3 >= 0 && dev->n3 < ckt->next_vsrc_branch) {
                dev->n3 = branch_offset + dev->n3;
            }
            if (dev->n5 >= 0 && dev->n5 < ckt->next_vsrc_branch) {
                dev->n5 = branch_offset + dev->n5;
            }
        }
    }

    return OK;
}
