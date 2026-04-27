/*
 * main.c - Main entry point for mini-spice
 */
#include "spice_types.h"
#include "circuit.h"
#include "parser.h"
#include "device.h"
#include "analysis.h"
#include "output.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog)
{
    printf("Usage: %s [options] <netlist_file>\n", prog);
    printf("\nOptions:\n");
    printf("  -h, --help          Show this help\n");
    printf("  -v, --version       Show version\n");
    printf("  -o <file>           Output file\n");
    printf("  -T <temp>           Set temperature (Celsius)\n");
    printf("\nSupported analyses:\n");
    printf("  .op                 DC operating point\n");
    printf("  .dc <src> <start> <stop> <step>  DC sweep\n");
    printf("  .ac <type> <points> <start> <stop>  AC analysis\n");
    printf("  .tran <tstep> <tstop>  Transient analysis\n");
}

static void print_version(void)
{
    printf("mini-spice v0.1 - A minimal SPICE simulator\n");
    printf("Based on ngspice-45 architecture\n");
    printf("Copyright (c) 2025\n");
}

int main(int argc, char *argv[])
{
    const char *netlist_file = NULL;
    const char *output_file = NULL;
    double temp_celsius = 27.0;
    int i;
    
    /* Parse command-line arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        }
        else if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            temp_celsius = atof(argv[++i]);
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
        else {
            netlist_file = argv[i];
        }
    }
    
    if (netlist_file == NULL) {
        fprintf(stderr, "Error: no input file specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Initialize devices and analyses */
    device_init_all();
    analysis_init_all();
    
    /* Parse netlist */
    printf("Reading netlist: %s\n", netlist_file);
    Circuit *ckt = parser_parse_file(netlist_file);
    if (ckt == NULL) {
        fprintf(stderr, "Error: failed to parse netlist\n");
        fprintf(stderr, "%s\n", parser_get_error());
        return 1;
    }
    
    /* Set temperature */
    ckt->temp_celsius = temp_celsius;
    ckt->temp = temp_celsius + 273.15;
    
    /* Print circuit summary */
    output_print_circuit(ckt);
    
    /* Run analyses */
    if (ckt->analyses == NULL) {
        printf("\nNo analyses specified. Adding default .op\n");
        Analysis *ana = (Analysis *)calloc(1, sizeof(Analysis));
        ana->params.type = ANA_DC_OP;
        ana->next = ckt->analyses;
        ckt->analyses = ana;
    }
    
    analysis_run_all(ckt);
    
    /* Write output if requested */
    if (output_file) {
        printf("\nWriting results to: %s\n", output_file);
        output_write_raw(output_file, ckt);
    }
    
    /* Cleanup */
    circuit_free(ckt);
    
    printf("\nSimulation complete.\n");
    return 0;
}
