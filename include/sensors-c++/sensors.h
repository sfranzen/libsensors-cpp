/*
 * This file is part of the sensors-c++ library.
 * Copyright (C) 2019  Steven Franzen <sfranzen85@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 */

#ifndef LIBSENSORS_CPP_SENSORS_H
#define LIBSENSORS_CPP_SENSORS_H

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sensors {

class chip_name;
class feature;
class subfeature;

enum class bus_type {
    any,
    i2c,
    isa,
    pci,
    spi,
    virt,
    acpi,
    hid,
    mdio,
    scsi
};

enum class feature_type {
    in,
    fan,
    temp,
    power,
    energy,
    current,
    humidity,
    vid,
    intrusion,
    beep,
    unknown
};

enum class subfeature_type {
    input,
    input_lowest,
    input_highest,
    cap,
    cap_hyst,
    cap_alarm,
    min,
    min_hyst,
    min_alarm,
    max,
    max_hyst,
    max_alarm,
    average,
    lowest,
    highest,
    average_lowest,
    average_highest,
    average_interval,
    crit,
    crit_hyst,
    crit_alarm,
    l_crit,
    l_crit_hyst,
    l_crit_alarm,
    alarm,
    fault,
    emergency,
    emergency_hyst,
    emergency_alarm,
    type,
    offset,
    div,
    beep,
    pulses,
    vid,
    enable,
    unknown
};

template<typename T>
struct _sensors_impl
{
    struct impl;
    std::shared_ptr<impl> m_impl;
    _sensors_impl(impl&&);
    operator impl const*() const;
};

// Holds the bus ID and number of a sensor chip
class bus_id : private _sensors_impl<bus_id>
{
public:
    // A bus_id may only be obtained through its parent chip_name
    bus_id() = delete;

    // String representation of adapter type, e.g. "PCI adapter", or an empty
    // string_view if it could not be found.
    std::string_view adapter_name() const;
    bus_type type() const;
    short nr() const;

private:
    using _sensors_impl::_sensors_impl;
};

// (Re)load a configuration file to use. This function attempts to call the
// sensors_init() function and will throw a sensors::init_error if this file was
// not found or there was an error in sensors_init. Calling this function is
// optional and the argument may be empty; in both cases libsensors will be
// loaded with the default configuration. Referencing any objects created before
// calling this function is undefined behaviour.
void load_config(std::string_view path);

// Return chip_name object for each sensor chip detected on the system, or fail
// with a sensors::init_error if libsensors failed to initialise.
std::vector<chip_name> get_detected_chips();

class chip_name : private _sensors_impl<chip_name>
{
public:
    friend feature;
    friend subfeature;

    // A chip_name can only be constructed from a string parameter or obtained
    // from get_detected_chips()
    chip_name() = delete;

    // Construct a chip_name from its path in the hwmon device class, e.g.
    // /sys/class/hwmon/hwmon0. This constructor throws a sensors::init_error if
    // there was an error loading the libsensors resources, or a
    // sensors::parse_error if no chip was found matching the given path.
    explicit chip_name(std::string_view path);

    // Chip data
    int address() const;
    bus_id bus() const;
    std::string_view prefix() const;
    std::string_view path() const;

    // Chip name as obtained from sensors_snprintf_chip_name. Will throw a
    // sensors::io_error if that function reports an error.
    std::string name() const;

    std::vector<feature> features() const;

private:
    using _sensors_impl::_sensors_impl;
    friend _sensors_impl<feature>;
    friend _sensors_impl<subfeature>;
};

class feature : private _sensors_impl<feature>
{
public:
    // A feature can only be constructed from string parameters or obtained from
    // its parent chip_name
    feature() = delete;

    // Construct a feature from its full filesystem path. This may include the
    // name of a subfeature, e.g. /sys/class/hwmon/hwmon0/temp1[_input]. Throws
    // a sensors::parse_error if no such feature was found.
    explicit feature(std::string_view full_path);

    // Construct a feature from the filesystem path of its chip and its name,
    // e.g. /sys/class/hwmon/hwmon0, temp1. Throws a sensors::parse_error if no
    // such feature was found.
    feature(std::string_view chip_path, std::string_view feature_name);

    // Parent chip
    chip_name const& chip() const;

    // Feature data
    std::string_view name() const;
    int number() const;
    feature_type type() const;

    // Feature label as reported by sensors_get_label; if no label exists, the
    // output is the same as name().
    std::string label() const;

    // Return all subfeatures of this feature
    std::vector<sensors::subfeature> subfeatures() const;

    // Return the subfeature of the given type, if it exists
    std::optional<sensors::subfeature> subfeature(subfeature_type type) const;

private:
    using _sensors_impl::_sensors_impl;
    friend _sensors_impl<class subfeature>;
};

// Represents a subfeature, e.g. input, min, max
class subfeature : private _sensors_impl<subfeature>
{
public:
    // A subfeature can only be constructed from a string parameter or obtained
    // from its parent feature
    subfeature() = delete;

    // Attempt to construct a subfeature from its filesystem path, e.g.
    // /sys/class/hwmon/hwmon0/temp1_input. Throws a sensors::parse_error if no
    // such subfeature was found.
    explicit subfeature(std::string_view path);

    // Parent feature
    sensors::feature const& feature() const;

    // Subfeature data
    std::string_view name() const;
    int number() const;
    subfeature_type type() const;

    // These three functions represent the flags member of sensors_subfeature:
    // SENSORS_MODE_R
    bool readable() const;

    // SENSORS_MODE_W
    bool writable() const;

    // SENSORS_COMPUTE_MAPPING (whether this subfeature is affected by the
    // computation rules of its parent feature)
    bool compute_mapping() const;

    // Return the value read, or throw a sensors::io_error
    double read() const;

    // Write the given value, or throw a sensors::io_error
    void write(double value) const;

private:
    using _sensors_impl::_sensors_impl;
};

} // sensors

#endif // LIBSENSORS_CPP_SENSORS_H
