/**
 * @file sparse.cpp
 * @brief Sparse matrix solver implementation (C++17 version)
 *
 * Implements the SparseMatrix class defined in sparse.hpp.
 * Uses LU decomposition with full pivoting for numerical stability.
 */
#include "sparse.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace spice {

//=============================================================================
// SparseMatrix Implementation
//=============================================================================

SparseMatrix::SparseMatrix(Index size_)
    : size(size_),
      num_elements(0),
      row_head(size_, nullptr),
      col_head(size_, nullptr),
      rhs(size_, 0.0),
      pivot_row(size_, -1),
      pivot_col(size_, -1),
      order(size_),
      factored(false),
      markowitz_count(0),
      fill_factor(0.0)
{
    // Initialize ordering
    for (Index i = 0; i < size; i++)
        order[i] = i;
}

SparseMatrix::~SparseMatrix() {
    // Free all elements (traverse row lists to avoid double-free)
    for (Index i = 0; i < size; i++) {
        SparseElement* elt = row_head[i];
        while (elt != nullptr) {
            SparseElement* next = elt->row_next;
            delete elt;
            elt = next;
        }
    }
}

SparseElement* SparseMatrix::createAndInsertElement(Index row, Index col) {
    auto* elt = new SparseElement(row, col);

    // Insert at head of row list
    elt->row_next = row_head[row];
    row_head[row] = elt;

    // Insert at head of column list
    elt->col_next = col_head[col];
    col_head[col] = elt;

    num_elements++;

    return elt;
}

SparseElement* SparseMatrix::findElementInRow(Index row, Index col) const {
    SparseElement* elt = row_head[row];
    while (elt != nullptr) {
        if (elt->col == col)
            return elt;
        elt = elt->row_next;
    }
    return nullptr;
}

Real& SparseMatrix::getElement(Index row, Index col) {
    if (row < 0 || row >= size || col < 0 || col >= size) {
        throw std::out_of_range("SparseMatrix: element index out of range");
    }

    // Search in row
    SparseElement* elt = findElementInRow(row, col);
    if (elt != nullptr)
        return elt->value;

    // Create new element
    elt = createAndInsertElement(row, col);
    return elt->value;
}

void SparseMatrix::setElement(Index row, Index col, Real value) {
    Real& elt = getElement(row, col);
    elt = value;
}

void SparseMatrix::addElement(Index row, Index col, Real value) {
    Real& elt = getElement(row, col);
    elt += value;
}

Real& SparseMatrix::getRhs(Index row) {
    if (row < 0 || row >= size) {
        throw std::out_of_range("SparseMatrix: RHS index out of range");
    }
    return rhs[row];
}

void SparseMatrix::setRhs(Index row, Real value) {
    if (row >= 0 && row < size)
        rhs[row] = value;
}

void SparseMatrix::addRhs(Index row, Real value) {
    if (row >= 0 && row < size)
        rhs[row] += value;
}

void SparseMatrix::clear() {
    // Clear all element values and factors
    for (Index i = 0; i < size; i++) {
        SparseElement* elt = row_head[i];
        while (elt != nullptr) {
            elt->value = 0.0;
            elt->factor = 0.0;
            elt = elt->row_next;
        }
        rhs[i] = 0.0;
    }
    factored = false;
}

ErrorCode SparseMatrix::factor(Real pivot_tol) {
    Index n = size;

    // Track unassigned rows and columns
    std::vector<Index> unassigned_rows(n);
    std::vector<Index> unassigned_cols(n);
    Index num_unassigned_rows = n;
    Index num_unassigned_cols = n;

    for (Index i = 0; i < n; i++) {
        unassigned_rows[i] = i;
        unassigned_cols[i] = i;
        pivot_row[i] = -1;
        pivot_col[i] = -1;
    }

    // Gaussian elimination with full pivoting
    for (Index k = 0; k < n; k++) {
        Index best_row = -1;
        Index best_col = -1;
        Real max_val = 0.0;

        // Search entire unassigned submatrix for largest element
        for (Index i = 0; i < num_unassigned_rows; i++) {
            Index row = unassigned_rows[i];
            SparseElement* elt = row_head[row];

            while (elt != nullptr) {
                // Check if column is also unassigned
                bool col_ok = false;
                for (Index j = 0; j < num_unassigned_cols; j++) {
                    if (unassigned_cols[j] == elt->col) {
                        col_ok = true;
                        break;
                    }
                }

                if (col_ok && std::fabs(elt->value) > max_val) {
                    max_val = std::fabs(elt->value);
                    best_row = row;
                    best_col = elt->col;
                }
                elt = elt->row_next;
            }
        }

        if (best_row < 0 || max_val < pivot_tol) {
            // Singular matrix or no valid pivot
            std::cerr << "Warning: sparse factorization failed at step " << k
                      << " (max_val=" << max_val << ")" << std::endl;
            return ErrorCode::TROUBLE;
        }

        // Record pivot
        pivot_row[k] = best_row;
        pivot_col[k] = best_col;

        // Remove pivot row from unassigned_rows
        for (Index i = 0; i < num_unassigned_rows; i++) {
            if (unassigned_rows[i] == best_row) {
                unassigned_rows[i] = unassigned_rows[num_unassigned_rows - 1];
                num_unassigned_rows--;
                break;
            }
        }

        // Remove pivot column from unassigned_cols
        for (Index i = 0; i < num_unassigned_cols; i++) {
            if (unassigned_cols[i] == best_col) {
                unassigned_cols[i] = unassigned_cols[num_unassigned_cols - 1];
                num_unassigned_cols--;
                break;
            }
        }

        // Get pivot value
        Real pivot_val = 0.0;
        SparseElement* pivot_elt = row_head[best_row];
        while (pivot_elt != nullptr && pivot_elt->col != best_col)
            pivot_elt = pivot_elt->row_next;

        if (pivot_elt)
            pivot_val = pivot_elt->value;

        if (std::fabs(pivot_val) < 1e-30) {
            std::cerr << "Warning: near-zero pivot at step " << k << std::endl;
        }

        // Eliminate other rows
        for (Index i = 0; i < num_unassigned_rows; i++) {
            Index row = unassigned_rows[i];
            SparseElement* elt = row_head[row];

            while (elt != nullptr) {
                if (elt->col == best_col && elt->value != 0.0) {
                    Real multiplier = elt->value / pivot_val;
                    elt->factor = multiplier;  // Store multiplier for solve
                    elt->value = 0.0;  // Eliminated

                    // Update row
                    SparseElement* row_elt = row_head[row];
                    while (row_elt != nullptr) {
                        if (row_elt->col != best_col) {
                            // Find corresponding element in pivot row
                            SparseElement* p_row_elt = row_head[best_row];
                            while (p_row_elt != nullptr && p_row_elt->col != row_elt->col)
                                p_row_elt = p_row_elt->row_next;

                            if (p_row_elt != nullptr) {
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

    factored = true;
    return ErrorCode::OK;
}

ErrorCode SparseMatrix::solve(const std::vector<Real>& b, std::vector<Real>& x) {
    Index n = size;

    if (!factored)
        return ErrorCode::TROUBLE;

    if (b.size() < static_cast<size_t>(n) || x.size() < static_cast<size_t>(n))
        return ErrorCode::TROUBLE;

    // Apply row permutation to b
    std::vector<Real> pb(n);
    for (Index k = 0; k < n; k++) {
        pb[k] = b[pivot_row[k]];
    }

    // Forward substitution: solve Ly = Pb
    // L is implicit from the elimination (diagonal of L is 1)
    std::vector<Real> y(n);
    for (Index k = 0; k < n; k++) {
        Index row = pivot_row[k];
        y[k] = pb[k];

        // Subtract contributions from previously solved variables
        SparseElement* elt = row_head[row];
        while (elt != nullptr) {
            Index col = elt->col;
            // Find which step this column was pivoted at
            for (Index j = 0; j < k; j++) {
                if (pivot_col[j] == col) {
                    y[k] -= elt->factor * y[j];
                    break;
                }
            }
            elt = elt->row_next;
        }
    }

    // Back substitution: solve Ux_perm = y
    std::vector<Real> x_perm(n);
    for (Index k = n - 1; k >= 0; k--) {
        Index row = pivot_row[k];
        Index col = pivot_col[k];

        // Find pivot element
        Real pivot_val = 0.0;
        SparseElement* elt = row_head[row];
        while (elt != nullptr) {
            if (elt->col == col) {
                pivot_val = elt->value;
                break;
            }
            elt = elt->row_next;
        }

        if (std::fabs(pivot_val) < 1e-30)
            return ErrorCode::TROUBLE;

        x_perm[k] = y[k] / pivot_val;

        // Subtract from earlier equations
        for (Index j = 0; j < k; j++) {
            Index other_row = pivot_row[j];
            SparseElement* other_elt = row_head[other_row];
            while (other_elt != nullptr) {
                if (other_elt->col == col) {
                    y[j] -= other_elt->factor * x_perm[k];
                    break;
                }
                other_elt = other_elt->row_next;
            }
        }
    }

    // Apply column permutation: x = Q * x_perm
    for (Index k = 0; k < n; k++) {
        x[pivot_col[k]] = x_perm[k];
    }

    return ErrorCode::OK;
}

std::vector<Real> SparseMatrix::solve(const std::vector<Real>& b) {
    std::vector<Real> x(size, 0.0);
    if (solve(b, x) != ErrorCode::OK) {
        throw std::runtime_error("SparseMatrix: solve failed");
    }
    return x;
}

void SparseMatrix::printStats(std::ostream& os) const {
    os << "Sparse matrix: " << size << " x " << size
       << ", " << num_elements << " non-zeros" << std::endl;
    os << "Markowitz count: " << markowitz_count
       << ", Fill factor: " << fill_factor << std::endl;
}

void SparseMatrix::print(std::ostream& os) const {
    os << "Sparse matrix (" << size << " x " << size << "):" << std::endl;

    // Print in dense format for debugging
    for (Index i = 0; i < size; i++) {
        os << "  [" << i << "] ";
        SparseElement* elt = row_head[i];
        while (elt != nullptr) {
            os << "(" << elt->col << "=" << elt->value << ") ";
            elt = elt->row_next;
        }
        os << "| rhs=" << rhs[i] << std::endl;
    }
}

} // namespace spice
