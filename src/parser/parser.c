/*
 * parser.c - SPICE netlist parser
 * Simplified single-pass parser for basic netlist syntax
 */
#include "parser.h"
#include "circuit.h"
#include "device.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 4096
#define MAX_TOKENS 20

static char last_error[256] = {0};

/* --- Helper functions --- */

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
    str_to_lower((char *)end);
    if (strcmp(end, "meg") == 0 || strcmp(end, "mege") == 0) val *= 1e6;
    else if (strcmp(end, "k") == 0) val *= 1e3;
    else if (strcmp(end, "m") == 0) val *= 1e-3;
    else if (strcmp(end, "u") == 0) val *= 1e-6;
    else if (strcmp(end, "n") == 0) val *= 1e-9;
    else if (strcmp(end, "p") == 0) val *= 1e-12;
    else if (strcmp(end, "f") == 0) val *= 1e-15;
    else if (strcmp(end, "t") == 0) val *= 1e12;
    else if (strcmp(end, "g") == 0) val *= 1e9;
    
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

static int parse_voltage_source(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Voltage source needs at least 4 tokens");
        return E_SYNTAX;
    }
    
    const char *name = tokens[0];
    const char *n1 = tokens[1];
    const char *n2 = tokens[2];
    
    /* Parse DC value - could be "DC <value>" or just "<value>" */
    double v = 0.0;
    int val_idx = 3;
    
    if (ntokens >= 5 && strcasecmp(tokens[3], "DC") == 0) {
        v = parse_number(tokens[4]);
        val_idx = 5;
    } else if (ntokens >= 4) {
        v = parse_number(tokens[3]);
        val_idx = 4;
    }
    
    /* Parse AC value if present */
    double vac = 1.0;  /* Default AC magnitude */
    for (int i = val_idx; i < ntokens - 1; i++) {
        if (strcasecmp(tokens[i], "AC") == 0) {
            vac = parse_number(tokens[i + 1]);
            break;
        }
    }
    
    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);
    
    /* Voltage source needs a branch current equation
     * Allocate branch index (will be resolved to equation number later) */
    int ib = circuit_alloc_vsrc_branch(ckt);
    
    Device *dev = circuit_add_device4(ckt, name, DEV_VSRC, eq1, eq2, ib, -1, v);
    if (dev == NULL)
        return E_NOMEM;
    
    dev->value2 = vac;  /* Store AC magnitude */
    
    return OK;
}

static int parse_current_source(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Current source needs at least 4 tokens");
        return E_SYNTAX;
    }
    
    const char *name = tokens[0];
    const char *n1 = tokens[1];
    const char *n2 = tokens[2];
    const char *i_str = tokens[3];
    
    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);
    double i = parse_number(i_str);
    
    Device *dev = circuit_add_device(ckt, name, DEV_ISRC, eq1, eq2, i);
    if (dev == NULL)
        return E_NOMEM;
    
    return OK;
}

static int parse_vccs(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 6) {
        snprintf(last_error, sizeof(last_error), "VCCS needs 6 tokens: Gname n+ n- nc+ nc- gm");
        return E_SYNTAX;
    }
    
    const char *name = tokens[0];
    const char *n1 = tokens[1];  /* Output + */
    const char *n2 = tokens[2];  /* Output - */
    const char *n3 = tokens[3];  /* Control + */
    const char *n4 = tokens[4];  /* Control - */
    const char *gm_str = tokens[5];
    
    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);
    int eq3 = get_node_eq(ckt, n3);
    int eq4 = get_node_eq(ckt, n4);
    double gm = parse_number(gm_str);
    
    Device *dev = circuit_add_device4(ckt, name, DEV_VCCS, eq1, eq2, eq3, eq4, gm);
    if (dev == NULL)
        return E_NOMEM;
    
    return OK;
}

static int parse_vcvs(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 7) {
        snprintf(last_error, sizeof(last_error), "VCVS needs 7 tokens: Ename n+ n- nc+ nc- gain");
        return E_SYNTAX;
    }
    
    const char *name = tokens[0];
    const char *n1 = tokens[1];  /* Output + */
    const char *n2 = tokens[2];  /* Output - */
    const char *n3 = tokens[3];  /* Control + */
    const char *n4 = tokens[4];  /* Control - */
    const char *gain_str = tokens[5];
    
    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);
    int eq3 = get_node_eq(ckt, n3);
    int eq4 = get_node_eq(ckt, n4);
    double gain = parse_number(gain_str);
    
    /* VCVS needs a branch current equation */
    int ib = circuit_alloc_vsrc_branch(ckt);
    
    Device *dev = circuit_add_device5(ckt, name, DEV_VCVS, eq1, eq2, eq3, eq4, ib, gain);
    if (dev == NULL)
        return E_NOMEM;
    
    return OK;
}

static int parse_cccs(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 5) {
        snprintf(last_error, sizeof(last_error), "CCCS needs 5 tokens: Fname n+ n- Vctrl gain");
        return E_SYNTAX;
    }
    
    const char *name = tokens[0];
    const char *n1 = tokens[1];  /* Output + */
    const char *n2 = tokens[2];  /* Output - */
    const char *vctrl_name = tokens[3];  /* Controlling voltage source */
    const char *gain_str = tokens[4];
    
    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);
    double gain = parse_number(gain_str);
    
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

static int parse_ccvs(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 6) {
        snprintf(last_error, sizeof(last_error), "CCVS needs 6 tokens: Hname n+ n- Vctrl Vbranch gain");
        return E_SYNTAX;
    }
    
    const char *name = tokens[0];
    const char *n1 = tokens[1];  /* Output + */
    const char *n2 = tokens[2];  /* Output - */
    const char *vctrl_name = tokens[3];  /* Controlling voltage source */
    const char *gain_str = tokens[4];
    
    int eq1 = get_node_eq(ckt, n1);
    int eq2 = get_node_eq(ckt, n2);
    double gain = parse_number(gain_str);
    
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
    
    /* CCVS needs an output branch current equation */
    int ib_out = circuit_alloc_vsrc_branch(ckt);
    
    Device *dev = circuit_add_device5(ckt, name, DEV_CCVS, eq1, eq2, ib_out, vctrl_eq, -1, gain);
    if (dev == NULL)
        return E_NOMEM;
    
    return OK;
}

static int parse_diode(Circuit *ckt, char **tokens, int ntokens)
{
    if (ntokens < 4) {
        snprintf(last_error, sizeof(last_error), "Diode needs at least 4 tokens: Dname n+ n- model");
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
            /* Create default diode parameters */
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

/* --- Parse .model statement --- */

static int parse_model(Circuit *ckt, char *line)
{
    char *tokens[MAX_TOKENS];
    int ntokens;
    
    /* Skip ".model" */
    char *p = line + 6;
    while (*p && isspace(*p)) p++;
    
    char save_line[MAX_LINE];
    strncpy(save_line, p, MAX_LINE - 1);
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
    else {
        snprintf(last_error, sizeof(last_error), "Unknown model type: %s", type_str);
        return E_SYNTAX;
    }
    
    /* Create model */
    Model *model = circuit_add_model(ckt, model_name, type);
    if (model == NULL)
        return E_NOMEM;
    
    /* Set model parameters */
    for (int i = 2; i < ntokens; i++) {
        char *eq = strchr(tokens[i], '=');
        if (eq) {
            *eq = '\0';
            double value = parse_number(eq + 1);
            
            if (type == DEV_DIODE) {
                extern int dio_set_param(Model *, const char *, double);
                dio_set_param(model, tokens[i], value);
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
    
    /* Sweep type: dec, oct, lin */
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
    
    fp = fopen(filename, "r");
    if (fp == NULL) {
        snprintf(last_error, sizeof(last_error), "Cannot open file: %s", filename);
        return NULL;
    }
    
    /* Read first line for title */
    if (fgets(line, MAX_LINE, fp) != NULL) {
        /* Remove newline */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        
        /* First line is title if it doesn't start with * */
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
        
        /* Remove newline and comments */
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *comment = strchr(line, '*');
        if (comment && comment > line)
            *comment = '\0';
        
        /* Skip empty lines */
        if (line[0] == '\0' || line[0] == ' ' || line[0] == '\t')
            continue;
        
        /* Skip comment lines */
        if (line[0] == '*')
            continue;
        
        /* Copy line for tokenizing */
        strncpy(save_line, line, MAX_LINE - 1);
        save_line[MAX_LINE - 1] = '\0';
        
        /* Check first character to determine type */
        char first = tolower(line[0]);
        
        if (first == 'r') {
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (parse_resistor(ckt, tokens, ntokens) != OK) {
                fprintf(stderr, "Error parsing line: %s\n", line);
                fprintf(stderr, "%s\n", last_error);
            }
        }
        else if (first == 'c') {
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (parse_capacitor(ckt, tokens, ntokens) != OK) {
                fprintf(stderr, "Error parsing line: %s\n", line);
                fprintf(stderr, "%s\n", last_error);
            }
        }
        else if (first == 'l') {
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (parse_inductor(ckt, tokens, ntokens) != OK) {
                fprintf(stderr, "Error parsing line: %s\n", line);
                fprintf(stderr, "%s\n", last_error);
            }
        }
        else if (first == 'v') {
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (parse_voltage_source(ckt, tokens, ntokens) != OK) {
                fprintf(stderr, "Error parsing line: %s\n", line);
                fprintf(stderr, "%s\n", last_error);
            }
        }
        else if (first == 'i') {
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (parse_current_source(ckt, tokens, ntokens) != OK) {
                fprintf(stderr, "Error parsing line: %s\n", line);
                fprintf(stderr, "%s\n", last_error);
            }
        }
        else if (first == 'g') {
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (parse_vccs(ckt, tokens, ntokens) != OK) {
                fprintf(stderr, "Error parsing line: %s\n", line);
                fprintf(stderr, "%s\n", last_error);
            }
        }
        else if (first == 'e') {
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (parse_vcvs(ckt, tokens, ntokens) != OK) {
                fprintf(stderr, "Error parsing line: %s\n", line);
                fprintf(stderr, "%s\n", last_error);
            }
        }
        else if (first == 'f') {
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (parse_cccs(ckt, tokens, ntokens) != OK) {
                fprintf(stderr, "Error parsing line: %s\n", line);
                fprintf(stderr, "%s\n", last_error);
            }
        }
        else if (first == 'h') {
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (parse_ccvs(ckt, tokens, ntokens) != OK) {
                fprintf(stderr, "Error parsing line: %s\n", line);
                fprintf(stderr, "%s\n", last_error);
            }
        }
        else if (first == 'd') {
            ntokens = tokenize(save_line, tokens, MAX_TOKENS);
            if (parse_diode(ckt, tokens, ntokens) != OK) {
                fprintf(stderr, "Error parsing line: %s\n", line);
                fprintf(stderr, "%s\n", last_error);
            }
        }
        else if (line[0] == '.') {
            /* Dot command */
            if (strncasecmp(line, ".model", 6) == 0) {
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
            /* Ignore other dot commands for now */
        }
        /* Skip continuation lines (start with +) */
        else if (first == '+') {
            continue;
        }
    }
    
    fclose(fp);
    
    /* Resolve branch current indices to equation numbers */
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
    /* For now, write to temp file and parse */
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
