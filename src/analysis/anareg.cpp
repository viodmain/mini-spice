/**
 * @file anareg.cpp
 * @brief Analysis registration system (C++17 version)
 *
 * Implements the AnalysisRegistry singleton and analysis execution.
 */
#include "analysis.hpp"
#include <iostream>

namespace spice {

//=============================================================================
// AnalysisRegistry Implementation
//=============================================================================

AnalysisRegistry& AnalysisRegistry::instance() {
    static AnalysisRegistry inst;
    return inst;
}

ErrorCode AnalysisRegistry::registerAnalysis(AnalysisOps ops) {
    // Check for duplicate registration
    if (analyses_by_type.count(static_cast<int>(ops.type))) {
        std::cerr << "Warning: analysis type " << static_cast<int>(ops.type)
                  << " already registered, overwriting" << std::endl;
    }

    analyses_by_type[static_cast<int>(ops.type)] = ops;
    name_to_type[ops.name] = ops.type;

    return ErrorCode::OK;
}

const AnalysisOps* AnalysisRegistry::getOps(AnalysisType type) const {
    auto it = analyses_by_type.find(static_cast<int>(type));
    if (it != analyses_by_type.end())
        return &it->second;
    return nullptr;
}

const AnalysisOps* AnalysisRegistry::getOpsByName(const std::string& name) const {
    auto type_it = name_to_type.find(name);
    if (type_it != name_to_type.end()) {
        return getOps(type_it->second);
    }
    return nullptr;
}

ErrorCode AnalysisRegistry::initAll() {
    // All analyses are auto-registered via static initialization
    // in their respective source files (dcop.cpp, dcsweep.cpp, etc.)
    return ErrorCode::OK;
}

//=============================================================================
// Analysis Execution
//=============================================================================

ErrorCode runAllAnalyses(Circuit* ckt) {
    if (!ckt || ckt->analyses.empty()) {
        std::cerr << "No analyses specified" << std::endl;
        return ErrorCode::OK;
    }

    for (auto& analysis : ckt->analyses) {
        const AnalysisOps* ops = getAnalysisOps(analysis->params.type);
        if (!ops) {
            std::cerr << "Warning: no operations for analysis type "
                      << static_cast<int>(analysis->params.type) << std::endl;
            continue;
        }

        // Call init
        if (ops->init) {
            ops->init(analysis.get(), ckt);
        }

        // Call run
        ErrorCode status = ErrorCode::OK;
        if (ops->run) {
            status = ops->run(analysis.get(), ckt);
        }

        // Call cleanup
        if (ops->cleanup) {
            ops->cleanup(analysis.get(), ckt);
        }

        if (status != ErrorCode::OK) {
            std::cerr << "Warning: analysis " << ops->name
                      << " returned error: " << errorToString(status) << std::endl;
        }
    }

    return ErrorCode::OK;
}

ErrorCode runAnalysis(Analysis* analysis, Circuit* ckt) {
    if (!analysis || !ckt)
        return ErrorCode::TROUBLE;

    const AnalysisOps* ops = getAnalysisOps(analysis->params.type);
    if (!ops)
        return ErrorCode::NOT_FOUND;

    // Call init
    if (ops->init) {
        ops->init(analysis, ckt);
    }

    // Call run
    ErrorCode status = ErrorCode::OK;
    if (ops->run) {
        status = ops->run(analysis, ckt);
    }

    // Call cleanup
    if (ops->cleanup) {
        ops->cleanup(analysis, ckt);
    }

    return status;
}

} // namespace spice
