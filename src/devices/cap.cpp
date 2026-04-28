/**
 * @file cap.cpp
 * @brief Capacitor device model (C++17 version)
 *
 * Implements the capacitor for transient analysis using trapezoidal integration.
 *
 * The capacitor is discretized as:
 * @code
 * i(t) = C * dv/dt ≈ (C/Δt) * (v(t) - v(t-Δt))
 * @endcode
 *
 * Equivalent circuit: Conductance G_eq = C/Δt in parallel with
 * history current source I_hist = G_eq * v(t-Δt).
 */
#include "device.hpp"
#include "sparse.hpp"
#include <memory>
#include <cmath>

namespace spice {

/**
 * @brief Capacitor instance state
 *
 * Stores history voltage for transient analysis.
 */
struct CapState {
    Real v_history = 0.0;   /**< Voltage at previous time step */
    Real g_eq = 0.0;        /**< Equivalent conductance (C/Δt) */
    Real i_hist = 0.0;      /**< History current */
};

/**
 * @brief Capacitor setup: allocate instance state
 */
static ErrorCode capSetup(Device* dev, Circuit* ckt) {
    dev->params = new CapState();
    return ErrorCode::OK;
}

/**
 * @brief Capacitor load: load DC contributions
 *
 * In DC analysis, capacitor is an open circuit (no contribution).
 */
static ErrorCode capLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    // Capacitor is open circuit in DC
    return ErrorCode::OK;
}

/**
 * @brief Capacitor AC load: load AC contributions
 *
 * Capacitor admittance: Y = jωC
 * In real-valued MNA, we use the magnitude |Y| = ωC.
 */
static ErrorCode capAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    Index eq1 = dev->n1;
    Index eq2 = dev->n2;

    if (eq1 < 0 && eq2 < 0)
        return ErrorCode::OK;

    // Admittance magnitude = ωC
    Real b = omega * dev->value;

    // Load stamp (same form as resistor but with susceptance)
    if (eq1 >= 0 && eq2 >= 0) {
        mat->addElement(eq1, eq1, b);
        mat->addElement(eq1, eq2, -b);
        mat->addElement(eq2, eq1, -b);
        mat->addElement(eq2, eq2, b);
    } else if (eq1 >= 0) {
        mat->addElement(eq1, eq1, b);
    } else if (eq2 >= 0) {
        mat->addElement(eq2, eq2, b);
    }

    return ErrorCode::OK;
}

/**
 * @brief Capacitor update: update history after converged step
 */
static ErrorCode capUpdate(Device* dev, Circuit* ckt) {
    auto* state = static_cast<CapState*>(dev->params);
    if (!state)
        return ErrorCode::OK;

    // Store current voltage as history for next step
    state->v_history = dev->getVoltageAcross(*ckt);

    return ErrorCode::OK;
}

/**
 * @brief Capacitor transient load: load trapezoidal equivalent circuit
 *
 * Equivalent circuit:
 * - Conductance G_eq = C/Δt between nodes
 * - Current source I_hist = G_eq * v_history (from previous step)
 */
static ErrorCode capTransientLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real dt) {
    auto* state = static_cast<CapState*>(dev->params);
    if (!state)
        return ErrorCode::OK;

    Index eq1 = dev->n1;
    Index eq2 = dev->n2;

    if (eq1 < 0 && eq2 < 0)
        return ErrorCode::OK;

    // Compute equivalent conductance
    state->g_eq = dev->value / dt;

    // Compute history current: I_hist = G_eq * v(t-Δt)
    state->i_hist = state->g_eq * state->v_history;

    // Load conductance stamp
    if (eq1 >= 0 && eq2 >= 0) {
        mat->addElement(eq1, eq1, state->g_eq);
        mat->addElement(eq1, eq2, -state->g_eq);
        mat->addElement(eq2, eq1, -state->g_eq);
        mat->addElement(eq2, eq2, state->g_eq);
    } else if (eq1 >= 0) {
        mat->addElement(eq1, eq1, state->g_eq);
    } else if (eq2 >= 0) {
        mat->addElement(eq2, eq2, state->g_eq);
    }

    // Load history current source
    // I_hist flows from n1 to n2, so:
    // - Add I_hist to RHS at n1 (current leaving node)
    // - Subtract I_hist from RHS at n2 (current entering node)
    if (eq1 >= 0)
        mat->addRhs(eq1, state->i_hist);
    if (eq2 >= 0)
        mat->addRhs(eq2, -state->i_hist);

    return ErrorCode::OK;
}

/**
 * @brief Create capacitor device operations
 */
static DeviceOps createCapOps() {
    DeviceOps ops;
    ops.name = "C";
    ops.type = DeviceType::CAPACITOR;
    ops.setup = capSetup;
    ops.load = capLoad;
    ops.acLoad = capAcLoad;
    ops.update = capUpdate;
    return ops;
}

/**
 * @brief Register capacitor device
 */
static bool registerCapacitor() {
    registerDevice(createCapOps());
    return true;
}

static bool capacitor_registered = registerCapacitor();

} // namespace spice
