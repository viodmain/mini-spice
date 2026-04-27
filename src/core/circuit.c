/*
 * circuit.c - Circuit data structures and management
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
    ckt->voltage = NULL;
    ckt->current = NULL;
    ckt->time = 0.0;
    ckt->temp = 300.15;       /* Default 27°C in Kelvin */
    ckt->temp_celsius = 27.0;
    ckt->abstol = DEFAULT_ABSTOL;
    ckt->reltol = DEFAULT_RELtol;
    ckt->vntol = DEFAULT_VNTOL;
    ckt->trtol = DEFAULT_TRTOL;
    ckt->maxiter = DEFAULT_MAXITER;
    ckt->num_vsources = 0;
    ckt->next_vsrc_branch = 0;
    
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
        free(ana);
        ana = next;
    }
    
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
    ckt->voltage = (double *)calloc(ckt->num_eqns + ckt->num_vsources + 1, sizeof(double));
    ckt->current = (double *)calloc(ckt->num_vsources + 1, sizeof(double));
    
    if (ckt->voltage == NULL || ckt->current == NULL) {
        fprintf(stderr, "Error: out of memory allocating result arrays\n");
        return E_NOMEM;
    }
    
    /* Initialize voltages to zero */
    for (i = 0; i <= ckt->num_eqns + ckt->num_vsources; i++)
        ckt->voltage[i] = 0.0;
    for (i = 0; i <= ckt->num_vsources; i++)
        ckt->current[i] = 0.0;
    
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
