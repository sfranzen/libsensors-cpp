# libsensors-c++
The [lm-sensors](https://github.com/lm-sensors/lm-sensors) package allows linux users to interact with the hardware monitoring chip(s) in their systems and includes libsensors, a C API to these chips. The aim of libsensors-c++ is to provide an easy to use, class-based C++ interface to libsensors.

## Installation
A C++17-conforming compiler, libsensors and its development headers are required.
```
$ git clone git@github.com:sfranzen/libsensors-cpp
$ cd libsensors-cpp
$ cmake -Bbuild
$ cmake --build build
$ sudo cmake --install build
```
Installation includes a CMake configuration file that allows your own CMake project to import this library using `find_package(sensors-c++)`.

## Example
```cpp
#include <sensors-c++/sensors.h>
#include <iostream>
//...
sensors::subfeature temp {"/sys/class/hwmon/hwmon0/temp1_input"};
std::cout << temp.read() << '\n';
```

## Usage
The library provides two public headers, similar to libsensors: [`<sensors-c++/sensors.h>`](include/sensors-c++/sensors.h) and [`<sensors-c++/error.h>`](include/sensors-c++/error.h). Neither includes any libsensors headers and it is not necessary to initialise or interact with libsensors directly yourself. All classes and functions are defined in the namespace `sensors` and named like their counterparts in libsensors.

### Classes
The following classes are provided in the `<sensors-c++/sensors.h>` header:

* `class bus_id` represents the bus a `chip_name` was found on;
* `class chip_name` represents a sensor chip hosting one or more `feature`s;
* `class feature` represents a sensor feature hosting one or more `subfeature`s, e.g. `fan1`, `temp2`;
* `class subfeature` represents a particular input/output on a feature, e.g. `fan1_input`, `temp2_alarm`.

None of these classes have a default constructor, but all support copy and move semantics. There are two ways of constructing objects:

* The function `std::vector<chip_name> sensors::get_detected_chips()` returns a list of all chips found on your system. New instances can be copy/move-constructed from these, and `chip_name` and `feature` objects can be queried for their child `feature`s and `subfeature`s, respectively;
* The `chip_name`, `feature` and `subfeature` classes have constructors that accept `std::string` arguments. These can be used to instantiate a component based on its path in the `/sys/class/hwmon` device directory, such as `/sys/class/hwmon/hwmon0/temp1_input`.

All of libsensors' enumerations and macro constants are mapped onto similar, scoped C++ enums:

* `enum class bus_type;`
* `enum class feature_type;`
* `enum class subfeature_type;`

### Configuration files
This library automatically calls `sensors_init(FILE*)` to allocate the resources for libsensors. You can optionally specify a configuration file to be used in this call using `sensors::load_config(std::string const& path)`. Note that this requires calling `sensors_cleanup()`; referencing any previously constructed sensor objects is undefined behaviour. Not calling this function or specifying an empty string will cause a `nullptr` to be passed instead and the default configuration to be used.

### Exceptions
Error conditions, including those in libsensors library calls, are reported as exceptions. All exceptions thrown by sensors-c++ are defined in `<sensors-c++/error.h>` and derived from `sensors::error`, which is itself a `std::runtime_error`.

### Thread safety
No explicit measures have been taken and `sensors::load_config()` in particular is not thread safe. Creating sensor objects and reading their values should be safe, although this has not been rigorously tested.
