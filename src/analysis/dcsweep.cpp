/**
 * @file dcsweep.cpp
 * @brief DC Sweep Analysis (C++17 version)
 *
 * Implements DC transfer characteristic sweep.
 *
 * Command: .DC <source> <start> <stop> <step>
 *
 * Algorithm:
 * 1. For each source value from start to stop:
 *    a. Set source value
 *    b. Run DC operating point analysis
 *    c. Record output
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
 * @brief DC sweep initialization
 */
static ErrorCode dcsweepInit(Analysis* analysis, Circuit* ckt) {
    std::cout << "\n**** DC Sweep Analysis ****" << std::endl;
    std::cout << "Source: " << analysis->params.src_name << std::endl;
    std::cout << "Range: " << analysis->params.start
              << " to " << analysis->params.stop
              << " step " << analysis->params.step << std::endl;
    return ErrorCode::OK;
}

/**
 * @brief DC sweep execution
 *
 * Loops over source values and runs DC OP at each step.
 */
static ErrorCode dcsweepRun(Analysis* analysis, Circuit* ckt) {
    const auto& params = analysis->params;
    std::string src_name = params.src_name;
    Real start = params.start;
    Real stop = params.stop;
    Real step = params.step;

    // Find the source device
    Device* src_dev = nullptr;
    for (auto& dev : ckt->devices) {
        if (dev->name == src_name) {
            src_dev = dev.get();
            break;
        }
    }

    if (!src_dev) {
        std::cerr << "Error: source " << src_name << " not found" << std::endl;
        return ErrorCode::NOT_FOUND;
    }

    // Compute number of steps
    int num_points = static_cast<int>(std::round((stop - start) / step)) + 1;

    // Print header
    std::cout << "\nDC Sweep Results:" << std::endl;
    std::cout << std::left << std::setw(15) << src_name
              << std::right << std::setw(15) << "Status" << std::endl;
    std::cout << std::string(30, '-') << std::endl;

    // Save original source value
    Real original_value = src_dev->value;

    // Sweep
    for (int i = 0; i < num_points; i++) {
        Real src_value = start + i * step;
        src_dev->value = src_value;

        // Reset circuit state
        ckt->init();

        // Run DC OP (simplified inline)
        Index total_eqns = ckt->num_eqns + ckt->num_vsources;
        SparseMatrix mat(total_eqns);
        std::vector<Real> x(total_eqns, 0.0);
        std::vector<Real> b(total_eqns, 0.0);

        bool converged = false;

        for (int iter = 0; iter < ckt->maxiter; iter++) {
            mat.clear();

            // Load devices
            for (auto& dev : ckt->devices) {
                const DeviceOps* ops = getDeviceOps(dev->type);
                if (!ops) continue;

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
            for (Index j = 0; j < total_eqns; j++) {
                b[j] = mat.getRhs(j);
            }

            // Factor and solve
            if (mat.factor() != ErrorCode::OK) break;
            if (mat.solve(b, x) != ErrorCode::OK) break;

            // Check convergence
            Real max_change = 0.0;
            for (Index j = 0; j < ckt->num_eqns; j++) {
                Real change = std::fabs(x[j] - ckt->voltage[j]);
                if (change > max_change) max_change = change;
                ckt->voltage[j] = x[j];
            }

            if (max_change < ckt->vntol) {
                converged = true;
                break;
            }
        }

        // Print result
        std::cout << std::left << std::setw(15) << std::scientific << src_value
                  << std::right << std::setw(15)
                  << (converged ? "OK" : "FAILED") << std::endl;

        if (!converged) {
            std::cerr << "Warning: DC sweep failed at " << src_name << " = " << src_value << std::endl;
        }
    }

    // Restore original source value
    src_dev->value = original_value;

    return ErrorCode::OK;
}

/**
 * @brief DC sweep cleanup
 */
static ErrorCode dcsweepCleanup(Analysis* analysis, Circuit* ckt) {
    return ErrorCode::OK;
}

/**
 * @brief Create DC sweep analysis operations
 */
static AnalysisOps createDcsweepOps() {
    AnalysisOps ops;
    ops.name = "dc sweep";
    ops.type = AnalysisType::DC_SWEEP;
    ops.init = dcsweepInit;
    ops.run = dcsweepRun;
    ops.cleanup = dcsweepCleanup;
    return ops;
}

/**
 * @brief Register DC sweep analysis
 */
static bool registerDcsweep() {
    registerAnalysis(createDcsweepOps());
    return true;
}

static bool dcsweep_registered = registerDcsweep();

} // namespace spice
