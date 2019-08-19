 /*
 * Copyright (C) 2019 Richard Preen <rpreen@gmail.com>
 *
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
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h> 
#include <errno.h>
#include "data_structures.h"
#include "loss.h"

double loss_mse(XCSF *xcsf, double *pred, double *y)
{
    double error = 0.0;
    for(int i = 0; i < xcsf->num_y_vars; i++) {
        error += (y[i] - pred[i]) * (y[i] - pred[i]);
    }
    error /= (double)xcsf->num_y_vars;
    return error;
}

double loss_rmse(XCSF *xcsf, double *pred, double *y)
{
    double error = loss_mse(xcsf, pred, y);
    return sqrt(error);
}

void loss_set_func(XCSF *xcsf)
{
    switch(xcsf->LOSS_FUNC) {
        case 0:
            xcsf->loss_ptr = &loss_mse;
            break;
        case 1:
            xcsf->loss_ptr = &loss_rmse;
            break;
        default:
            printf("invalid loss function: %d\n", xcsf->LOSS_FUNC);
            exit(EXIT_FAILURE);
    }
}