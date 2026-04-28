/**
 * @file output.cpp
 * @brief Output/results handling (C++17 version)
 *
 * Implements the Output class for formatting and printing simulation results.
 */
#include "output.hpp"
#include <fstream>
#include <iomanip>
#include <cmath>

namespace spice {

//=============================================================================
// Output Implementation
//=============================================================================

Output::Output(std::ostream& os_)
    : os(os_) {}

void Output::printCircuit(const Circuit& ckt) {
    os << "\n=== Circuit: " << ckt.title << " ===" << std::endl;
    os << "Temperature: " << std::fixed << std::setprecision(1)
       << ckt.temp_celsius << "°C (" << std::fixed << std::setprecision(2)
       << ckt.temp << " K)" << std::endl;
    os << "Nodes: " << ckt.num_nodes << ", Equations: " << ckt.num_eqns << std::endl;
    os << "Voltage sources: " << ckt.num_vsources << std::endl;

    os << "\nDevices:" << std::endl;
    for (auto& dev : ckt.devices) {
        os << "  " << dev->name << ": type=" << deviceTypeToChar(dev->type)
           << ", n1=" << dev->n1 << ", n2=" << dev->n2;
        if (dev->n3 >= 0)
            os << ", n3=" << dev->n3;
        if (dev->n4 >= 0)
            os << ", n4=" << dev->n4;
        os << ", value=" << dev->value << std::endl;
    }

    os << "\nAnalyses:" << std::endl;
    for (auto& ana : ckt.analyses) {
        os << "  Type: " << static_cast<int>(ana->params.type) << std::endl;
    }
}

void Output::printOp(const Circuit& ckt) {
    os << "\n=== Operating Point ===" << std::endl;

    os << "\nNode Voltages:" << std::endl;
    for (auto& node : ckt.nodes) {
        if (node->is_ground) {
            os << "  " << node->name << ": 0 V" << std::endl;
        } else if (node->eqnum >= 0) {
            os << "  " << node->name << ": " << std::scientific
               << ckt.voltage[node->eqnum] << " V" << std::endl;
        }
    }

    os << "\nVoltage Source Currents:" << std::endl;
    for (auto& dev : ckt.devices) {
        if (dev->type == DeviceType::VSRC) {
            os << "  " << dev->name << ": current through branch" << std::endl;
        }
    }
}

void Output::printDC(const Circuit& ckt, const std::string& srcName,
                     const std::vector<Real>& vData) {
    os << "\n=== DC Sweep Results ===" << std::endl;
    os << "Source: " << srcName << std::endl;
    os << "Points: " << vData.size() << std::endl;
}

void Output::printAC(const Circuit& ckt, const std::string& nodeName,
                     const std::vector<Real>& mag, const std::vector<Real>& phase) {
    os << "\n=== AC Analysis Results ===" << std::endl;
    os << "Node: " << nodeName << std::endl;
    os << "Points: " << mag.size() << std::endl;
}

void Output::printTransient(const Circuit& ckt, const std::string& nodeName,
                            const std::vector<Real>& vData) {
    os << "\n=== Transient Analysis Results ===" << std::endl;
    os << "Node: " << nodeName << std::endl;
    os << "Points: " << vData.size() << std::endl;
}

void Output::printFourier(const Circuit& ckt, Real fundamentalFreq,
                          int numHarmonics, const std::vector<Real>& mag,
                          const std::vector<Real>& phase) {
    os << "\n=== Fourier Analysis Results ===" << std::endl;
    os << "Fundamental frequency: " << fundamentalFreq << " Hz" << std::endl;
    os << "Harmonics: " << numHarmonics << std::endl;

    // Compute THD
    Real fundamental_mag = mag.size() > 1 ? mag[1] : 0.0;
    Real harmonic_power = 0.0;

    for (int h = 2; h <= numHarmonics && h < static_cast<int>(mag.size()); h++) {
        harmonic_power += mag[h] * mag[h];
    }

    Real thd = (fundamental_mag > 0) ?
               std::sqrt(harmonic_power) / fundamental_mag * 100.0 : 0.0;

    os << "THD: " << std::fixed << std::setprecision(2) << thd << "%" << std::endl;
}

void Output::printSensitivity(const Circuit& ckt, const std::string& outputName,
                              const std::vector<std::string>& compNames,
                              const std::vector<Real>& sensitivities) {
    os << "\n=== Sensitivity Analysis Results ===" << std::endl;
    os << "Output: " << outputName << std::endl;
}

void Output::printNoise(const Circuit& ckt, const std::vector<Real>& freq,
                        const std::vector<Real>& inputNoise,
                        const std::vector<Real>& outputNoise) {
    os << "\n=== Noise Analysis Results ===" << std::endl;
    os << "Points: " << freq.size() << std::endl;
}

void Output::printSeparator() {
    os << std::string(40, '-') << std::endl;
}

std::string Output::formatReal(Real value) {
    std::ostringstream ss;
    ss << std::scientific << std::setprecision(6) << value;
    return ss.str();
}

//=============================================================================
// Raw Output
//=============================================================================

ErrorCode writeRaw(const std::string& filename, const Circuit& ckt) {
    std::ofstream file(filename);
    if (!file.is_open())
        return ErrorCode::TROUBLE;

    // Write header
    file << "Title: " << ckt.title << std::endl;
    file << "Variables:" << std::endl;

    int idx = 0;
    for (auto& node : ckt.nodes) {
        if (!node->is_ground) {
            file << idx++ << "\t" << node->name << "\tvoltage" << std::endl;
        }
    }

    file.close();
    return ErrorCode::OK;
}

ErrorCode writeTransientCSV(const std::string& filename, const Circuit& ckt,
                            const std::vector<Real>& timeData,
                            const std::vector<std::vector<Real>>& voltageData) {
    std::ofstream file(filename);
    if (!file.is_open())
        return ErrorCode::TROUBLE;

    // Write header
    file << "time";
    for (auto& node : ckt.nodes) {
        if (!node->is_ground) {
            file << ",v(" << node->name << ")";
        }
    }
    file << std::endl;

    // Write data
    for (size_t i = 0; i < timeData.size(); i++) {
        file << std::scientific << timeData[i];
        if (i < voltageData.size()) {
            for (Real v : voltageData[i]) {
                file << "," << v;
            }
        }
        file << std::endl;
    }

    file.close();
    return ErrorCode::OK;
}

//=============================================================================
// Convenience Functions
//=============================================================================

void printCircuit(const Circuit& ckt) {
    Output out;
    out.printCircuit(ckt);
}

void printOp(const Circuit& ckt) {
    Output out;
    out.printOp(ckt);
}

} // namespace spice
