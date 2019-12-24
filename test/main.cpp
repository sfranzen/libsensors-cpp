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
#include <sensors/sensors.h>

#include <iostream>

using namespace sensors;

// TODO: write proper tests
int main()
{
    subfeature sub{"/sys/class/hwmon/hwmon0/temp1_input"};
    auto feat = sub.feature();
    auto chip = feat.chip();
    std::cout << chip.prefix() << " " << feat.name() << " " << sub.name() << "\n";
    std::cout << std::boolalpha << sub.readable() << " " << sub.writable() << " " << sub.number() << "\n";
    std:: cout << sub.read() << "\n";

    return 0;
}
