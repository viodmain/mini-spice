/**
 * @file fourier.cpp
 * @brief Fourier Analysis (C++17 version)
 *
 * Implements Discrete Fourier Transform (DFT) for harmonic analysis.
 *
 * Command: .FOUR <fund_freq> V(out) [harmonics]
 *
 * Algorithm:
 * 1. Run transient analysis for one period
 * 2. Collect waveform samples
 * 3. Compute DFT for fundamental and harmonics
 * 4. Calculate THD (Total Harmonic Distortion)
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
 * @brief Fourier analysis initialization
 */
static ErrorCode fourierInit(Analysis* analysis, Circuit* ckt) {
    std::cout << "\n**** Fourier Analysis ****" << std::endl;
    return ErrorCode::OK;
}

/**
 * @brief Fourier analysis execution
 *
 * Runs transient analysis, collects samples, and computes DFT.
 */
static ErrorCode fourierRun(Analysis* analysis, Circuit* ckt) {
    const auto& params = analysis->params;
    Real fund_freq = params.four_freq;
    int num_harmonics = params.four_harmonics;

    Real period = 1.0 / fund_freq;
    int num_samples = 1000;  // Default number of samples
    Real dt = period / num_samples;

    // Run transient for one period (simplified)
    std::cout << "Running transient for one period..." << std::endl;

    ckt->init();
    Index total_eqns = ckt->num_eqns + ckt->num_vsources;

    // Collect waveform samples
    std::vector<Real> samples(num_samples);
    ckt->time = 0.0;

    SparseMatrix mat(total_eqns);
    std::vector<Real> x(total_eqns, 0.0);
    std::vector<Real> b(total_eqns, 0.0);

    for (int i = 0; i < num_samples; i++) {
        ckt->time = i * dt;

        // Solve circuit at this time step
        mat.clear();

        for (auto& dev : ckt->devices) {
            const DeviceOps* ops = getDeviceOps(dev->type);
            if (!ops) continue;

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

        for (auto& node : ckt->nodes) {
            if (!node->is_ground && node->eqnum >= 0) {
                mat.addElement(node->eqnum, node->eqnum, ckt->gmin);
            }
        }

        for (Index j = 0; j < total_eqns; j++) {
            b[j] = mat.getRhs(j);
        }

        if (mat.factor() == ErrorCode::OK && mat.solve(b, x) == ErrorCode::OK) {
            // Find output node voltage
            // (Simplified: use first non-ground node)
            for (auto& node : ckt->nodes) {
                if (!node->is_ground && node->eqnum >= 0) {
                    samples[i] = x[node->eqnum];
                    break;
                }
            }
        }

        // Update device state
        for (auto& dev : ckt->devices) {
            const DeviceOps* ops = getDeviceOps(dev->type);
            if (ops && ops->update) {
                ops->update(dev.get(), ckt);
            }
        }
    }

    // Compute DFT
    std::cout << "\nFourier Components:" << std::endl;
    std::cout << std::left << std::setw(10) << "Harmonic"
              << std::right << std::setw(15) << "Freq (Hz)"
              << std::setw(15) << "Magnitude"
              << std::setw(15) << "Phase (deg)" << std::endl;
    std::cout << std::string(55, '-') << std::endl;

    // DFT coefficients
    std::vector<Real> mag(num_harmonics + 1);
    std::vector<Real> phase(num_harmonics + 1);

    for (int h = 0; h <= num_harmonics; h++) {
        Real a_h = 0.0, b_h = 0.0;
        Real freq = h * fund_freq;

        for (int n = 0; n < num_samples; n++) {
            Real t = n * dt;
            Real angle = 2.0 * M_PI * freq * t;
            a_h += samples[n] * std::cos(angle);
            b_h -= samples[n] * std::sin(angle);
        }

        a_h *= 2.0 / num_samples;
        b_h *= 2.0 / num_samples;

        mag[h] = std::sqrt(a_h * a_h + b_h * b_h);
        phase[h] = std::atan2(-b_h, a_h) * 180.0 / M_PI;

        std::cout << std::left << std::setw(10) << h
                  << std::right << std::setw(15) << std::scientific << freq
                  << std::setw(15) << mag[h]
                  << std::setw(15) << phase[h] << std::endl;
    }

    // Compute THD
    Real fundamental_mag = mag[1];
    Real harmonic_power = 0.0;

    for (int h = 2; h <= num_harmonics; h++) {
        harmonic_power += mag[h] * mag[h];
    }

    Real thd = (fundamental_mag > 0) ?
               std::sqrt(harmonic_power) / fundamental_mag * 100.0 : 0.0;

    std::cout << "\nTotal Harmonic Distortion: " << std::fixed
              << std::setprecision(2) << thd << "%" << std::endl;

    return ErrorCode::OK;
}

/**
 * @brief Fourier analysis cleanup
 */
static ErrorCode fourierCleanup(Analysis* analysis, Circuit* ckt) {
    return ErrorCode::OK;
}

/**
 * @brief Create Fourier analysis operations
 */
static AnalysisOps createFourierOps() {
    AnalysisOps ops;
    ops.name = "fourier";
    ops.type = AnalysisType::FOURIER;
    ops.init = fourierInit;
    ops.run = fourierRun;
    ops.cleanup = fourierCleanup;
    return ops;
}

/**
 * @brief Register Fourier analysis
 */
static bool registerFourier() {
    registerAnalysis(createFourierOps());
    return true;
}

static bool fourier_registered = registerFourier();

} // namespace spice
