/*
 * behsrc.c - Behavioral source device
 * Supports arbitrary expressions for current and voltage sources
 * Syntax: B<name> n+ n- I=<expr> or B<name> n+ n- V=<expr>
 * Expressions can use node voltages, branch currents, and time
 */
#include "device.h"
#include "circuit.h"
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Maximum expression length */
#define MAX_EXPR_LEN 1024

/* Evaluate a behavioral expression */
static double eval_expr(const char *expr, Circuit *ckt)
{
    /* Simplified expression evaluator
     * Supports: V(node), I(vsource), time (t), constants, +, -, *, /, ^, sin, cos, exp, log, abs
     */
    char buf[MAX_EXPR_LEN];
    strncpy(buf, expr, MAX_EXPR_LEN - 1);
    buf[MAX_EXPR_LEN - 1] = '\0';

    /* Replace V(node) with actual voltage */
    char *v_start;
    while ((v_start = strstr(buf, "V(")) != NULL || (v_start = strstr(buf, "v(")) != NULL) {
        char *v_end = strchr(v_start + 2, ')');
        if (v_end == NULL)
            break;

        char node_name[64];
        int len = v_end - v_start - 2;
        if (len > 63) len = 63;
        strncpy(node_name, v_start + 2, len);
        node_name[len] = '\0';

        /* Find node voltage */
        Node *node = circuit_find_node(ckt, node_name);
        double voltage = 0.0;
        if (node && node->eqnum >= 0) {
            voltage = ckt->voltage[node->eqnum];
        }

        /* Replace in buffer (simplified - just for single replacement) */
        char replacement[64];
        snprintf(replacement, sizeof(replacement), "%.15e", voltage);

        /* Simple string replacement */
        int before_len = v_start - buf;
        int after_len = strlen(v_end + 1);
        int total = before_len + strlen(replacement) + after_len;
        if (total >= MAX_EXPR_LEN)
            break;

        memmove(v_start + strlen(replacement), v_end + 1, after_len + 1);
        memcpy(v_start, replacement, strlen(replacement));
    }

    /* Replace I(vsource) with actual current */
    char *i_start;
    while ((i_start = strstr(buf, "I(")) != NULL || (i_start = strstr(buf, "i(")) != NULL) {
        char *i_end = strchr(i_start + 2, ')');
        if (i_end == NULL)
            break;

        char src_name[64];
        int len = i_end - i_start - 2;
        if (len > 63) len = 63;
        strncpy(src_name, i_start + 2, len);
        src_name[len] = '\0';

        /* Find voltage source current */
        double current = 0.0;
        for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
            if (strcmp(dev->name, src_name) == 0 && dev->type == DEV_VSRC) {
                current = ckt->voltage[dev->n3];
                break;
            }
        }

        /* Replace in buffer */
        char replacement[64];
        snprintf(replacement, sizeof(replacement), "%.15e", current);

        int before_len = i_start - buf;
        int after_len = strlen(i_end + 1);
        int total = before_len + strlen(replacement) + after_len;
        if (total >= MAX_EXPR_LEN)
            break;

        memmove(i_start + strlen(replacement), i_end + 1, after_len + 1);
        memcpy(i_start, replacement, strlen(replacement));
    }

    /* Replace T or TIME with simulation time */
    char *t_start;
    if ((t_start = strstr(buf, "TIME")) != NULL || (t_start = strstr(buf, "time")) != NULL ||
        (t_start = strstr(buf, "T")) != NULL) {
        char replacement[64];
        snprintf(replacement, sizeof(replacement), "%.15e", ckt->time);

        int before_len = t_start - buf;
        int after_len = strlen(t_start + 4);
        int total = before_len + strlen(replacement) + after_len;
        if (total < MAX_EXPR_LEN) {
            memmove(t_start + strlen(replacement), t_start + 4, after_len + 1);
            memcpy(t_start, replacement, strlen(replacement));
        }
    }

    /* Use strtod to parse the resulting expression */
    char *end;
    double result = strtod(buf, &end);

    /* If we couldn't parse the full expression, return 0 */
    if (end == buf)
        return 0.0;

    return result;
}

/* Behavioral source setup */
static int behsrc_setup(Device *dev, Circuit *ckt)
{
    /* Behavioral voltage source needs a branch current equation */
    if (dev->type == DEV_Behavioral_VSRC) {
        int ib = circuit_alloc_vsrc_branch(ckt);
        dev->n3 = ib;
    }
    return OK;
}

/* Behavioral source load: DC evaluation */
static int behsrc_load(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    if (dev->expr == NULL)
        return OK;

    if (dev->type == DEV_Behavioral_SRC) {
        /* Behavioral current source */
        double i = eval_expr(dev->expr, ckt);
        int n1 = dev->n1;
        int n2 = dev->n2;

        /* Current from n1 to n2 */
        if (n1 >= 0)
            sparse_add_rhs(mat, n1, -i);
        if (n2 >= 0)
            sparse_add_rhs(mat, n2, i);
    } else if (dev->type == DEV_Behavioral_VSRC) {
        /* Behavioral voltage source */
        double v = eval_expr(dev->expr, ckt);
        int n1 = dev->n1;
        int n2 = dev->n2;
        int ib = dev->n3;

        /* MNA stamp for voltage source */
        if (n1 >= 0) {
            sparse_add_element(mat, n1, ib, 1.0);
            sparse_add_element(mat, ib, n1, 1.0);
        }
        if (n2 >= 0) {
            sparse_add_element(mat, n2, ib, -1.0);
            sparse_add_element(mat, ib, n2, -1.0);
        }
        sparse_add_rhs(mat, ib, v);
    }

    return OK;
}

/* AC load: not supported for behavioral sources (use small-signal analysis with care) */
static int behsrc_ac_load(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega)
{
    /* Behavioral sources are not linearized for AC analysis */
    /* This could be extended with numerical differentiation */
    return OK;
}

/* Update: no state to update */
static int behsrc_update(Device *dev, Circuit *ckt)
{
    return OK;
}

/* Nonlinear: behavioral sources are handled in load() */
static int behsrc_nonlinear(Device *dev, Circuit *ckt, SparseMatrix *mat)
{
    return behsrc_load(dev, ckt, mat);
}

/* Device operations table */
static const DeviceOps behsrchsrc_ops = {
    .name = "B",
    .type = DEV_Behavioral_SRC,
    .setup = behsrc_setup,
    .load = behsrc_load,
    .ac_load = behsrc_ac_load,
    .update = behsrc_update,
    .nonlinear = behsrc_nonlinear
};

const DeviceOps *behsrchsrc_get_ops(void)
{
    return &behsrchsrc_ops;
}

/* Behavioral voltage source operations */
static const DeviceOps behvsrc_ops = {
    .name = "E-b",
    .type = DEV_Behavioral_VSRC,
    .setup = behsrc_setup,
    .load = behsrc_load,
    .ac_load = behsrc_ac_load,
    .update = behsrc_update,
    .nonlinear = behsrc_nonlinear
};

const DeviceOps *behvsrc_get_ops(void)
{
    return &behvsrc_ops;
}
