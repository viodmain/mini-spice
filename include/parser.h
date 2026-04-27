/*
 * parser.h - Netlist parser interface
 */
#ifndef PARSER_H
#define PARSER_H

#include "spice_types.h"
#include "circuit.h"

/* Parse a netlist file and create a circuit */
Circuit *parser_parse_file(const char *filename);

/* Parse a netlist from a string */
Circuit *parser_parse_string(const char *input);

/* Get last error message */
const char *parser_get_error(void);

#endif /* PARSER_H */
