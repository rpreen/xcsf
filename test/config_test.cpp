/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file config_test.cpp
 * @author Richard Preen <rpreen@gmail.com>
 * @copyright The Authors.
 * @date 2020.
 * @brief Configuration file reading tests.
 */ 

#include "../lib/doctest/doctest/doctest.h"

extern "C" {   
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include "../xcsf/xcsf.h"
#include "../xcsf/utils.h"
#include "../xcsf/param.h"
#include "../xcsf/config.h"
}

TEST_CASE("XCSF") {
    XCSF xcsf;
    random_init();
    param_init(&xcsf);
    config_read(&xcsf, "../default.ini");
    CHECK_EQ(xcsf.ALPHA, 0.1);
}