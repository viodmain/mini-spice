/*
 * output.h - Output/results interface
 */
#ifndef OUTPUT_H
#define OUTPUT_H

#include "spice_types.h"
#include "circuit.h"

/* Print circuit summary */
void output_print_circuit(Circuit *ckt);

/* Print operating point results */
void output_print_op(Circuit *ckt);

/* Print DC sweep results */
void output_print_dc(Circuit *ckt, const char *src_name, 
                     double *v_data, int num_points);

/* Print AC analysis results */
void output_print_ac(Circuit *ckt, const char *node_name,
                     double *mag, double *phase, int num_points);

/* Print transient analysis results */
void output_print_trans(Circuit *ckt, const char *node_name,
                        double *v_data, int num_points);

/* Print results to file */
int output_write_raw(const char *filename, Circuit *ckt);

#endif /* OUTPUT_H */
