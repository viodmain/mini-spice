/**
 * @file bjt.cpp
 * @brief BJT (Bipolar Junction Transistor) device model (C++17 version)
 *
 * Implements the simplified Ebers-Moll model for NPN and PNP transistors.
 *
 * Device: Q<name> collector base emitter [substrate] <model_name>
 *
 * The model computes:
 * - Forward and reverse junction currents
 * - Dynamic conductances (gm, gpi, gmu, gx)
 * - Newton-Raphson linearization for nonlinear convergence
 *
 * For AC analysis, the transistor is linearized around the DC operating point.
 */
#include "device.hpp"
#include "sparse.hpp"
#include <cmath>

namespace spice {

// Forward declaration
static ErrorCode bjtNonlinear(Device* dev, Circuit* ckt, SparseMatrix* mat);

/**
 * @brief BJT load: linearized model (delegates to nonlinear)
 */
static ErrorCode bjtLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    return bjtNonlinear(dev, ckt, mat);
}

/**
 * @brief BJT AC load: small-signal model
 *
 * Linearizes the BJT around the DC operating point.
 * Computes transconductance (gm), input conductance (gpi), etc.
 */
static ErrorCode bjtAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    Model* model = dev->model.get();
    if (!model)
        return ErrorCode::OK;

    BjtModelParams* params = model->getBjtParams();
    if (!params)
        return ErrorCode::OK;

    Index c = dev->n1;  // Collector
    Index b = dev->n2;  // Base
    Index e = dev->n3;  // Emitter

    // Thermal voltage
    Real vt = 8.617e-5 * ckt->temp;
    Real polarity = static_cast<Real>(params->polarity);

    // Get junction voltages
    Real vbe = polarity * (dev->getNodeVoltage(*ckt, b) - dev->getNodeVoltage(*ckt, e));
    Real vbc = polarity * (dev->getNodeVoltage(*ckt, c) - dev->getNodeVoltage(*ckt, b));

    // Forward and reverse currents
    Real exp_vbe = 0.0, exp_vbc = 0.0;
    if (vbe / (params->nf * vt) < 50)
        exp_vbe = std::exp(vbe / (params->nf * vt));
    if (vbc / (params->nr * vt) < 50)
        exp_vbc = std::exp(vbc / (params->nr * vt));

    // Transconductance
    Real gm = params->is / vt * (exp_vbe / params->nf + exp_vbc / params->nr);

    // Input conductances
    Real gpi = params->is / (params->nf * vt) * exp_vbe;
    Real gmu = params->is / (params->nr * vt) * exp_vbc;

    // Load small-signal model (simplified hybrid-pi)
    // Collector to emitter: gm * Vbe current source
    if (c >= 0 && e >= 0) {
        mat->addElement(c, b, gm);
        mat->addElement(c, e, -gm);
    }

    // Base to emitter: gpi
    if (b >= 0 && e >= 0) {
        mat->addElement(b, b, gpi);
        mat->addElement(b, e, -gpi);
        mat->addElement(e, b, -gpi);
        mat->addElement(e, e, gpi);
    }

    return ErrorCode::OK;
}

/**
 * @brief BJT nonlinear: Newton-Raphson iteration
 *
 * Implements the Ebers-Moll model:
 * 1. Compute forward and reverse junction currents
 * 2. Compute dynamic conductances
 * 3. Load linearized model into MNA matrix
 */
static ErrorCode bjtNonlinear(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Model* model = dev->model.get();
    if (!model)
        return ErrorCode::OK;

    BjtModelParams* params = model->getBjtParams();
    if (!params)
        return ErrorCode::OK;

    Index c = dev->n1;  // Collector
    Index b = dev->n2;  // Base
    Index e = dev->n3;  // Emitter

    // Thermal voltage
    Real vt = 8.617e-5 * ckt->temp;
    Real polarity = static_cast<Real>(params->polarity);

    // Get junction voltages (apply polarity for NPN/PNP)
    Real vbe = polarity * (dev->getNodeVoltage(*ckt, b) - dev->getNodeVoltage(*ckt, e));
    Real vbc = polarity * (dev->getNodeVoltage(*ckt, c) - dev->getNodeVoltage(*ckt, b));

    // Limit voltages to prevent overflow
    Real vlimit = 20.0 * vt;
    if (vbe > vlimit) vbe = vlimit;
    if (vbe < -vlimit) vbe = -vlimit;
    if (vbc > vlimit) vbc = vlimit;
    if (vbc < -vlimit) vbc = -vlimit;

    // Forward and reverse exponential terms
    Real exp_vbe = 0.0, exp_vbc = 0.0;
    if (vbe / (params->nf * vt) < 50)
        exp_vbe = std::exp(vbe / (params->nf * vt));
    if (vbc / (params->nr * vt) < 50)
        exp_vbc = std::exp(vbc / (params->nr * vt));

    // Junction currents
    Real if_ = params->is * (exp_vbe - 1.0);  // Forward current
    Real ir_ = params->is * (exp_vbc - 1.0);  // Reverse current

    // Dynamic conductances
    Real gf = params->is / (params->nf * vt) * exp_vbe;  // Forward conductance
    Real gr = params->is / (params->nr * vt) * exp_vbc;  // Reverse conductance

    // Transconductance
    Real gm = params->is / vt * (exp_vbe / params->nf + exp_vbc / params->nr);

    // Currents with polarity
    Real ic = polarity * (gf / params->bf * vbe + gm * vbe - gr * vbc / params->br);
    Real ib = polarity * (gf * vbe + gr * vbc - ic / (params->bf * params->br));
    Real ie = -ic - ib;

    // Load conductance matrix (simplified Ebers-Moll)
    // gbe = dIb/dVbe, gbc = dIb/dVbc, gce = dIc/dVce, gcb = dIc/dVbc, etc.
    Real gbe = gf / params->bf;
    Real gbc = gr / params->br;
    Real gce = gm;

    // Collector node
    if (c >= 0) {
        if (b >= 0) mat->addElement(c, b, gce / polarity);
        if (e >= 0) mat->addElement(c, e, -gce / polarity);
    }

    // Base node
    if (b >= 0) {
        if (b >= 0) mat->addElement(b, b, gbe + gbc);
        if (e >= 0) mat->addElement(b, e, -gbe);
        if (c >= 0) mat->addElement(b, c, -gbc);
    }

    // Emitter node
    if (e >= 0) {
        if (b >= 0) mat->addElement(e, b, -(gbe + gce) / polarity);
        if (e >= 0) mat->addElement(e, e, (gbe + gce) / polarity);
        if (c >= 0) mat->addElement(e, c, -gce / polarity);
    }

    // Current source terms (Newton-Raphson correction)
    // I_hist = I_nonlinear - G * V
    Real ic_nonlinear = polarity * params->is * (exp_vbe / params->bf - exp_vbc / params->br);
    Real ib_nonlinear = polarity * params->is * ((exp_vbe - 1.0) / params->nf + (exp_vbc - 1.0) / params->nr);

    if (c >= 0) mat->addRhs(c, -polarity * ic_nonlinear);
    if (b >= 0) mat->addRhs(b, -polarity * ib_nonlinear);
    if (e >= 0) mat->addRhs(e, polarity * (ic_nonlinear + ib_nonlinear));

    return ErrorCode::OK;
}

/**
 * @brief Create BJT device operations
 */
static DeviceOps createBjtOps(DeviceType type) {
    DeviceOps ops;
    ops.name = (type == DeviceType::NPN) ? "Q(NPN)" : "Q(PNP)";
    ops.type = type;
    ops.load = bjtLoad;
    ops.acLoad = bjtAcLoad;
    ops.nonlinear = bjtNonlinear;
    return ops;
}

/**
 * @brief Register BJT devices
 */
static bool registerBjt() {
    registerDevice(createBjtOps(DeviceType::NPN));
    registerDevice(createBjtOps(DeviceType::PNP));
    return true;
}

static bool bjt_registered = registerBjt();

} // namespace spice
