/*
 * analysis.h - Analysis interface
 */
#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "spice_types.h"
#include "circuit.h"

/* Forward declarations */
typedef struct AnalysisOps AnalysisOps;

/* Analysis operations */
struct AnalysisOps {
    const char *name;             /* Analysis type name */
    analysis_type_t type;         /* Analysis type enum */
    
    /* Initialize analysis */
    int (*init)(Analysis *analysis, Circuit *ckt);
    
    /* Run analysis */
    int (*run)(Analysis *analysis, Circuit *ckt);
    
    /* Cleanup after analysis */
    int (*cleanup)(Analysis *analysis, Circuit *ckt);
};

/* --- Analysis registration --- */

/* Register an analysis type */
int analysis_register(const AnalysisOps *ops);

/* Get analysis operations by type */
const AnalysisOps *analysis_get_ops(analysis_type_t type);

/* Get analysis operations by name */
const AnalysisOps *analysis_get_ops_by_name(const char *name);

/* Initialize all analyses */
int analysis_init_all(void);

/* --- Analysis execution --- */

/* Run all analyses in the circuit */
int analysis_run_all(Circuit *ckt);

/* Run a specific analysis */
int analysis_run(Analysis *analysis, Circuit *ckt);

#endif /* ANALYSIS_H */
