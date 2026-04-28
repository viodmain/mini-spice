/*
 * parser.c - SPICE netlist parser
 * Extended parser supporting:
 * - Waveform sources (SIN, PULSE, PWL, EXP)
 * - BJT (Q), MOSFET (M), behavioral sources (B)
 * - Switches (S, W), transmission lines (T)
 * - Subcircuits (.SUBCKT/.ENDS)
 * - .PARAM, .STEP, .OPTIONS, .PRINT, .IC
 * - Continuation lines (+)
 * - .NOISE, .FOUR, .SENS analyses
 */
#include "parser.h"
#include "circuit.h"
#include "device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 4096
#define MAX_TOKENS 50
#define MAX_CONT_LINES 20

static char last_error[256] = {0};

/* Convert string to lowercase */
static void str_to_lower(char *str)
{
    for (int i = 0; str[i]; i++)
        str[i] = tolower(str[i]);
}

/* Tokenize a line */
static int tokenize(char *line, char *tokens[], int max_tokens)
{
    int count = 0;
    char *p = line;

    /* Skip leading whitespace */
    while (*p && isspace(*p)) p++;

    while (*p && count < max_tokens) {
        if (*p == '"') {
            /* Quoted string */
            p++;
            tokens[count++] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p = '\0';
            p++;
        } else {
            tokens[count++] = p;
            while (*p && !isspace(*p) && *p != '"') p++;
            if (*p) *p++ = '\0';
        }

        /* Skip whitespace */
        while (*p && isspace(*p)) p++;
    }

    return count;
}

/* Parse a number with optional suffix */
static double parse_number(const char *str)
{
    char *end;
    double val = strtod(str, &end);

    /* Handle suffixes */
    char suffix[8];
    strncpy(suffix, end, 7);
    suffix[7] = '\0';
    str_to_lower(suffix);

    if (strcmp(suffix, "meg") == 0 || strcmp(suffix, "mege") == 0) val *= 1e6;
    else if (strcmp(suffix, "k") == 0) val *= 1e3;
    else if (strcmp(suffix, "m") == 0) val *= 1e-3;
    else if (strcmp(suffix, "u") == 0) val *= 1e-6;
    else if (strcmp(suffix, "n") == 0) val *= 1e-9;
    else if (strcmp(suffix, "p") == 0) val *= 1e-12;
    else if (strcmp(suffix, "f") == 0) val *= 1e-15;
    else if (strcmp(suffix, "t") == 0) val *= 1e12;
    else if (strcmp(suffix, "g") == 0) val *= 1e9;

    return val;
}

/* Get or create node and return its equation number */
static int get_node_eq(Circuit *ckt, const char *name)
{
    Node *node = circuit_get_or_create_node(ckt, name);
    if (node == NULL)
        return -1;

    if (node->is_ground)
        return -1;  /* Ground */

    return circuit_get_eqnum(ckt, node);
}

/* --- Parse device lines --- */

/* Parse resistor */
static int parse_resistor(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Resistor needs at least 4 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *n1 = tokens[1];
    const char *n2 = tokens[2];
    const char *r_str = tokens[3];

    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);
    double r = parse_number(r_str);

    if (r <= 0) {
        snprintf(last_error, sizeof(last_error), "Invalid resistance: %s", r_str);
        return E_SYNTAX;
    }

    Device *dev = circuit_add_device(ckt, name, DEV_RESISTOR, eq1, eq2, r);
    if (dev == NULL)
        return E_NOMEM;

    return OK;
}

/* Parse capacitor */
static int parse_capacitor(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Capacitor needs at least 4 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *n1 = tokens[1];
    const char *n2 = tokens[2];
    const char *c_str = tokens[3];

    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);
    double c = parse_number(c_str);

    if (c <= 0) {
        snprintf(last_error, sizeof(last_error), "Invalid capacitance: %s", c_str);
        return E_SYNTAX;
    }

    Device *dev = circuit_add_device(ckt, name, DEV_CAPACITOR, eq1, eq2, c);
    if (dev == NULL)
        return E_NOMEM;

    return OK;
}

/* Parse inductor */
static int parse_inductor(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Inductor needs at least 4 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *n1 = tokens[1];
    const char *n2 = tokens[2];
    const char *l_str = tokens[3];

    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);
    double l = parse_number(l_str);

    if (l <= 0) {
        snprintf(last_error, sizeof(last_error), "Invalid inductance: %s", l_str);
        return E_SYNTAX;
    }

    Device *dev = circuit_add_device(ckt, name, DEV_INDUCTOR, eq1, eq2, l);
    if (dev == NULL)
        return E_NOMEM;

    return OK;
}

/* Parse voltage source with waveform support */
static int parse_voltage_source(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Voltage source needs at least 4 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *n1 = tokens[1];
    const char *n2 = tokens[2];

    double v = 0.0;
    int val_idx = 3;
    waveform_params_t *wf = NULL;

    /* Parse DC value - could be "DC <value>" or just "<value>" or waveform keyword */
    if (ntokens >= 5 && strcasecmp(tokens[3], "DC") == 0) {
        v = parse_number(tokens[4]);
        val_idx = 5;
    } else if (ntokens >= 4) {
        /* Check if token[3] is a waveform keyword */
        if (strcasecmp(tokens[3], "SIN") == 0 || strcasecmp(tokens[3], "PULSE") == 0 ||
            strcasecmp(tokens[3], "PWL") == 0 || strcasecmp(tokens[3], "EXP") == 0 ||
            strcasecmp(tokens[3], "AC") == 0) {
            /* Waveform only, no DC value */
            val_idx = 3;
        } else {
            /* Plain DC value */
            v = parse_number(tokens[3]);
            val_idx = 4;
        }
    }

    /* Check for waveform types */
    for (int i = val_idx; i < ntokens; i++) {
        if (strcasecmp(tokens[i], "SIN") == 0 && i + 6 < ntokens) {
            /* SIN(Voffset Vamp Freq Td Theta Phi) */
            wf = (waveform_params_t *)calloc(1, sizeof(waveform_params_t));
            wf->type = WAVEFORM_SIN;
            wf->sin_voffset = parse_number(tokens[i + 1]);
            wf->sin_vamp = parse_number(tokens[i + 2]);
            wf->sin_freq = parse_number(tokens[i + 3]);
            wf->sin_td = parse_number(tokens[i + 4]);
            wf->sin_theta = parse_number(tokens[i + 5]);
            wf->sin_phi = parse_number(tokens[i + 6]);
            i += 6;
        }
        else if (strcasecmp(tokens[i], "PULSE") == 0 && i + 7 < ntokens) {
            /* PULSE(V1 V2 Td Tr Tf Pw Per) */
            wf = (waveform_params_t *)calloc(1, sizeof(waveform_params_t));
            wf->type = WAVEFORM_PULSE;
            wf->pulse_v1 = parse_number(tokens[i + 1]);
            wf->pulse_v2 = parse_number(tokens[i + 2]);
            wf->pulse_td = parse_number(tokens[i + 3]);
            wf->pulse_tr = parse_number(tokens[i + 4]);
            wf->pulse_tf = parse_number(tokens[i + 5]);
            wf->pulse_pw = parse_number(tokens[i + 6]);
            wf->pulse_per = parse_number(tokens[i + 7]);
            i += 7;
        }
        else if (strcasecmp(tokens[i], "PWL") == 0) {
            /* PWL(t1 v1 t2 v2 ...) */
            wf = (waveform_params_t *)calloc(1, sizeof(waveform_params_t));
            wf->type = WAVEFORM_PWL;
            int np = 0;
            wf->pwl_time = (double *)malloc(MAX_PWL_POINTS * sizeof(double));
            wf->pwl_value = (double *)malloc(MAX_PWL_POINTS * sizeof(double));
            for (int j = i + 1; j < ntokens - 1 && np < MAX_PWL_POINTS; j += 2) {
                wf->pwl_time[np] = parse_number(tokens[j]);
                wf->pwl_value[np] = parse_number(tokens[j + 1]);
                np++;
            }
            wf->pwl_npoints = np;
            i = ntokens;  /* Consumed all remaining tokens */
        }
        else if (strcasecmp(tokens[i], "EXP") == 0 && i + 6 < ntokens) {
            /* EXP(V1 V2 Td1 Tau1 Td2 Tau2) */
            wf = (waveform_params_t *)calloc(1, sizeof(waveform_params_t));
            wf->type = WAVEFORM_EXP;
            wf->exp_v1 = parse_number(tokens[i + 1]);
            wf->exp_v2 = parse_number(tokens[i + 2]);
            wf->exp_td1 = parse_number(tokens[i + 3]);
            wf->exp_tau1 = parse_number(tokens[i + 4]);
            wf->exp_td2 = parse_number(tokens[i + 5]);
            wf->exp_tau2 = parse_number(tokens[i + 6]);
            i += 6;
        }
        else if (strcasecmp(tokens[i], "AC") == 0 && i + 1 < ntokens) {
            /* AC magnitude [phase] */
            if (wf == NULL) {
                wf = (waveform_params_t *)calloc(1, sizeof(waveform_params_t));
            }
            wf->type = WAVEFORM_AC;
            wf->ac_mag = parse_number(tokens[i + 1]);
            if (i + 2 < ntokens)
                wf->ac_phase = parse_number(tokens[i + 2]);
            else
                wf->ac_phase = 0.0;
            i++;
        }
    }

    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);

    /* Voltage source needs a branch current equation */
    int ib = circuit_alloc_vsrc_branch(ckt);

    Device *dev = circuit_add_device4(ckt, name, DEV_VSRC, eq1, eq2, ib, -1, v);
    if (dev == NULL) {
        free(wf);
        return E_NOMEM;
    }

    dev->waveform = wf;

    return OK;
}

/* Parse current source with waveform support */
static int parse_current_source(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Current source needs at least 4 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *n1 = tokens[1];
    const char *n2 = tokens[2];

    double i = 0.0;
    int val_idx = 3;
    waveform_params_t *wf = NULL;

    /* Parse DC value */
    if (ntokens >= 5 && strcasecmp(tokens[3], "DC") == 0) {
        i = parse_number(tokens[4]);
        val_idx = 5;
    } else if (ntokens >= 4) {
        i = parse_number(tokens[3]);
        val_idx = 4;
    }

    /* Check for waveform types (same as voltage source) */
    for (int j = val_idx; j < ntokens; j++) {
        if (strcasecmp(tokens[j], "AC") == 0 && j + 1 < ntokens) {
            wf = (waveform_params_t *)calloc(1, sizeof(waveform_params_t));
            wf->type = WAVEFORM_AC;
            wf->ac_mag = parse_number(tokens[j + 1]);
            j++;
        }
    }

    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);

    Device *dev = circuit_add_device(ckt, name, DEV_ISRC, eq1, eq2, i);
    if (dev == NULL) {
        free(wf);
        return E_NOMEM;
    }

    dev->waveform = wf;

    return OK;
}

/* Parse VCCS */
static int parse_vccs(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 6) {
        snprintf(last_error, sizeof(last_error), "VCCS needs 6 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    int eq1 = get_node_eq(ckt, tokens[1]);
    int eq2 = get_node_eq(ckt, tokens[2]);
    int eq3 = get_node_eq(ckt, tokens[3]);
    int eq4 = get_node_eq(ckt, tokens[4]);
    double gm = parse_number(tokens[5]);

    Device *dev = circuit_add_device4(ckt, name, DEV_VCCS, eq1, eq2, eq3, eq4, gm);
    if (dev == NULL)
        return E_NOMEM;

    return OK;
}

/* Parse VCVS */
static int parse_vcvs(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 7) {
        snprintf(last_error, sizeof(last_error), "VCVS needs 7 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    int eq1 = get_node_eq(ckt, tokens[1]);
    int eq2 = get_node_eq(ckt, tokens[2]);
    int eq3 = get_node_eq(ckt, tokens[3]);
    int eq4 = get_node_eq(ckt, tokens[4]);
    double gain = parse_number(tokens[5]);

    int ib = circuit_alloc_vsrc_branch(ckt);

    Device *dev = circuit_add_device5(ckt, name, DEV_VCVS, eq1, eq2, eq3, eq4, ib, gain);
    if (dev == NULL)
        return E_NOMEM;

    return OK;
}

/* Parse CCCS */
static int parse_cccs(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 5) {
        snprintf(last_error, sizeof(last_error), "CCCS needs 5 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    int eq1 = get_node_eq(ckt, tokens[1]);
    int eq2 = get_node_eq(ckt, tokens[2]);
    const char *vctrl_name = tokens[3];
    double gain = parse_number(tokens[4]);

    /* Find controlling voltage source's branch current equation */
    int vctrl_eq = -1;
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (strcmp(dev->name, vctrl_name) == 0 && dev->type == DEV_VSRC) {
            vctrl_eq = dev->n3;
            break;
        }
    }

    if (vctrl_eq < 0) {
        snprintf(last_error, sizeof(last_error), "Controlling source %s not found", vctrl_name);
        return E_NOTFOUND;
    }

    Device *dev = circuit_add_device4(ckt, name, DEV_CCCS, eq1, eq2, vctrl_eq, -1, gain);
    if (dev == NULL)
        return E_NOMEM;

    return OK;
}

/* Parse CCVS */
static int parse_ccvs(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 6) {
        snprintf(last_error, sizeof(last_error), "CCVS needs 6 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    int eq1 = get_node_eq(ckt, tokens[1]);
    int eq2 = get_node_eq(ckt, tokens[2]);
    const char *vctrl_name = tokens[3];
    double gain = parse_number(tokens[4]);

    int vctrl_eq = -1;
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (strcmp(dev->name, vctrl_name) == 0 && dev->type == DEV_VSRC) {
            vctrl_eq = dev->n3;
            break;
        }
    }

    if (vctrl_eq < 0) {
        snprintf(last_error, sizeof(last_error), "Controlling source %s not found", vctrl_name);
        return E_NOTFOUND;
    }

    int ib_out = circuit_alloc_vsrc_branch(ckt);

    Device *dev = circuit_add_device5(ckt, name, DEV_CCVS, eq1, eq2, ib_out, vctrl_eq, -1, gain);
    if (dev == NULL)
        return E_NOMEM;

    return OK;
}

/* Parse diode */
static int parse_diode(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Diode needs at least 4 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *n1 = tokens[1];
    const char *n2 = tokens[2];
    const char *model_name = tokens[3];

    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);

    /* Find or create model */
    Model *model = circuit_find_model(ckt, model_name);
    if (model == NULL) {
        model = circuit_add_model(ckt, model_name, DEV_DIODE);
        if (model) {
            extern void *dio_create_params(void);
            model->params = dio_create_params();
        }
    }

    Device *dev = circuit_add_device(ckt, name, DEV_DIODE, eq1, eq2, 0.0);
    if (dev == NULL)
        return E_NOMEM;

    dev->model = model;

    return OK;
}

/* Parse BJT */
static int parse_bjt(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 5) {
        snprintf(last_error, sizeof(last_error), "BJT needs at least 5 tokens: Qname c b e model");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *nc = tokens[1];  /* Collector */
    const char *nb = tokens[2];  /* Base */
    const char *ne = tokens[3];  /* Emitter */
    const char *model_name = tokens[4];

    int eq1 = get_node_eq(ckt, nc);
    int eq2 = get_node_eq(ckt, nb);
    int eq3 = get_node_eq(ckt, ne);
    int eq4 = -1;  /* Substrate (optional) */
    if (ntokens > 5)
        eq4 = get_node_eq(ckt, tokens[5]);

    /* Determine BJT type from model */
    device_type_t type = DEV_NPN;
    Model *model = circuit_find_model(ckt, model_name);
    if (model == NULL) {
        model = circuit_add_model(ckt, model_name, DEV_NPN);
        if (model) {
            extern void *bjt_create_model_params(void);
            model->params = bjt_create_model_params();
        }
    } else {
        type = model->type;
    }

    Device *dev = circuit_add_device5(ckt, name, type, eq1, eq2, eq3, eq4, -1, 0.0);
    if (dev == NULL)
        return E_NOMEM;

    dev->model = model;

    return OK;
}

/* Parse MOSFET */
static int parse_mosfet(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 5) {
        snprintf(last_error, sizeof(last_error), "MOSFET needs at least 5 tokens: Mname d g s model");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *nd = tokens[1];  /* Drain */
    const char *ng = tokens[2];  /* Gate */
    const char *ns = tokens[3];  /* Source */
    const char *model_name = tokens[4];

    int eq1 = get_node_eq(ckt, nd);
    int eq2 = get_node_eq(ckt, ng);
    int eq3 = get_node_eq(ckt, ns);
    int eq4 = -1;  /* Body (optional) */
    if (ntokens > 5)
        eq4 = get_node_eq(ckt, tokens[5]);

    /* Determine MOSFET type from model */
    device_type_t type = DEV_NMOS;
    Model *model = circuit_find_model(ckt, model_name);
    if (model == NULL) {
        model = circuit_add_model(ckt, model_name, DEV_NMOS);
        if (model) {
            extern void *mos_create_model_params(void);
            model->params = mos_create_model_params();
        }
    } else {
        type = model->type;
    }

    /* W and L can be specified as additional parameters */
    double w = 0.0, l = 0.0;
    if (ntokens > 6) {
        w = parse_number(tokens[6]);
        l = parse_number(tokens[7]);
    }

    Device *dev = circuit_add_device5(ckt, name, type, eq1, eq2, eq3, eq4, -1, w);
    if (dev == NULL)
        return E_NOMEM;

    dev->model = model;
    dev->value2 = l;

    return OK;
}

/* Parse behavioral source */
static int parse_behsrc(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Behavioral source needs at least 4 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *n1 = tokens[1];
    const char *n2 = tokens[2];

    /* Find expression (I=... or V=...) */
    char *expr = NULL;
    int is_voltage = 0;
    for (int i = 3; i < ntokens; i++) {
        if (strncmp(tokens[i], "I=", 2) == 0) {
            expr = tokens[i] + 2;
        } else if (strncmp(tokens[i], "V=", 2) == 0) {
            expr = tokens[i] + 2;
            is_voltage = 1;
        }
    }

    if (expr == NULL) {
        snprintf(last_error, sizeof(last_error), "Behavioral source needs I= or V= expression");
        return E_SYNTAX;
    }

    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);

    device_type_t type = is_voltage ? DEV_Behavioral_VSRC : DEV_Behavioral_SRC;
    Device *dev = circuit_add_device(ckt, name, type, eq1, eq2, 0.0);
    if (dev == NULL)
        return E_NOMEM;

    dev->expr = strdup(expr);

    return OK;
}

/* Parse switch */
static int parse_switch(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 6) {
        snprintf(last_error, sizeof(last_error), "Switch needs 6 tokens: Sname nc+ nc- n+ n- model");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *nc1 = tokens[1];  /* Control + */
    const char *nc2 = tokens[2];  /* Control - */
    const char *n1 = tokens[3];   /* Switch + */
    const char *n2 = tokens[4];   /* Switch - */
    const char *model_name = tokens[5];

    int eq1 = get_node_eq(ckt, nc1);
    int eq2 = get_node_eq(ckt, nc2);
    int eq3 = get_node_eq(ckt, n1);
    int eq4 = get_node_eq(ckt, n2);

    device_type_t type = (name[0] == 'S' || name[0] == 's') ? DEV_SWITCH_VOLTAGE : DEV_SWITCH_CURRENT;

    Model *model = circuit_find_model(ckt, model_name);
    if (model == NULL) {
        model = circuit_add_model(ckt, model_name, type);
        if (model) {
            extern void *switch_create_model_params(void);
            model->params = switch_create_model_params();
        }
    }

    Device *dev = circuit_add_device4(ckt, name, type, eq1, eq2, eq3, eq4, 0.0);
    if (dev == NULL)
        return E_NOMEM;

    dev->model = model;

    return OK;
}

/* Parse transmission line */
static int parse_tline(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 6) {
        snprintf(last_error, sizeof(last_error), "Transmission line needs 6 tokens: Tname n1+ n1- n2+ n2- model");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    const char *n1p = tokens[1];
    const char *n1m = tokens[2];
    const char *n2p = tokens[3];
    const char *n2m = tokens[4];
    const char *model_name = tokens[5];

    int eq1 = get_node_eq(ckt, n1p);
    int eq2 = get_node_eq(ckt, n1m);
    int eq3 = get_node_eq(ckt, n2p);
    int eq4 = get_node_eq(ckt, n2m);

    Model *model = circuit_find_model(ckt, model_name);
    if (model == NULL) {
        model = circuit_add_model(ckt, model_name, DEV_TRANSMISSION_LINE);
        if (model) {
            extern void *tline_create_model_params(void);
            model->params = tline_create_model_params();
        }
    }

    Device *dev = circuit_add_device4(ckt, name, DEV_TRANSMISSION_LINE, eq1, eq2, eq3, eq4, 0.0);
    if (dev == NULL)
        return E_NOMEM;

    dev->model = model;

    return OK;
}

/* Parse subcircuit instance */
static int parse_subckt_instance(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 3) {
        snprintf(last_error, sizeof(last_error), "Subcircuit instance needs at least 3 tokens");
        return E_SYNTAX;
    }

    const char *name = tokens[0];

    /* Find subcircuit definition */
    const char *subckt_name = tokens[ntokens - 1];
    SubcktDef *subckt = circuit_find_subckt(ckt, subckt_name);
    if (subckt == NULL) {
        snprintf(last_error, sizeof(last_error), "Subcircuit %s not defined", subckt_name);
        return E_NOTFOUND;
    }

    /* Create device instance */
    Device *dev = circuit_add_device(ckt, name, DEV_SUBCKT, -1, -1, 0.0);
    if (dev == NULL)
        return E_NOMEM;

    dev->subckt_def = subckt;

    /* Map ports to nodes */
    int nports = subckt->nports;
    if (ntokens - 1 != nports + 1) {
        snprintf(last_error, sizeof(last_error), "Subcircuit %s expects %d ports, got %d",
                 subckt_name, nports, ntokens - 2);
        return E_SYNTAX;
    }

    dev->port_map = (int *)malloc(nports * sizeof(int));
    for (int i = 0; i < nports; i++) {
        Node *node = circuit_get_or_create_node(ckt, tokens[i + 1]);
        dev->port_map[i] = (node && !node->is_ground) ? circuit_get_eqnum(ckt, node) : -1;
    }

    return OK;
}

/* --- Parse .model statement --- */

static int parse_model(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 6, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
    if (ntokens < 2) {
        snprintf(last_error, sizeof(last_error), "Invalid .model statement");
        return E_SYNTAX;
    }

    const char *model_name = tokens[0];
    const char *type_str = tokens[1];

    /* Determine device type */
    device_type_t type;
    str_to_lower((char *)type_str);
    if (strcmp(type_str, "d") == 0 || strcmp(type_str, "diode") == 0)
        type = DEV_DIODE;
    else if (strcmp(type_str, "npn") == 0)
        type = DEV_NPN;
    else if (strcmp(type_str, "pnp") == 0)
        type = DEV_PNP;
    else if (strcmp(type_str, "nmos") == 0)
        type = DEV_NMOS;
    else if (strcmp(type_str, "pmos") == 0)
        type = DEV_PMOS;
    else if (strcmp(type_str, "sw") == 0 || strcmp(type_str, "switch") == 0)
        type = DEV_SWITCH_VOLTAGE;
    else if (strcmp(type_str, "csw") == 0 || strcmp(type_str, "current_switch") == 0)
        type = DEV_SWITCH_CURRENT;
    else if (strcmp(type_str, "tline") == 0 || strcmp(type_str, "transmission_line") == 0)
        type = DEV_TRANSMISSION_LINE;
    else {
        snprintf(last_error, sizeof(last_error), "Unknown model type: %s", type_str);
        return E_SYNTAX;
    }

    /* Create model */
    Model *model = circuit_add_model(ckt, model_name, type);
    if (model == NULL)
        return E_NOMEM;

    /* Create default parameters */
    switch (type) {
    case DEV_DIODE:
        { extern void *dio_create_params(void); model->params = dio_create_params(); break; }
    case DEV_NPN:
    case DEV_PNP:
        { extern void *bjt_create_model_params(void); model->params = bjt_create_model_params(); break; }
    case DEV_NMOS:
    case DEV_PMOS:
        { extern void *mos_create_model_params(void); model->params = mos_create_model_params(); break; }
    case DEV_SWITCH_VOLTAGE:
    case DEV_SWITCH_CURRENT:
        { extern void *switch_create_model_params(void); model->params = switch_create_model_params(); break; }
    case DEV_TRANSMISSION_LINE:
        { extern void *tline_create_model_params(void); model->params = tline_create_model_params(); break; }
    default:
        break;
    }

    /* Set model parameters */
    for (int i = 2; i < ntokens; i++) {
        char *eq = strchr(tokens[i], '=');
        if (eq) {
            *eq = '\0';
            double value = parse_number(eq + 1);

            switch (type) {
            case DEV_DIODE:
                { extern int dio_set_param(Model *, const char *, double); dio_set_param(model, tokens[i], value); break; }
            case DEV_NPN:
            case DEV_PNP:
                { extern int bjt_set_model_param(Model *, const char *, double); bjt_set_model_param(model, tokens[i], value); break; }
            case DEV_NMOS:
            case DEV_PMOS:
                { extern int mos_set_model_param(Model *, const char *, double); mos_set_model_param(model, tokens[i], value); break; }
            case DEV_SWITCH_VOLTAGE:
            case DEV_SWITCH_CURRENT:
                { extern int switch_set_model_param(Model *, const char *, double); switch_set_model_param(model, tokens[i], value); break; }
            case DEV_TRANSMISSION_LINE:
                { extern int tline_set_model_param(Model *, const char *, double); tline_set_model_param(model, tokens[i], value); break; }
            default:
                break;
            }
        }
    }

    return OK;
}

/* --- Parse .dc statement --- */

static int parse_dc(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 3, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Invalid .dc statement");
        return E_SYNTAX;
    }

    Analysis *ana = (Analysis *)calloc(1, sizeof(Analysis));
    if (ana == NULL)
        return E_NOMEM;

    ana->params.type = ANA_DC_SWEEP;
    ana->params.src_name = strdup(tokens[0]);
    ana->params.start = parse_number(tokens[1]);
    ana->params.stop = parse_number(tokens[2]);
    if (ntokens > 4)
        ana->params.step = parse_number(tokens[3]);
    else
        ana->params.step = (ana->params.stop - ana->params.start) / 100;

    ana->next = ckt->analyses;
    ckt->analyses = ana;

    return OK;
}

/* --- Parse .ac statement --- */

static int parse_ac(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 3, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Invalid .ac statement");
        return E_SYNTAX;
    }

    Analysis *ana = (Analysis *)calloc(1, sizeof(Analysis));
    if (ana == NULL)
        return E_NOMEM;

    ana->params.type = ANA_AC;

    str_to_lower(tokens[0]);
    if (strcmp(tokens[0], "dec") == 0)
        ana->params.ac_sweep_type = SRC_DECADE;
    else if (strcmp(tokens[0], "oct") == 0)
        ana->params.ac_sweep_type = SRC_OCTAVE;
    else
        ana->params.ac_sweep_type = SRC_LINEAR;

    ana->params.ac_points = parse_number(tokens[1]);
    ana->params.ac_start = parse_number(tokens[2]);
    if (ntokens > 3)
        ana->params.ac_stop = parse_number(tokens[3]);
    else
        ana->params.ac_stop = ana->params.ac_start;

    ana->next = ckt->analyses;
    ckt->analyses = ana;

    return OK;
}

/* --- Parse .tran statement --- */

static int parse_tran(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 5, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
    if (ntokens < 2) {
        snprintf(last_error, sizeof(last_error), "Invalid .tran statement");
        return E_SYNTAX;
    }

    Analysis *ana = (Analysis *)calloc(1, sizeof(Analysis));
    if (ana == NULL)
        return E_NOMEM;

    ana->params.type = ANA_TRANSIENT;
    ana->params.tstep = parse_number(tokens[0]);
    ana->params.tstop = parse_number(tokens[1]);
    if (ntokens > 2)
        ana->params.tstart = parse_number(tokens[2]);
    else
        ana->params.tstart = 0.0;
    ana->params.tmax = ana->params.tstep;
    ana->params.integration = INTEGR_TRAPEZOIDAL;
    ana->params.use_uic = 0;

    /* Check for UIC flag */
    for (int i = 0; i < ntokens; i++) {
        if (strcasecmp(tokens[i], "UIC") == 0) {
            ana->params.use_uic = 1;
        }
    }

    ana->next = ckt->analyses;
    ckt->analyses = ana;

    return OK;
}

/* --- Parse .op statement --- */

static int parse_op(Circuit *ckt, char *line __attribute__((unused)))
{
    Analysis *ana = (Analysis *)calloc(1, sizeof(Analysis));
    if (ana == NULL)
        return E_NOMEM;

    ana->params.type = ANA_DC_OP;
    ana->next = ckt->analyses;
    ckt->analyses = ana;

    return OK;
}

/* --- Parse .temp statement --- */

static int parse_temp(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 5, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
    if (ntokens >= 1) {
        double temp_c = parse_number(tokens[0]);
        ckt->temp_celsius = temp_c;
        ckt->temp = temp_c + 273.15;
    }

    return OK;
}

/* --- Parse .param statement --- */

static int parse_param(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 6, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);

    /* Parse name=value pairs */
    for (int i = 0; i < ntokens; i++) {
        char *eq = strchr(tokens[i], '=');
        if (eq) {
            *eq = '\0';
            double value = parse_number(eq + 1);
            circuit_set_param(ckt, tokens[i], value);
        }
    }

    return OK;
}

/* --- Parse .step statement --- */

static int parse_step(Circuit *ckt, char *line)
{
    /* .STEP is a control command that loops over other analyses */
    /* For simplicity, we parse it but don't implement the loop here */
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 5, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
    if (ntokens >= 1) {
        printf("Note: .STEP command parsed (parametric sweep not fully implemented)\n");
    }

    return OK;
}

/* --- Parse .options statement --- */

static int parse_options(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 8, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);

    for (int i = 0; i < ntokens; i++) {
        char *eq = strchr(tokens[i], '=');
        if (eq) {
            *eq = '\0';
            double value = parse_number(eq + 1);
            str_to_lower(tokens[i]);

            if (strcmp(tokens[i], "abstol") == 0) ckt->options.abstol = value;
            else if (strcmp(tokens[i], "vntol") == 0) ckt->options.vntol = value;
            else if (strcmp(tokens[i], "reltol") == 0) ckt->options.reltol = value;
            else if (strcmp(tokens[i], "trtol") == 0) ckt->options.trtol = value;
            else if (strcmp(tokens[i], "maxiter") == 0) ckt->options.maxiter = (int)value;
            else if (strcmp(tokens[i], "gmin") == 0) ckt->options.gmin = value;
        } else {
            /* Boolean options */
            str_to_lower(tokens[i]);
            if (strcmp(tokens[i], "nopage") == 0) {
                /* No page break in output */
            }
        }
    }

    return OK;
}

/* --- Parse .print statement --- */

static int parse_print(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 6, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);

    ckt->print_all = 0;
    ckt->num_print_nodes = 0;
    ckt->print_nodes = (char **)malloc(ntokens * sizeof(char *));

    for (int i = 0; i < ntokens; i++) {
        /* Extract node name from V(node) or I(source) */
        char *node_name = tokens[i];
        if (strncmp(node_name, "V(", 2) == 0) {
            char *end = strchr(node_name, ')');
            if (end) {
                *end = '\0';
                node_name += 2;
            }
        }

        ckt->print_nodes[ckt->num_print_nodes] = strdup(node_name);
        ckt->num_print_nodes++;
    }

    return OK;
}

/* --- Parse .ic statement (initial conditions) --- */

static int parse_ic(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 3, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);

    /* Parse V(node)=value pairs */
    for (int i = 0; i < ntokens; i++) {
        char *eq = strchr(tokens[i], '=');
        if (eq) {
            *eq = '\0';
            double value = parse_number(eq + 1);

            /* Extract node name from V(node) */
            char *node_name = tokens[i];
            if (strncmp(node_name, "V(", 2) == 0) {
                char *end = strchr(node_name, ')');
                if (end) {
                    *end = '\0';
                    node_name += 2;
                }
            }

            Node *node = circuit_get_or_create_node(ckt, node_name);
            if (node) {
                node->init_voltage = value;
                node->has_init = 1;
            }
        }
    }

    return OK;
}

/* --- Parse .noise statement --- */

static int parse_noise(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 6, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
    if (ntokens < 5) {
        snprintf(last_error, sizeof(last_error), "Invalid .noise statement");
        return E_SYNTAX;
    }

    Analysis *ana = (Analysis *)calloc(1, sizeof(Analysis));
    if (ana == NULL)
        return E_NOMEM;

    ana->params.type = ANA_NOISE;

    /* Parse output: V(node) */
    char *out = tokens[0];
    if (strncmp(out, "V(", 2) == 0) {
        char *end = strchr(out, ')');
        if (end) {
            *end = '\0';
            out += 2;
        }
    }
    ana->params.noise_output = strdup(out);

    /* Parse source */
    ana->params.noise_src = strdup(tokens[1]);

    /* Parse sweep parameters */
    str_to_lower(tokens[2]);
    if (strcmp(tokens[2], "dec") == 0)
        ana->params.noise_sweep_type = SRC_DECADE;
    else if (strcmp(tokens[2], "oct") == 0)
        ana->params.noise_sweep_type = SRC_OCTAVE;
    else
        ana->params.noise_sweep_type = SRC_LINEAR;

    ana->params.noise_points = parse_number(tokens[3]);
    ana->params.noise_start = parse_number(tokens[4]);
    if (ntokens > 5)
        ana->params.noise_stop = parse_number(tokens[5]);

    ana->next = ckt->analyses;
    ckt->analyses = ana;

    return OK;
}

/* --- Parse .four statement (Fourier) --- */

static int parse_fourier(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 5, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
    if (ntokens < 2) {
        snprintf(last_error, sizeof(last_error), "Invalid .four statement");
        return E_SYNTAX;
    }

    Analysis *ana = (Analysis *)calloc(1, sizeof(Analysis));
    if (ana == NULL)
        return E_NOMEM;

    ana->params.type = ANA_FOURIER;
    ana->params.four_freq = parse_number(tokens[0]);

    /* Parse output: V(node) */
    char *out = tokens[1];
    if (strncmp(out, "V(", 2) == 0) {
        char *end = strchr(out, ')');
        if (end) {
            *end = '\0';
            out += 2;
        }
    }
    ana->params.sens_output = strdup(out);  /* Reuse field */

    if (ntokens > 2)
        ana->params.four_harmonics = (int)parse_number(tokens[2]);
    else
        ana->params.four_harmonics = 9;

    ana->next = ckt->analyses;
    ckt->analyses = ana;

    return OK;
}

/* --- Parse .sens statement (sensitivity) --- */

static int parse_sens(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 5, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
    if (ntokens < 1) {
        snprintf(last_error, sizeof(last_error), "Invalid .sens statement");
        return E_SYNTAX;
    }

    Analysis *ana = (Analysis *)calloc(1, sizeof(Analysis));
    if (ana == NULL)
        return E_NOMEM;

    ana->params.type = ANA_SENSITIVITY;

    /* Parse output: V(node) */
    char *out = tokens[0];
    if (strncmp(out, "V(", 2) == 0) {
        char *end = strchr(out, ')');
        if (end) {
            *end = '\0';
            out += 2;
        }
    }
    ana->params.sens_output = strdup(out);

    ana->next = ckt->analyses;
    ckt->analyses = ana;

    return OK;
}

/* --- Parse .subckt statement --- */

static int parse_subckt_def(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;

    char save_line[MAX_LINE];
    strncpy(save_line, line + 7, MAX_LINE - 1);
    save_line[MAX_LINE - 1] = '\0';

    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
    if (ntokens < 2) {
        snprintf(last_error, sizeof(last_error), "Invalid .subckt statement");
        return E_SYNTAX;
    }

    const char *name = tokens[0];
    SubcktDef *subckt = circuit_add_subckt(ckt, name);
    if (subckt == NULL)
        return E_NOMEM;

    /* Store port names */
    subckt->nports = ntokens - 1;
    for (int i = 0; i < subckt->nports && i < MAX_SUBCIT_ARGS; i++) {
        subckt->port_names[i] = strdup(tokens[i + 1]);
    }

    return OK;
}

/* --- Main parser --- */

const char *parser_get_error(void)
{
    return last_error;
}

Circuit *parser_parse_file(const char *filename)
{
    FILE *fp;
    char line[MAX_LINE];
    Circuit *ckt = NULL;
    char title_line[MAX_LINE] = {0};
    int in_subckt = 0;
    SubcktDef *current_subckt = NULL;
    char joined_line[MAX_LINE * MAX_CONT_LINES];
    char save_line[MAX_LINE];
    char *tokens[MAX_TOKENS];
    int ntokens;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        snprintf(last_error, sizeof(last_error), "Cannot open file: %s", filename);
        return NULL;
    }

    /* Read first line for title */
    if (fgets(line, MAX_LINE, fp) != NULL) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        if (line[0] != '*' && line[0] != '\0') {
            strncpy(title_line, line, MAX_LINE - 1);
        }
    }

    ckt = circuit_create(title_line);
    if (ckt == NULL) {
        fclose(fp);
        return NULL;
    }

    /* Read rest of file */
    while (fgets(line, MAX_LINE, fp) != NULL) {
        char *tokens[MAX_TOKENS];
        int ntokens;
        char save_line[MAX_LINE];

        /* Remove newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';

        /* Handle continuation lines (start with +) */
        if (line[0] == '+') {
            /* Append to joined_line */
            char *p = line + 1;
            while (*p && isspace(*p)) p++;
            if (*p) {
                strncat(joined_line, " ", MAX_LINE * MAX_CONT_LINES - strlen(joined_line) - 1);
                strncat(joined_line, p, MAX_LINE * MAX_CONT_LINES - strlen(joined_line) - 1);
            }
            continue;
        }

        /* Process previous joined line if any */
        if (strlen(joined_line) > 0) {
            /* Process the joined line */
            char proc_line[MAX_LINE * MAX_CONT_LINES];
            strncpy(proc_line, joined_line, sizeof(proc_line) - 1);
            proc_line[sizeof(proc_line) - 1] = '\0';

            /* Remove comments */
            char *comment = strchr(proc_line, '*');
            if (comment && comment > proc_line)
                *comment = '\0';

            /* Process proc_line */
            if (proc_line[0] != '\0') {
                /* Handle subcircuit definitions */
                if (in_subckt) {
                    if (strncasecmp(proc_line, ".ends", 5) == 0) {
                        in_subckt = 0;
                        current_subckt = NULL;
                        joined_line[0] = '\0';
                        continue;
                    }
                    /* Add device to subcircuit */
                    strncpy(save_line, proc_line, MAX_LINE - 1);
                    save_line[MAX_LINE - 1] = '\0';
                    ntokens = tokenize(save_line, tokens, MAX_TOKENS);
                    if (ntokens > 0) {
                        char first = tolower(proc_line[0]);
                        if (first == 'r') parse_resistor(ckt, tokens, ntokens);
                        else if (first == 'c') parse_capacitor(ckt, tokens, ntokens);
                        else if (first == 'l') parse_inductor(ckt, tokens, ntokens);
                        else if (first == 'v') parse_voltage_source(ckt, tokens, ntokens);
                        else if (first == 'i') parse_current_source(ckt, tokens, ntokens);
                        else if (first == 'g') parse_vccs(ckt, tokens, ntokens);
                        else if (first == 'e') parse_vcvs(ckt, tokens, ntokens);
                        else if (first == 'f') parse_cccs(ckt, tokens, ntokens);
                        else if (first == 'h') parse_ccvs(ckt, tokens, ntokens);
                        else if (first == 'd') parse_diode(ckt, tokens, ntokens);
                        else if (first == 'q') parse_bjt(ckt, tokens, ntokens);
                        else if (first == 'm') parse_mosfet(ckt, tokens, ntokens);
                        else if (first == 'b') parse_behsrc(ckt, tokens, ntokens);
                        else if (first == 's' || first == 'w') parse_switch(ckt, tokens, ntokens);
                        else if (first == 't') parse_tline(ckt, tokens, ntokens);
                        else if (first == 'x') parse_subckt_instance(ckt, tokens, ntokens);
                    }
                }
            }

            joined_line[0] = '\0';
        }

        /* Skip empty lines and comments */
        if (line[0] == '\0' || line[0] == ' ' || line[0] == '\t' || line[0] == '*')
            continue;

        /* Check for dot commands */
        if (line[0] == '.') {
            if (strncasecmp(line, ".subckt", 7) == 0) {
                parse_subckt_def(ckt, line);
                in_subckt = 1;
                current_subckt = circuit_find_subckt(ckt, line + 8);
                continue;
            }
            else if (strncasecmp(line, ".ends", 5) == 0) {
                in_subckt = 0;
                current_subckt = NULL;
                continue;
            }
            else if (strncasecmp(line, ".model", 6) == 0) {
                parse_model(ckt, line);
            }
            else if (strncasecmp(line, ".op", 3) == 0) {
                parse_op(ckt, line);
            }
            else if (strncasecmp(line, ".dc", 3) == 0) {
                parse_dc(ckt, line);
            }
            else if (strncasecmp(line, ".ac", 3) == 0) {
                parse_ac(ckt, line);
            }
            else if (strncasecmp(line, ".tran", 5) == 0) {
                parse_tran(ckt, line);
            }
            else if (strncasecmp(line, ".temp", 5) == 0) {
                parse_temp(ckt, line);
            }
            else if (strncasecmp(line, ".param", 6) == 0) {
                parse_param(ckt, line);
            }
            else if (strncasecmp(line, ".step", 5) == 0) {
                parse_step(ckt, line);
            }
            else if (strncasecmp(line, ".options", 8) == 0) {
                parse_options(ckt, line);
            }
            else if (strncasecmp(line, ".print", 6) == 0) {
                parse_print(ckt, line);
            }
            else if (strncasecmp(line, ".ic", 3) == 0) {
                parse_ic(ckt, line);
            }
            else if (strncasecmp(line, ".noise", 6) == 0) {
                parse_noise(ckt, line);
            }
            else if (strncasecmp(line, ".four", 5) == 0) {
                parse_fourier(ckt, line);
            }
            else if (strncasecmp(line, ".sens", 5) == 0) {
                parse_sens(ckt, line);
            }
            /* Ignore other dot commands */
            continue;
        }

        /* Start a new joined line */
        strncpy(joined_line, line, MAX_LINE - 1);
        joined_line[MAX_LINE - 1] = '\0';

        /* If not in subcircuit, process immediately */
        if (!in_subckt) {
            char proc_line[MAX_LINE * MAX_CONT_LINES];
            strncpy(proc_line, joined_line, sizeof(proc_line) - 1);
            proc_line[sizeof(proc_line) - 1] = '\0';

            /* Remove comments */
            char *comment = strchr(proc_line, '*');
            if (comment && comment > proc_line)
                *comment = '\0';

            if (proc_line[0] != '\0') {
                strncpy(save_line, proc_line, MAX_LINE - 1);
                save_line[MAX_LINE - 1] = '\0';
                ntokens = tokenize(save_line, tokens, MAX_TOKENS);
                if (ntokens > 0) {
                    char first = tolower(proc_line[0]);
                    if (first == 'r') parse_resistor(ckt, tokens, ntokens);
                    else if (first == 'c') parse_capacitor(ckt, tokens, ntokens);
                    else if (first == 'l') parse_inductor(ckt, tokens, ntokens);
                    else if (first == 'v') parse_voltage_source(ckt, tokens, ntokens);
                    else if (first == 'i') parse_current_source(ckt, tokens, ntokens);
                    else if (first == 'g') parse_vccs(ckt, tokens, ntokens);
                    else if (first == 'e') parse_vcvs(ckt, tokens, ntokens);
                    else if (first == 'f') parse_cccs(ckt, tokens, ntokens);
                    else if (first == 'h') parse_ccvs(ckt, tokens, ntokens);
                    else if (first == 'd') parse_diode(ckt, tokens, ntokens);
                    else if (first == 'q') parse_bjt(ckt, tokens, ntokens);
                    else if (first == 'm') parse_mosfet(ckt, tokens, ntokens);
                    else if (first == 'b') parse_behsrc(ckt, tokens, ntokens);
                    else if (first == 's' || first == 'w') parse_switch(ckt, tokens, ntokens);
                    else if (first == 't') parse_tline(ckt, tokens, ntokens);
                    else if (first == 'x') parse_subckt_instance(ckt, tokens, ntokens);
                }
            }

            joined_line[0] = '\0';
        }
    }

    /* Process last joined line */
    if (strlen(joined_line) > 0) {
        char proc_line[MAX_LINE * MAX_CONT_LINES];
        strncpy(proc_line, joined_line, sizeof(proc_line) - 1);
        proc_line[sizeof(proc_line) - 1] = '\0';

        char *comment = strchr(proc_line, '*');
        if (comment && comment > proc_line)
            *comment = '\0';

        if (proc_line[0] != '\0' && !in_subckt) {
            strncpy(save_line, proc_line, MAX_LINE - 1);
            save_line[MAX_LINE - 1] = '\0';
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (ntokens > 0) {
                char first = tolower(proc_line[0]);
                if (first == 'r') parse_resistor(ckt, tokens, ntokens);
                else if (first == 'c') parse_capacitor(ckt, tokens, ntokens);
                else if (first == 'l') parse_inductor(ckt, tokens, ntokens);
                else if (first == 'v') parse_voltage_source(ckt, tokens, ntokens);
                else if (first == 'i') parse_current_source(ckt, tokens, ntokens);
                else if (first == 'g') parse_vccs(ckt, tokens, ntokens);
                else if (first == 'e') parse_vcvs(ckt, tokens, ntokens);
                else if (first == 'f') parse_cccs(ckt, tokens, ntokens);
                else if (first == 'h') parse_ccvs(ckt, tokens, ntokens);
                else if (first == 'd') parse_diode(ckt, tokens, ntokens);
                else if (first == 'q') parse_bjt(ckt, tokens, ntokens);
                else if (first == 'm') parse_mosfet(ckt, tokens, ntokens);
                else if (first == 'b') parse_behsrc(ckt, tokens, ntokens);
                else if (first == 's' || first == 'w') parse_switch(ckt, tokens, ntokens);
                else if (first == 't') parse_tline(ckt, tokens, ntokens);
                else if (first == 'x') parse_subckt_instance(ckt, tokens, ntokens);
            }
        }
    }

    fclose(fp);

    /* Resolve branch current indices */
    circuit_resolve_branches(ckt);

    /* Initialize circuit */
    if (circuit_init(ckt) != OK) {
        circuit_free(ckt);
        return NULL;
    }

    return ckt;
}

Circuit *parser_parse_string(const char *input)
{
    char tmpfile[] = "/tmp/spice_XXXXXX.net";
    int fd = mkstemp(tmpfile);
    if (fd < 0)
        return NULL;

    write(fd, input, strlen(input));
    close(fd);

    Circuit *ckt = parser_parse_file(tmpfile);
    unlink(tmpfile);

    return ckt;
}