/**
 * @file dcop.cpp
 * @brief DC Operating Point Analysis (C++17 version)
 *
 * Implements DC operating point analysis using Newton-Raphson iteration.
 *
 * Algorithm:
 * 1. Initialize voltages (from .IC or zero)
 * 2. Loop until convergence or max iterations:
 *    a. Clear MNA matrix
 *    b. Load all devices (linear and nonlinear)
 *    c. Add GMIN for stability
 *    d. Factor and solve sparse system
 *    e. Check convergence (max voltage change < vntol)
 * 3. Print results
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
 * @brief DC OP analysis initialization
 */
static ErrorCode dcopInit(Analysis* analysis, Circuit* ckt) {
    std::cout << "\n**** DC Operating Point Analysis ****" << std::endl;
    return ErrorCode::OK;
}

/**
 * @brief DC OP analysis execution
 *
 * Uses Newton-Raphson iteration to find the DC operating point.
 * At each iteration:
 * 1. Linearize nonlinear devices around current voltage estimate
 * 2. Assemble MNA matrix
 * 3. Solve for new voltages
 * 4. Check convergence
 */
static ErrorCode dcopRun(Analysis* analysis, Circuit* ckt) {
    Index total_eqns = ckt->num_eqns + ckt->num_vsources;

    // Create sparse matrix
    SparseMatrix mat(total_eqns);

    // Solution vectors
    std::vector<Real> x(total_eqns, 0.0);
    std::vector<Real> b(total_eqns, 0.0);

    // Initialize voltages
    ckt->init();

    // Newton-Raphson iteration
    bool converged = false;
    Real max_volt_change = 0.0;
    int final_iter = 0;

    for (int iter = 0; iter < ckt->maxiter; iter++) {
        final_iter = iter;
        // Clear matrix
        mat.clear();

        // Load all devices
        for (auto& dev : ckt->devices) {
            const DeviceOps* ops = getDeviceOps(dev->type);
            if (!ops)
                continue;

            if (ops->hasNonlinear()) {
                // Nonlinear device (diode, BJT, MOSFET)
                ops->nonlinear(dev.get(), ckt, &mat);
            } else if (ops->load) {
                // Linear device
                ops->load(dev.get(), ckt, &mat);
            }
        }

        // Add GMIN (minimum conductance to ground) for stability
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
        if (mat.factor() != ErrorCode::OK) {
            std::cerr << "Warning: matrix factorization failed at iteration " << iter << std::endl;
            break;
        }

        if (mat.solve(b, x) != ErrorCode::OK) {
            std::cerr << "Warning: matrix solve failed at iteration " << iter << std::endl;
            break;
        }

        // Check convergence
        max_volt_change = 0.0;
        for (Index i = 0; i < ckt->num_eqns; i++) {
            Real change = std::fabs(x[i] - ckt->voltage[i]);
            if (change > max_volt_change)
                max_volt_change = change;
            ckt->voltage[i] = x[i];
        }

        if (max_volt_change < ckt->vntol) {
            converged = true;
            break;
        }
    }

    // Print results
    if (converged) {
        std::cout << "Converged after " << final_iter + 1 << " iterations" << std::endl;
    } else {
        std::cout << "WARNING: DC analysis did not converge after " << ckt->maxiter
                  << " iterations" << std::endl;
        std::cout << "Max voltage change: " << max_volt_change << std::endl;
    }

    // Print node voltages
    std::cout << "\nNode Voltages:" << std::endl;
    std::cout << std::left << std::setw(15) << "Node"
              << std::right << std::setw(15) << "Voltage (V)" << std::endl;
    std::cout << std::string(30, '-') << std::endl;

    for (auto& node : ckt->nodes) {
        if (node->is_ground) {
            std::cout << std::left << std::setw(15) << node->name
                      << std::right << std::setw(15) << std::scientific << 0.0
                      << std::endl;
        } else if (node->eqnum >= 0) {
            std::cout << std::left << std::setw(15) << node->name
                      << std::right << std::setw(15) << std::scientific
                      << ckt->voltage[node->eqnum] << std::endl;
        }
    }

    // Print voltage source currents
    std::cout << "\nVoltage Source Currents:" << std::endl;
    std::cout << std::left << std::setw(15) << "Source"
              << std::right << std::setw(15) << "Current (A)" << std::endl;
    std::cout << std::string(30, '-') << std::endl;

    for (auto& dev : ckt->devices) {
        if (dev->type == DeviceType::VSRC) {
            Index ib = dev->n3;
            if (ib >= 0 && ib < total_eqns) {
                std::cout << std::left << std::setw(15) << dev->name
                          << std::right << std::setw(15) << std::scientific
                          << x[ib] << std::endl;
            }
        }
    }

    return converged ? ErrorCode::OK : ErrorCode::CONVERGENCE_FAILURE;
}

/**
 * @brief DC OP analysis cleanup
 */
static ErrorCode dcopCleanup(Analysis* analysis, Circuit* ckt) {
    return ErrorCode::OK;
}

/**
 * @brief Create DC OP analysis operations
 */
static AnalysisOps createDcopOps() {
    AnalysisOps ops;
    ops.name = "dc op";
    ops.type = AnalysisType::DC_OP;
    ops.init = dcopInit;
    ops.run = dcopRun;
    ops.cleanup = dcopCleanup;
    return ops;
}

/**
 * @brief Register DC OP analysis
 */
static bool registerDcop() {
    registerAnalysis(createDcopOps());
    return true;
}

static bool dcop_registered = registerDcop();

} // namespace spice
