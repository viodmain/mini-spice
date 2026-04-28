/**
 * @file dio.cpp
 * @brief Diode device model (C++17 version)
 *
 * Implements the PN junction diode model using the Shockley equation:
 * @code
 * I = Is * (exp(Vd/(n*Vt)) - 1)
 * @endcode
 *
 * The diode is a nonlinear device requiring Newton-Raphson iteration.
 * At each iteration, the diode is linearized as:
 * @code
 * I ≈ g * V + I_hist
 * @endcode
 * where:
 * - g = dI/dV = Is/(n*Vt) * exp(Vd/(n*Vt)) (dynamic conductance)
 * - I_hist = I - g*V (history current source)
 *
 * Series resistance (RS) is handled by modifying the stamp.
 */
#include "device.hpp"
#include "sparse.hpp"
#include <cmath>

namespace spice {

/**
 * @brief Diode load: linearized model for Newton-Raphson
 *
 * Delegates to nonlinear() for the actual loading.
 */
static ErrorCode dioLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    // Diode is nonlinear, handled in dioNonlinear
    return ErrorCode::OK;
}

/**
 * @brief Diode AC load: small-signal model
 *
 * Linearizes the diode around the DC operating point.
 * Small-signal conductance: g_d = Is/(n*Vt) * exp(Vd/(n*Vt))
 * Junction capacitance: Cj = Cjo / (1 - Vd/Vj)^m
 */
static ErrorCode dioAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    Model* model = dev->model.get();
    if (!model)
        return ErrorCode::OK;

    DiodeModelParams* params = model->getDiodeParams();
    if (!params)
        return ErrorCode::OK;

    Index n1 = dev->n1;
    Index n2 = dev->n2;

    // Get voltage across diode
    Real vd = dev->getVoltageAcross(*ckt);

    // Thermal voltage: Vt = k*T/q
    Real vt = 8.617e-5 * ckt->temp;

    // Small-signal conductance
    Real gd;
    if (vd < 0)
        gd = params->is / (params->n * vt);  // Reverse bias
    else {
        Real exp_arg = vd / (params->n * vt);
        if (exp_arg > 50) exp_arg = 50;  // Prevent overflow
        gd = params->is / (params->n * vt) * std::exp(exp_arg);
    }

    // Junction capacitance
    Real cj = params->cjo;
    if (params->vj > 0 && vd < params->vj) {
        cj = params->cjo / std::pow(1.0 - vd / params->vj, params->m);
    }
    Real b = omega * cj;  // Susceptance

    // AC stamp (conductance + susceptance in parallel)
    if (n1 >= 0 && n2 >= 0) {
        mat->addElement(n1, n1, gd + b);
        mat->addElement(n1, n2, -gd - b);
        mat->addElement(n2, n1, -gd - b);
        mat->addElement(n2, n2, gd + b);
    } else if (n1 >= 0) {
        mat->addElement(n1, n1, gd + b);
    } else if (n2 >= 0) {
        mat->addElement(n2, n2, gd + b);
    }

    return ErrorCode::OK;
}

/**
 * @brief Diode nonlinear: Newton-Raphson iteration
 *
 * Computes the linearized diode model:
 * 1. Evaluate current at current voltage estimate
 * 2. Compute dynamic conductance
 * 3. Load conductance stamp and current source term
 */
static ErrorCode dioNonlinear(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Model* model = dev->model.get();
    if (!model)
        return ErrorCode::OK;

    DiodeModelParams* params = model->getDiodeParams();
    if (!params)
        return ErrorCode::OK;

    Index n1 = dev->n1;
    Index n2 = dev->n2;

    // Get voltage across diode
    Real vd = dev->getVoltageAcross(*ckt);

    // Thermal voltage
    Real vt = 8.617e-5 * ckt->temp;

    // Limit voltage to prevent overflow
    Real vlimit = 20.0 * vt;
    if (vd > vlimit) vd = vlimit;
    if (vd < -vlimit) vd = -vlimit;

    // Diode current and dynamic conductance
    Real id;    // Diode current
    Real g;     // Dynamic conductance

    if (vd >= 0) {
        Real exp_arg = vd / (params->n * vt);
        if (exp_arg > 50) exp_arg = 50;  // Prevent overflow
        id = params->is * (std::exp(exp_arg) - 1.0);
        g = params->is / (params->n * vt) * std::exp(exp_arg);
    } else {
        id = -params->is;  // Reverse saturation current
        g = params->is / (params->n * vt);
    }

    // Handle series resistance
    if (params->rs > 0.0) {
        // Modify conductance for series resistance
        g = g / (1.0 + params->rs * g);
    }

    // Load conductance stamp
    if (n1 >= 0 && n2 >= 0) {
        mat->addElement(n1, n1, g);
        mat->addElement(n1, n2, -g);
        mat->addElement(n2, n1, -g);
        mat->addElement(n2, n2, g);
    } else if (n1 >= 0) {
        mat->addElement(n1, n1, g);
    } else if (n2 >= 0) {
        mat->addElement(n2, n2, g);
    }

    // Load current source term for Newton-Raphson
    // I_hist = I - g*V (current that makes the linearized model match the nonlinear one)
    Real i_hist = id - g * vd;
    if (n1 >= 0)
        mat->addRhs(n1, -i_hist);
    if (n2 >= 0)
        mat->addRhs(n2, i_hist);

    return ErrorCode::OK;
}

/**
 * @brief Create diode device operations
 */
static DeviceOps createDiodeOps() {
    DeviceOps ops;
    ops.name = "D";
    ops.type = DeviceType::DIODE;
    ops.load = dioLoad;
    ops.acLoad = dioAcLoad;
    ops.nonlinear = dioNonlinear;
    return ops;
}

/**
 * @brief Register diode device
 */
static bool registerDiode() {
    registerDevice(createDiodeOps());
    return true;
}

static bool diode_registered = registerDiode();

} // namespace spice
