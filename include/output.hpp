/**
 * @file output.hpp
 * @brief Output/results interface (C++17 version)
 *
 * This header defines functions for formatting and printing simulation
 * results to standard output or files.
 *
 * Supported output formats:
 * - Terminal: Human-readable text with formatted tables
 * - Raw: Simple text-based waveform export
 */
#ifndef OUTPUT_HPP
#define OUTPUT_HPP

#include "spice_types.hpp"
#include "circuit.hpp"
#include <string>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cmath>

namespace spice {

//=============================================================================
// Output Formatting
//=============================================================================

/**
 * @brief Output formatter for simulation results
 *
 * Provides methods for printing circuit summary, operating point,
 * DC sweep, AC analysis, and transient analysis results.
 */
class Output {
public:
    /**
     * @brief Construct an Output formatter
     * @param os Output stream (default: std::cout)
     */
    explicit Output(std::ostream& os = std::cout);

    /**
     * @brief Print circuit summary
     *
     * Displays circuit title, temperature, node count, device count,
     * and device list.
     *
     * @param ckt Reference to circuit
     */
    void printCircuit(const Circuit& ckt);

    /**
     * @brief Print operating point results
     *
     * Displays node voltages and voltage source currents.
     *
     * @param ckt Reference to circuit
     */
    void printOp(const Circuit& ckt);

    /**
     * @brief Print DC sweep results
     *
     * Displays source value vs. output voltage/current table.
     *
     * @param ckt Reference to circuit
     * @param srcName Source name being swept
     * @param vData Output voltage data
     * @param numPoints Number of data points
     */
    void printDC(const Circuit& ckt, const std::string& srcName,
                 const std::vector<Real>& vData);

    /**
     * @brief Print AC analysis results
     *
     * Displays frequency, magnitude (dB), and phase (degrees) table.
     *
     * @param ckt Reference to circuit
     * @param nodeName Output node name
     * @param mag Magnitude data
     * @param phase Phase data (degrees)
     */
    void printAC(const Circuit& ckt, const std::string& nodeName,
                 const std::vector<Real>& mag, const std::vector<Real>& phase);

    /**
     * @brief Print transient analysis results
     *
     * Displays time vs. voltage table.
     *
     * @param ckt Reference to circuit
     * @param nodeName Output node name
     * @param vData Voltage data
     */
    void printTransient(const Circuit& ckt, const std::string& nodeName,
                        const std::vector<Real>& vData);

    /**
     * @brief Print Fourier analysis results
     *
     * Displays harmonic number, frequency, magnitude, phase, and
     * normalized magnitude/phase. Also computes THD.
     *
     * @param ckt Reference to circuit
     * @param fundamentalFreq Fundamental frequency
     * @param numHarmonics Number of harmonics
     * @param mag Magnitude data (fundamental + harmonics)
     * @param phase Phase data (degrees)
     */
    void printFourier(const Circuit& ckt, Real fundamentalFreq,
                      int numHarmonics, const std::vector<Real>& mag,
                      const std::vector<Real>& phase);

    /**
     * @brief Print sensitivity analysis results
     *
     * Displays component name, value, sensitivity, and relative sensitivity.
     *
     * @param ckt Reference to circuit
     * @param outputName Output variable name
     * @param compNames Component names
     * @param sensitivities Sensitivity values
     */
    void printSensitivity(const Circuit& ckt, const std::string& outputName,
                          const std::vector<std::string>& compNames,
                          const std::vector<Real>& sensitivities);

    /**
     * @brief Print noise analysis results
     *
     * Displays frequency, input noise density, and output noise density.
     *
     * @param ckt Reference to circuit
     * @param freq Frequency data
     * @param inputNoise Input noise density
     * @param outputNoise Output noise density
     */
    void printNoise(const Circuit& ckt, const std::vector<Real>& freq,
                    const std::vector<Real>& inputNoise,
                    const std::vector<Real>& outputNoise);

private:
    std::ostream& os;   /**< Output stream */

    /**
     * @brief Print a separator line
     */
    void printSeparator();

    /**
     * @brief Format a real number in engineering notation
     * @param value Value to format
     * @return Formatted string
     */
    std::string formatReal(Real value);
};

//=============================================================================
// Raw Output
//=============================================================================

/**
 * @brief Write results to a simple raw text file
 *
 * Format:
 * @code
 * Title: <circuit title>
 * Variables:
 * 0    node1    voltage
 * 1    node2    voltage
 * ...
 * Data:
 * time    v(node1)    v(node2)    ...
 * ...
 * @endcode
 *
 * @param filename Output file path
 * @param ckt Reference to circuit
 * @return ErrorCode
 */
ErrorCode writeRaw(const std::string& filename, const Circuit& ckt);

/**
 * @brief Write transient results to CSV file
 *
 * Format:
 * @code
 * time,v(node1),v(node2),...
 * 0.0,1.0,0.5,...
 * 1e-06,1.0,0.5,...
 * ...
 * @endcode
 *
 * @param filename Output file path
 * @param ckt Reference to circuit
 * @param timeData Time values
 * @param voltageData Voltage data (one row per time point)
 * @return ErrorCode
 */
ErrorCode writeTransientCSV(const std::string& filename, const Circuit& ckt,
                            const std::vector<Real>& timeData,
                            const std::vector<std::vector<Real>>& voltageData);

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * @brief Print circuit summary (convenience function)
 * @param ckt Reference to circuit
 */
void printCircuit(const Circuit& ckt);

/**
 * @brief Print operating point (convenience function)
 * @param ckt Reference to circuit
 */
void printOp(const Circuit& ckt);

} // namespace spice

#endif // OUTPUT_HPP
