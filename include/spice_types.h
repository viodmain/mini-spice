/*
 * spice_types.h - Common type definitions for mini-spice
 * Extended version with waveform sources, BJT/MOSFET models,
 * behavioral sources, subcircuits, and advanced analyses
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
#include <complex.h>

/* Basic types */
typedef double real_t;
typedef int index_t;

/* Tolerances */
#define DEFAULT_ABSTOL   1.0e-12    /* Absolute current tolerance (A) */
#define DEFAULT_RELTOL   1.0e-6     /* Relative tolerance */
#define DEFAULT_VNTOL    1.0e-6     /* Voltage tolerance (V) */
#define DEFAULT_TRTOL    1.0        /* Truncation error tolerance */
#define DEFAULT_PIVTOL   1.0e-13    /* Pivoting tolerance */
#define DEFAULT_MAXITER  50         /* Max DC/transient iterations */
#define DEFAULT_GMIN     1.0e-12    /* Minimum conductance (S) */

/* Circuit constants */
#define NUMDEV 20                   /* Number of device types */
#define NUMANA 8                    /* Number of analysis types */
#define MAX_PWL_POINTS 100          /* Max piecewise-linear points */
#define MAX_SUBCIT_ARGS 20          /* Max subcircuit ports */

/* Thermal voltage at nominal temperature (27°C = 300.15K) */
#define VT_NOMINAL      0.02585     /* Vt at 27°C */
#define KB              1.380649e-23 /* Boltzmann constant (J/K) */
#define QE              1.602176634e-19 /* Electron charge (C) */
#define TNOM            27.0        /* Nominal temperature (°C) */
#define TNOM_K          300.15      /* Nominal temperature (K) */

/* Device type enumeration */
typedef enum {
    /* Basic linear devices */
    DEV_RESISTOR = 0,       /* R - Resistor */
    DEV_CAPACITOR,          /* C - Capacitor */
    DEV_INDUCTOR,           /* L - Inductor */
    DEV_VSRC,               /* V - Independent voltage source */
    DEV_ISRC,               /* I - Independent current source */

    /* Dependent sources */
    DEV_VCCS,               /* G - Voltage-controlled current source */
    DEV_VCVS,               /* E - Voltage-controlled voltage source */
    DEV_CCCS,               /* F - Current-controlled current source */
    DEV_CCVS,               /* H - Current-controlled voltage source */

    /* Nonlinear devices */
    DEV_DIODE,              /* D - Diode */
    DEV_NPN,                /* Q - NPN BJT */
    DEV_PNP,                /* Q - PNP BJT */
    DEV_NMOS,               /* M - NMOS transistor */
    DEV_PMOS,               /* M - PMOS transistor */

    /* Behavioral/advanced devices */
    DEV_Behavioral_SRC,     /* B - Behavioral source (current) */
    DEV_Behavioral_VSRC,    /* E-b - Behavioral voltage source */
    DEV_SWITCH_VOLTAGE,     /* S - Voltage-controlled switch */
    DEV_SWITCH_CURRENT,     /* W - Current-controlled switch */
    DEV_TRANSMISSION_LINE,  /* T - Lossless transmission line */
    DEV_SUBCKT,             /* X - Subcircuit instance */

    NUM_DEVICES
} device_type_t;

/* Analysis type enumeration */
typedef enum {
    ANA_DC_OP = 0,          /* DC operating point */
    ANA_DC_SWEEP,           /* DC transfer characteristic sweep */
    ANA_AC,                 /* AC small-signal analysis */
    ANA_TRANSIENT,          /* Transient analysis */
    ANA_NOISE,              /* Noise analysis */
    ANA_FOURIER,            /* Fourier analysis */
    ANA_SENSITIVITY,        /* Sensitivity analysis */
    ANA_POLE_ZERO,          /* Pole-zero analysis */
    NUM_ANALYSES
} analysis_type_t;

/* Waveform type for voltage/current sources */
typedef enum {
    WAVEFORM_NONE = 0,      /* DC only */
    WAVEFORM_SIN,           /* Sinusoidal: SIN(Voffset Vamp Freq Td Theta Phi) */
    WAVEFORM_PULSE,         /* Pulse: PULSE(V1 V2 Td Tr Tf Pw Per) */
    WAVEFORM_PWL,           /* Piecewise linear: PWL(t1 v1 t2 v2 ...) */
    WAVEFORM_EXP,           /* Exponential: EXP(V1 V2 Td1 Tau1 Td2 Tau2) */
    WAVEFORM_SFFM,          /* Sinusoidal frequency modulated */
    WAVEFORM_AC             /* AC small-signal only */
} waveform_type_t;

/* Integration method */
typedef enum {
    INTEGR_TRAPEZOIDAL = 0, /* Trapezoidal integration */
    INTEGR_GEAR             /* Gear integration (more stable, less accurate) */
} integration_method_t;

/* Return codes */
typedef enum {
    OK = 0,
    E_NOMEM,        /* Out of memory */
    E_SYNTAX,       /* Syntax error */
    E_NOTFOUND,     /* Not found */
    E_DUP,          /* Duplicate */
    E_LOOP,         /* Logical loop */
    E_CONVERGE,     /* Convergence failure */
    E_TROUBLE,      /* Serious trouble */
    E_OVERFLOW      /* Numerical overflow */
} spice_err_t;

/* Simulation options */
typedef struct {
    double abstol;          /* Absolute current tolerance */
    double vntol;           /* Voltage tolerance */
    double reltol;          /* Relative tolerance */
    double trtol;           /* Truncation error tolerance */
    int maxiter;            /* Max iterations for DC analysis */
    int trmaxiter;          /* Max iterations for transient */
    double gmin;            /* Minimum conductance */
    int gminsteps;          /* Gmin stepping count */
    int srcsteps;           /* Source stepping count */
    int numdgt;             /* Output significant digits */
    int method;             /* Integration method (trapezoidal/gear) */
    int ltol;               /* Current rel tolerance enable */
    double chgtol;          /* Charge tolerance */
} sim_options_t;

#endif /* SPICE_TYPES_H */
