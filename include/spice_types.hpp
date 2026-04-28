/**
 * @file spice_types.hpp
 * @brief Common type definitions for mini-spice (C++17 version)
 *
 * This header defines fundamental types, constants, and enums used throughout
 * the mini-spice circuit simulator. It replaces the C version with modern C++17
 * features including constexpr, scoped enums, and type aliases.
 */
#ifndef SPICE_TYPES_HPP
#define SPICE_TYPES_HPP

#include <cmath>
#include <cfloat>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <memory>

namespace spice {

//=============================================================================
// Basic Type Aliases
//=============================================================================

/** @brief Real number type (double precision floating point) */
using Real = double;

/** @brief Index type for matrix positions and node numbers */
using Index = int;

//=============================================================================
// Simulation Constants (constexpr instead of #define)
//=============================================================================

/** @brief Absolute current tolerance (Amperes) */
inline constexpr Real DEFAULT_ABSTOL = 1.0e-12;

/** @brief Relative tolerance for convergence checks */
inline constexpr Real DEFAULT_RELTOL = 1.0e-6;

/** @brief Voltage tolerance (Volts) */
inline constexpr Real DEFAULT_VNTOL = 1.0e-6;

/** @brief Truncation error tolerance for transient analysis */
inline constexpr Real DEFAULT_TRTOL = 1.0;

/** @brief Pivoting tolerance for sparse matrix factorization */
inline constexpr Real DEFAULT_PIVTOL = 1.0e-13;

/** @brief Maximum iterations for DC/transient analysis convergence */
inline constexpr int DEFAULT_MAXITER = 50;

/** @brief Minimum conductance for GMIN stepping (Siemens) */
inline constexpr Real DEFAULT_GMIN = 1.0e-12;

//=============================================================================
// Physical Constants
//=============================================================================

/** @brief Number of device types supported */
inline constexpr int NUMDEV = 20;

/** @brief Number of analysis types supported */
inline constexpr int NUMANA = 8;

/** @brief Maximum piecewise-linear waveform points */
inline constexpr int MAX_PWL_POINTS = 100;

/** @brief Maximum subcircuit port arguments */
inline constexpr int MAX_SUBCKT_ARGS = 20;

/** @brief Thermal voltage at nominal temperature (27°C = 300.15K) in Volts */
inline constexpr Real VT_NOMINAL = 0.02585;

/** @brief Boltzmann constant (Joules/Kelvin) */
inline constexpr Real KB = 1.380649e-23;

/** @brief Elementary charge (Coulombs) */
inline constexpr Real QE = 1.602176634e-19;

/** @brief Nominal temperature (Celsius) */
inline constexpr Real TNOM_C = 27.0;

/** @brief Nominal temperature (Kelvin) */
inline constexpr Real TNOM_K = 300.15;

//=============================================================================
// Device Type Enumeration
//=============================================================================

/**
 * @brief Enumerates all supported device types
 *
 * Each device type corresponds to a SPICE element prefix:
 * - R: Resistor, C: Capacitor, L: Inductor
 * - V: Voltage source, I: Current source
 * - G: VCCS, E: VCVS, F: CCCS, H: CCVS
 * - D: Diode, Q: BJT, M: MOSFET
 * - B: Behavioral source, S/W: Switches, T: Transmission line, X: Subcircuit
 */
enum class DeviceType : int {
    /* Basic linear devices */
    RESISTOR = 0,           /**< R - Resistor */
    CAPACITOR,              /**< C - Capacitor */
    INDUCTOR,               /**< L - Inductor */
    VSRC,                   /**< V - Independent voltage source */
    ISRC,                   /**< I - Independent current source */

    /* Dependent sources */
    VCCS,                   /**< G - Voltage-controlled current source */
    VCVS,                   /**< E - Voltage-controlled voltage source */
    CCCS,                   /**< F - Current-controlled current source */
    CCVS,                   /**< H - Current-controlled voltage source */

    /* Nonlinear devices */
    DIODE,                  /**< D - Diode */
    NPN,                    /**< Q - NPN BJT */
    PNP,                    /**< Q - PNP BJT */
    NMOS,                   /**< M - NMOS transistor */
    PMOS,                   /**< M - PMOS transistor */

    /* Behavioral/advanced devices */
    BEHAVIORAL_SRC,         /**< B - Behavioral source (current) */
    BEHAVIORAL_VSRC,        /**< E-b - Behavioral voltage source */
    SWITCH_VOLTAGE,         /**< S - Voltage-controlled switch */
    SWITCH_CURRENT,         /**< W - Current-controlled switch */
    TRANSMISSION_LINE,      /**< T - Lossless transmission line */
    SUBCKT,                 /**< X - Subcircuit instance */

    NUM_DEVICES             /**< Total number of device types */
};

//=============================================================================
// Analysis Type Enumeration
//=============================================================================

/**
 * @brief Enumerates all supported analysis types
 *
 * These correspond to SPICE dot-commands:
 * - .OP: DC operating point
 * - .DC: DC sweep
 * - .AC: AC small-signal
 * - .TRAN: Transient
 * - .NOISE, .FOUR, .SENS: Advanced analyses
 */
enum class AnalysisType : int {
    DC_OP = 0,              /**< DC operating point analysis */
    DC_SWEEP,               /**< DC transfer characteristic sweep */
    AC,                     /**< AC small-signal analysis */
    TRANSIENT,              /**< Transient (time-domain) analysis */
    NOISE,                  /**< Noise analysis */
    FOURIER,                /**< Fourier analysis (harmonic distortion) */
    SENSITIVITY,            /**< Sensitivity analysis */
    POLE_ZERO,              /**< Pole-zero analysis */
    NUM_ANALYSES            /**< Total number of analysis types */
};

//=============================================================================
// Waveform Type Enumeration
//=============================================================================

/**
 * @brief Enumerates waveform types for voltage/current sources
 *
 * Supports time-varying sources in transient analysis and AC sources
 * for small-signal frequency domain analysis.
 */
enum class WaveformType : int {
    NONE = 0,               /**< DC only (no waveform) */
    SIN,                    /**< Sinusoidal: SIN(Voffset Vamp Freq Td Theta Phi) */
    PULSE,                  /**< Pulse: PULSE(V1 V2 Td Tr Tf Pw Per) */
    PWL,                    /**< Piecewise linear: PWL(t1 v1 t2 v2 ...) */
    EXP,                    /**< Exponential: EXP(V1 V2 Td1 Tau1 Td2 Tau2) */
    SFFM,                   /**< Sinusoidal frequency modulated */
    AC                      /**< AC small-signal only (for .AC analysis) */
};

//=============================================================================
// Integration Method Enumeration
//=============================================================================

/**
 * @brief Integration methods for transient analysis
 *
 * Trapezoidal is more accurate but can oscillate.
 * Gear is more stable but less accurate (second-order).
 */
enum class IntegrationMethod : int {
    TRAPEZOIDAL = 0,        /**< Trapezoidal integration (default) */
    GEAR                      /**< Gear integration (more stable) */
};

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Return codes for simulation functions
 *
 * These follow the ngspice convention with descriptive names.
 */
enum class ErrorCode : int {
    OK = 0,                 /**< Success */
    NO_MEMORY,              /**< Out of memory */
    SYNTAX_ERROR,           /**< Syntax error in netlist */
    NOT_FOUND,              /**< Entity not found */
    DUPLICATE,              /**< Duplicate entity */
    LOOP,                   /**< Logical loop detected */
    CONVERGENCE_FAILURE,    /**< Convergence failure */
    TROUBLE,                /**< Serious trouble */
    OVERFLOW                /**< Numerical overflow */
};

//=============================================================================
// Simulation Options
//=============================================================================

/**
 * @brief Configuration options for the simulator
 *
 * These control convergence tolerances, iteration limits, and numerical methods.
 * Can be set via .OPTIONS command in the netlist.
 */
struct SimulationOptions {
    Real abstol = DEFAULT_ABSTOL;       /**< Absolute current tolerance (A) */
    Real vntol = DEFAULT_VNTOL;         /**< Voltage tolerance (V) */
    Real reltol = DEFAULT_RELTOL;       /**< Relative tolerance */
    Real trtol = DEFAULT_TRTOL;         /**< Truncation error tolerance */
    int maxiter = DEFAULT_MAXITER;      /**< Max DC iterations */
    int trmaxiter = DEFAULT_MAXITER;    /**< Max transient iterations */
    Real gmin = DEFAULT_GMIN;           /**< Minimum conductance (S) */
    int gminsteps = 0;                  /**< Gmin stepping count */
    int srcsteps = 0;                   /**< Source stepping count */
    int numdgt = 6;                     /**< Output significant digits */
    IntegrationMethod method = IntegrationMethod::TRAPEZOIDAL; /**< Integration method */
    int ltol = 0;                       /**< Current rel tolerance enable */
    Real chgtol = 1.0e-14;              /**< Charge tolerance */
};

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Convert error code to human-readable string
 * @param code The error code to convert
 * @return Const string describing the error
 */
inline const char* errorToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK:                  return "OK";
        case ErrorCode::NO_MEMORY:           return "Out of memory";
        case ErrorCode::SYNTAX_ERROR:        return "Syntax error";
        case ErrorCode::NOT_FOUND:           return "Not found";
        case ErrorCode::DUPLICATE:           return "Duplicate";
        case ErrorCode::LOOP:                return "Logical loop";
        case ErrorCode::CONVERGENCE_FAILURE: return "Convergence failure";
        case ErrorCode::TROUBLE:             return "Serious trouble";
        case ErrorCode::OVERFLOW:            return "Numerical overflow";
        default:                             return "Unknown error";
    }
}

/**
 * @brief Convert device type enum to character prefix
 * @param type The device type
 * @return Character prefix (R, C, L, V, etc.)
 */
inline char deviceTypeToChar(DeviceType type) {
    switch (type) {
        case DeviceType::RESISTOR:          return 'R';
        case DeviceType::CAPACITOR:         return 'C';
        case DeviceType::INDUCTOR:          return 'L';
        case DeviceType::VSRC:              return 'V';
        case DeviceType::ISRC:              return 'I';
        case DeviceType::VCCS:              return 'G';
        case DeviceType::VCVS:              return 'E';
        case DeviceType::CCCS:              return 'F';
        case DeviceType::CCVS:              return 'H';
        case DeviceType::DIODE:             return 'D';
        case DeviceType::NPN:               return 'Q';
        case DeviceType::PNP:               return 'Q';
        case DeviceType::NMOS:              return 'M';
        case DeviceType::PMOS:              return 'M';
        case DeviceType::BEHAVIORAL_SRC:    return 'B';
        case DeviceType::SWITCH_VOLTAGE:    return 'S';
        case DeviceType::SWITCH_CURRENT:    return 'W';
        case DeviceType::TRANSMISSION_LINE: return 'T';
        case DeviceType::SUBCKT:            return 'X';
        default:                            return '?';
    }
}

} // namespace spice

#endif // SPICE_TYPES_HPP
