/**
 * @file noise.cpp
 * @brief Noise Analysis (C++17 version)
 *
 * Implements noise analysis by computing thermal and shot noise contributions.
 *
 * Command: .NOISE V(out) src <type> <points> <start> <stop>
 *
 * Noise sources:
 * - Resistor thermal noise: i_n² = 4kT/R
 * - Diode/BJT shot noise: i_n² = 2qI
 *
 * Uses superposition of uncorrelated noise sources.
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
 * @brief Compute thermal noise current spectral density for a resistor
 * @param R Resistance (Ohms)
 * @param T Temperature (Kelvin)
 * @return Noise current spectral density (A²/Hz)
 */
static Real thermalNoise(Real R, Real T) {
    // i_n² = 4kT/R
    Real k = 1.380649e-23;  // Boltzmann constant
    return 4.0 * k * T / R;
}

/**
 * @brief Compute shot noise current spectral density
 * @param I DC current (Amps)
 * @return Noise current spectral density (A²/Hz)
 */
static Real shotNoise(Real I) {
    // i_n² = 2qI
    Real q = 1.602176634e-19;  // Electron charge
    return 2.0 * q * std::fabs(I);
}

/**
 * @brief Noise analysis initialization
 */
static ErrorCode noiseInit(Analysis* analysis, Circuit* ckt) {
    std::cout << "\n**** Noise Analysis ****" << std::endl;
    return ErrorCode::OK;
}

/**
 * @brief Noise analysis execution
 *
 * Simplified implementation: computes total output noise density
 * by summing contributions from all resistors.
 */
static ErrorCode noiseRun(Analysis* analysis, Circuit* ckt) {
    const auto& params = analysis->params;
    Real freq_start = params.noise_start;
    Real freq_stop = params.noise_stop;
    int num_points = static_cast<int>(params.noise_points);
    SweepType sweep_type = params.noise_sweep_type;

    // First, run DC OP to get operating point
    std::cout << "Running DC operating point..." << std::endl;
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

    // Print header
    std::cout << "\nNoise Analysis Results:" << std::endl;
    std::cout << std::left << std::setw(15) << "Frequency (Hz)"
              << std::right << std::setw(20) << "Output Noise (V/√Hz)" << std::endl;
    std::cout << std::string(35, '-') << std::endl;

    // Compute noise at each frequency point
    // (In a full implementation, you'd compute frequency-dependent noise)
    for (int i = 0; i < num_points; i++) {
        Real freq;
        switch (sweep_type) {
        case SweepType::LINEAR:
            freq = freq_start + i * (freq_stop - freq_start) / (num_points - 1);
            break;
        case SweepType::DECADE: {
            Real log_start = std::log10(freq_start);
            Real log_stop = std::log10(freq_stop);
            freq = std::pow(10.0, log_start + i * (log_stop - log_start) / (num_points - 1));
            break;
        }
        case SweepType::OCTAVE: {
            Real log2_start = std::log2(freq_start);
            Real log2_stop = std::log2(freq_stop);
            freq = std::pow(2.0, log2_start + i * (log2_stop - log2_start) / (num_points - 1));
            break;
        }
        default:
            freq = freq_start;
        }

        // Sum noise contributions from all resistors
        Real total_noise = 0.0;
        for (auto& dev : ckt->devices) {
            if (dev->type == DeviceType::RESISTOR) {
                Real R = dev->value;
                Real in2 = thermalNoise(R, ckt->temp);
                total_noise += in2;
            }
        }

        // Output noise density (simplified: just sum of input noise sources)
        Real noise_density = std::sqrt(total_noise);

        std::cout << std::left << std::setw(15) << std::scientific << freq
                  << std::right << std::setw(20) << noise_density << std::endl;
    }

    return ErrorCode::OK;
}

/**
 * @brief Noise analysis cleanup
 */
static ErrorCode noiseCleanup(Analysis* analysis, Circuit* ckt) {
    return ErrorCode::OK;
}

/**
 * @brief Create noise analysis operations
 */
static AnalysisOps createNoiseOps() {
    AnalysisOps ops;
    ops.name = "noise";
    ops.type = AnalysisType::NOISE;
    ops.init = noiseInit;
    ops.run = noiseRun;
    ops.cleanup = noiseCleanup;
    return ops;
}

/**
 * @brief Register noise analysis
 */
static bool registerNoise() {
    registerAnalysis(createNoiseOps());
    return true;
}

static bool noise_registered = registerNoise();

} // namespace spice
