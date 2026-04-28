/**
 * @file vcvs.cpp
 * @brief Voltage-Controlled Voltage Source (VCVS) device model (C++17 version)
 *
 * Implements a voltage source controlled by voltage across two nodes.
 *
 * Device: E<name> out+ out- ctrl+ ctrl- <gain>
 *
 * Stamp:
 * @code
 *       out+  out-  ib    ctrl+  ctrl-
 * out+ [  0    0    1     0      0    ]
 * out- [  0    0   -1     0      0    ]
 * ib   [  1   -1    0    -gain  gain  ]
 * @endcode
 */
#include "device.hpp"
#include "sparse.hpp"

namespace spice {

/**
 * @brief VCVS load: load DC contributions
 */
static ErrorCode vcvsLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Index out_plus = dev->n1;
    Index out_minus = dev->n2;
    Index ctrl_plus = dev->n3;
    Index ctrl_minus = dev->n4;
    Index ib = dev->n5;  // Branch current equation number

    Real gain = dev->value;

    // Load stamp
    if (out_plus >= 0 && ib >= 0) {
        mat->addElement(out_plus, ib, 1.0);
        mat->addElement(ib, out_plus, 1.0);
    }
    if (out_minus >= 0 && ib >= 0) {
        mat->addElement(out_minus, ib, -1.0);
        mat->addElement(ib, out_minus, -1.0);
    }
    if (ib >= 0 && ctrl_plus >= 0) {
        mat->addElement(ib, ctrl_plus, -gain);
    }
    if (ib >= 0 && ctrl_minus >= 0) {
        mat->addElement(ib, ctrl_minus, gain);
    }

    return ErrorCode::OK;
}

/**
 * @brief VCVS AC load: same as DC
 */
static ErrorCode vcvsAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    return vcvsLoad(dev, ckt, mat);
}

/**
 * @brief Create VCVS device operations
 */
static DeviceOps createVcvsOps() {
    DeviceOps ops;
    ops.name = "E";
    ops.type = DeviceType::VCVS;
    ops.load = vcvsLoad;
    ops.acLoad = vcvsAcLoad;
    return ops;
}

/**
 * @brief Register VCVS device
 */
static bool registerVcvs() {
    registerDevice(createVcvsOps());
    return true;
}

static bool vcvs_registered = registerVcvs();

} // namespace spice
