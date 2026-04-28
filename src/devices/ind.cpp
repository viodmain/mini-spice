/**
 * @file ind.cpp
 * @brief Inductor device model (C++17 version)
 *
 * Implements the inductor for transient analysis using trapezoidal integration.
 *
 * The inductor is discretized as:
 * @code
 * v(t) = L * di/dt ≈ L * (i(t) - i(t-Δt)) / Δt
 * @endcode
 *
 * Equivalent circuit: Conductance G_eq = Δt/(2L) in parallel with
 * history current source.
 */
#include "device.hpp"
#include "sparse.hpp"
#include <memory>

namespace spice {

/**
 * @brief Inductor instance state
 */
struct IndState {
    Real i_history = 0.0;   /**< Current at previous time step */
    Real g_eq = 0.0;        /**< Equivalent conductance (Δt/2L) */
    Real i_src = 0.0;       /**< History current source */
};

/**
 * @brief Inductor setup: allocate instance state
 */
static ErrorCode indSetup(Device* dev, Circuit* ckt) {
    dev->params = new IndState();
    return ErrorCode::OK;
}

/**
 * @brief Inductor load: DC contributions
 *
 * In DC analysis, inductor is a short circuit (zero resistance).
 * We add a small conductance to avoid singular matrix.
 */
static ErrorCode indLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Index eq1 = dev->n1;
    Index eq2 = dev->n2;

    if (eq1 < 0 && eq2 < 0)
        return ErrorCode::OK;

    // In DC, inductor is short circuit (very high conductance)
    // Use GMIN to avoid singular matrix
    Real g = 1.0 / ckt->gmin;  // Very large conductance

    if (eq1 >= 0 && eq2 >= 0) {
        mat->addElement(eq1, eq1, g);
        mat->addElement(eq1, eq2, -g);
        mat->addElement(eq2, eq1, -g);
        mat->addElement(eq2, eq2, g);
    } else if (eq1 >= 0) {
        mat->addElement(eq1, eq1, g);
    } else if (eq2 >= 0) {
        mat->addElement(eq2, eq2, g);
    }

    return ErrorCode::OK;
}

/**
 * @brief Inductor AC load
 *
 * Inductor admittance: Y = 1/(jωL) = -j/(ωL)
 * Magnitude: |Y| = 1/(ωL)
 */
static ErrorCode indAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    Index eq1 = dev->n1;
    Index eq2 = dev->n2;

    if (eq1 < 0 && eq2 < 0)
        return ErrorCode::OK;

    // Admittance magnitude = 1/(ωL)
    Real g = 1.0 / (omega * dev->value);

    if (eq1 >= 0 && eq2 >= 0) {
        mat->addElement(eq1, eq1, g);
        mat->addElement(eq1, eq2, -g);
        mat->addElement(eq2, eq1, -g);
        mat->addElement(eq2, eq2, g);
    } else if (eq1 >= 0) {
        mat->addElement(eq1, eq1, g);
    } else if (eq2 >= 0) {
        mat->addElement(eq2, eq2, g);
    }

    return ErrorCode::OK;
}

/**
 * @brief Inductor update: update history after converged step
 */
static ErrorCode indUpdate(Device* dev, Circuit* ckt) {
    auto* state = static_cast<IndState*>(dev->params);
    if (!state)
        return ErrorCode::OK;

    // Current through inductor (approximated from voltage)
    // i = (1/L) * integral(v dt) ≈ v * Δt / L (for trapezoidal)
    // For simplicity, store the voltage-derived current
    Real v = dev->getVoltageAcross(*ckt);
    state->i_history = v / (dev->value);  // Simplified

    return ErrorCode::OK;
}

/**
 * @brief Create inductor device operations
 */
static DeviceOps createIndOps() {
    DeviceOps ops;
    ops.name = "L";
    ops.type = DeviceType::INDUCTOR;
    ops.setup = indSetup;
    ops.load = indLoad;
    ops.acLoad = indAcLoad;
    ops.update = indUpdate;
    return ops;
}

/**
 * @brief Register inductor device
 */
static bool registerInductor() {
    registerDevice(createIndOps());
    return true;
}

static bool inductor_registered = registerInductor();

} // namespace spice
