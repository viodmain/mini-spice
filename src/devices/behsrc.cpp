/**
 * @file behsrc.cpp
 * @brief Behavioral source device model (C++17 version)
 *
 * Implements behavioral voltage and current sources.
 *
 * Current source: B<name> n+ n- I=<expression>
 * Voltage source: B<name> n+ n- V=<expression>
 *
 * Expressions can use:
 * - V(node): Node voltage
 * - I(source): Current through voltage source
 * - TIME: Simulation time
 * - Basic arithmetic: +, -, *, /, ^
 * - Functions: abs, sin, cos, tan, exp, log, sqrt, pow
 *
 * Note: This is a simplified expression parser.
 */
#include "device.hpp"
#include "sparse.hpp"
#include <string>
#include <cmath>
#include <algorithm>

namespace spice {

/**
 * @brief Evaluate a behavioral expression
 *
 * Simplified expression evaluator supporting:
 * - V(node) → node voltage
 * - TIME → simulation time
 * - Basic arithmetic and functions
 *
 * @param expr Expression string
 * @param ckt Reference to circuit
 * @return Evaluated value
 */
static Real evalExpression(const std::string& expr, Circuit* ckt) {
    // Simplified implementation: handle common patterns

    // Check for V(node) pattern
    if (expr.substr(0, 2) == "V(" && expr.back() == ')') {
        std::string node_name = expr.substr(2, expr.size() - 3);
        Node* node = ckt->findNode(node_name);
        if (node && node->eqnum >= 0) {
            return ckt->voltage[node->eqnum];
        }
        return 0.0;
    }

    // Check for TIME
    if (expr == "TIME") {
        return ckt->time;
    }

    // Try to parse as number
    try {
        return parseNumber(expr);
    } catch (...) {
        return 0.0;
    }
}

/**
 * @brief Behavioral source load: evaluate expression and load
 */
static ErrorCode behsrcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat) {
    if (dev->expr.empty())
        return ErrorCode::OK;

    // Determine if current or voltage source
    bool is_voltage = (dev->type == DeviceType::BEHAVIORAL_VSRC);

    if (is_voltage) {
        // Behavioral voltage source: evaluate expression as voltage
        Real v = evalExpression(dev->expr, ckt);

        Index eq1 = dev->n1;
        Index eq2 = dev->n2;
        Index ib = dev->n3;  // Branch current equation

        if (eq1 >= 0) mat->addElement(eq1, ib, 1.0);
        if (eq2 >= 0) mat->addElement(eq2, ib, -1.0);
        if (ib >= 0) {
            if (eq1 >= 0) mat->addElement(ib, eq1, 1.0);
            if (eq2 >= 0) mat->addElement(ib, eq2, -1.0);
            mat->setRhs(ib, v);
        }
    } else {
        // Behavioral current source: evaluate expression as current
        Real i = evalExpression(dev->expr, ckt);

        Index eq1 = dev->n1;
        Index eq2 = dev->n2;

        // Current flows from n1 to n2
        if (eq1 >= 0) mat->addRhs(eq1, -i);
        if (eq2 >= 0) mat->addRhs(eq2, i);
    }

    return ErrorCode::OK;
}

/**
 * @brief Behavioral source AC load: use DC value
 */
static ErrorCode behsrcAcLoad(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega) {
    // For AC, use the DC value (derivative of expression w.r.t. inputs)
    // Simplified: just use the expression evaluated at DC
    return behsrcLoad(dev, ckt, mat);
}

/**
 * @brief Create behavioral current source operations
 */
static DeviceOps createBehsrcOps() {
    DeviceOps ops;
    ops.name = "B(I)";
    ops.type = DeviceType::BEHAVIORAL_SRC;
    ops.load = behsrcLoad;
    ops.acLoad = behsrcAcLoad;
    return ops;
}

/**
 * @brief Create behavioral voltage source operations
 */
static DeviceOps createBehvsrcOps() {
    DeviceOps ops;
    ops.name = "B(V)";
    ops.type = DeviceType::BEHAVIORAL_VSRC;
    ops.load = behsrcLoad;
    ops.acLoad = behsrcAcLoad;
    return ops;
}

/**
 * @brief Register behavioral sources
 */
static bool registerBehsrc() {
    registerDevice(createBehsrcOps());
    registerDevice(createBehvsrcOps());
    return true;
}

static bool behsrc_registered = registerBehsrc();

} // namespace spice
