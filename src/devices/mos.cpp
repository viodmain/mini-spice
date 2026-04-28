/**
 * @file mos.cpp
 * @brief MOSFET device model (C++17 version)
 *
 * Implements the Level 1 (Shichman-Hodges) MOSFET model for NMOS and PMOS.
 *
 * Device: M<name> drain gate source [body] <model_name> [W=<width> L=<length>]
 *
 * The model supports three regions of operation:
 * 1. Cutoff: Vgs < Vth → Id = 0
 * 2. Linear (Triode): Vgs > Vth, Vds < Vgs - Vth
 * 3. Saturation: Vgs > Vth, Vds >= Vgs - Vth
 *
 * Drain current equations:
 * - Linear: Id = Kp * (W/L) * [(Vgs - Vth)*Vds - Vds²/2] * (1 + λ*Vds)
 * - Saturation: Id = Kp/2 * (W/L) * (Vgs - Vth)² * (1 + λ*Vds)
 *
 * Newton-Raphson iteration is used for nonlinear convergence.
 */
#include "device.hpp"
#include "sparse.hpp"
#include <cmath>

namespace spice {

// Forward declaration
static ErrorCode mosNonlinear(Device* dev, Circuit* ckt, SparseMatrix* mat);

/**
 * @brief MOSFET load: delegates to nonlinear
 */
static ErrorCode mosLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    return mosNonlinear(dev, ckt, mat);
}

/**
 * @brief MOSFET AC load: small-signal model
 *
 * Linearizes the MOSFET around the DC operating point.
 * Computes transconductance (gm) and output conductance (gds).
 */
static ErrorCode mosAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    Model* model = dev->model.get();
    if (!model)
        return ErrorCode::OK;

    MosModelParams* params = model->getMosParams();
    if (!params)
        return ErrorCode::OK;

    Index d = dev->n1;  // Drain
    Index g = dev->n2;  // Gate
    Index s = dev->n3;  // Source

    Real polarity = static_cast<Real>(params->polarity);
    Real W = dev->value > 0 ? dev->value : params->w;
    Real L = dev->value2 > 0 ? dev->value2 : params->l;

    // Get terminal voltages
    Real Vgs = polarity * (dev->getNodeVoltage(*ckt, g) - dev->getNodeVoltage(*ckt, s));
    Real Vds = polarity * (dev->getNodeVoltage(*ckt, d) - dev->getNodeVoltage(*ckt, s));
    Real Vth = params->vto;

    // Transconductance parameter
    Real kp = params->kp * W / L;

    // Compute gm and gds based on region
    Real gm = 0.0, gds = 0.0;

    if (Vgs <= Vth) {
        // Cutoff: no conduction
        gm = 0.0;
        gds = 1.0e-12;  // Small conductance for numerical stability
    } else if (Vds < Vgs - Vth) {
        // Linear region
        gm = kp * Vds;
        gds = kp * (Vgs - Vth - Vds) * (1.0 + params->lambda * Vds) +
              kp * ((Vgs - Vth) * Vds - Vds * Vds / 2.0) * params->lambda;
    } else {
        // Saturation region
        gm = kp * (Vgs - Vth) * (1.0 + params->lambda * Vds);
        gds = kp / 2.0 * (Vgs - Vth) * (Vgs - Vth) * params->lambda;
    }

    // Load small-signal model
    // Gate is open circuit (infinite impedance)
    // Drain to source: gm*Vgs current source in parallel with gds

    if (d >= 0 && s >= 0) {
        mat->addElement(d, d, gds);
        mat->addElement(d, s, -gds);
        mat->addElement(s, d, -gds);
        mat->addElement(s, s, gds);
        // Transconductance: Id = gm * Vgs = gm * (Vg - Vs)
        mat->addElement(d, g, gm);
        mat->addElement(s, g, -gm);
    }

    return ErrorCode::OK;
}

/**
 * @brief MOSFET nonlinear: Newton-Raphson iteration
 *
 * Computes the linearized MOSFET model:
 * 1. Determine operating region (cutoff, linear, saturation)
 * 2. Compute drain current and dynamic conductances
 * 3. Load conductance stamp and current source term
 */
static ErrorCode mosNonlinear(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Model* model = dev->model.get();
    if (!model)
        return ErrorCode::OK;

    MosModelParams* params = model->getMosParams();
    if (!params)
        return ErrorCode::OK;

    Index d = dev->n1;  // Drain
    Index g = dev->n2;  // Gate
    Index s = dev->n3;  // Source

    Real polarity = static_cast<Real>(params->polarity);
    Real W = dev->value > 0 ? dev->value : params->w;
    Real L = dev->value2 > 0 ? dev->value2 : params->l;

    // Get terminal voltages
    Real Vgs = polarity * (dev->getNodeVoltage(*ckt, g) - dev->getNodeVoltage(*ckt, s));
    Real Vds = polarity * (dev->getNodeVoltage(*ckt, d) - dev->getNodeVoltage(*ckt, s));
    Real Vth = params->vto;

    // Transconductance parameter
    Real kp = params->kp * W / L;

    // Compute drain current and conductances based on region
    Real id = 0.0;    // Drain current
    Real gm = 0.0;    // Transconductance (dId/dVgs)
    Real gds = 0.0;   // Output conductance (dId/dVds)

    if (Vgs <= Vth) {
        // Cutoff region
        id = 0.0;
        gm = 0.0;
        gds = 1.0e-12;  // Small conductance for numerical stability
    } else if (Vds < Vgs - Vth) {
        // Linear (triode) region
        id = kp * ((Vgs - Vth) * Vds - Vds * Vds / 2.0) * (1.0 + params->lambda * Vds);
        gm = kp * Vds * (1.0 + params->lambda * Vds);
        gds = kp * (Vgs - Vth - Vds) * (1.0 + params->lambda * Vds) +
              kp * ((Vgs - Vth) * Vds - Vds * Vds / 2.0) * params->lambda;
    } else {
        // Saturation region
        id = kp / 2.0 * (Vgs - Vth) * (Vgs - Vth) * (1.0 + params->lambda * Vds);
        gm = kp * (Vgs - Vth) * (1.0 + params->lambda * Vds);
        gds = kp / 2.0 * (Vgs - Vth) * (Vgs - Vth) * params->lambda;
    }

    // Limit current to prevent numerical issues
    Real imax = 1.0;  // 1A max
    if (id > imax) id = imax;
    if (id < -imax) id = -imax;

    // Load conductance stamp
    // Gate is open circuit (no connection in MNA)
    // Drain to source: conductance gds with transconductance gm

    if (d >= 0 && s >= 0) {
        mat->addElement(d, d, gds);
        mat->addElement(d, s, -gds);
        mat->addElement(s, d, -gds);
        mat->addElement(s, s, gds);
        // Transconductance: Id = gm * Vgs = gm * (Vg - Vs)
        mat->addElement(d, g, gm);
        mat->addElement(s, g, -gm);
    }

    // Current source term for Newton-Raphson
    // I_hist = I_nonlinear - G * V
    Real i_hist = id - gds * Vds - gm * (Vgs - Vds);

    if (d >= 0)
        mat->addRhs(d, -polarity * i_hist);
    if (s >= 0)
        mat->addRhs(s, polarity * i_hist);

    return ErrorCode::OK;
}

/**
 * @brief Create MOSFET device operations
 */
static DeviceOps createMosOps(DeviceType type) {
    DeviceOps ops;
    ops.name = (type == DeviceType::NMOS) ? "M(NMOS)" : "M(PMOS)";
    ops.type = type;
    ops.load = mosLoad;
    ops.acLoad = mosAcLoad;
    ops.nonlinear = mosNonlinear;
    return ops;
}

/**
 * @brief Register MOSFET devices
 */
static bool registerMos() {
    registerDevice(createMosOps(DeviceType::NMOS));
    registerDevice(createMosOps(DeviceType::PMOS));
    return true;
}

static bool mos_registered = registerMos();

} // namespace spice
