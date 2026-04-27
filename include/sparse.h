/*
 * sparse.h - Sparse matrix solver interface
 * Adapted from ngspice sparse matrix package
 */
#ifndef SPARSE_H
#define SPARSE_H

#include "spice_types.h"

/* Forward declarations */
typedef struct SparseMatrix SparseMatrix;
typedef struct SparseElement SparseElement;

/* Sparse matrix element */
struct SparseElement {
    int row;              /* Row index */
    int col;              /* Column index */
    double value;         /* Matrix value */
    double factor;        /* LU factor value */
    SparseElement *row_next;  /* Next in row */
    SparseElement *col_next;  /* Next in column */
};

/* Sparse matrix structure */
struct SparseMatrix {
    int size;             /* Matrix dimension (n x n) */
    int num_elements;     /* Number of non-zero elements */
    
    /* Row and column headers */
    SparseElement **row_head;
    SparseElement **col_head;
    
    /* RHS vector (separate from matrix) */
    double *rhs;
    
    /* LU factorization state */
    int *pivot_row;       /* Pivot row for each column */
    int *pivot_col;       /* Pivot column for each row */
    int *order;           /* Ordering */
    int factored;         /* 1 if matrix has been factored */
    
    /* Statistics */
    int markowitz;        /* Markowitz count */
    double fill_factor;   /* Fill-in factor */
};

/* --- Matrix operations --- */

/* Create a new sparse matrix */
SparseMatrix *sparse_create(int size);

/* Free a sparse matrix */
void sparse_free(SparseMatrix *mat);

/* Get or create a matrix element (for loading) */
double *sparse_get_element(SparseMatrix *mat, int row, int col);

/* Set a matrix element directly */
void sparse_set_element(SparseMatrix *mat, int row, int col, double value);

/* Add to a matrix element */
void sparse_add_element(SparseMatrix *mat, int row, int col, double value);

/* Get/set RHS vector element */
double *sparse_get_rhs(SparseMatrix *mat, int row);
void sparse_set_rhs(SparseMatrix *mat, int row, double value);
void sparse_add_rhs(SparseMatrix *mat, int row, double value);

/* Clear matrix (set all to zero) */
void sparse_clear(SparseMatrix *mat);

/* --- LU Factorization and Solve --- */

/* Factor the matrix using LU decomposition with Markowitz pivoting */
int sparse_factor(SparseMatrix *mat, double pivot_tol);

/* Solve the system Ax = b (matrix must be factored) */
int sparse_solve(SparseMatrix *mat, double *x, double *b);

/* Print matrix statistics */
void sparse_print_stats(SparseMatrix *mat);

#endif /* SPARSE_H */
