/**
 * @file sparse.hpp
 * @brief Sparse matrix solver interface (C++17 version)
 *
 * This header defines the SparseMatrix class for efficient storage and
 * solution of sparse linear systems Ax = b. It implements LU decomposition
 * with full pivoting, adapted from the ngspice sparse matrix package.
 *
 * Key features:
 * - Row/column linked lists for non-zero elements
 * - Separate RHS vector (not embedded in matrix)
 * - LU factorization with full pivoting for numerical stability
 * - Forward/backward substitution with permutation tracking
 * - RAII for automatic memory management
 */
#ifndef SPARSE_HPP
#define SPARSE_HPP

#include "spice_types.hpp"
#include <vector>
#include <memory>
#include <cassert>
#include <iostream>

namespace spice {

//=============================================================================
// SparseElement Class
//=============================================================================

/**
 * @brief Represents a single non-zero element in the sparse matrix
 *
 * Each element belongs to both a row linked list and a column linked list,
 * enabling efficient traversal in either direction.
 *
 * The `factor` field stores the LU multiplier after factorization.
 */
class SparseElement {
public:
    Index row;                      /**< Row index */
    Index col;                      /**< Column index */
    Real value;                     /**< Matrix value (A_ij) */
    Real factor;                    /**< LU factor value (multiplier) */

    SparseElement* row_next;        /**< Next element in row */
    SparseElement* col_next;        /**< Next element in column */

    /**
     * @brief Construct a new SparseElement
     * @param row_ Row index
     * @param col_ Column index
     */
    SparseElement(Index row_, Index col_);

    /**
     * @brief Get string representation
     * @return String in format "(row,col)=value"
     */
    std::string toString() const;
};

//=============================================================================
// SparseMatrix Class
//=============================================================================

/**
 * @brief Sparse matrix with LU decomposition support
 *
 * Stores only non-zero elements using row and column linked lists.
 * The RHS vector is stored separately.
 *
 * After factorization, the matrix contains L (lower triangular, implicit)
 * and U (upper triangular) factors. Forward/backward substitution solves
 * the system.
 *
 * Usage:
 * @code
 * SparseMatrix mat(size);
 * mat.addElement(0, 0, 1.0);
 * mat.setRhs(0, 5.0);
 * mat.factor(pivot_tol);
 * std::vector<Real> x = mat.solve();
 * @endcode
 */
class SparseMatrix {
public:
    Index size;                     /**< Matrix dimension (n x n) */
    Index num_elements;             /**< Number of non-zero elements */

    /**
     * @brief Construct a new SparseMatrix
     * @param size_ Matrix dimension
     */
    explicit SparseMatrix(Index size_);

    /**
     * @brief Destructor - frees all elements
     */
    ~SparseMatrix();

    // Prevent copying (matrix owns elements)
    SparseMatrix(const SparseMatrix&) = delete;
    SparseMatrix& operator=(const SparseMatrix&) = delete;

    // --- Matrix element access ---

    /**
     * @brief Get or create a matrix element
     *
     * If the element doesn't exist, it's created and added to the
     * row/column linked lists.
     *
     * @param row Row index
     * @param col Column index
     * @return Reference to element value (can be used for reading/writing)
     */
    Real& getElement(Index row, Index col);

    /**
     * @brief Set a matrix element directly
     * @param row Row index
     * @param col Column index
     * @param value Value to set
     */
    void setElement(Index row, Index col, Real value);

    /**
     * @brief Add to a matrix element
     * @param row Row index
     * @param col Column index
     * @param value Value to add
     */
    void addElement(Index row, Index col, Real value);

    // --- RHS vector access ---

    /**
     * @brief Get reference to RHS element
     * @param row Row index
     * @return Reference to RHS value
     */
    Real& getRhs(Index row);

    /**
     * @brief Set RHS element
     * @param row Row index
     * @param value Value to set
     */
    void setRhs(Index row, Real value);

    /**
     * @brief Add to RHS element
     * @param row Row index
     * @param value Value to add
     */
    void addRhs(Index row, Real value);

    // --- Matrix operations ---

    /**
     * @brief Clear matrix (set all values to zero)
     *
     * Resets all element values, factors, and RHS to zero.
     * Does not remove elements from the linked lists.
     */
    void clear();

    // --- LU Factorization and Solve ---

    /**
     * @brief Factor the matrix using LU decomposition
     *
     * Uses full pivoting (searches entire unassigned submatrix for largest
     * element) for numerical stability.
     *
     * After factorization, the matrix is in factored state and solve() can
     * be called.
     *
     * @param pivot_tol Pivoting tolerance (elements smaller than this are
     *                  considered zero)
     * @return ErrorCode
     */
    ErrorCode factor(Real pivot_tol = DEFAULT_PIVTOL);

    /**
     * @brief Solve the system Ax = b
     *
     * Matrix must be factored first via factor().
     *
     * @param b RHS vector (input)
     * @param x Solution vector (output, must be pre-allocated to size)
     * @return ErrorCode
     */
    ErrorCode solve(const std::vector<Real>& b, std::vector<Real>& x);

    /**
     * @brief Solve the system Ax = b (convenience version)
     * @param b RHS vector
     * @return Solution vector
     */
    std::vector<Real> solve(const std::vector<Real>& b);

    /**
     * @brief Check if matrix has been factored
     * @return True if factored
     */
    bool isFactored() const { return factored; }

    // --- Statistics ---

    /**
     * @brief Print matrix statistics
     * @param os Output stream (default: std::cout)
     */
    void printStats(std::ostream& os = std::cout) const;

    /**
     * @brief Print matrix contents (for debugging)
     * @param os Output stream (default: std::cout)
     */
    void print(std::ostream& os = std::cout) const;

private:
    // Row and column headers (arrays of pointers to first element in each row/col)
    std::vector<SparseElement*> row_head;
    std::vector<SparseElement*> col_head;

    // RHS vector (separate from matrix)
    std::vector<Real> rhs;

    // LU factorization state
    std::vector<Index> pivot_row;   /**< Pivot row for each step */
    std::vector<Index> pivot_col;   /**< Pivot column for each step */
    std::vector<Index> order;       /**< Ordering */
    bool factored;                  /**< True if matrix has been factored */

    // Statistics
    int markowitz_count;            /**< Markowitz count */
    Real fill_factor;               /**< Fill-in factor */

    // Helper: create a new element and insert into lists
    SparseElement* createAndInsertElement(Index row, Index col);

    // Helper: find element in row (without creating)
    SparseElement* findElementInRow(Index row, Index col) const;
};

//=============================================================================
// Inline Implementations
//=============================================================================

inline SparseElement::SparseElement(Index row_, Index col_)
    : row(row_), col(col_), value(0.0), factor(0.0),
      row_next(nullptr), col_next(nullptr) {}

inline std::string SparseElement::toString() const {
    return "(" + std::to_string(row) + "," + std::to_string(col) + ")=" +
           std::to_string(value);
}

} // namespace spice

#endif // SPARSE_HPP
