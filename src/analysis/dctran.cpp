/**
 * @file dctran.cpp
 * @brief Transient Analysis (C++17 version)
 *
 * Implements transient (time-domain) analysis using trapezoidal integration.
 *
 * Command: .TRAN <tstep> <tstop> [UIC]
 *
 * Algorithm:
 * 1. Initialize circuit (DC OP unless UIC)
 * 2. For each time step:
 *    a. Update time
 *    b. Evaluate waveform sources
 *    c. Load capacitors/inductors (trapezoidal equivalent)
 *    d. Load other devices
 *    e. Solve MNA system
 *    f. Check convergence (nonlinear)
 *    g. Update device state (history)
 *    h. Record output
 */
#include "analysis.hpp"
#include "device.hpp"
#include "sparse.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

namespace spice {

/**
 * @brief Transient analysis initialization
 */
static ErrorCode dctranInit(Analysis* analysis, Circuit* ckt) {
    std::cout << "\n**** Transient Analysis ****" << std::endl;
    const auto& params = analysis->params;
    std::cout << "Time step: " << params.tstep << " s" << std::endl;
    std::cout << "Stop time: " << params.tstop << " s" << std::endl;
    return ErrorCode::OK;
}

/**
 * @brief Transient analysis execution
 */
static ErrorCode dctranRun(Analysis* analysis, Circuit* ckt) {
    const auto& params = analysis->params;
    Real tstep = params.tstep;
    Real tstop = params.tstop;
    bool use_uic = params.use_uic;

    Index total_eqns = ckt->num_eqns + ckt->num_vsources;

    // Initialize
    ckt->init();

    // Run DC OP first (unless UIC - Use Initial Conditions)
    if (!use_uic) {
        std::cout << "Running DC operating point..." << std::endl;

        SparseMatrix mat(total_eqns);
        std::vector<Real> x(total_eqns, 0.0);
        std::vector<Real> b(total_eqns, 0.0);

        for (int iter = 0; iter < ckt->maxiter; iter++) {
            mat.clear();

            for (auto& dev : ckt->devices) {
                const DeviceOps* ops = getDeviceOps(dev->type);
                if (!ops) continue;

                if (ops->hasNonlinear()) {
                    ops->nonlinear(dev.get(), ckt, &mat);
                } else if (ops->load) {
                    ops->load(dev.get(), ckt, &mat);
                }
            }

            for (auto& node : ckt->nodes) {
                if (!node->is_ground && node->eqnum >= 0) {
                    mat.addElement(node->eqnum, node->eqnum, ckt->gmin);
                }
            }

            for (Index i = 0; i < total_eqns; i++) {
                b[i] = mat.getRhs(i);
            }

            if (mat.factor() != ErrorCode::OK) break;
            if (mat.solve(b, x) != ErrorCode::OK) break;

            Real max_change = 0.0;
            for (Index i = 0; i < ckt->num_eqns; i++) {
                Real change = std::fabs(x[i] - ckt->voltage[i]);
                if (change > max_change) max_change = change;
                ckt->voltage[i] = x[i];
            }

            if (max_change < ckt->vntol) break;
        }
    }

    // Print header
    std::cout << "\nTransient Analysis Results:" << std::endl;
    std::cout << std::left << std::setw(15) << "Time (s)"
              << std::right << std::setw(15) << "Status" << std::endl;
    std::cout << std::string(30, '-') << std::endl;

    // Time stepping
    int num_steps = static_cast<int>(std::round(tstop / tstep));
    ckt->time = 0.0;

    for (int step = 0; step <= num_steps; step++) {
        ckt->time = step * tstep;

        // Newton-Raphson for nonlinear convergence at this time step
        bool converged = false;

        SparseMatrix mat(total_eqns);
        std::vector<Real> x(total_eqns, 0.0);
        std::vector<Real> b(total_eqns, 0.0);

        for (int iter = 0; iter < ckt->trmaxiter; iter++) {
            mat.clear();

            // Load all devices
            for (auto& dev : ckt->devices) {
                const DeviceOps* ops = getDeviceOps(dev->type);
                if (!ops) continue;

                // Evaluate waveform sources at current time
                if (dev->waveform && (dev->type == DeviceType::VSRC ||
                    dev->type == DeviceType::ISRC)) {
                    dev->value = dev->waveform->evaluate(ckt->time);
                }

                if (ops->hasNonlinear()) {
                    ops->nonlinear(dev.get(), ckt, &mat);
                } else if (ops->load) {
                    ops->load(dev.get(), ckt, &mat);
                }
            }

            // Add GMIN
            for (auto& node : ckt->nodes) {
                if (!node->is_ground && node->eqnum >= 0) {
                    mat.addElement(node->eqnum, node->eqnum, ckt->gmin);
                }
            }

            // Copy RHS
            for (Index i = 0; i < total_eqns; i++) {
                b[i] = mat.getRhs(i);
            }

            // Factor and solve
            if (mat.factor() != ErrorCode::OK) break;
            if (mat.solve(b, x) != ErrorCode::OK) break;

            // Check convergence
            Real max_change = 0.0;
            for (Index i = 0; i < ckt->num_eqns; i++) {
                Real change = std::fabs(x[i] - ckt->voltage[i]);
                if (change > max_change) max_change = change;
                ckt->voltage[i] = x[i];
            }

            if (max_change < ckt->vntol) {
                converged = true;
                break;
            }
        }

        // Print result
        std::cout << std::left << std::setw(15) << std::scientific << ckt->time
                  << std::right << std::setw(15)
                  << (converged ? "OK" : "FAILED") << std::endl;

        // Update device state (capacitor/inductor history)
        for (auto& dev : ckt->devices) {
            const DeviceOps* ops = getDeviceOps(dev->type);
            if (ops && ops->update) {
                ops->update(dev.get(), ckt);
            }
        }
    }

    return ErrorCode::OK;
}

/**
 * @brief Transient analysis cleanup
 */
static ErrorCode dctranCleanup(Analysis* analysis, Circuit* ckt) {
    return ErrorCode::OK;
}

/**
 * @brief Create transient analysis operations
 */
static AnalysisOps createDctranOps() {
    AnalysisOps ops;
    ops.name = "transient";
    ops.type = AnalysisType::TRANSIENT;
    ops.init = dctranInit;
    ops.run = dctranRun;
    ops.cleanup = dctranCleanup;
    return ops;
}

/**
 * @brief Register transient analysis
 */
static bool registerDctran() {
    registerAnalysis(createDctranOps());
    return true;
}

static bool tran_registered = registerDctran();

} // namespace spice
