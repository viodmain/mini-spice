/**
 * @file vsrc.cpp
 * @brief Independent voltage source device model (C++17 version)
 *
 * Implements the voltage source with support for:
 * - DC voltage
 * - AC small-signal
 * - Waveform sources: SIN, PULSE, PWL, EXP
 *
 * The voltage source adds a row/column to the MNA matrix for the
 * branch current, with the voltage value on the RHS.
 */
#include "device.hpp"
#include "sparse.hpp"
#include <cmath>

namespace spice {

/**
 * @brief Voltage source load: load DC contributions
 *
 * Stamp:
 * @code
 *   [ n1  n2  ib ]
 * n1 [  0   0   1 ]
 * n2 [  0   0  -1 ]
 * ib [  1  -1   0 ]  = V
 * @endcode
 * where ib is the branch current equation number.
 */
static ErrorCode vsrcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Index eq1 = dev->n1;
    Index eq2 = dev->n2;
    Index ib = dev->n3;  // Branch current equation number

    // Get DC voltage value (or waveform value at t=0)
    Real v = dev->value;
    if (dev->waveform) {
        // For DC operating point, use waveform at t=0 or DC offset
        if (dev->waveform->type == WaveformType::SIN) {
            v = dev->waveform->sin_voffset;
        } else if (dev->waveform->type == WaveformType::PULSE) {
            v = dev->waveform->pulse_v1;
        } else if (dev->waveform->type == WaveformType::PWL) {
            v = dev->waveform->evaluate(ckt->time);
        } else if (dev->waveform->type == WaveformType::EXP) {
            v = dev->waveform->evaluate(ckt->time);
        } else {
            v = dev->waveform->evaluate(ckt->time);
        }
    }

    // Load stamp
    if (eq1 >= 0) {
        mat->addElement(eq1, ib, 1.0);
    }
    if (eq2 >= 0) {
        mat->addElement(eq2, ib, -1.0);
    }
    if (ib >= 0) {
        if (eq1 >= 0)
            mat->addElement(ib, eq1, 1.0);
        if (eq2 >= 0)
            mat->addElement(ib, eq2, -1.0);
        // RHS = voltage value
        mat->setRhs(ib, v);
    }

    return ErrorCode::OK;
}

/**
 * @brief Voltage source AC load
 *
 * For AC analysis, the voltage source contributes its AC magnitude
 * and phase to the RHS vector.
 */
static ErrorCode vsrcAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    Index eq1 = dev->n1;
    Index eq2 = dev->n2;
    Index ib = dev->n3;

    // Get AC magnitude and phase from waveform
    Real ac_mag = 0.0;
    Real ac_phase = 0.0;

    if (dev->waveform && dev->waveform->type == WaveformType::AC) {
        ac_mag = dev->waveform->ac_mag;
        ac_phase = dev->waveform->ac_phase;
    } else {
        // Default: use value as AC magnitude
        ac_mag = dev->value;
    }

    // Convert phase to radians
    Real phase_rad = ac_phase * M_PI / 180.0;

    // For real-valued MNA, use cosine component
    Real v_ac = ac_mag * std::cos(phase_rad);

    // Load stamp (same as DC but with AC value)
    if (eq1 >= 0) {
        mat->addElement(eq1, ib, 1.0);
    }
    if (eq2 >= 0) {
        mat->addElement(eq2, ib, -1.0);
    }
    if (ib >= 0) {
        if (eq1 >= 0)
            mat->addElement(ib, eq1, 1.0);
        if (eq2 >= 0)
            mat->addElement(ib, eq2, -1.0);
        mat->setRhs(ib, v_ac);
    }

    return ErrorCode::OK;
}

/**
 * @brief Create voltage source device operations
 */
static DeviceOps createVsrcOps() {
    DeviceOps ops;
    ops.name = "V";
    ops.type = DeviceType::VSRC;
    ops.load = vsrcLoad;
    ops.acLoad = vsrcAcLoad;
    return ops;
}

/**
 * @brief Register voltage source device
 */
static bool registerVoltageSource() {
    registerDevice(createVsrcOps());
    return true;
}

static bool vsrc_registered = registerVoltageSource();

} // namespace spice
