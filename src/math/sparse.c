/*
 * sparse.c - Sparse matrix solver
 * Simplified sparse matrix with LU decomposition
 */
#include "sparse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* --- Helper: create a new matrix element --- */
static SparseElement *create_element(int row, int col)
{
    SparseElement *elt = (SparseElement *)malloc(sizeof(SparseElement));
    if (elt == NULL)
        return NULL;
    
    elt->row = row;
    elt->col = col;
    elt->value = 0.0;
    elt->factor = 0.0;
    elt->row_next = NULL;
    elt->col_next = NULL;
    
    return elt;
}

/* --- Create a new sparse matrix --- */
SparseMatrix *sparse_create(int size)
{
    SparseMatrix *mat = (SparseMatrix *)malloc(sizeof(SparseMatrix));
    if (mat == NULL)
        return NULL;
    
    mat->size = size;
    mat->num_elements = 0;
    
    mat->row_head = (SparseElement **)calloc(size, sizeof(SparseElement *));
    mat->col_head = (SparseElement **)calloc(size, sizeof(SparseElement *));
    mat->rhs = (double *)calloc(size, sizeof(double));
    mat->pivot_row = (int *)malloc(size * sizeof(int));
    mat->pivot_col = (int *)malloc(size * sizeof(int));
    mat->order = (int *)malloc(size * sizeof(int));
    
    if (!mat->row_head || !mat->col_head || !mat->rhs || !mat->pivot_row || 
        !mat->pivot_col || !mat->order) {
        sparse_free(mat);
        return NULL;
    }
    
    memset(mat->row_head, 0, size * sizeof(SparseElement *));
    memset(mat->col_head, 0, size * sizeof(SparseElement *));
    memset(mat->rhs, 0, size * sizeof(double));
    memset(mat->pivot_row, -1, size * sizeof(int));
    memset(mat->pivot_col, -1, size * sizeof(int));
    
    for (int i = 0; i < size; i++)
        mat->order[i] = i;
    
    mat->factored = 0;
    mat->markowitz = 0;
    mat->fill_factor = 0.0;
    
    return mat;
}

/* --- Free a sparse matrix --- */
void sparse_free(SparseMatrix *mat)
{
    if (mat == NULL)
        return;
    
    /* Free all elements */
    for (int i = 0; i < mat->size; i++) {
        SparseElement *elt = mat->row_head[i];
        while (elt != NULL) {
            SparseElement *next = elt->row_next;
            free(elt);
            elt = next;
        }
    }
    
    free(mat->row_head);
    free(mat->col_head);
    free(mat->rhs);
    free(mat->pivot_row);
    free(mat->pivot_col);
    free(mat->order);
    free(mat);
}

/* --- Find or create a matrix element --- */
double *sparse_get_element(SparseMatrix *mat, int row, int col)
{
    if (row < 0 || row >= mat->size || col < 0 || col >= mat->size)
        return NULL;
    
    /* Search in row */
    SparseElement *elt = mat->row_head[row];
    while (elt != NULL) {
        if (elt->col == col)
            return &elt->value;
        elt = elt->row_next;
    }
    
    /* Create new element */
    SparseElement *new_elt = create_element(row, col);
    if (new_elt == NULL)
        return NULL;
    
    /* Insert at head of row */
    new_elt->row_next = mat->row_head[row];
    mat->row_head[row] = new_elt;
    
    /* Insert at head of column */
    new_elt->col_next = mat->col_head[col];
    mat->col_head[col] = new_elt;
    
    mat->num_elements++;
    
    return &new_elt->value;
}

/* --- Set a matrix element directly --- */
void sparse_set_element(SparseMatrix *mat, int row, int col, double value)
{
    double *elt = sparse_get_element(mat, row, col);
    if (elt != NULL)
        *elt = value;
}

/* --- Add to a matrix element --- */
void sparse_add_element(SparseMatrix *mat, int row, int col, double value)
{
    double *elt = sparse_get_element(mat, row, col);
    if (elt != NULL)
        *elt += value;
}

/* --- Get/set RHS vector element --- */
double *sparse_get_rhs(SparseMatrix *mat, int row)
{
    if (row < 0 || row >= mat->size)
        return NULL;
    return &mat->rhs[row];
}

void sparse_set_rhs(SparseMatrix *mat, int row, double value)
{
    if (row >= 0 && row < mat->size)
        mat->rhs[row] = value;
}

void sparse_add_rhs(SparseMatrix *mat, int row, double value)
{
    if (row >= 0 && row < mat->size)
        mat->rhs[row] += value;
}

/* --- Clear matrix --- */
void sparse_clear(SparseMatrix *mat)
{
    for (int i = 0; i < mat->size; i++) {
        SparseElement *elt = mat->row_head[i];
        while (elt != NULL) {
            elt->value = 0.0;
            elt->factor = 0.0;
            elt = elt->row_next;
        }
        mat->rhs[i] = 0.0;
    }
    mat->factored = 0;
}

/* --- LU Factorization with Markowitz pivoting --- */

int sparse_factor(SparseMatrix *mat, double pivot_tol)
{
    int n = mat->size;
    int i, k;
    
    /* Count non-zeros per row/column for Markowitz */
    int *row_count = (int *)calloc(n, sizeof(int));
    int *col_count = (int *)calloc(n, sizeof(int));
    int *unassigned_rows = (int *)malloc(n * sizeof(int));
    int *unassigned_cols = (int *)malloc(n * sizeof(int));
    int num_unassigned_rows = n;
    int num_unassigned_cols = n;
    
    if (!row_count || !col_count || !unassigned_rows || !unassigned_cols) {
        free(row_count);
        free(col_count);
        free(unassigned_rows);
        free(unassigned_cols);
        return E_NOMEM;
    }
    
    /* Initialize */
    for (i = 0; i < n; i++) {
        unassigned_rows[i] = i;
        unassigned_cols[i] = i;
        mat->pivot_row[i] = -1;
        mat->pivot_col[i] = -1;
    }
    
    /* Count elements per row/column */
    for (i = 0; i < n; i++) {
        SparseElement *elt = mat->row_head[i];
        while (elt != NULL) {
            if (elt->value != 0.0) {
                row_count[i]++;
                col_count[elt->col]++;
            }
            elt = elt->row_next;
        }
    }
    
    /* Gaussian elimination with full pivoting */
    for (k = 0; k < n; k++) {
        int best_row = -1, best_col = -1;
        double max_val = 0.0;
        
        /* Search entire unassigned submatrix for largest element */
        for (i = 0; i < num_unassigned_rows; i++) {
            int row = unassigned_rows[i];
            SparseElement *elt = mat->row_head[row];
            
            while (elt != NULL) {
                /* Check if column is also unassigned */
                int col_ok = 0;
                for (int j = 0; j < num_unassigned_cols; j++) {
                    if (unassigned_cols[j] == elt->col) {
                        col_ok = 1;
                        break;
                    }
                }
                
                if (col_ok && fabs(elt->value) > max_val) {
                    max_val = fabs(elt->value);
                    best_row = row;
                    best_col = elt->col;
                }
                elt = elt->row_next;
            }
        }
        
        if (best_row < 0 || max_val < pivot_tol) {
            /* Singular matrix or no valid pivot */
            fprintf(stderr, "Warning: sparse factorization failed at step %d (max_val=%g)\n", k, max_val);
            free(row_count);
            free(col_count);
            free(unassigned_rows);
            free(unassigned_cols);
            return E_TROUBLE;
        }
        
        /* Record pivot */
        mat->pivot_row[k] = best_row;
        mat->pivot_col[k] = best_col;
        
        /* Remove pivot row from unassigned_rows */
        for (i = 0; i < num_unassigned_rows; i++) {
            if (unassigned_rows[i] == best_row) {
                unassigned_rows[i] = unassigned_rows[num_unassigned_rows - 1];
                num_unassigned_rows--;
                break;
            }
        }
        
        /* Remove pivot column from unassigned_cols */
        for (i = 0; i < num_unassigned_cols; i++) {
            if (unassigned_cols[i] == best_col) {
                unassigned_cols[i] = unassigned_cols[num_unassigned_cols - 1];
                num_unassigned_cols--;
                break;
            }
        }
        
        /* Get pivot value */
        double pivot_val = 0.0;
        SparseElement *pivot_elt = mat->row_head[best_row];
        while (pivot_elt != NULL && pivot_elt->col != best_col)
            pivot_elt = pivot_elt->row_next;
        
        if (pivot_elt)
            pivot_val = pivot_elt->value;
        
        if (fabs(pivot_val) < 1e-30) {
            fprintf(stderr, "Warning: near-zero pivot at step %d\n", k);
        }
        
        /* Eliminate other rows */
        for (i = 0; i < num_unassigned_rows; i++) {
            int row = unassigned_rows[i];
            SparseElement *elt = mat->row_head[row];
            
            while (elt != NULL) {
                if (elt->col == best_col && elt->value != 0.0) {
                    double multiplier = elt->value / pivot_val;
                    elt->factor = multiplier;  /* Store multiplier for solve */
                    elt->value = 0.0;  /* Eliminated */
                    
                    /* Update row */
                    SparseElement *row_elt = mat->row_head[row];
                    while (row_elt != NULL) {
                        if (row_elt->col != best_col) {
                            /* Find corresponding element in pivot row */
                            SparseElement *p_row_elt = mat->row_head[best_row];
                            while (p_row_elt != NULL && p_row_elt->col != row_elt->col)
                                p_row_elt = p_row_elt->row_next;
                            
                            if (p_row_elt != NULL) {
                                row_elt->value -= multiplier * p_row_elt->value;
                            }
                        }
                        row_elt = row_elt->row_next;
                    }
                    break;
                }
                elt = elt->row_next;
            }
        }
    }
    
    mat->factored = 1;
    
    free(row_count);
    free(col_count);
    free(unassigned_rows);
    free(unassigned_cols);
    
    return OK;
}

/* --- Solve the system Ax = b --- */
int sparse_solve(SparseMatrix *mat, double *x, double *b)
{
    int n = mat->size;
    int k;
    
    if (!mat->factored)
        return E_TROUBLE;
    
    /* Apply row permutation to b */
    double *pb = (double *)malloc(n * sizeof(double));
    if (pb == NULL)
        return E_NOMEM;
    
    for (k = 0; k < n; k++) {
        pb[k] = b[mat->pivot_row[k]];
    }
    
    /* Forward substitution: solve Ly = Pb
     * L is implicit from the elimination (diagonal of L is 1) */
    double *y = (double *)malloc(n * sizeof(double));
    if (y == NULL) {
        free(pb);
        return E_NOMEM;
    }
    
    for (k = 0; k < n; k++) {
        int row = mat->pivot_row[k];
        y[k] = pb[k];
        
        /* Subtract contributions from previously solved variables */
        SparseElement *elt = mat->row_head[row];
        while (elt != NULL) {
            int col = elt->col;
            /* Find which step this column was pivoted at */
            for (int j = 0; j < k; j++) {
                if (mat->pivot_col[j] == col) {
                    y[k] -= elt->factor * y[j];
                    break;
                }
            }
            elt = elt->row_next;
        }
    }
    
    /* Back substitution: solve Ux_perm = y */
    double *x_perm = (double *)malloc(n * sizeof(double));
    if (x_perm == NULL) {
        free(pb);
        free(y);
        return E_NOMEM;
    }
    
    for (k = n - 1; k >= 0; k--) {
        int row = mat->pivot_row[k];
        int col = mat->pivot_col[k];
        
        /* Find pivot element */
        double pivot_val = 0.0;
        SparseElement *elt = mat->row_head[row];
        while (elt != NULL) {
            if (elt->col == col) {
                pivot_val = elt->value;
                break;
            }
            elt = elt->row_next;
        }
        
        if (fabs(pivot_val) < 1e-30) {
            free(pb);
            free(y);
            free(x_perm);
            return E_TROUBLE;
        }
        
        x_perm[k] = y[k] / pivot_val;
        
        /* Subtract from earlier equations */
        for (int j = 0; j < k; j++) {
            int other_row = mat->pivot_row[j];
            SparseElement *other_elt = mat->row_head[other_row];
            while (other_elt != NULL) {
                if (other_elt->col == col) {
                    y[j] -= other_elt->factor * x_perm[k];
                    break;
                }
                other_elt = other_elt->row_next;
            }
        }
    }
    
    /* Apply column permutation: x = Q * x_perm */
    for (k = 0; k < n; k++) {
        x[mat->pivot_col[k]] = x_perm[k];
    }
    
    free(pb);
    free(y);
    free(x_perm);
    
    return OK;
}

/* --- Print matrix statistics --- */
void sparse_print_stats(SparseMatrix *mat)
{
    printf("Sparse matrix: %d x %d, %d non-zeros\n", 
           mat->size, mat->size, mat->num_elements);
    printf("Markowitz count: %d, Fill factor: %.2f\n", 
           mat->markowitz, mat->fill_factor);
}
