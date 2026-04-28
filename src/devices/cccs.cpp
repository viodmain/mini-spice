/**
 * @file cccs.cpp
 * @brief Current-Controlled Current Source (CCCS) device model (C++17 version)
 *
 * Implements a current source controlled by current through a voltage source.
 *
 * Device: F<name> out+ out- vctrl <gain>
 *
 * The controlling current is sensed through a voltage source (vctrl).
 * The gain is dimensionless (A/A).
 *
 * Stamp:
 * @code
 *     vctrl_ib  out+  out-
 * out+ [ gain   0     0   ]
 * out- [ -gain  0     0   ]
 * @endcode
 */
#include "device.hpp"
#include "sparse.hpp"

namespace spice {

/**
 * @brief CCCS load: load DC contributions
 */
static ErrorCode cccsLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Index out_plus = dev->n1;
    Index out_minus = dev->n2;
    Index vctrl_eq = dev->n3;  // Controlling voltage source branch current eq

    Real gain = dev->value;

    // Load stamp
    if (out_plus >= 0 && vctrl_eq >= 0)
        mat->addElement(out_plus, vctrl_eq, gain);
    if (out_minus >= 0 && vctrl_eq >= 0)
        mat->addElement(out_minus, vctrl_eq, -gain);

    return ErrorCode::OK;
}

/**
 * @brief CCCS AC load: same as DC
 */
static ErrorCode cccsAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    return cccsLoad(dev, ckt, mat);
}

/**
 * @brief Create CCCS device operations
 */
static DeviceOps createCccsOps() {
    DeviceOps ops;
    ops.name = "F";
    ops.type = DeviceType::CCCS;
    ops.load = cccsLoad;
    ops.acLoad = cccsAcLoad;
    return ops;
}

/**
 * @brief Register CCCS device
 */
static bool registerCccs() {
    registerDevice(createCccsOps());
    return true;
}

static bool cccs_registered = registerCccs();

} // namespace spice
