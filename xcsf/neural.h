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
 * @file neural.h
 * @author Richard Preen <rpreen@gmail.com>
 * @copyright The Authors.
 * @date 2012--2020.
 * @brief An implementation of a multi-layer perceptron neural network.
 */

#pragma once

#include "neural_layer.h"
#include "xcsf.h"

/**
 * @brief Double linked list of layers data structure.
 */
struct Llist {
    struct Layer *layer; //!< Pointer to the layer data structure
    struct Llist *prev; //!< Pointer to the previous layer (forward)
    struct Llist *next; //!< Pointer to the next layer (backward)
};

/**
 * @brief Neural network data structure.
 */
struct Net {
    int n_layers; //!< Number of layers (hidden + output)
    int n_inputs; //!< Number of network inputs
    int n_outputs; //!< Number of network outputs
    double *output; //!< Pointer to the network output
    struct Llist *head; //!< Pointer to the head layer (output layer)
    struct Llist *tail; //!< Pointer to the tail layer (first layer)
};

_Bool
neural_mutate(const struct XCSF *xcsf, const struct Net *net);

double
neural_output(const struct XCSF *xcsf, const struct Net *net, const int IDX);

double *
neural_outputs(const struct XCSF *xcsf, const struct Net *net);

double
neural_size(const struct XCSF *xcsf, const struct Net *net);

size_t
neural_load(const struct XCSF *xcsf, struct Net *net, FILE *fp);

size_t
neural_save(const struct XCSF *xcsf, const struct Net *net, FILE *fp);

void
neural_copy(const struct XCSF *xcsf, struct Net *dest, const struct Net *src);

void
neural_free(const struct XCSF *xcsf, struct Net *net);

void
neural_init(const struct XCSF *xcsf, struct Net *net);

void
neural_insert(const struct XCSF *xcsf, struct Net *net, struct Layer *l,
              const int pos);

void
neural_remove(const struct XCSF *xcsf, struct Net *net, const int pos);

void
neural_push(const struct XCSF *xcsf, struct Net *net, struct Layer *l);

void
neural_pop(const struct XCSF *xcsf, struct Net *net);

void
neural_learn(const struct XCSF *xcsf, const struct Net *net,
             const double *output, const double *input);

void
neural_print(const struct XCSF *xcsf, const struct Net *net,
             const _Bool print_weights);

void
neural_propagate(const struct XCSF *xcsf, const struct Net *net,
                 const double *input);

void
neural_rand(const struct XCSF *xcsf, const struct Net *net);

void
neural_resize(const struct XCSF *xcsf, const struct Net *net);

static uint32_t
neural_cond_lopt(const struct XCSF *xcsf)
{
    uint32_t lopt = 0;
    if (xcsf->COND_EVOLVE_WEIGHTS) {
        lopt |= LAYER_EVOLVE_WEIGHTS;
    }
    if (xcsf->COND_EVOLVE_NEURONS) {
        lopt |= LAYER_EVOLVE_NEURONS;
    }
    if (xcsf->COND_EVOLVE_FUNCTIONS) {
        lopt |= LAYER_EVOLVE_FUNCTIONS;
    }
    if (xcsf->COND_EVOLVE_CONNECTIVITY) {
        lopt |= LAYER_EVOLVE_CONNECT;
    }
    return lopt;
}

static uint32_t
neural_pred_lopt(const struct XCSF *xcsf)
{
    uint32_t lopt = 0;
    if (xcsf->PRED_EVOLVE_ETA) {
        lopt |= LAYER_EVOLVE_ETA;
    }
    if (xcsf->PRED_SGD_WEIGHTS) {
        lopt |= LAYER_SGD_WEIGHTS;
    }
    if (xcsf->PRED_EVOLVE_WEIGHTS) {
        lopt |= LAYER_EVOLVE_WEIGHTS;
    }
    if (xcsf->PRED_EVOLVE_NEURONS) {
        lopt |= LAYER_EVOLVE_NEURONS;
    }
    if (xcsf->PRED_EVOLVE_FUNCTIONS) {
        lopt |= LAYER_EVOLVE_FUNCTIONS;
    }
    if (xcsf->PRED_EVOLVE_CONNECTIVITY) {
        lopt |= LAYER_EVOLVE_CONNECT;
    }
    return lopt;
}
