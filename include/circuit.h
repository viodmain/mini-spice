/*
 * circuit.h - Circuit data structures
 * Extended with waveform sources, BJT/MOSFET models, subcircuits
 */
#ifndef CIRCUIT_H
#define CIRCUIT_H

#include "spice_types.h"

/* Forward declarations */
typedef struct Circuit Circuit;
typedef struct Node Node;
typedef struct Device Device;
typedef struct Model Model;
typedef struct Analysis Analysis;
typedef struct SubcktDef SubcktDef;
typedef struct SubcktPort SubcktPort;

/* --- Node --- */
struct Node {
    char *name;           /* Node name */
    int number;           /* Node number (internal) */
    int eqnum;            /* Equation number in matrix (-1 if none) */
    int is_ground;        /* 1 if this is ground (node 0) */
    int is_vsource;       /* 1 if connected to voltage source branch */
    double init_voltage;  /* Initial condition voltage (for .IC) */
    int has_init;         /* 1 if initial condition is specified */
    Node *next;           /* Next node in list */
};

/* --- Waveform parameters for sources --- */
typedef struct {
    waveform_type_t type;

    /* SIN parameters */
    double sin_voffset;       /* Offset voltage */
    double sin_vamp;          /* Amplitude */
    double sin_freq;          /* Frequency (Hz) */
    double sin_td;            /* Delay time */
    double sin_theta;         /* Damping factor */
    double sin_phi;           /* Phase (degrees) */

    /* PULSE parameters */
    double pulse_v1;          /* Initial value */
    double pulse_v2;          /* Pulsed value */
    double pulse_td;          /* Delay time */
    double pulse_tr;          /* Rise time */
    double pulse_tf;          /* Fall time */
    double pulse_pw;          /* Pulse width */
    double pulse_per;         /* Period */

    /* PWL parameters */
    int pwl_npoints;          /* Number of points */
    double *pwl_time;         /* Time values */
    double *pwl_value;        /* Value at each time */

    /* EXP parameters */
    double exp_v1;            /* Initial value */
    double exp_v2;            /* Peak value */
    double exp_td1;           /* Rise delay time */
    double exp_tau1;          /* Rise time constant */
    double exp_td2;           /* Fall delay time */
    double exp_tau2;          /* Fall time constant */

    /* AC parameters */
    double ac_mag;            /* AC magnitude */
    double ac_phase;          /* AC phase (degrees) */
} waveform_params_t;

/* --- Device instance --- */
struct Device {
    char *name;           /* Device instance name (R1, C2, etc.) */
    device_type_t type;   /* Device type */
    int n1, n2;           /* Node connections (n1, n2 for 2-terminal) */
    int n3, n4, n5;       /* Additional nodes (for dependent sources, MNA) */
    double value;         /* Primary value (R, C, L, gain, etc.) */
    double value2;        /* Secondary value (for some devices) */
    void *params;         /* Device-specific parameters */
    Model *model;         /* Model pointer (for diodes, BJT, MOSFET) */
    Device *next;         /* Next device in list */

    /* Waveform for sources */
    waveform_params_t *waveform;  /* Time-varying waveform */

    /* Subcircuit specific */
    SubcktDef *subckt_def;        /* Subcircuit definition */
    Device *sub_devices;          /* Devices inside subcircuit */
    int *port_map;                /* Port-to-node mapping */

    /* Behavioral source expression (stored as string) */
    char *expr;           /* Behavioral expression string */
};

/* --- Model --- */
struct Model {
    char *name;           /* Model name */
    device_type_t type;   /* Device type this model is for */
    void *params;         /* Model parameters */
    Model *next;          /* Next model in list */
};

/* --- Diode model parameters --- */
typedef struct {
    double is;            /* Saturation current */
    double n;             /* Emission coefficient */
    double rs;            /* Ohmic resistance */
    double cjo;           /* Zero-bias junction capacitance */
    double vj;            /* Junction potential */
    double m;             /* Grading coefficient */
    double tt;            /* Transit time */
    double eg;            /* Activation energy */
    double xti;           /* Temperature exponent */
    double kf;            /* Flicker noise coefficient */
    double af;            /* Flicker noise exponent */
    double fc;            /* Forward bias coefficient */
} diode_model_t;

/* --- BJT model parameters (simple Ebers-Moll) --- */
typedef struct {
    /* BJT type: NPN or PNP */
    int polarity;         /* 1 = NPN, -1 = PNP */

    /* Transport parameters */
    double is;            /* Saturation current */
    double bf;            /* Ideal maximum forward beta */
    double nf;            /* Forward emission coefficient */
    double br;            /* Ideal maximum reverse beta */
    double nr;            /* Reverse emission coefficient */

    /* Parasitic resistances */
    double rb;            /* Ohmic base resistance */
    double re;            /* Ohmic emitter resistance */
    double rc;            /* Ohmic collector resistance */

    /* Junction capacitances */
    double cje;           /* Base-emitter zero-bias capacitance */
    double vje;           /* Base-emitter built-in potential */
    double me;            /* Base-emitter grading coefficient */
    double cjcs;          /* Base-collector substrate capacitance */
    double vjc;           /* Base-collector built-in potential */
    double mc;            /* Base-collector grading coefficient */

    /* Transit times */
    double tf;            /* Ideal forward transit time */
    double tr;            /* Ideal reverse transit time */

    /* Breakdown */
    double bvc;           /* Base-collector breakdown voltage */
    double bve;           /* Base-emitter breakdown voltage */
    double ibvc;          /* Current at BVC breakdown */
    double ibve;          /* Current at BVE breakdown */

    /* Temperature */
    double eg;            /* Activation energy */
    double xti;           /* Temperature exponent for IS */
} bjt_model_t;

/* --- MOSFET model parameters (Level 1 - Shichman-Hodges) --- */
typedef struct {
    /* MOSFET type: NMOS or PMOS */
    int polarity;         /* 1 = NMOS, -1 = PMOS */

    /* Process parameters */
    double kp;            /* Transconductance parameter (A/V^2) */
    double vto;           /* Threshold voltage */
    double gamma;         /* Body effect parameter */
    double lambda;        /* Channel length modulation */
    double phi;           /* Surface potential */

    /* Geometric parameters (can be overridden per instance) */
    double w;             /* Channel width */
    double l;             /* Channel length */

    /* Junction capacitances */
    double cj;            /* Bottom junction capacitance */
    double mj;            /* Bottom junction grading coefficient */
    double cjsw;          /* Side junction capacitance */
    double mjs;           /* Side junction grading coefficient */
    double cjo;           /* Zero-bias depletion capacitance */

    /* Resistances */
    double rd;            /* Drain ohmic resistance */
    double rs;            /* Source ohmic resistance */
    double rb;            /* Gate ohmic resistance */

    /* Mobility and velocity */
    double u0;            /* Surface mobility */
    double vmax;          /* Maximum lateral velocity */

    /* Temperature */
    double eg;            /* Activation energy */
    double xti;           /* Temperature exponent */
} mos_model_t;

/* --- Switch model parameters --- */
typedef struct {
    double ron;           /* On resistance */
    double roff;          /* Off resistance */
    double vt;            /* Threshold voltage (on/off) */
    double vh;            /* Hysteresis voltage */
} switch_model_t;

/* --- Transmission line parameters --- */
typedef struct {
    double td;            /* Delay time */
    double z0;            /* Characteristic impedance */
    double f;             /* Frequency (for lossy) */
    double n;             /* Number of segments */
} transmission_line_t;

/* --- Subcircuit definition --- */
struct SubcktDef {
    char *name;                   /* Subcircuit name */
    int nports;                   /* Number of ports */
    char *port_names[MAX_SUBCIT_ARGS]; /* Port names */
    Device *devices;              /* Device list inside subcircuit */
    Node *nodes;                  /* Node list inside subcircuit */
    SubcktDef *next;              /* Next definition */
};

/* --- Analysis parameters --- */
typedef enum {
    SRC_LINEAR,
    SRC_OCTAVE,
    SRC_DECADE
} sweep_type_t;

typedef struct {
    analysis_type_t type;

    /* DC sweep parameters */
    char *src_name;       /* Source to sweep */
    double start, stop;   /* Sweep range */
    double step;          /* Sweep step */
    sweep_type_t sweep_type;

    /* AC analysis parameters */
    double ac_start, ac_stop;
    double ac_points;
    sweep_type_t ac_sweep_type;

    /* Transient parameters */
    double tstart, tstop;
    double tstep;
    double tmax;
    integration_method_t integration;
    int use_uic;          /* Use initial conditions (skip DC OP) */

    /* Noise analysis parameters */
    char *noise_output;   /* Output node */
    char *noise_src;      /* Input source */
    double noise_start, noise_stop;
    double noise_points;
    sweep_type_t noise_sweep_type;

    /* Fourier analysis parameters */
    double four_freq;     /* Fundamental frequency */
    int four_harmonics;   /* Number of harmonics */

    /* Sensitivity analysis */
    char *sens_output;    /* Output variable */

    /* Pole-zero analysis */
    char *pz_input;       /* Input source */
    char *pz_output;      /* Output node */

} analysis_params_t;

/* --- Analysis --- */
struct Analysis {
    analysis_params_t params;
    Analysis *next;
};

/* --- Parameter --- */
typedef struct Param {
    char *name;
    double value;
    struct Param *next;
} Param;

/* --- Circuit --- */
struct Circuit {
    char *title;          /* Circuit title */
    Node *nodes;          /* Node list */
    int num_nodes;        /* Number of nodes */
    int num_eqns;         /* Number of equations */
    Device *devices;      /* Device list */
    Model *models;        /* Model list */
    Analysis *analyses;   /* Analysis list */
    Param *params;        /* Parameter list (.PARAM) */
    SubcktDef *subckts;   /* Subcircuit definitions */

    /* Node voltages and results */
    double *voltage;      /* Node voltages (indexed by eqnum) */
    double *current;      /* Voltage source currents */

    /* Simulation state */
    double time;          /* Current simulation time */
    double temp;          /* Temperature (Kelvin) */
    double temp_celsius;  /* Temperature (Celsius) */

    /* Tolerances */
    double abstol;
    double reltol;
    double vntol;
    double trtol;
    int maxiter;
    int trmaxiter;        /* Max transient iterations */
    double gmin;          /* Minimum conductance */

    /* Simulation options */
    sim_options_t options;

    /* Voltage source branch currents */
    int num_vsources;
    int next_vsrc_branch;  /* Next branch current index (0, 1, 2, ...) */

    /* Output specification */
    int print_all;        /* Print all nodes if no .PRINT specified */
    char **print_nodes;   /* Nodes to print */
    int num_print_nodes;  /* Number of print nodes */
};

/* --- Node creation and lookup --- */
Node *circuit_find_node(Circuit *ckt, const char *name);
Node *circuit_get_or_create_node(Circuit *ckt, const char *name);
int circuit_get_eqnum(Circuit *ckt, Node *node);

/* --- Device management --- */
Device *circuit_add_device(Circuit *ckt, const char *name, device_type_t type,
                           int n1, int n2, double value);
Device *circuit_add_device4(Circuit *ckt, const char *name, device_type_t type,
                            int n1, int n2, int n3, int n4, double value);
Device *circuit_add_device5(Circuit *ckt, const char *name, device_type_t type,
                            int n1, int n2, int n3, int n4, int n5, double value);

/* --- Model management --- */
Model *circuit_find_model(Circuit *ckt, const char *name);
Model *circuit_add_model(Circuit *ckt, const char *name, device_type_t type);

/* --- Parameter management --- */
Param *circuit_find_param(Circuit *ckt, const char *name);
int circuit_set_param(Circuit *ckt, const char *name, double value);
double circuit_eval_param(Circuit *ckt, const char *expr);

/* --- Subcircuit management --- */
SubcktDef *circuit_find_subckt(Circuit *ckt, const char *name);
SubcktDef *circuit_add_subckt(Circuit *ckt, const char *name);

/* --- Circuit lifecycle --- */
Circuit *circuit_create(const char *title);
void circuit_free(Circuit *ckt);

/* --- Circuit initialization --- */
int circuit_init(Circuit *ckt);

/* Allocate a voltage source branch current index (not equation number) */
int circuit_alloc_vsrc_branch(Circuit *ckt);

/* Resolve branch current indices to equation numbers after parsing */
int circuit_resolve_branches(Circuit *ckt);

/* --- Waveform evaluation --- */
double waveform_eval(waveform_params_t *wf, double t);

#endif /* CIRCUIT_H */
