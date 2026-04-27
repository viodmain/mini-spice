/*
 * anareg.c - Analysis registration system
 */
#include "analysis.h"
#include <stdio.h>
#include <string.h>

/* External analysis operations */
extern const AnalysisOps *dcop_get_ops(void);
extern const AnalysisOps *dcsweep_get_ops(void);
extern const AnalysisOps *acan_get_ops(void);
extern const AnalysisOps *dctran_get_ops(void);

/* Analysis registry */
static const AnalysisOps *analysis_registry[NUM_ANALYSES];

/* Register an analysis type */
int analysis_register(const AnalysisOps *ops)
{
    if (ops->type >= NUM_ANALYSES)
        return E_TROUBLE;
    
    analysis_registry[ops->type] = ops;
    return OK;
}

/* Get analysis operations by type */
const AnalysisOps *analysis_get_ops(analysis_type_t type)
{
    if (type >= NUM_ANALYSES)
        return NULL;
    return analysis_registry[type];
}

/* Get analysis operations by name */
const AnalysisOps *analysis_get_ops_by_name(const char *name)
{
    for (int i = 0; i < NUM_ANALYSES; i++) {
        if (analysis_registry[i] && strcmp(analysis_registry[i]->name, name) == 0)
            return analysis_registry[i];
    }
    return NULL;
}

/* Initialize all analyses */
int analysis_init_all(void)
{
    analysis_register(dcop_get_ops());
    analysis_register(dcsweep_get_ops());
    analysis_register(acan_get_ops());
    analysis_register(dctran_get_ops());
    
    return OK;
}

/* Run all analyses in the circuit */
int analysis_run_all(Circuit *ckt)
{
    for (Analysis *ana = ckt->analyses; ana != NULL; ana = ana->next) {
        const AnalysisOps *ops = analysis_get_ops(ana->params.type);
        if (ops == NULL) {
            fprintf(stderr, "Warning: no operations for analysis type %d\n", ana->params.type);
            continue;
        }
        
        /* Initialize */
        if (ops->init)
            ops->init(ana, ckt);
        
        /* Run */
        if (ops->run)
            ops->run(ana, ckt);
        
        /* Cleanup */
        if (ops->cleanup)
            ops->cleanup(ana, ckt);
    }
    
    return OK;
}

/* Run a specific analysis */
int analysis_run(Analysis *analysis, Circuit *ckt)
{
    const AnalysisOps *ops = analysis_get_ops(analysis->params.type);
    if (ops == NULL)
        return E_NOTFOUND;
    
    if (ops->init)
        ops->init(analysis, ckt);
    if (ops->run)
        ops->run(analysis, ckt);
    if (ops->cleanup)
        ops->cleanup(analysis, ckt);
    
    return OK;
}
