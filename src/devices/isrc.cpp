/**
 * @file isrc.cpp
 * @brief Independent current source device model (C++17 version)
 *
 * Implements the current source with support for:
 * - DC current
 * - AC small-signal
 *
 * The current source contributes to the RHS vector only
 * (no matrix entries needed for ideal current source).
 */
#include "device.hpp"
#include "sparse.hpp"

namespace spice {

/**
 * @brief Current source load: load DC contributions
 *
 * Stamp:
 * @code
 * n1: RHS -= I  (current leaving node)
 * n2: RHS += I  (current entering node)
 * @endcode
 */
static ErrorCode isrcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    Index eq1 = dev->n1;
    Index eq2 = dev->n2;

    // Get DC current value
    Real i = dev->value;

    // Current flows from n1 to n2
    if (eq1 >= 0)
        mat->addRhs(eq1, -i);  // Current leaving n1
    if (eq2 >= 0)
        mat->addRhs(eq2, i);   // Current entering n2

    return ErrorCode::OK;
}

/**
 * @brief Current source AC load
 *
 * For AC analysis, the current source contributes its AC magnitude
 * to the RHS vector.
 */
static ErrorCode isrcAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    Index eq1 = dev->n1;
    Index eq2 = dev->n2;

    // Get AC magnitude from waveform
    Real ac_mag = 0.0;

    if (dev->waveform && dev->waveform->type == WaveformType::AC) {
        ac_mag = dev->waveform->ac_mag;
    } else {
        ac_mag = dev->value;
    }

    // AC current flows from n1 to n2
    if (eq1 >= 0)
        mat->addRhs(eq1, -ac_mag);
    if (eq2 >= 0)
        mat->addRhs(eq2, ac_mag);

    return ErrorCode::OK;
}

/**
 * @brief Create current source device operations
 */
static DeviceOps createIsrcOps() {
    DeviceOps ops;
    ops.name = "I";
    ops.type = DeviceType::ISRC;
    ops.load = isrcLoad;
    ops.acLoad = isrcAcLoad;
    return ops;
}

/**
 * @brief Register current source device
 */
static bool registerCurrentSource() {
    registerDevice(createIsrcOps());
    return true;
}

static bool isrc_registered = registerCurrentSource();

} // namespace spice
