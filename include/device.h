/*
 * device.h - Device model interface
 */
#ifndef DEVICE_H
#define DEVICE_H

#include "spice_types.h"
#include "circuit.h"

/* Forward declaration */
typedef struct SparseMatrix SparseMatrix;

/* Forward declarations */
typedef struct DeviceOps DeviceOps;
typedef struct DeviceModelOps DeviceModelOps;

/* Device operations (per-instance) */
struct DeviceOps {
    const char *name;             /* Device type name */
    device_type_t type;           /* Device type enum */
    
    /* Setup: allocate matrix entries for this device */
    int (*setup)(Device *dev, Circuit *ckt);
    
    /* Load: load device contributions into matrix */
    int (*load)(Device *dev, Circuit *ckt, SparseMatrix *mat);
    
    /* AC load: load AC contributions */
    int (*ac_load)(Device *dev, Circuit *ckt, SparseMatrix *mat, double omega);
    
    /* Update: update device state after solution */
    int (*update)(Device *dev, Circuit *ckt);
    
    /* Nonlinear: compute nonlinear currents for Newton-Raphson */
    int (*nonlinear)(Device *dev, Circuit *ckt, SparseMatrix *mat);
};

/* Model operations (per-model) */
struct DeviceModelOps {
    const char *name;             /* Model type name */
    device_type_t type;           /* Device type enum */
    
    /* Create default model parameters */
    void *(*create_params)(void);
    
    /* Set model parameter */
    int (*set_param)(Model *model, const char *param, double value);
    
    /* Free model parameters */
    void (*free_params)(void *params);
};

/* --- Device registration --- */

/* Register a device type */
int device_register(const DeviceOps *ops);

/* Get device operations by type */
const DeviceOps *device_get_ops(device_type_t type);

/* Get device operations by name */
const DeviceOps *device_get_ops_by_name(const char *name);

/* Initialize all devices */
int device_init_all(void);

/* --- Model parameter helpers --- */
double device_get_param(Model *model, const char *param, double default_val);

#endif /* DEVICE_H */
