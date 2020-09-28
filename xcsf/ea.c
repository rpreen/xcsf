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
 * @file ea.c
 * @author Richard Preen <rpreen@gmail.com>
 * @copyright The Authors.
 * @date 2015--2020.
 * @brief Evolutionary algorithm functions.
 */

#include "ea.h"
#include "cl.h"
#include "clset.h"
#include "utils.h"

#define EA_SELECT_ROULETTE (0) //!< Roulette wheel parental selection
#define EA_SELECT_TOURNAMENT (1) //!< Tournament parental selection

/**
 * @brief Initialises offspring error and fitness.
 * @param xcsf The XCSF data structure.
 * @param c1p First parent classifier.
 * @param c2p Second parent classifier.
 * @param c1 The first offspring classifier to initialise.
 * @param c2 The second offspring classifier to initialise.
 * @param cmod Whether crossover modified the offspring.
 */
static void
ea_init_offspring(const struct XCSF *xcsf, const struct Cl *c1p,
                  const struct Cl *c2p, struct Cl *c1, struct Cl *c2,
                  const _Bool cmod)
{
    if (cmod) {
        c1->err = xcsf->ERR_REDUC * ((c1p->err + c2p->err) * 0.5);
        c2->err = c1->err;
        c1->fit = c1p->fit / c1p->num;
        c2->fit = c2p->fit / c2p->num;
        c1->fit = xcsf->FIT_REDUC * ((c1->fit + c2->fit) * 0.5);
        c2->fit = c1->fit;
    } else {
        c1->err = xcsf->ERR_REDUC * c1p->err;
        c2->err = xcsf->ERR_REDUC * c2p->err;
        c1->fit = xcsf->FIT_REDUC * (c1p->fit / c1p->num);
        c2->fit = xcsf->FIT_REDUC * (c2p->fit / c2p->num);
    }
}

/**
 * @brief Performs evolutionary algorithm subsumption.
 * @param xcsf The XCSF data structure.
 * @param c The offspring classifier to attempt to subsume.
 * @param c1p First parent classifier.
 * @param c2p Second parent classifier.
 * @param set The set in which the EA is being run.
 */
static void
ea_subsume(struct XCSF *xcsf, struct Cl *c, struct Cl *c1p, struct Cl *c2p,
           const struct Set *set)
{
    // check if either parent subsumes the offspring
    if (cl_subsumer(xcsf, c1p) && cl_general(xcsf, c1p, c)) {
        ++(c1p->num);
        ++(xcsf->pset.num);
        cl_free(xcsf, c);
    } else if (cl_subsumer(xcsf, c2p) && cl_general(xcsf, c2p, c)) {
        ++(c2p->num);
        ++(xcsf->pset.num);
        cl_free(xcsf, c);
    }
    // attempt to find a random subsumer from the set
    else {
        struct Clist *candidates[set->size];
        int choices = 0;
        for (struct Clist *iter = set->list; iter != NULL; iter = iter->next) {
            if (cl_subsumer(xcsf, iter->cl) && cl_general(xcsf, iter->cl, c)) {
                candidates[choices] = iter;
                ++choices;
            }
        }
        if (choices > 0) { // found
            ++(candidates[rand_uniform_int(0, choices)]->cl->num);
            ++(xcsf->pset.num);
            cl_free(xcsf, c);
        }
        // if no subsumers are found the offspring is added to the population
        else {
            clset_add(&xcsf->pset, c);
        }
    }
}

/**
 * @brief Adds offspring to the population.
 * @param xcsf The XCSF data structure.
 * @param set The set in which the EA is being run.
 * @param c1p First parent classifier.
 * @param c2p Second parent classifier.
 * @param c1 The offspring classifier to add.
 * @param cmod Whether crossover modified the offspring.
 * @param mmod Whether mutation modified the offspring.
 */
static void
ea_add(struct XCSF *xcsf, const struct Set *set, struct Cl *c1p, struct Cl *c2p,
       struct Cl *c1, const _Bool cmod, const _Bool mmod)
{
    if (!cmod && !mmod) {
        ++(c1p->num);
        ++(xcsf->pset.num);
        cl_free(xcsf, c1);
    } else if (xcsf->EA_SUBSUMPTION) {
        ea_subsume(xcsf, c1, c1p, c2p, set);
    } else {
        clset_add(&xcsf->pset, c1);
    }
}

/**
 * @brief Selects a classifier from the set via roulete wheel.
 * @param xcsf The XCSF data structure.
 * @param set The set to select from.
 * @param fit_sum The sum of all the fitnesses in the set.
 * @return A pointer to the selected classifier.
 */
static struct Cl *
ea_select_rw(const struct XCSF *xcsf, const struct Set *set,
             const double fit_sum)
{
    (void) xcsf;
    const double p = rand_uniform(0, fit_sum);
    const struct Clist *iter = set->list;
    double sum = iter->cl->fit;
    while (p > sum) {
        iter = iter->next;
        sum += iter->cl->fit;
    }
    return iter->cl;
}

/**
 * @brief Selects a classifier from the set via tournament.
 * @param xcsf The XCSF data structure.
 * @param set The set to select from.
 * @return A pointer to the selected classifier.
 */
static struct Cl *
ea_select_tournament(const struct XCSF *xcsf, const struct Set *set)
{
    struct Cl *winner = NULL;
    while (winner == NULL) {
        const struct Clist *iter = set->list;
        while (iter != NULL) {
            if ((rand_uniform(0, 1) < xcsf->EA_SELECT_SIZE) &&
                (winner == NULL || iter->cl->fit > winner->fit)) {
                winner = iter->cl;
            }
            iter = iter->next;
        }
    }
    return winner;
}

/**
 * @brief Selects two parents.
 * @param xcsf The XCSF data structure.
 * @param set The set in which the EA is being run.
 * @param c1p First parent classifier (set by this function).
 * @param c2p Second parent classifier (set by this function).
 */
static void
ea_select(const struct XCSF *xcsf, const struct Set *set, struct Cl **c1p,
          struct Cl **c2p)
{
    if (xcsf->EA_SELECT_TYPE == EA_SELECT_ROULETTE) {
        const double fit_sum = clset_total_fit(set);
        *c1p = ea_select_rw(xcsf, set, fit_sum);
        *c2p = ea_select_rw(xcsf, set, fit_sum);
    } else {
        *c1p = ea_select_tournament(xcsf, set);
        *c2p = ea_select_tournament(xcsf, set);
    }
}

/**
 * @brief Executes the evolutionary algorithm (EA).
 * @param xcsf The XCSF data structure.
 * @param set The set in which to run the EA.
 */
void
ea(struct XCSF *xcsf, const struct Set *set)
{
    // increase EA time
    ++(xcsf->time);
    // check if the EA should be run
    if (set->size == 0 || xcsf->time - clset_mean_time(set) < xcsf->THETA_EA) {
        return;
    }
    clset_set_times(xcsf, set);
    // select parents
    struct Cl *c1p = NULL;
    struct Cl *c2p = NULL;
    ea_select(xcsf, set, &c1p, &c2p);
    // create offspring
    for (int i = 0; i * 2 < xcsf->LAMBDA; ++i) {
        // create copies of parents
        struct Cl *c1 = malloc(sizeof(struct Cl));
        struct Cl *c2 = malloc(sizeof(struct Cl));
        cl_init(xcsf, c1, c1p->size, c1p->time);
        cl_init(xcsf, c2, c2p->size, c2p->time);
        cl_copy(xcsf, c1, c1p);
        cl_copy(xcsf, c2, c2p);
        // apply evolutionary operators to offspring
        const _Bool cmod = cl_crossover(xcsf, c1, c2);
        const _Bool m1mod = cl_mutate(xcsf, c1);
        const _Bool m2mod = cl_mutate(xcsf, c2);
        // initialise parameters
        ea_init_offspring(xcsf, c1p, c2p, c1, c2, cmod);
        // add to population
        ea_add(xcsf, set, c1p, c2p, c1, cmod, m1mod);
        ea_add(xcsf, set, c2p, c1p, c2, cmod, m2mod);
    }
    clset_pop_enforce_limit(xcsf);
}
