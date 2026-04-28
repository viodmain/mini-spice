/**
 * @file analysis.hpp
 * @brief Analysis interface (C++17 version)
 *
 * This header defines the AnalysisOps interface for implementing analysis types.
 * Each analysis type (DC OP, DC sweep, AC, transient, etc.) implements this
 * interface to provide:
 * - init(): Initialize analysis
 * - run(): Execute analysis
 * - cleanup(): Clean up after analysis
 *
 * Analyses are registered in a central registry and executed in order.
 */
#ifndef ANALYSIS_HPP
#define ANALYSIS_HPP

#include "spice_types.hpp"
#include "circuit.hpp"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace spice {

// Forward declarations
class Analysis;
class Circuit;

//=============================================================================
// Analysis Operations Interface
//=============================================================================

/**
 * @brief Interface for analysis-specific operations
 *
 * Each analysis type implements this interface to provide its execution logic.
 * The interface uses std::function for flexible implementation.
 *
 * Analysis lifecycle:
 * 1. init() - Called once before analysis starts
 * 2. run() - Called to execute the analysis
 * 3. cleanup() - Called after analysis completes (success or failure)
 */
struct AnalysisOps {
    std::string name;                             /**< Analysis type name (e.g., "dc op", "ac") */
    AnalysisType type;                            /**< Analysis type enum */

    /**
     * @brief Initialize analysis
     *
     * Called once before analysis starts. Can print headers, allocate
     * temporary storage, or validate parameters.
     *
     * @param analysis Pointer to analysis instance
     * @param ckt Reference to circuit
     * @return ErrorCode
     */
    std::function<ErrorCode(Analysis* analysis, Circuit* ckt)> init;

    /**
     * @brief Run analysis
     *
     * Executes the analysis algorithm:
     * - DC OP: Newton-Raphson iteration to find operating point
     * - DC sweep: Loop over source values, run DC OP at each step
     * - AC: Linearize around DC OP, solve complex system per frequency
     * - Transient: Time stepping with capacitor/inductor history update
     *
     * @param analysis Pointer to analysis instance
     * @param ckt Reference to circuit
     * @return ErrorCode
     */
    std::function<ErrorCode(Analysis* analysis, Circuit* ckt)> run;

    /**
     * @brief Cleanup after analysis
     *
     * Called after analysis completes (success or failure). Can free
     * temporary storage, close files, or print summaries.
     *
     * @param analysis Pointer to analysis instance
     * @param ckt Reference to circuit
     * @return ErrorCode
     */
    std::function<ErrorCode(Analysis* analysis, Circuit* ckt)> cleanup;

    /**
     * @brief Check if analysis has cleanup
     * @return True if cleanup() is implemented
     */
    bool hasCleanup() const { return cleanup != nullptr; }
};

//=============================================================================
// Analysis Registration
//=============================================================================

/**
 * @brief Analysis registry - manages analysis type registration and lookup
 *
 * Singleton pattern: use AnalysisRegistry::instance() to access.
 */
class AnalysisRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to registry
     */
    static AnalysisRegistry& instance();

    /**
     * @brief Register an analysis type
     * @param ops Analysis operations (copied into registry)
     * @return ErrorCode
     */
    ErrorCode registerAnalysis(AnalysisOps ops);

    /**
     * @brief Get analysis operations by type
     * @param type Analysis type
     * @return Pointer to operations, or nullptr if not found
     */
    const AnalysisOps* getOps(AnalysisType type) const;

    /**
     * @brief Get analysis operations by name
     * @param name Analysis name (e.g., "dc op", "ac")
     * @return Pointer to operations, or nullptr if not found
     */
    const AnalysisOps* getOpsByName(const std::string& name) const;

    /**
     * @brief Initialize all built-in analyses
     *
     * Registers all analysis types: DC OP, DC sweep, AC, transient,
     * noise, Fourier, sensitivity, pole-zero
     *
     * @return ErrorCode
     */
    ErrorCode initAll();

    /**
     * @brief Get number of registered analyses
     * @return Count
     */
    size_t size() const { return analyses_by_type.size(); }

private:
    AnalysisRegistry() = default;

    // Lookup by type
    std::unordered_map<int, AnalysisOps> analyses_by_type;

    // Lookup by name
    std::unordered_map<std::string, AnalysisType> name_to_type;
};

//=============================================================================
// Analysis Execution
//=============================================================================

/**
 * @brief Run all analyses in the circuit
 *
 * Iterates through the circuit's analysis list and executes each one
 * in order. Calls init(), run(), and cleanup() for each analysis.
 *
 * @param ckt Reference to circuit
 * @return ErrorCode (first error encountered, or OK)
 */
ErrorCode runAllAnalyses(Circuit* ckt);

/**
 * @brief Run a specific analysis
 *
 * Looks up the analysis operations by type and executes init(), run(), cleanup().
 *
 * @param analysis Pointer to analysis
 * @param ckt Reference to circuit
 * @return ErrorCode
 */
ErrorCode runAnalysis(Analysis* analysis, Circuit* ckt);

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * @brief Register an analysis type (convenience function)
 * @param ops Analysis operations
 * @return ErrorCode
 */
inline ErrorCode registerAnalysis(AnalysisOps ops) {
    return AnalysisRegistry::instance().registerAnalysis(std::move(ops));
}

/**
 * @brief Get analysis operations by type (convenience function)
 * @param type Analysis type
 * @return Pointer to operations, or nullptr
 */
inline const AnalysisOps* getAnalysisOps(AnalysisType type) {
    return AnalysisRegistry::instance().getOps(type);
}

/**
 * @brief Get analysis operations by name (convenience function)
 * @param name Analysis name
 * @return Pointer to operations, or nullptr
 */
inline const AnalysisOps* getAnalysisOpsByName(const std::string& name) {
    return AnalysisRegistry::instance().getOpsByName(name);
}

/**
 * @brief Initialize all analyses (convenience function)
 * @return ErrorCode
 */
inline ErrorCode initAllAnalyses() {
    return AnalysisRegistry::instance().initAll();
}

} // namespace spice

#endif // ANALYSIS_HPP
