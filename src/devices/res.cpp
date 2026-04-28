/**
 * @file res.cpp
 * @brief Resistor device model (C++17 version)
 *
 * Implements the resistor MNA stamp:
 * @code
 *   [ n1  n2 ]
 * n1 [ g  -g ]
 * n2 [ -g  g ]
 * @endcode
 * where g = 1/R is the conductance.
 */
#include "device.hpp"
#include "sparse.hpp"
#include <memory>

namespace spice {

/**
 * @brief Resistor load: load DC contributions into MNA matrix
 *
 * The resistor stamp adds conductance terms to the MNA matrix.
 * Ground connections (eqnum = -1) are handled by omitting the
 * corresponding row/column.
 */
static ErrorCode resLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Index eq1 = dev->n1;
    Index eq2 = dev->n2;

    // Check for both ends grounded (shouldn't happen)
    if (eq1 < 0 && eq2 < 0) {
        std::cerr << "Warning: resistor " << dev->name
                  << " connected to ground on both ends" << std::endl;
        return ErrorCode::OK;
    }

    // Conductance = 1/R
    Real g = 1.0 / dev->value;

    // Load stamp into matrix
    if (eq1 >= 0 && eq2 >= 0) {
        mat->addElement(eq1, eq1, g);
        mat->addElement(eq1, eq2, -g);
        mat->addElement(eq2, eq1, -g);
        mat->addElement(eq2, eq2, g);
    } else if (eq1 >= 0) {
        // n2 is ground
        mat->addElement(eq1, eq1, g);
    } else {
        // n1 is ground
        mat->addElement(eq2, eq2, g);
    }

    return ErrorCode::OK;
}

/**
 * @brief Resistor AC load: same as DC (resistor is frequency-independent)
 */
static ErrorCode resAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    return resLoad(dev, ckt, mat);
}

/**
 * @brief Create resistor device operations
 * @return Fully initialized DeviceOps struct
 */
static DeviceOps createResOps() {
    DeviceOps ops;
    ops.name = "R";
    ops.type = DeviceType::RESISTOR;
    ops.load = resLoad;
    ops.acLoad = resAcLoad;
    // setup, update, nonlinear not needed for linear resistor
    return ops;
}

/**
 * @brief Register resistor device (called during initialization)
 */
static bool registerResistor() {
    registerDevice(createResOps());
    return true;
}

// Auto-register on program startup
static bool resistor_registered = registerResistor();

} // namespace spice
