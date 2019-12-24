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

#ifndef LIBSENSORS_CPP_ERROR_H
#define LIBSENSORS_CPP_ERROR_H

#include <stdexcept>

namespace sensors {

// Base exception type, not used directly
class error : public std::runtime_error
{
public:
    using runtime_error::runtime_error;
    explicit error(int code);
};

class init_error : public error
{
    using error::error;
};

class io_error : public error
{
    using error::error;
};

class parse_error : public error
{
    using error::error;
};

} // sensors

#endif // LIBSENSORS_CPP_ERROR_H
