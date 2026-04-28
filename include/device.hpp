/**
 * @file device.hpp
 * @brief Device model interface (C++17 version)
 *
 * This header defines the DeviceOps interface for implementing device models.
 * Each device type (resistor, capacitor, diode, BJT, MOSFET, etc.) implements
 * this interface to provide:
 * - setup(): Allocate matrix entries
 * - load(): Load DC contributions into MNA matrix
 * - acLoad(): Load AC contributions (complex admittance)
 * - update(): Update device state after solution
 * - nonlinear(): Compute nonlinear contributions for Newton-Raphson
 *
 * Devices are registered in a central registry and looked up by type.
 */
#ifndef DEVICE_HPP
#define DEVICE_HPP

#include "spice_types.hpp"
#include "circuit.hpp"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace spice {

// Forward declaration
class SparseMatrix;
class Device;
class Circuit;

//=============================================================================
// Device Operations Interface
//=============================================================================

/**
 * @brief Interface for device-specific operations
 *
 * Each device type implements this interface to provide its MNA stamp,
 * nonlinear behavior, and AC model.
 *
 * The interface uses std::function for flexible implementation.
 * Device implementations are registered globally and looked up by type.
 */
struct DeviceOps {
    std::string name;                             /**< Device type name (e.g., "R", "D", "Q") */
    DeviceType type;                              /**< Device type enum */

    /**
     * @brief Setup: prepare device for analysis
     *
     * Called once per analysis to allow device to allocate any
     * needed resources or precompute values.
     *
     * @param dev Pointer to device instance
     * @param ckt Reference to circuit
     * @return ErrorCode
     */
    std::function<ErrorCode(Device* dev, Circuit* ckt)> setup;

    /**
     * @brief Load: load DC contributions into MNA matrix
     *
     * For linear devices, this loads the device stamp into the matrix.
     * For nonlinear devices, this is typically a no-op (see nonlinear()).
     *
     * @param dev Pointer to device instance
     * @param ckt Reference to circuit
     * @param mat Reference to sparse matrix
     * @return ErrorCode
     */
    std::function<ErrorCode(Device* dev, Circuit* ckt, SparseMatrix* mat)> load;

    /**
     * @brief AC load: load AC contributions into MNA matrix
     *
     * Called during AC analysis to load the linearized small-signal model.
     *
     * @param dev Pointer to device instance
     * @param ckt Reference to circuit
     * @param mat Reference to sparse matrix
     * @param omega Angular frequency (rad/s)
     * @return ErrorCode
     */
    std::function<ErrorCode(Device* dev, Circuit* ckt, SparseMatrix* mat, Real omega)> acLoad;

    /**
     * @brief Update: update device state after solution
     *
     * Called after each successful solution to update internal state
     * (e.g., capacitor/inductor history for transient analysis).
     *
     * @param dev Pointer to device instance
     * @param ckt Reference to circuit
     * @return ErrorCode
     */
    std::function<ErrorCode(Device* dev, Circuit* ckt)> update;

    /**
     * @brief Nonlinear: compute nonlinear contributions
     *
     * Called during Newton-Raphson iteration for nonlinear devices
     * (diodes, BJTs, MOSFETs). Loads the linearized model (conductance
     * in parallel with current source) into the matrix.
     *
     * @param dev Pointer to device instance
     * @param ckt Reference to circuit
     * @param mat Reference to sparse matrix
     * @return ErrorCode
     */
    std::function<ErrorCode(Device* dev, Circuit* ckt, SparseMatrix* mat)> nonlinear;

    /**
     * @brief Check if device has nonlinear behavior
     * @return True if nonlinear() is implemented
     */
    bool hasNonlinear() const { return nonlinear != nullptr; }

    /**
     * @brief Check if device has AC model
     * @return True if acLoad() is implemented
     */
    bool hasAcLoad() const { return acLoad != nullptr; }
};

//=============================================================================
// Device Registration
//=============================================================================

/**
 * @brief Device registry - manages device type registration and lookup
 *
 * Singleton pattern: use DeviceRegistry::instance() to access.
 */
class DeviceRegistry {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to registry
     */
    static DeviceRegistry& instance();

    /**
     * @brief Register a device type
     * @param ops Device operations (copied into registry)
     * @return ErrorCode
     */
    ErrorCode registerDevice(DeviceOps ops);

    /**
     * @brief Get device operations by type
     * @param type Device type
     * @return Pointer to operations, or nullptr if not found
     */
    const DeviceOps* getOps(DeviceType type) const;

    /**
     * @brief Get device operations by name
     * @param name Device name (e.g., "R", "D")
     * @return Pointer to operations, or nullptr if not found
     */
    const DeviceOps* getOpsByName(const std::string& name) const;

    /**
     * @brief Initialize all built-in devices
     *
     * Registers all device types: R, C, L, V, I, G, E, F, H, D, Q, M, B, S, W, T, X
     *
     * @return ErrorCode
     */
    ErrorCode initAll();

    /**
     * @brief Get number of registered devices
     * @return Count
     */
    size_t size() const { return devices_by_type.size(); }

private:
    DeviceRegistry() = default;

    // Lookup by type
    std::unordered_map<int, DeviceOps> devices_by_type;

    // Lookup by name
    std::unordered_map<std::string, DeviceType> name_to_type;
};

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * @brief Register a device type (convenience function)
 * @param ops Device operations
 * @return ErrorCode
 */
inline ErrorCode registerDevice(DeviceOps ops) {
    return DeviceRegistry::instance().registerDevice(std::move(ops));
}

/**
 * @brief Get device operations by type (convenience function)
 * @param type Device type
 * @return Pointer to operations, or nullptr
 */
inline const DeviceOps* getDeviceOps(DeviceType type) {
    return DeviceRegistry::instance().getOps(type);
}

/**
 * @brief Get device operations by name (convenience function)
 * @param name Device name
 * @return Pointer to operations, or nullptr
 */
inline const DeviceOps* getDeviceOpsByName(const std::string& name) {
    return DeviceRegistry::instance().getOpsByName(name);
}

/**
 * @brief Initialize all devices (convenience function)
 * @return ErrorCode
 */
inline ErrorCode initAllDevices() {
    return DeviceRegistry::instance().initAll();
}

//=============================================================================
// Device Factory Function Type
//=============================================================================

/**
 * @brief Function type for creating device operations
 *
 * Each device module provides a function of this type that returns
 * a fully initialized DeviceOps struct.
 */
using DeviceOpsFactory = std::function<DeviceOps()>;

/**
 * @brief Register a device factory
 * @param factory Factory function
 * @return ErrorCode
 */
inline ErrorCode registerDeviceFactory(DeviceOpsFactory factory) {
    DeviceOps ops = factory();
    return DeviceRegistry::instance().registerDevice(std::move(ops));
}

} // namespace spice

#endif // DEVICE_HPP
