/**
 * @file acan.cpp
 * @brief AC Small-Signal Analysis (C++17 version)
 *
 * Implements AC analysis by linearizing the circuit around the DC operating
 * point and solving the frequency-domain equations.
 *
 * Command: .AC <type> <points> <start> <stop>
 *   type: LIN, DEC, OCT
 *
 * Algorithm:
 * 1. Run DC OP to find operating point
 * 2. Linearize nonlinear devices
 * 3. For each frequency:
 *    a. Compute omega = 2*pi*f
 *    b. Load AC device models (capacitors, inductors, linearized nonlinear)
 *    c. Solve MNA system
 *    d. Record magnitude and phase
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
 * @brief Compute frequency for sweep type
 */
static Real computeFrequency(int point, int total_points, Real start, Real stop, SweepType type) {
    switch (type) {
    case SweepType::LINEAR:
        return start + point * (stop - start) / (total_points - 1);

    case SweepType::DECADE: {
        Real log_start = std::log10(start);
        Real log_stop = std::log10(stop);
        Real log_freq = log_start + point * (log_stop - log_start) / (total_points - 1);
        return std::pow(10.0, log_freq);
    }

    case SweepType::OCTAVE: {
        Real log2_start = std::log2(start);
        Real log2_stop = std::log2(stop);
        Real log2_freq = log2_start + point * (log2_stop - log2_start) / (total_points - 1);
        return std::pow(2.0, log2_freq);
    }

    default:
        return start;
    }
}

/**
 * @brief AC analysis initialization
 */
static ErrorCode acanInit(Analysis* analysis, Circuit* ckt) {
    std::cout << "\n**** AC Small-Signal Analysis ****" << std::endl;
    return ErrorCode::OK;
}

/**
 * @brief AC analysis execution
 */
static ErrorCode acanRun(Analysis* analysis, Circuit* ckt) {
    const auto& params = analysis->params;
    Real freq_start = params.ac_start;
    Real freq_stop = params.ac_stop;
    int num_points = static_cast<int>(params.ac_points);
    SweepType sweep_type = params.ac_sweep_type;

    // First, run DC OP to linearize
    std::cout << "Running DC operating point..." << std::endl;
    ckt->init();

    Index total_eqns = ckt->num_eqns + ckt->num_vsources;
    SparseMatrix mat(total_eqns);
    std::vector<Real> x(total_eqns, 0.0);
    std::vector<Real> b(total_eqns, 0.0);

    // DC OP iteration (simplified)
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

    // Print header
    std::cout << "\nAC Analysis Results:" << std::endl;
    std::cout << std::left << std::setw(15) << "Frequency (Hz)"
              << std::right << std::setw(15) << "Magnitude"
              << std::setw(15) << "Phase (deg)" << std::endl;
    std::cout << std::string(45, '-') << std::endl;

    // Sweep frequencies
    for (int i = 0; i < num_points; i++) {
        Real freq = computeFrequency(i, num_points, freq_start, freq_stop, sweep_type);
        Real omega = 2.0 * M_PI * freq;

        // Clear and load AC model
        mat.clear();

        for (auto& dev : ckt->devices) {
            const DeviceOps* ops = getDeviceOps(dev->type);
            if (!ops) continue;

            if (ops->hasAcLoad()) {
                ops->acLoad(dev.get(), ckt, &mat, omega);
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

        // Solve
        if (mat.factor() != ErrorCode::OK) {
            std::cerr << "Warning: AC analysis failed at f = " << freq << std::endl;
            continue;
        }

        std::vector<Real> ac_result(total_eqns);
        if (mat.solve(b, ac_result) != ErrorCode::OK) {
            std::cerr << "Warning: AC solve failed at f = " << freq << std::endl;
            continue;
        }

        // For simplicity, print the first non-ground node voltage
        // In a full implementation, you'd track which nodes to print
        for (auto& node : ckt->nodes) {
            if (!node->is_ground && node->eqnum >= 0) {
                Real mag = std::fabs(ac_result[node->eqnum]);
                Real phase = 0.0;  // Simplified (would need complex numbers)

                std::cout << std::left << std::setw(15) << std::scientific << freq
                          << std::right << std::setw(15) << mag
                          << std::setw(15) << phase << std::endl;
                break;  // Just print first node for now
            }
        }
    }

    return ErrorCode::OK;
}

/**
 * @brief AC analysis cleanup
 */
static ErrorCode acanCleanup(Analysis* analysis, Circuit* ckt) {
    return ErrorCode::OK;
}

/**
 * @brief Create AC analysis operations
 */
static AnalysisOps createAcanOps() {
    AnalysisOps ops;
    ops.name = "ac";
    ops.type = AnalysisType::AC;
    ops.init = acanInit;
    ops.run = acanRun;
    ops.cleanup = acanCleanup;
    return ops;
}

/**
 * @brief Register AC analysis
 */
static bool registerAcan() {
    registerAnalysis(createAcanOps());
    return true;
}

static bool ac_registered = registerAcan();

} // namespace spice
