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
 * @file neural_layer_convolutional.c
 * @author Richard Preen <rpreen@gmail.com>
 * @copyright The Authors.
 * @date 2016--2020.
 * @brief An implementation of a 2D convolutional layer.
 */

#include "neural_layer_convolutional.h"
#include "blas.h"
#include "image.h"
#include "neural_activations.h"
#include "sam.h"
#include "utils.h"

#define N_MU (6) //!< Number of mutation rates applied to a convolutional layer

/**
 * @brief Self-adaptation method for mutating a convolutional layer.
 */
static const int MU_TYPE[N_MU] = {
    SAM_RATE_SELECT, //!< Rate of gradient descent mutation
    SAM_RATE_SELECT, //!< Number of filters mutation rate
    SAM_RATE_SELECT, //!< Weight enabling mutation rate
    SAM_RATE_SELECT, //!< Weight disabling mutation rate
    SAM_RATE_SELECT, //!< Weight magnitude mutation
    SAM_RATE_SELECT //!< Activation function mutation rate
};

/**
 * @brief Returns the memory workspace size for a convolutional layer.
 * @param [in] l A convolutional layer.
 * @return The workspace size.
 */
static size_t
get_workspace_size(const struct Layer *l)
{
    const int size = l->out_h * l->out_w * l->size * l->size * l->channels;
    if (size < 1) {
        printf("neural_layer_convolutional: workspace_size overflow\n");
        exit(EXIT_FAILURE);
    }
    return sizeof(double) * size;
}

/**
 * @brief Allocate memory used by a convolutional layer.
 * @param [in] l The layer to be allocated memory.
 */
static void
malloc_layer_arrays(struct Layer *l)
{
    if (l->n_biases < 1 || l->n_biases > N_OUTPUTS_MAX || l->n_outputs < 1 ||
        l->n_outputs > N_OUTPUTS_MAX || l->n_weights < 1 ||
        l->n_weights > N_WEIGHTS_MAX || l->workspace_size < 1) {
        printf("neural_layer_convolutional: malloc() invalid size\n");
        l->n_biases = 1;
        l->n_outputs = 1;
        l->n_weights = 1;
        l->workspace_size = 1;
        exit(EXIT_FAILURE);
    }
    l->delta = calloc(l->n_outputs, sizeof(double));
    l->state = calloc(l->n_outputs, sizeof(double));
    l->output = calloc(l->n_outputs, sizeof(double));
    l->weights = malloc(sizeof(double) * l->n_weights);
    l->biases = malloc(sizeof(double) * l->n_biases);
    l->bias_updates = calloc(l->n_biases, sizeof(double));
    l->weight_updates = calloc(l->n_weights, sizeof(double));
    l->weight_active = malloc(sizeof(bool) * l->n_weights);
    l->temp = malloc(l->workspace_size);
    l->mu = malloc(sizeof(double) * N_MU);
}

/**
 * @brief Returns the output height of a convolutional layer.
 * @param [in] l A convolutional layer.
 * @return The output height.
 */
static int
convolutional_out_height(const struct Layer *l)
{
    return (l->height + 2 * l->pad - l->size) / l->stride + 1;
}

/**
 * @brief Returns the output width of a convolutional layer.
 * @param [in] l A convolutional layer.
 * @return The output width.
 */
static int
convolutional_out_width(const struct Layer *l)
{
    return (l->width + 2 * l->pad - l->size) / l->stride + 1;
}

/**
 * @brief Initialises a 2D convolutional layer.
 * @param [in] l Layer to initialise.
 * @param [in] args Parameters to initialise the layer.
 */
void
neural_layer_convolutional_init(struct Layer *l, const struct ArgsLayer *args)
{
    l->options = layer_opt(args);
    l->function = args->function;
    l->height = args->height;
    l->width = args->width;
    l->channels = args->channels;
    l->n_filters = args->n_init;
    l->max_outputs = args->n_max;
    l->stride = args->stride;
    l->size = args->size;
    l->pad = args->pad;
    l->max_neuron_grow = args->max_neuron_grow;
    l->eta_max = args->eta;
    l->eta_min = args->eta_min;
    l->momentum = args->momentum;
    l->decay = args->decay;
    l->n_biases = l->n_filters;
    l->n_weights = l->channels * l->n_filters * l->size * l->size;
    l->n_active = l->n_weights;
    l->out_h = convolutional_out_height(l);
    l->out_w = convolutional_out_width(l);
    l->out_c = l->n_filters;
    l->n_inputs = l->width * l->height * l->channels;
    l->n_outputs = l->out_h * l->out_w * l->out_c;
    l->workspace_size = get_workspace_size(l);
    layer_init_eta(l);
    malloc_layer_arrays(l);
    for (int i = 0; i < l->n_weights; ++i) {
        l->weights[i] = rand_normal(0, 0.1);
        l->weight_active[i] = true;
    }
    memset(l->biases, 0, sizeof(double) * l->n_biases);
    sam_init(l->mu, N_MU, MU_TYPE);
}

/**
 * @brief Free memory used by a convolutional layer.
 * @param [in] l The layer to be freed.
 */
void
neural_layer_convolutional_free(const struct Layer *l)
{
    free(l->delta);
    free(l->state);
    free(l->output);
    free(l->weights);
    free(l->biases);
    free(l->bias_updates);
    free(l->weight_updates);
    free(l->weight_active);
    free(l->temp);
    free(l->mu);
}

/**
 * @brief Initialises and copies one convolutional layer from another.
 * @param [in] src The source layer.
 * @return A pointer to the new layer.
 */
struct Layer *
neural_layer_convolutional_copy(const struct Layer *src)
{
    if (src->type != CONVOLUTIONAL) {
        printf("neural_layer_convolut_copy() incorrect source layer type\n");
        exit(EXIT_FAILURE);
    }
    struct Layer *l = malloc(sizeof(struct Layer));
    layer_defaults(l);
    l->type = src->type;
    l->layer_vptr = src->layer_vptr;
    l->options = src->options;
    l->function = src->function;
    l->height = src->height;
    l->width = src->width;
    l->channels = src->channels;
    l->n_filters = src->n_filters;
    l->stride = src->stride;
    l->size = src->size;
    l->pad = src->pad;
    l->n_weights = src->n_weights;
    l->n_active = src->n_active;
    l->out_h = src->out_h;
    l->out_w = src->out_w;
    l->out_c = src->out_c;
    l->n_outputs = src->n_outputs;
    l->n_inputs = src->n_inputs;
    l->max_outputs = src->max_outputs;
    l->max_neuron_grow = src->max_neuron_grow;
    l->n_biases = src->n_biases;
    l->eta = src->eta;
    l->eta_max = src->eta_max;
    l->eta_min = src->eta_min;
    l->workspace_size = src->workspace_size;
    malloc_layer_arrays(l);
    memcpy(l->weights, src->weights, sizeof(double) * src->n_weights);
    memcpy(l->weight_active, src->weight_active, sizeof(bool) * src->n_weights);
    memcpy(l->biases, src->biases, sizeof(double) * src->n_biases);
    memcpy(l->mu, src->mu, sizeof(double) * N_MU);
    return l;
}

/**
 * @brief Randomises the weights of a convolutional layer.
 * @param [in] l The layer to randomise.
 */
void
neural_layer_convolutional_rand(struct Layer *l)
{
    layer_weight_rand(l);
}

/**
 * @brief Forward propagates a convolutional layer.
 * @param [in] xcsf The XCSF data structure.
 * @param [in] l The layer to forward propagate.
 * @param [in] input The input to the layer.
 */
void
neural_layer_convolutional_forward(const struct XCSF *xcsf,
                                   const struct Layer *l, const double *input)
{
    (void) xcsf;
    const int m = l->n_filters;
    const int k = l->size * l->size * l->channels;
    const int n = l->out_w * l->out_h;
    const double *a = l->weights;
    double *b = l->temp;
    double *c = l->state;
    memset(l->state, 0, sizeof(double) * l->n_outputs);
    if (l->size == 1) {
        blas_gemm(0, 0, m, n, k, 1, a, k, input, n, 1, c, n);
    } else {
        im2col(input, l->channels, l->height, l->width, l->size, l->stride,
               l->pad, b);
        blas_gemm(0, 0, m, n, k, 1, a, k, b, n, 1, c, n);
    }
    for (int i = 0; i < l->n_biases; ++i) {
        for (int j = 0; j < n; ++j) {
            l->state[i * n + j] += l->biases[i];
        }
    }
    neural_activate_array(l->state, l->output, l->n_outputs, l->function);
}

/**
 * @brief Backward propagates a convolutional layer.
 * @param [in] l The layer to backward propagate.
 * @param [in] input The input to the layer.
 * @param [out] delta The previous layer's error.
 */
void
neural_layer_convolutional_backward(const struct Layer *l, const double *input,
                                    double *delta)
{
    const int m = l->n_filters;
    const int n = l->size * l->size * l->channels;
    const int k = l->out_w * l->out_h;
    if (l->options & LAYER_SGD_WEIGHTS) {
        neural_gradient_array(l->state, l->delta, l->n_outputs, l->function);
        for (int i = 0; i < l->n_biases; ++i) {
            l->bias_updates[i] += blas_sum(l->delta + k * i, k);
        }
        const double *a = l->delta;
        double *b = l->temp;
        double *c = l->weight_updates;
        if (l->size == 1) {
            blas_gemm(0, 1, m, n, k, 1, a, k, input, k, 1, c, n);
        } else {
            im2col(input, l->channels, l->height, l->width, l->size, l->stride,
                   l->pad, b);
            blas_gemm(0, 1, m, n, k, 1, a, k, b, k, 1, c, n);
        }
    }
    if (delta) {
        const double *a = l->weights;
        const double *b = l->delta;
        double *c = l->temp;
        if (l->size == 1) {
            c = delta;
        }
        blas_gemm(1, 0, n, k, m, 1, a, n, b, k, 0, c, k);
        if (l->size != 1) {
            col2im(l->temp, l->channels, l->height, l->width, l->size,
                   l->stride, l->pad, delta);
        }
    }
}

/**
 * @brief Updates the weights and biases of a convolutional layer.
 * @param [in] l The layer to update.
 */
void
neural_layer_convolutional_update(const struct Layer *l)
{
    if (l->options & LAYER_SGD_WEIGHTS) {
        blas_axpy(l->n_biases, l->eta, l->bias_updates, 1, l->biases, 1);
        blas_scal(l->n_biases, l->momentum, l->bias_updates, 1);
        if (l->decay > 0) {
            blas_axpy(l->n_weights, -(l->decay), l->weights, 1,
                      l->weight_updates, 1);
        }
        blas_axpy(l->n_weights, l->eta, l->weight_updates, 1, l->weights, 1);
        blas_scal(l->n_weights, l->momentum, l->weight_updates, 1);
        layer_weight_clamp(l);
    }
}

/**
 * @brief Resizes a convolutional layer if the previous layer has changed size.
 * @param [in] l The layer to resize.
 * @param [in] prev The layer previous to the one being resized.
 */
void
neural_layer_convolutional_resize(struct Layer *l, const struct Layer *prev)
{
    l->width = prev->out_w;
    l->height = prev->out_h;
    l->channels = prev->out_c;
    l->out_w = convolutional_out_width(l);
    l->out_h = convolutional_out_height(l);
    l->n_outputs = l->out_h * l->out_w * l->out_c;
    l->max_outputs = l->n_outputs;
    l->n_inputs = l->width * l->height * l->channels;
    l->state = realloc(l->state, sizeof(double) * l->n_outputs);
    l->output = realloc(l->output, sizeof(double) * l->n_outputs);
    l->delta = realloc(l->delta, sizeof(double) * l->n_outputs);
    l->workspace_size = get_workspace_size(l);
}

/**
 * @brief Returns the number of kernel filters to add or remove from a layer
 * @param [in] l The neural network layer to mutate.
 * @param [in] mu The rate of mutation.
 * @return The number of filters to be added or removed.
 */
static int
neural_layer_convolutional_mutate_filter(const struct Layer *l, const double mu)
{
    int n = 0;
    if (rand_uniform(0, 0.1) < mu) { // 10x higher probability
        while (n == 0) {
            const double m = clamp(rand_normal(0, 0.5), -1, 1);
            n = (int) round(m * l->max_neuron_grow);
        }
        if (l->n_filters + n < 1) {
            n = -(l->n_filters - 1);
        } else if (l->n_filters + n > l->max_outputs) {
            n = l->max_outputs - l->n_filters;
        }
    }
    return n;
}

/**
 * @brief Adds N filters to a layer. Negative N removes filters.
 * @pre N must be appropriately bounds checked for the layer.
 * @param [in] l The neural network layer to mutate.
 * @param [in] N The number of filters to add.
 */
static void
neural_layer_convolutional_add_filters(struct Layer *l, const int N)
{
    const int n_filters = l->n_filters + N;
    const int n_weights = l->channels * n_filters * l->size * l->size;
    const int n_outputs = l->out_h * l->out_w * n_filters;
    l->state = realloc(l->state, sizeof(double) * n_outputs);
    l->output = realloc(l->output, sizeof(double) * n_outputs);
    l->delta = realloc(l->delta, sizeof(double) * n_outputs);
    l->weights = realloc(l->weights, sizeof(double) * n_weights);
    l->weight_active = realloc(l->weight_active, sizeof(bool) * n_weights);
    l->weight_updates = realloc(l->weight_updates, sizeof(double) * n_weights);
    l->biases = realloc(l->biases, sizeof(double) * n_filters);
    l->bias_updates = realloc(l->bias_updates, sizeof(double) * n_filters);
    if (N > 0) {
        for (int i = l->n_weights; i < n_weights; ++i) {
            if (l->options & LAYER_EVOLVE_CONNECT && rand_uniform(0, 1) < 0.5) {
                l->weights[i] = 0;
                l->weight_active[i] = false;
            } else {
                l->weights[i] = rand_normal(0, 0.1);
                l->weight_active[i] = true;
            }
            l->weight_updates[i] = 0;
        }
        for (int i = l->n_filters; i < n_filters; ++i) {
            l->biases[i] = 0;
            l->bias_updates[i] = 0;
            l->output[i] = 0;
            l->state[i] = 0;
            l->delta[i] = 0;
        }
    }
    l->n_weights = n_weights;
    l->n_filters = n_filters;
    l->n_biases = n_filters;
    l->out_c = n_filters;
    l->n_outputs = n_outputs;
    l->workspace_size = get_workspace_size(l);
    l->temp = realloc(l->temp, l->workspace_size);
    layer_calc_n_active(l);
}

/**
 * @brief Mutates a convolutional layer.
 * @param [in] l The layer to mutate.
 * @return Whether any alterations were made.
 */
bool
neural_layer_convolutional_mutate(struct Layer *l)
{
    sam_adapt(l->mu, N_MU, MU_TYPE);
    bool mod = false;
    if ((l->options & LAYER_EVOLVE_ETA) && layer_mutate_eta(l, l->mu[0])) {
        mod = true;
    }
    if (l->options & LAYER_EVOLVE_NEURONS) {
        const int n = neural_layer_convolutional_mutate_filter(l, l->mu[1]);
        if (n != 0) {
            neural_layer_convolutional_add_filters(l, n);
            mod = true;
        }
    }
    if ((l->options & LAYER_EVOLVE_CONNECT) &&
        layer_mutate_connectivity(l, l->mu[2], l->mu[3])) {
        mod = true;
    }
    if ((l->options & LAYER_EVOLVE_WEIGHTS) &&
        layer_mutate_weights(l, l->mu[4])) {
        mod = true;
    }
    if ((l->options & LAYER_EVOLVE_FUNCTIONS) &&
        layer_mutate_functions(l, l->mu[5])) {
        mod = true;
    }
    return mod;
}

/**
 * @brief Returns the output from a convolutional layer.
 * @param [in] l The layer whose output to return.
 * @return The layer output.
 */
double *
neural_layer_convolutional_output(const struct Layer *l)
{
    return l->output;
}

/**
 * @brief Prints a convolutional layer.
 * @param [in] l The layer to print.
 * @param [in] print_weights Whether to print the values of weights and biases.
 */
void
neural_layer_convolutional_print(const struct Layer *l,
                                 const bool print_weights)
{
    printf("convolutional %s, in=%d, out=%d, filters=%d, size=%d, stride=%d, "
           "pad=%d, ",
           neural_activation_string(l->function), l->n_inputs, l->n_outputs,
           l->n_filters, l->size, l->stride, l->pad);
    layer_weight_print(l, print_weights);
    printf("\n");
}

/**
 * @brief Writes a convolutional layer to a file.
 * @param [in] l The layer to save.
 * @param [in] fp Pointer to the file to be written.
 * @return The number of elements written.
 */
size_t
neural_layer_convolutional_save(const struct Layer *l, FILE *fp)
{
    size_t s = 0;
    s += fwrite(&l->options, sizeof(uint32_t), 1, fp);
    s += fwrite(&l->function, sizeof(int), 1, fp);
    s += fwrite(&l->height, sizeof(int), 1, fp);
    s += fwrite(&l->width, sizeof(int), 1, fp);
    s += fwrite(&l->channels, sizeof(int), 1, fp);
    s += fwrite(&l->n_filters, sizeof(int), 1, fp);
    s += fwrite(&l->stride, sizeof(int), 1, fp);
    s += fwrite(&l->size, sizeof(int), 1, fp);
    s += fwrite(&l->pad, sizeof(int), 1, fp);
    s += fwrite(&l->out_h, sizeof(int), 1, fp);
    s += fwrite(&l->out_w, sizeof(int), 1, fp);
    s += fwrite(&l->out_c, sizeof(int), 1, fp);
    s += fwrite(&l->n_biases, sizeof(int), 1, fp);
    s += fwrite(&l->n_outputs, sizeof(int), 1, fp);
    s += fwrite(&l->n_inputs, sizeof(int), 1, fp);
    s += fwrite(&l->max_outputs, sizeof(int), 1, fp);
    s += fwrite(&l->n_weights, sizeof(int), 1, fp);
    s += fwrite(&l->n_active, sizeof(int), 1, fp);
    s += fwrite(&l->workspace_size, sizeof(int), 1, fp);
    s += fwrite(&l->eta, sizeof(double), 1, fp);
    s += fwrite(&l->eta_max, sizeof(double), 1, fp);
    s += fwrite(&l->eta_min, sizeof(double), 1, fp);
    s += fwrite(&l->momentum, sizeof(double), 1, fp);
    s += fwrite(&l->decay, sizeof(double), 1, fp);
    s += fwrite(&l->max_neuron_grow, sizeof(int), 1, fp);
    s += fwrite(l->weights, sizeof(double), l->n_weights, fp);
    s += fwrite(l->weight_updates, sizeof(double), l->n_weights, fp);
    s += fwrite(l->weight_active, sizeof(bool), l->n_weights, fp);
    s += fwrite(l->biases, sizeof(double), l->n_biases, fp);
    s += fwrite(l->bias_updates, sizeof(double), l->n_filters, fp);
    s += fwrite(l->mu, sizeof(double), N_MU, fp);
    return s;
}

/**
 * @brief Reads a convolutional layer from a file.
 * @param [in] l The layer to load.
 * @param [in] fp Pointer to the file to be read.
 * @return The number of elements read.
 */
size_t
neural_layer_convolutional_load(struct Layer *l, FILE *fp)
{
    size_t s = 0;
    s += fread(&l->options, sizeof(uint32_t), 1, fp);
    s += fread(&l->function, sizeof(int), 1, fp);
    s += fread(&l->height, sizeof(int), 1, fp);
    s += fread(&l->width, sizeof(int), 1, fp);
    s += fread(&l->channels, sizeof(int), 1, fp);
    s += fread(&l->n_filters, sizeof(int), 1, fp);
    s += fread(&l->stride, sizeof(int), 1, fp);
    s += fread(&l->size, sizeof(int), 1, fp);
    s += fread(&l->pad, sizeof(int), 1, fp);
    s += fread(&l->out_h, sizeof(int), 1, fp);
    s += fread(&l->out_w, sizeof(int), 1, fp);
    s += fread(&l->out_c, sizeof(int), 1, fp);
    s += fread(&l->n_biases, sizeof(int), 1, fp);
    s += fread(&l->n_outputs, sizeof(int), 1, fp);
    s += fread(&l->n_inputs, sizeof(int), 1, fp);
    s += fread(&l->max_outputs, sizeof(int), 1, fp);
    s += fread(&l->n_weights, sizeof(int), 1, fp);
    s += fread(&l->n_active, sizeof(int), 1, fp);
    s += fread(&l->workspace_size, sizeof(int), 1, fp);
    s += fread(&l->eta, sizeof(double), 1, fp);
    s += fread(&l->eta_max, sizeof(double), 1, fp);
    s += fread(&l->eta_min, sizeof(double), 1, fp);
    s += fread(&l->momentum, sizeof(double), 1, fp);
    s += fread(&l->decay, sizeof(double), 1, fp);
    s += fread(&l->max_neuron_grow, sizeof(int), 1, fp);
    malloc_layer_arrays(l);
    s += fread(l->weights, sizeof(double), l->n_weights, fp);
    s += fread(l->weight_updates, sizeof(double), l->n_weights, fp);
    s += fread(l->weight_active, sizeof(bool), l->n_weights, fp);
    s += fread(l->biases, sizeof(double), l->n_biases, fp);
    s += fread(l->bias_updates, sizeof(double), l->n_biases, fp);
    s += fread(l->mu, sizeof(double), N_MU, fp);
    return s;
}
