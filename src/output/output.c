/*
 * output.c - Output/results handling
 */
#include "output.h"
#include <stdio.h>
#include <stdlib.h>

void output_print_circuit(Circuit *ckt)
{
    printf("\n=== Circuit: %s ===\n", ckt->title);
    printf("Temperature: %.1f°C (%.2f K)\n", ckt->temp_celsius, ckt->temp);
    printf("Nodes: %d, Equations: %d\n", ckt->num_nodes, ckt->num_eqns);
    printf("Voltage sources: %d\n", ckt->num_vsources);
    
    printf("\nDevices:\n");
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        printf("  %s: type=%c, n1=%d, n2=%d", dev->name, 
               dev->type < 10 ? 'R' + dev->type : '?',
               dev->n1, dev->n2);
        if (dev->n3 >= 0)
            printf(", n3=%d", dev->n3);
        if (dev->n4 >= 0)
            printf(", n4=%d", dev->n4);
        printf(", value=%g\n", dev->value);
    }
    
    printf("\nAnalyses:\n");
    for (Analysis *ana = ckt->analyses; ana != NULL; ana = ana->next) {
        printf("  Type: %d\n", ana->params.type);
    }
}

void output_print_op(Circuit *ckt)
{
    printf("\n=== Operating Point ===\n");
    
    printf("\nNode Voltages:\n");
    for (Node *node = ckt->nodes; node != NULL; node = node->next) {
        if (node->is_ground)
            printf("  %s: 0 V\n", node->name);
        else if (node->eqnum >= 0)
            printf("  %s: %g V\n", node->name, ckt->voltage[node->eqnum]);
    }
    
    printf("\nVoltage Source Currents:\n");
    for (Device *dev = ckt->devices; dev != NULL; dev = dev->next) {
        if (dev->type == DEV_VSRC) {
            printf("  %s: current through branch\n", dev->name);
        }
    }
}

void output_print_dc(Circuit *ckt, const char *src_name,
                     double *v_data, int num_points)
{
    printf("\n=== DC Sweep Results ===\n");
    printf("Source: %s\n", src_name);
    printf("Points: %d\n", num_points);
}

void output_print_ac(Circuit *ckt, const char *node_name,
                     double *mag, double *phase, int num_points)
{
    printf("\n=== AC Analysis Results ===\n");
    printf("Node: %s\n", node_name);
    printf("Points: %d\n", num_points);
}

void output_print_trans(Circuit *ckt, const char *node_name,
                        double *v_data, int num_points)
{
    printf("\n=== Transient Analysis Results ===\n");
    printf("Node: %s\n", node_name);
    printf("Points: %d\n", num_points);
}

int output_write_raw(const char *filename, Circuit *ckt)
{
    FILE *fp = fopen(filename, "w");
    if (fp == NULL)
        return E_TROUBLE;
    
    /* Write raw file header */
    fprintf(fp, "Title: %s\n", ckt->title);
    fprintf(fp, "Variables:\n");
    
    int idx = 0;
    for (Node *node = ckt->nodes; node != NULL; node = node->next) {
        if (!node->is_ground) {
            fprintf(fp, "%d\t%s\tvoltage\n", idx++, node->name);
        }
    }
    
    fclose(fp);
    return OK;
}
