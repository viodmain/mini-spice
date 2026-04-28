/**
 * @file tline.cpp
 * @brief Transmission line device model (C++17 version)
 *
 * Implements the lossless transmission line using the Method of Characteristics.
 *
 * Device: T<name> n1+ n1- n2+ n2- <model>
 *
 * The transmission line is characterized by:
 * - TD: Delay time
 * - Z0: Characteristic impedance
 *
 * The Method of Characteristics models the line as:
 * - At port 1: Current source I1_hist = V2(t-TD)/Z0 flowing into port 1
 * - At port 2: Current source I2_hist = V1(t-TD)/Z0 flowing into port 2
 * - Conductance G = 1/Z0 at each port
 */
#include "device.hpp"
#include "sparse.hpp"
#include <vector>
#include <cmath>

namespace spice {

/**
 * @brief Transmission line history buffer
 *
 * Stores past voltages for the Method of Characteristics.
 */
struct TlineHistory {
    std::vector<Real> v1_history;   /**< Port 1 voltage history */
    std::vector<Real> v2_history;   /**< Port 2 voltage history */
    Real dt = 0.0;                  /**< Time step */
    Real td = 0.0;                  /**< Delay time */
    int nsteps = 0;                 /**< Number of delay steps */
};

/**
 * @brief Transmission line setup: allocate history buffer
 */
static ErrorCode tlineSetup(Device* dev, Circuit* ckt) {
    dev->params = new TlineHistory();
    return ErrorCode::OK;
}

/**
 * @brief Transmission line load: load DC contributions
 *
 * In DC, the transmission line is just a wire (short circuit).
 */
static ErrorCode tlineLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    // In DC, transmission line is a short (direct connection)
    // Port 1 connects to Port 2
    Index n1p = dev->n1;
    Index n1m = dev->n2;
    Index n2p = dev->n3;
    Index n2m = dev->n4;

    // For DC, ports are directly connected (short circuit)
    // This is handled by the transient model's history
    return ErrorCode::OK;
}

/**
 * @brief Transmission line transient load
 *
 * Implements the Method of Characteristics:
 * - I1 = V1/Z0 + I1_hist
 * - I2 = V2/Z0 + I2_hist
 *
 * where I1_hist = -V2(t-TD)/Z0 and I2_hist = -V1(t-TD)/Z0
 */
static ErrorCode tlineTransientLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real dt) {
    auto* history = static_cast<TlineHistory*>(dev->params);
    if (!history)
        return ErrorCode::OK;

    Model* model = dev->model.get();
    if (!model)
        return ErrorCode::OK;

    TransmissionLineParams* params = model->getTlineParams();
    if (!params)
        return ErrorCode::OK;

    Index n1p = dev->n1;
    Index n1m = dev->n2;
    Index n2p = dev->n3;
    Index n2m = dev->n4;

    Real Z0 = params->z0;
    Real G = 1.0 / Z0;  // Characteristic conductance

    // Compute number of delay steps
    if (history->dt != dt && dt > 0) {
        history->nsteps = static_cast<int>(std::round(params->td / dt));
        history->dt = dt;
        history->td = params->td;
    }

    // Get current port voltages
    Real v1 = 0.0, v2 = 0.0;
    if (n1p >= 0 && n1m >= 0)
        v1 = dev->getNodeVoltage(*ckt, n1p) - dev->getNodeVoltage(*ckt, n1m);
    else if (n1p >= 0)
        v1 = dev->getNodeVoltage(*ckt, n1p);

    if (n2p >= 0 && n2m >= 0)
        v2 = dev->getNodeVoltage(*ckt, n2p) - dev->getNodeVoltage(*ckt, n2m);
    else if (n2p >= 0)
        v2 = dev->getNodeVoltage(*ckt, n2p);

    // Compute history currents (from delayed voltages)
    Real i1_hist = 0.0, i2_hist = 0.0;
    if (history->nsteps > 0 &&
        static_cast<int>(history->v2_history.size()) > history->nsteps) {
        i1_hist = -history->v2_history[history->v2_history.size() - history->nsteps - 1] / Z0;
    }
    if (history->nsteps > 0 &&
        static_cast<int>(history->v1_history.size()) > history->nsteps) {
        i2_hist = -history->v1_history[history->v1_history.size() - history->nsteps - 1] / Z0;
    }

    // Load conductance stamp at each port
    // Port 1
    if (n1p >= 0 && n1m >= 0) {
        mat->addElement(n1p, n1p, G);
        mat->addElement(n1p, n1m, -G);
        mat->addElement(n1m, n1p, -G);
        mat->addElement(n1m, n1m, G);
    } else if (n1p >= 0) {
        mat->addElement(n1p, n1p, G);
    } else if (n1m >= 0) {
        mat->addElement(n1m, n1m, G);
    }

    // Port 2
    if (n2p >= 0 && n2m >= 0) {
        mat->addElement(n2p, n2p, G);
        mat->addElement(n2p, n2m, -G);
        mat->addElement(n2m, n2p, -G);
        mat->addElement(n2m, n2m, G);
    } else if (n2p >= 0) {
        mat->addElement(n2p, n2p, G);
    } else if (n2m >= 0) {
        mat->addElement(n2m, n2m, G);
    }

    // Load history current sources
    // I1_hist flows into port 1, I2_hist flows into port 2
    if (n1p >= 0) mat->addRhs(n1p, -i1_hist);
    if (n1m >= 0) mat->addRhs(n1m, i1_hist);
    if (n2p >= 0) mat->addRhs(n2p, -i2_hist);
    if (n2m >= 0) mat->addRhs(n2m, i2_hist);

    return ErrorCode::OK;
}

/**
 * @brief Transmission line update: store voltage history
 */
static ErrorCode tlineUpdate(Device* dev, Circuit* ckt) {
    auto* history = static_cast<TlineHistory*>(dev->params);
    if (!history)
        return ErrorCode::OK;

    Index n1p = dev->n1;
    Index n1m = dev->n2;
    Index n2p = dev->n3;
    Index n2m = dev->n4;

    // Store current voltages
    Real v1 = 0.0, v2 = 0.0;
    if (n1p >= 0 && n1m >= 0)
        v1 = dev->getNodeVoltage(*ckt, n1p) - dev->getNodeVoltage(*ckt, n1m);
    else if (n1p >= 0)
        v1 = dev->getNodeVoltage(*ckt, n1p);

    if (n2p >= 0 && n2m >= 0)
        v2 = dev->getNodeVoltage(*ckt, n2p) - dev->getNodeVoltage(*ckt, n2m);
    else if (n2p >= 0)
        v2 = dev->getNodeVoltage(*ckt, n2p);

    history->v1_history.push_back(v1);
    history->v2_history.push_back(v2);

    // Limit history size to prevent memory issues
    int max_history = 10000;
    if (static_cast<int>(history->v1_history.size()) > max_history) {
        history->v1_history.erase(history->v1_history.begin());
        history->v2_history.erase(history->v2_history.begin());
    }

    return ErrorCode::OK;
}

/**
 * @brief Create transmission line device operations
 */
static DeviceOps createTlineOps() {
    DeviceOps ops;
    ops.name = "T";
    ops.type = DeviceType::TRANSMISSION_LINE;
    ops.setup = tlineSetup;
    ops.load = tlineLoad;
    ops.update = tlineUpdate;
    return ops;
}

/**
 * @brief Register transmission line device
 */
static bool registerTline() {
    registerDevice(createTlineOps());
    return true;
}

static bool tline_registered = registerTline();

} // namespace spice
