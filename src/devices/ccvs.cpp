/**
 * @file ccvs.cpp
 * @brief Current-Controlled Voltage Source (CCVS) device model (C++17 version)
 *
 * Implements a voltage source controlled by current through a voltage source.
 *
 * Device: H<name> out+ out- vctrl <transresistance>
 *
 * The controlling current is sensed through a voltage source (vctrl).
 * The transresistance has units of Ohms (V/A).
 *
 * Stamp:
 * @code
 *       out+  out-  ib_out  vctrl_ib
 * out+ [  0    0     1       0     ]
 * out- [  0    0    -1       0     ]
 * ib_out [ 1   -1     0     -R     ]
 * @endcode
 */
#include "device.hpp"
#include "sparse.hpp"

namespace spice {

/**
 * @brief CCVS load: load DC contributions
 */
static ErrorCode ccvsLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Index out_plus = dev->n1;
    Index out_minus = dev->n2;
    Index ib_out = dev->n3;      // Output branch current equation number
    Index vctrl_eq = dev->n4;    // Controlling voltage source branch current eq

    Real R = dev->value;  // Transresistance

    // Load stamp
    if (out_plus >= 0 && ib_out >= 0) {
        mat->addElement(out_plus, ib_out, 1.0);
        mat->addElement(ib_out, out_plus, 1.0);
    }
    if (out_minus >= 0 && ib_out >= 0) {
        mat->addElement(out_minus, ib_out, -1.0);
        mat->addElement(ib_out, out_minus, -1.0);
    }
    if (ib_out >= 0 && vctrl_eq >= 0) {
        mat->addElement(ib_out, vctrl_eq, -R);
    }

    return ErrorCode::OK;
}

/**
 * @brief CCVS AC load: same as DC
 */
static ErrorCode ccvsAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    return ccvsLoad(dev, ckt, mat);
}

/**
 * @brief Create CCVS device operations
 */
static DeviceOps createCcvsOps() {
    DeviceOps ops;
    ops.name = "H";
    ops.type = DeviceType::CCVS;
    ops.load = ccvsLoad;
    ops.acLoad = ccvsAcLoad;
    return ops;
}

/**
 * @brief Register CCVS device
 */
static bool registerCcvs() {
    registerDevice(createCcvsOps());
    return true;
}

static bool ccvs_registered = registerCcvs();

} // namespace spice
