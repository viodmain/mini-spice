/*
 * devreg.c - Device registration system
 * Registers all device types for the simulator
 */
#include "device.h"
#include <stdio.h>
#include <string.h>

/* External device operations */
extern const DeviceOps *res_get_ops(void);
extern const DeviceOps *cap_get_ops(void);
extern const DeviceOps *ind_get_ops(void);
extern const DeviceOps *vsrc_get_ops(void);
extern const DeviceOps *isrc_get_ops(void);
extern const DeviceOps *vccs_get_ops(void);
extern const DeviceOps *vcvs_get_ops(void);
extern const DeviceOps *cccs_get_ops(void);
extern const DeviceOps *ccvs_get_ops(void);
extern const DeviceOps *dio_get_ops(void);
extern const DeviceOps *bjt_get_ops(void);
extern const DeviceOps *mos_get_ops(void);
extern const DeviceOps *behsrchsrc_get_ops(void);
extern const DeviceOps *behvsrc_get_ops(void);
extern const DeviceOps *switch_get_ops(void);
extern const DeviceOps *cswitch_get_ops(void);
extern const DeviceOps *tline_get_ops(void);

/* Device registry */
static const DeviceOps *device_registry[NUM_DEVICES];

/* Register a device type */
int device_register(const DeviceOps *ops)
{
    if (ops->type >= NUM_DEVICES)
        return E_TROUBLE;

    device_registry[ops->type] = ops;
    return OK;
}

/* Get device operations by type */
const DeviceOps *device_get_ops(device_type_t type)
{
    if (type >= NUM_DEVICES)
        return NULL;
    return device_registry[type];
}

/* Get device operations by name */
const DeviceOps *device_get_ops_by_name(const char *name)
{
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (device_registry[i] && strcmp(device_registry[i]->name, name) == 0)
            return device_registry[i];
    }
    return NULL;
}

/* Initialize all devices */
int device_init_all(void)
{
    /* Register basic linear devices */
    device_register(res_get_ops());
    device_register(cap_get_ops());
    device_register(ind_get_ops());
    device_register(vsrc_get_ops());
    device_register(isrc_get_ops());

    /* Register dependent sources */
    device_register(vccs_get_ops());
    device_register(vcvs_get_ops());
    device_register(cccs_get_ops());
    device_register(ccvs_get_ops());

    /* Register nonlinear devices */
    device_register(dio_get_ops());
    device_register(bjt_get_ops());
    device_register(mos_get_ops());

    /* Register behavioral sources */
    device_register(behsrchsrc_get_ops());
    device_register(behvsrc_get_ops());

    /* Register switches and transmission lines */
    device_register(switch_get_ops());
    device_register(cswitch_get_ops());
    device_register(tline_get_ops());

    return OK;
}

/* Get model parameter */
double device_get_param(Model *model, const char *param, double default_val)
{
    /* This is a simplified version - full implementation would use model-specific ops */
    return default_val;
}
