/**
 * @file devreg.cpp
 * @brief Device registration system (C++17 version)
 *
 * Implements the DeviceRegistry singleton and initialization of all
 * built-in device types.
 */
#include "device.hpp"
#include <iostream>

namespace spice {

//=============================================================================
// DeviceRegistry Implementation
//=============================================================================

DeviceRegistry& DeviceRegistry::instance() {
    static DeviceRegistry inst;
    return inst;
}

ErrorCode DeviceRegistry::registerDevice(DeviceOps ops) {
    // Check for duplicate registration
    if (devices_by_type.count(static_cast<int>(ops.type))) {
        std::cerr << "Warning: device type " << static_cast<int>(ops.type)
                  << " already registered, overwriting" << std::endl;
    }

    devices_by_type[static_cast<int>(ops.type)] = ops;
    name_to_type[ops.name] = ops.type;

    return ErrorCode::OK;
}

const DeviceOps* DeviceRegistry::getOps(DeviceType type) const {
    auto it = devices_by_type.find(static_cast<int>(type));
    if (it != devices_by_type.end())
        return &it->second;
    return nullptr;
}

const DeviceOps* DeviceRegistry::getOpsByName(const std::string& name) const {
    auto type_it = name_to_type.find(name);
    if (type_it != name_to_type.end()) {
        return getOps(type_it->second);
    }
    return nullptr;
}

ErrorCode DeviceRegistry::initAll() {
    // All devices are auto-registered via static initialization
    // in their respective source files (res.cpp, cap.cpp, etc.)
    // This function is a no-op but kept for API compatibility.
    return ErrorCode::OK;
}

} // namespace spice
