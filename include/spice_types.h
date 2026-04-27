/*
 * spice_types.h - Common type definitions for mini-spice
 */
#ifndef SPICE_TYPES_H
#define SPICE_TYPES_H

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <float.h>
#include <stdbool.h>
#include <unistd.h>

/* Basic types */
typedef double real_t;
typedef int index_t;

/* Tolerances */
#define DEFAULT_ABSTOL   1.0e-12    /* Absolute tolerance */
#define DEFAULT_RELtol   1.0e-6     /* Relative tolerance */
#define DEFAULT_VNTOL    1.0e-6     /* Voltage tolerance */
#define DEFAULT_TRTOL    1.0        /* Truncation error tolerance */
#define DEFAULT_PIVTOL   1.0e-13    /* Pivoting tolerance */
#define DEFAULT_MAXITER  50         /* Max DC/transient iterations */
#define DEFAULT_RELTOL   0.001      /* Relative tolerance for convergence */

/* Circuit constants */
#define NUMDEV 10                   /* Number of device types */
#define NUMANA 4                    /* Number of analysis types */

/* Device type enumeration */
typedef enum {
    DEV_RESISTOR = 0,
    DEV_CAPACITOR,
    DEV_INDUCTOR,
    DEV_VSRC,
    DEV_ISRC,
    DEV_VCCS,
    DEV_VCVS,
    DEV_CCCS,
    DEV_CCVS,
    DEV_DIODE,
    NUM_DEVICES
} device_type_t;

/* Analysis type enumeration */
typedef enum {
    ANA_DC_OP = 0,      /* DC operating point */
    ANA_DC_SWEEP,       /* DC transfer characteristic */
    ANA_AC,             /* AC small-signal analysis */
    ANA_TRANSIENT,      /* Transient analysis */
    NUM_ANALYSES
} analysis_type_t;

/* Integration method */
typedef enum {
    INTEGR_TRAPEZOIDAL = 0,
    INTEGR_GEAR
} integration_method_t;

/* Return codes */
typedef enum {
    OK = 0,
    E_NOMEM,
    E_SYNTAX,
    E_NOTFOUND,
    E_DUP,
    E_LOOP,
    E_CONVERGE,
    E_TROUBLE
} spice_err_t;

#endif /* SPICE_TYPES_H */
