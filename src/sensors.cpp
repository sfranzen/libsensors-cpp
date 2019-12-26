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

#include "sensors-c++/sensors.h"
#include "sensors-c++/error.h"
#include <sensors/sensors.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>

using namespace sensors;
namespace fs = std::filesystem;

namespace sensors { namespace {

// RAII class for libsensors resources
class libsensors_handle
{
public:
    libsensors_handle(std::string const& config_path = {})
        : m_path{config_path}, m_config{std::fopen(m_path.c_str(), "r")}
    {
        if (!m_path.empty() && !m_config)
            throw init_error{std::string{"Failed to open config file ("} + std::strerror(errno) + ")"};
        auto const error = sensors_init(m_config);
        if (error)
            throw init_error{error};
    }

    ~libsensors_handle()
    {
        sensors_cleanup();
        if (m_config)
            std::fclose(m_config);
    }

    std::string const& config_path() const
    {
        return m_path;
    }

private:
    std::string m_path;
    std::FILE* m_config;
};

auto& get_handle()
{
    auto static handle = std::make_unique<libsensors_handle>();
    return handle;
}

template<typename T>
struct impl_base : public std::reference_wrapper<T const>
{
    using std::reference_wrapper<T const>::reference_wrapper;
    operator T const*() const { return &this->get(); }
};

} // anonymous namespace

// Implementation helper classes
template<typename T>
_sensors_impl<T>::_sensors_impl(impl&& _impl)
    : m_impl{std::make_shared<impl>(std::move(_impl))}
{
}

template<typename T>
_sensors_impl<T>::operator const impl*() const
{
    return m_impl.get();
}

template<>
struct _sensors_impl<bus_id>::impl : public impl_base<sensors_bus_id>
{
    using impl_base::impl_base;
};

template<>
struct _sensors_impl<chip_name>::impl : public impl_base<sensors_chip_name>
{
    using impl_base::impl_base;

    impl static find(std::string path)
    {
        get_handle();
        int nr = 0;
        sensors_chip_name const* name;
        while((name = sensors_get_detected_chips(nullptr, &nr))) {
            if (path.rfind(name->path, 0) == 0)
                return *name;
        }
        throw parse_error{"No chip found at " + path};
    }
};

template<>
struct _sensors_impl<feature>::impl : public impl_base<sensors_feature>
{
    using impl_base::impl_base;

    chip_name m_chip;

    impl(chip_name chip, sensors_feature const& feat) : impl_base{feat}, m_chip{std::move(chip)} {}

    // E.g. /sys/class/hwmon/hwmon0/temp1_input -> chip hwmon0, feature temp1
    impl static find(std::string const& full_path)
    {
        fs::path const path {full_path};
        auto const filename = path.filename().string();
        auto const pos = filename.rfind('_');
        auto const feat_name = pos < std::string::npos ? filename.substr(0, pos) : filename;
        return find(path.parent_path(), feat_name);
    }

    // E.g. /sys/class/hwmon/hwmon0, temp1
    impl static find(std::string const& chip_path, std::string const& feature_name)
    {
        int nr = 0;
        sensors_feature const* feat;
        chip_name chip {chip_path};
        while ((feat = sensors_get_features(*chip, &nr))) {
            if (feat->name == feature_name)
                return {std::move(chip), *feat};
        }
        throw parse_error("Feature " + feature_name + " not found on chip " + chip.prefix());
    }
};

template<>
struct _sensors_impl<subfeature>::impl : public impl_base<sensors_subfeature>
{
    using impl_base::impl_base;

    ::feature m_feature;

    impl(::feature feat, sensors_subfeature const& subfeat) : impl_base{subfeat}, m_feature{std::move(feat)} {}

    impl static find(std::string const& full_path)
    {
        fs::path path {full_path};
        if (!path.has_filename())
            throw parse_error{"Path does not contain filename: " + path.string()};

        auto const sub_name = path.filename().string();
        int nr = 0;
        sensors_subfeature const* sub;
        ::feature feat {path};
        while ((sub = sensors_get_all_subfeatures(*feat.chip(), *feat, &nr)))
            if (sub->name == sub_name)
                return {std::move(feat), *sub};

        throw parse_error{"Subfeature not found: " + sub_name};
    }
};

//
// free functions
//
void load_config(std::string const& path)
{
    auto &handle = get_handle();
    if (path != handle->config_path()) {
        handle.reset();
        handle = std::make_unique<libsensors_handle>(path);
    }
}

std::vector<chip_name> get_detected_chips()
{
    get_handle();
    int nr = 0;
    sensors_chip_name const* cn;
    std::vector<chip_name> chips;
    while ((cn = sensors_get_detected_chips(0, &nr)))
        chips.emplace_back(chip_name{*cn});
    return chips;
}

//
// sensors::bus_id
//
std::string bus_id::adapter_name() const
{
    return sensors_get_adapter_name(**this);
}

bus_type bus_id::type() const
{
    switch (m_impl->get().type) {
    case SENSORS_BUS_TYPE_I2C: return bus_type::i2c;
    case SENSORS_BUS_TYPE_ISA: return bus_type::isa;
    case SENSORS_BUS_TYPE_PCI: return bus_type::pci;
    case SENSORS_BUS_TYPE_SPI: return bus_type::spi;
    case SENSORS_BUS_TYPE_VIRTUAL: return bus_type::virt;
    case SENSORS_BUS_TYPE_ACPI: return bus_type::acpi;
    case SENSORS_BUS_TYPE_HID: return bus_type::hid;
    case SENSORS_BUS_TYPE_MDIO: return bus_type::mdio;
    case SENSORS_BUS_TYPE_SCSI: return bus_type::scsi;
    default: return bus_type::any;
    }
}

short bus_id::nr() const
{
    return m_impl->get().nr;
}

//
// sensors::chip_name
//
chip_name::chip_name(std::string const& path)
    : chip_name{impl::find(path)}
{
}

int chip_name::address() const
{
    return m_impl->get().addr;
}

bus_id chip_name::bus() const
{
    return {m_impl->get().bus};
}

std::string chip_name::prefix() const
{
    return m_impl->get().prefix;
}

std::string chip_name::path() const
{
    return m_impl->get().path;
}

std::string chip_name::name() const
{
    auto const size = sensors_snprintf_chip_name(nullptr, 0, **this);
    if (size < 0)
        throw io_error{std::strerror(size)};
    std::string name(size, '\0');
    auto const written = sensors_snprintf_chip_name(name.data(), size, **this);
    if (written < 0)
        throw io_error{std::strerror(written)};
    return name;
}

std::vector<feature> chip_name::features() const
{
    int nr = 0;
    sensors_feature const* feat;
    std::vector<feature> features;
    while ((feat = sensors_get_features(**this, &nr)))
        features.emplace_back(feature{{*this, *feat}});
    return features;
}

//
// sensors::feature
//
feature::feature(std::string const& full_path)
    : feature{impl::find(full_path)}
{}

feature::feature(std::string const& chip_path, std::string const& feature_name)
    : feature{impl::find(chip_path, feature_name)}
{
}

chip_name const& feature::chip() const
{
    return m_impl->m_chip;
}

std::string feature::name() const
{
    return m_impl->get().name;
}

int feature::number() const
{
    return m_impl->get().number;
}


feature_type feature::type() const
{
    switch (m_impl->get().type) {
    case SENSORS_FEATURE_IN: return feature_type::in;
    case SENSORS_FEATURE_FAN: return feature_type::fan;
    case SENSORS_FEATURE_TEMP: return feature_type::temp;
    case SENSORS_FEATURE_POWER: return feature_type::power;
    case SENSORS_FEATURE_ENERGY: return feature_type::energy;
    case SENSORS_FEATURE_CURR: return feature_type::current;
    case SENSORS_FEATURE_HUMIDITY: return feature_type::humidity;
    case SENSORS_FEATURE_VID: return feature_type::vid;
    case SENSORS_FEATURE_INTRUSION: return feature_type::intrusion;
    case SENSORS_FEATURE_BEEP_ENABLE: return feature_type::beep;
    default: return feature_type::unknown;
    }
}

std::string feature::label() const
{
    auto const c_ptr = sensors_get_label(*chip(), **this);
    if (!c_ptr)
        throw io_error{"Failed to obtain feature label"};
    std::string label {c_ptr};
    std::free(c_ptr);
    return label;
}

std::optional<subfeature> feature::subfeature(subfeature_type type) const
{
    auto subs = subfeatures();
    auto const it = std::find_if(subs.cbegin(), subs.cend(), [=](auto const& sf){ return sf.type() == type; });
    if (it != subs.end())
        return *it;
    return {};
}

std::vector<subfeature> feature::subfeatures() const
{
    int nr = 0;
    sensors_subfeature const* sub;
    std::vector<::subfeature> subfeatures;
    while ((sub = sensors_get_all_subfeatures(*chip(), **this, &nr)))
        subfeatures.emplace_back(::subfeature{{*this, *sub}});
    return subfeatures;
}

//
// sensors::subfeature
//
subfeature::subfeature(std::string const& path)
    : subfeature{impl::find(path)}
{
}

feature const& subfeature::feature() const
{
    return m_impl->m_feature;
}

std::string subfeature::name() const
{
    return m_impl->get().name;
}

int subfeature::number() const
{
    return m_impl->get().number;
}

subfeature_type subfeature::type() const
{
    switch (m_impl->get().type) {
        default:
            return subfeature_type::unknown;

        case SENSORS_SUBFEATURE_IN_INPUT:
        case SENSORS_SUBFEATURE_FAN_INPUT:
        case SENSORS_SUBFEATURE_TEMP_INPUT:
        case SENSORS_SUBFEATURE_POWER_INPUT:
        case SENSORS_SUBFEATURE_ENERGY_INPUT:
        case SENSORS_SUBFEATURE_CURR_INPUT:
        case SENSORS_SUBFEATURE_HUMIDITY_INPUT:
            return subfeature_type::input;

        case SENSORS_SUBFEATURE_POWER_INPUT_LOWEST:
            return subfeature_type::input_lowest;

        case SENSORS_SUBFEATURE_POWER_INPUT_HIGHEST:
            return subfeature_type::input_highest;

        case SENSORS_SUBFEATURE_POWER_CAP:
            return subfeature_type::cap;

        case SENSORS_SUBFEATURE_POWER_CAP_ALARM:
            return subfeature_type::cap_alarm;

        case SENSORS_SUBFEATURE_POWER_CAP_HYST:
            return subfeature_type::cap_hyst;

        case SENSORS_SUBFEATURE_IN_MIN:
        case SENSORS_SUBFEATURE_FAN_MIN:
        case SENSORS_SUBFEATURE_TEMP_MIN:
        case SENSORS_SUBFEATURE_POWER_MIN:
        case SENSORS_SUBFEATURE_CURR_MIN:
            return subfeature_type::min;

        case SENSORS_SUBFEATURE_IN_MIN_ALARM:
        case SENSORS_SUBFEATURE_FAN_MIN_ALARM:
        case SENSORS_SUBFEATURE_TEMP_MIN_ALARM:
        case SENSORS_SUBFEATURE_POWER_MIN_ALARM:
        case SENSORS_SUBFEATURE_CURR_MIN_ALARM:
            return subfeature_type::min_alarm;

        case SENSORS_SUBFEATURE_TEMP_MIN_HYST:
            return subfeature_type::min_hyst;

        case SENSORS_SUBFEATURE_IN_MAX:
        case SENSORS_SUBFEATURE_FAN_MAX:
        case SENSORS_SUBFEATURE_TEMP_MAX:
        case SENSORS_SUBFEATURE_POWER_MAX:
        case SENSORS_SUBFEATURE_CURR_MAX:
            return subfeature_type::max;

        case SENSORS_SUBFEATURE_IN_MAX_ALARM:
        case SENSORS_SUBFEATURE_FAN_MAX_ALARM:
        case SENSORS_SUBFEATURE_TEMP_MAX_ALARM:
        case SENSORS_SUBFEATURE_POWER_MAX_ALARM:
        case SENSORS_SUBFEATURE_CURR_MAX_ALARM:
            return subfeature_type::max_alarm;

        case SENSORS_SUBFEATURE_TEMP_MAX_HYST:
            return subfeature_type::max_hyst;

        case SENSORS_SUBFEATURE_IN_LOWEST:
        case SENSORS_SUBFEATURE_TEMP_LOWEST:
        case SENSORS_SUBFEATURE_CURR_LOWEST:
            return subfeature_type::lowest;

        case SENSORS_SUBFEATURE_IN_HIGHEST:
        case SENSORS_SUBFEATURE_TEMP_HIGHEST:
        case SENSORS_SUBFEATURE_CURR_HIGHEST:
            return subfeature_type::highest;

        case SENSORS_SUBFEATURE_IN_AVERAGE:
        case SENSORS_SUBFEATURE_POWER_AVERAGE:
        case SENSORS_SUBFEATURE_CURR_AVERAGE:
            return subfeature_type::average;

        case SENSORS_SUBFEATURE_POWER_AVERAGE_LOWEST:
            return subfeature_type::average_lowest;

        case SENSORS_SUBFEATURE_POWER_AVERAGE_HIGHEST:
            return subfeature_type::average_highest;

        case SENSORS_SUBFEATURE_POWER_AVERAGE_INTERVAL:
            return subfeature_type::average_interval;

        case SENSORS_SUBFEATURE_IN_LCRIT:
        case SENSORS_SUBFEATURE_TEMP_LCRIT:
        case SENSORS_SUBFEATURE_POWER_LCRIT:
        case SENSORS_SUBFEATURE_CURR_LCRIT:
            return subfeature_type::l_crit;

        case SENSORS_SUBFEATURE_IN_LCRIT_ALARM:
        case SENSORS_SUBFEATURE_TEMP_LCRIT_ALARM:
        case SENSORS_SUBFEATURE_POWER_LCRIT_ALARM:
        case SENSORS_SUBFEATURE_CURR_LCRIT_ALARM:
            return subfeature_type::l_crit_alarm;

        case SENSORS_SUBFEATURE_TEMP_LCRIT_HYST:
            return subfeature_type::l_crit_hyst;

        case SENSORS_SUBFEATURE_IN_CRIT:
        case SENSORS_SUBFEATURE_TEMP_CRIT:
        case SENSORS_SUBFEATURE_POWER_CRIT:
        case SENSORS_SUBFEATURE_CURR_CRIT:
            return subfeature_type::crit;

        case SENSORS_SUBFEATURE_IN_CRIT_ALARM:
        case SENSORS_SUBFEATURE_TEMP_CRIT_ALARM:
        case SENSORS_SUBFEATURE_POWER_CRIT_ALARM:
        case SENSORS_SUBFEATURE_CURR_CRIT_ALARM:
            return subfeature_type::crit_alarm;

        case SENSORS_SUBFEATURE_TEMP_CRIT_HYST:
            return subfeature_type::crit_hyst;

        case SENSORS_SUBFEATURE_IN_BEEP:
        case SENSORS_SUBFEATURE_FAN_BEEP:
        case SENSORS_SUBFEATURE_TEMP_BEEP:
        case SENSORS_SUBFEATURE_CURR_BEEP:
        case SENSORS_SUBFEATURE_INTRUSION_BEEP:
            return subfeature_type::beep;

        case SENSORS_SUBFEATURE_FAN_DIV:
            return subfeature_type::div;

        case SENSORS_SUBFEATURE_FAN_PULSES:
            return subfeature_type::pulses;

        case SENSORS_SUBFEATURE_BEEP_ENABLE:
            return subfeature_type::enable;

        case SENSORS_SUBFEATURE_TEMP_TYPE:
            return subfeature_type::type;

        case SENSORS_SUBFEATURE_TEMP_OFFSET:
            return subfeature_type::offset;

        case SENSORS_SUBFEATURE_VID:
            return subfeature_type::vid;

        case SENSORS_SUBFEATURE_IN_ALARM:
        case SENSORS_SUBFEATURE_FAN_ALARM:
        case SENSORS_SUBFEATURE_TEMP_ALARM:
        case SENSORS_SUBFEATURE_POWER_ALARM:
        case SENSORS_SUBFEATURE_CURR_ALARM:
        case SENSORS_SUBFEATURE_INTRUSION_ALARM:
            return subfeature_type::alarm;

        case SENSORS_SUBFEATURE_FAN_FAULT:
        case SENSORS_SUBFEATURE_TEMP_FAULT:
            return subfeature_type::fault;

        case SENSORS_SUBFEATURE_TEMP_EMERGENCY:
            return subfeature_type::emergency;

        case SENSORS_SUBFEATURE_TEMP_EMERGENCY_ALARM:
            return subfeature_type::emergency_alarm;

        case SENSORS_SUBFEATURE_TEMP_EMERGENCY_HYST:
            return subfeature_type::emergency_hyst;
    }
}

bool subfeature::readable() const
{
    return m_impl->get().flags & SENSORS_MODE_R;
}

bool subfeature::writable() const
{
    return m_impl->get().flags & SENSORS_MODE_W;
}

bool subfeature::compute_mapping() const
{
    return m_impl->get().flags & SENSORS_COMPUTE_MAPPING;
}

double subfeature::read() const
{
    double val;
    auto const error = sensors_get_value(*feature().chip(), number(), &val);
    if (error)
        throw io_error(error);
    return val;
}

void subfeature::write(double value) const
{
    auto const error = sensors_set_value(*feature().chip(), number(), value);
    if (error)
        throw io_error(error);
}

} // sensors
