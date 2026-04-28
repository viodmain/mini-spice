/**
 * @file switch.cpp
 * @brief Switch device model (C++17 version)
 *
 * Implements voltage-controlled (S) and current-controlled (W) switches.
 *
 * Voltage-controlled: S<name> nc+ nc- n+ n- <model>
 * Current-controlled: W<name> nc+ nc- n+ n- <model>
 *
 * The switch uses a piecewise-linear conductance model:
 * - On: G = 1/Ron
 * - Off: G = 1/Roff
 * - Transition region with hysteresis
 */
#include "device.hpp"
#include "sparse.hpp"
#include <cmath>

namespace spice {

/**
 * @brief Compute switch conductance based on control signal
 *
 * Uses a smooth transition with hysteresis:
 * - Vctrl > Vt + Vh/2 → On
 * - Vctrl < Vt - Vh/2 → Off
 * - Transition region in between
 */
static Real computeSwitchConductance(Real control_signal, SwitchModelParams* params) {
    Real vt = params->vt;
    Real vh = params->vh;
    Real ron = params->ron;
    Real roff = params->roff;

    Real g_on = 1.0 / ron;
    Real g_off = 1.0 / roff;

    if (vh > 0) {
        // With hysteresis
        if (control_signal > vt + vh / 2.0)
            return g_on;
        else if (control_signal < vt - vh / 2.0)
            return g_off;
        else {
            // Linear transition
            Real frac = (control_signal - (vt - vh / 2.0)) / vh;
            return g_off + frac * (g_on - g_off);
        }
    } else {
        // No hysteresis
        if (control_signal > vt)
            return g_on;
        else
            return g_off;
    }
}

/**
 * @brief Switch load: load DC contributions
 */
static ErrorCode switchLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Model* model = dev->model.get();
    if (!model)
        return ErrorCode::OK;

    SwitchModelParams* params = model->getSwitchParams();
    if (!params)
        return ErrorCode::OK;

    Index nc_plus = dev->n1;    // Control +
    Index nc_minus = dev->n2;   // Control -
    Index n_plus = dev->n3;     // Switch +
    Index n_minus = dev->n4;    // Switch -

    // Get control voltage
    Real vctrl = 0.0;
    if (nc_plus >= 0 && nc_minus >= 0)
        vctrl = dev->getNodeVoltage(*ckt, nc_plus) - dev->getNodeVoltage(*ckt, nc_minus);
    else if (nc_plus >= 0)
        vctrl = dev->getNodeVoltage(*ckt, nc_plus);

    // Compute conductance
    Real g = computeSwitchConductance(vctrl, params);

    // Load conductance stamp
    if (n_plus >= 0 && n_minus >= 0) {
        mat->addElement(n_plus, n_plus, g);
        mat->addElement(n_plus, n_minus, -g);
        mat->addElement(n_minus, n_plus, -g);
        mat->addElement(n_minus, n_minus, g);
    } else if (n_plus >= 0) {
        mat->addElement(n_plus, n_plus, g);
    } else if (n_minus >= 0) {
        mat->addElement(n_minus, n_minus, g);
    }

    return ErrorCode::OK;
}

/**
 * @brief Create voltage-controlled switch operations
 */
static DeviceOps createSwOps() {
    DeviceOps ops;
    ops.name = "S";
    ops.type = DeviceType::SWITCH_VOLTAGE;
    ops.load = switchLoad;
    return ops;
}

/**
 * @brief Create current-controlled switch operations
 */
static DeviceOps createCswOps() {
    DeviceOps ops;
    ops.name = "W";
    ops.type = DeviceType::SWITCH_CURRENT;
    ops.load = switchLoad;
    return ops;
}

/**
 * @brief Register switches
 */
static bool registerSwitch() {
    registerDevice(createSwOps());
    registerDevice(createCswOps());
    return true;
}

static bool switch_registered = registerSwitch();

} // namespace spice
