/**
 * @file main.cpp
 * @brief Main entry point for mini-spice (C++17 version)
 *
 * CLI entry point that:
 * 1. Parses command-line arguments
 * 2. Initializes devices and analyses
 * 3. Parses netlist file
 * 4. Runs simulations
 * 5. Outputs results
 */
#include "spice_types.hpp"
#include "circuit.hpp"
#include "parser.hpp"
#include "device.hpp"
#include "analysis.hpp"
#include "output.hpp"
#include <iostream>
#include <string>
#include <cstring>

using namespace spice;

/**
 * @brief Print usage information
 */
static void printUsage(const char* prog) {
    std::cout << "Usage: " << prog << " [options] <netlist_file>\n"
              << "\nOptions:\n"
              << "  -h, --help          Show this help\n"
              << "  -v, --version       Show version\n"
              << "  -o <file>           Output file\n"
              << "  -T <temp>           Set temperature (Celsius)\n"
              << "\nSupported analyses:\n"
              << "  .op                 DC operating point\n"
              << "  .dc <src> <start> <stop> <step>  DC sweep\n"
              << "  .ac <type> <points> <start> <stop>  AC analysis\n"
              << "  .tran <tstep> <tstop>  Transient analysis\n"
              << std::endl;
}

/**
 * @brief Print version information
 */
static void printVersion() {
    std::cout << "mini-spice v0.2 (C++17) - A minimal SPICE simulator\n"
              << "Based on ngspice-45 architecture\n"
              << "Copyright (c) 2025\n" << std::endl;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    const char* netlistFile = nullptr;
    const char* outputFile = nullptr;
    Real tempCelsius = 27.0;

    // Parse command-line arguments
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        else if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--version") == 0) {
            printVersion();
            return 0;
        }
        else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outputFile = argv[++i];
        }
        else if (std::strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            tempCelsius = std::atof(argv[++i]);
        }
        else if (argv[i][0] == '-') {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
        else {
            netlistFile = argv[i];
        }
    }

    if (netlistFile == nullptr) {
        std::cerr << "Error: no input file specified" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Initialize devices and analyses
    initAllDevices();
    initAllAnalyses();

    // Parse netlist
    std::cout << "Reading netlist: " << netlistFile << std::endl;

    auto ckt = parseFile(netlistFile);
    if (!ckt) {
        std::cerr << "Error: failed to parse netlist" << std::endl;
        std::cerr << getParserError() << std::endl;
        return 1;
    }

    // Set temperature
    ckt->temp_celsius = tempCelsius;
    ckt->temp = tempCelsius + 273.15;

    // Print circuit summary
    Output out;
    out.printCircuit(*ckt);

    // Run analyses
    if (ckt->analyses.empty()) {
        std::cout << "\nNo analyses specified. Adding default .op" << std::endl;
        auto ana = std::make_unique<Analysis>(AnalysisType::DC_OP);
        ckt->analyses.push_back(std::move(ana));
    }

    runAllAnalyses(ckt.get());

    // Write output if requested
    if (outputFile) {
        std::cout << "\nWriting results to: " << outputFile << std::endl;
        writeRaw(outputFile, *ckt);
    }

    // Circuit is automatically freed by unique_ptr
    std::cout << "\nSimulation complete." << std::endl;
    return 0;
}
