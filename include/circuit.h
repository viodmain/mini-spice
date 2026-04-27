/*
 * circuit.h - Circuit data structures
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

/* --- Node --- */
struct Node {
    char *name;           /* Node name */
    int number;           /* Node number (internal) */
    int eqnum;            /* Equation number in matrix (-1 if none) */
    int is_ground;        /* 1 if this is ground (node 0) */
    int is_vsource;       /* 1 if connected to voltage source branch */
    Node *next;           /* Next node in list */
};

/* --- Device instance --- */
struct Device {
    char *name;           /* Device instance name (R1, C2, etc.) */
    device_type_t type;   /* Device type */
    int n1, n2;           /* Node connections (n1, n2 for 2-terminal) */
    int n3, n4, n5;       /* Additional nodes (for dependent sources, MNA) */
    double value;         /* Primary value (R, C, L, gain, etc.) */
    double value2;        /* Secondary value (for some devices) */
    void *params;         /* Device-specific parameters */
    Model *model;         /* Model pointer (for diodes, etc.) */
    Device *next;         /* Next device in list */
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
    
} analysis_params_t;

/* --- Analysis --- */
struct Analysis {
    analysis_params_t params;
    Analysis *next;
};

/* --- Circuit --- */
struct Circuit {
    char *title;          /* Circuit title */
    Node *nodes;          /* Node list */
    int num_nodes;        /* Number of nodes */
    int num_eqns;         /* Number of equations */
    Device *devices;      /* Device list */
    Model *models;        /* Model list */
    Analysis *analyses;   /* Analysis list */
    
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
    
    /* Voltage source branch currents */
    int num_vsources;
    int next_vsrc_branch;  /* Next branch current index (0, 1, 2, ...) */
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

/* --- Circuit lifecycle --- */
Circuit *circuit_create(const char *title);
void circuit_free(Circuit *ckt);

/* --- Circuit initialization --- */
int circuit_init(Circuit *ckt);

/* Allocate a voltage source branch current index (not equation number) */
int circuit_alloc_vsrc_branch(Circuit *ckt);

/* Resolve branch current indices to equation numbers after parsing */
int circuit_resolve_branches(Circuit *ckt);

#endif /* CIRCUIT_H */
