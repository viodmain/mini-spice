/**
 * @file vccs.cpp
 * @brief Voltage-Controlled Current Source (VCCS) device model (C++17 version)
 *
 * Implements a current source controlled by voltage across two nodes.
 *
 * Device: G<name> out+ out- ctrl+ ctrl- <transconductance>
 *
 * Stamp:
 * @code
 *     ctrl+  ctrl-  out+  out-
 * out+ [ gm  -gm   0     0   ]
 * out- [ -gm  gm   0     0   ]
 * @endcode
 */
#include "device.hpp"
#include "sparse.hpp"

namespace spice {

/**
 * @brief VCCS load: load DC contributions
 */
static ErrorCode vccsLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Index out_plus = dev->n1;
    Index out_minus = dev->n2;
    Index ctrl_plus = dev->n3;
    Index ctrl_minus = dev->n4;

    Real gm = dev->value;  // Transconductance

    // Load stamp
    if (out_plus >= 0 && ctrl_plus >= 0)
        mat->addElement(out_plus, ctrl_plus, gm);
    if (out_plus >= 0 && ctrl_minus >= 0)
        mat->addElement(out_plus, ctrl_minus, -gm);
    if (out_minus >= 0 && ctrl_plus >= 0)
        mat->addElement(out_minus, ctrl_plus, -gm);
    if (out_minus >= 0 && ctrl_minus >= 0)
        mat->addElement(out_minus, ctrl_minus, gm);

    return ErrorCode::OK;
}

/**
 * @brief VCCS AC load: same as DC (gm is frequency-independent)
 */
static ErrorCode vccsAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    return vccsLoad(dev, ckt, mat);
}

/**
 * @brief Create VCCS device operations
 */
static DeviceOps createVccsOps() {
    DeviceOps ops;
    ops.name = "G";
    ops.type = DeviceType::VCCS;
    ops.load = vccsLoad;
    ops.acLoad = vccsAcLoad;
    return ops;
}

/**
 * @brief Register VCCS device
 */
static bool registerVccs() {
    registerDevice(createVccsOps());
    return true;
}

static bool vccs_registered = registerVccs();

} // namespace spice
