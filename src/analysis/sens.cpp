/**
 * @file sens.cpp
 * @brief Sensitivity Analysis (C++17 version)
 *
 * Implements DC sensitivity analysis using the perturbation method.
 *
 * Command: .SENS V(out)
 *
 * Algorithm:
 * 1. Run DC OP to find baseline operating point
 * 2. For each resistor:
 *    a. Perturb value by small amount (1%)
 *    b. Re-run DC OP
 *    c. Compute sensitivity: S = (V_out(R+ΔR) - V_out(R)) / ΔR
 *    d. Compute relative sensitivity: S_rel = S * R / V_out
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
 * @brief Run a simplified DC OP (helper function)
 */
static ErrorCode runDcop(Circuit* ckt) {
    ckt->init();

    Index total_eqns = ckt->num_eqns + ckt->num_vsources;
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

    return ErrorCode::OK;
}

/**
 * @brief Get voltage at a node
 */
static Real getNodeVoltage(Circuit* ckt, const std::string& node_name) {
    Node* node = ckt->findNode(node_name);
    if (node && node->eqnum >= 0) {
        return ckt->voltage[node->eqnum];
    }
    return 0.0;
}

/**
 * @brief Sensitivity analysis initialization
 */
static ErrorCode sensInit(Analysis* analysis, Circuit* ckt) {
    std::cout << "\n**** Sensitivity Analysis ****" << std::endl;
    std::cout << "Output: " << analysis->params.sens_output << std::endl;
    return ErrorCode::OK;
}

/**
 * @brief Sensitivity analysis execution
 */
static ErrorCode sensRun(Analysis* analysis, Circuit* ckt) {
    const auto& params = analysis->params;
    std::string output_name = params.sens_output;

    // Extract node name from "V(node)" format
    std::string node_name = output_name;
    if (node_name.substr(0, 2) == "V(" && node_name.back() == ')') {
        node_name = node_name.substr(2, node_name.size() - 3);
    }

    // Run baseline DC OP
    std::cout << "Running baseline DC operating point..." << std::endl;
    runDcop(ckt);
    Real v_baseline = getNodeVoltage(ckt, node_name);

    // Print header
    std::cout << "\nSensitivity Analysis Results:" << std::endl;
    std::cout << std::left << std::setw(15) << "Component"
              << std::right << std::setw(15) << "Value"
              << std::setw(15) << "Sensitivity"
              << std::setw(15) << "Rel. Sensitivity" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    // Perturb each resistor
    Real perturbation = 0.01;  // 1%

    for (auto& dev : ckt->devices) {
        if (dev->type != DeviceType::RESISTOR)
            continue;

        Real R_original = dev->value;
        Real delta_R = R_original * perturbation;

        // Perturb resistor
        dev->value = R_original + delta_R;

        // Re-run DC OP
        runDcop(ckt);
        Real v_perturbed = getNodeVoltage(ckt, node_name);

        // Compute sensitivity
        Real sensitivity = (v_perturbed - v_baseline) / delta_R;
        Real rel_sensitivity = (v_baseline != 0) ?
                               sensitivity * R_original / v_baseline : 0.0;

        // Print result
        std::cout << std::left << std::setw(15) << dev->name
                  << std::right << std::setw(15) << std::scientific << R_original
                  << std::setw(15) << sensitivity
                  << std::setw(15) << rel_sensitivity << std::endl;

        // Restore original value
        dev->value = R_original;
    }

    return ErrorCode::OK;
}

/**
 * @brief Sensitivity analysis cleanup
 */
static ErrorCode sensCleanup(Analysis* analysis, Circuit* ckt) {
    return ErrorCode::OK;
}

/**
 * @brief Create sensitivity analysis operations
 */
static AnalysisOps createSensOps() {
    AnalysisOps ops;
    ops.name = "sensitivity";
    ops.type = AnalysisType::SENSITIVITY;
    ops.init = sensInit;
    ops.run = sensRun;
    ops.cleanup = sensCleanup;
    return ops;
}

/**
 * @brief Register sensitivity analysis
 */
static bool registerSens() {
    registerAnalysis(createSensOps());
    return true;
}

static bool sens_registered = registerSens();

} // namespace spice
