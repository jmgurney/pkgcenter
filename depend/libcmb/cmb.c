/*-
 * Copyright (c) 2002-2018 Devin Teske <dteske@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifdef __FBSDID
__FBSDID("$FrauBSD: depend/libcmb/cmb.c 2018-03-27 17:00:01 -0700 freebsdfrau $");
__FBSDID("$FreeBSD$");
#endif

#include <err.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmb.h"

/*
 * Takes pointer to `struct cmb_config' options and number of items. Returns
 * total number of combinations according to config options.
 */
uint64_t
cmb_count(struct cmb_config *config, uint64_t nitems)
{
	int8_t nextset = 1;
	uint64_t curset;
	uint64_t setinit = 1;
	uint64_t setdone = nitems;
	uint64_t count = 0;
	uint64_t d, z;
	uint64_t ncombos;

	if (nitems == 0) return (0);
	if (config != NULL) {
		if (config->range_min != 0 || config->range_max != 0) {
			setinit = config->range_min;
			setdone = config->range_max;
		}
	}

	/* Adjust values to be non-zero (mathematical constraint) */
	if (setinit == 0) setinit = 1;
	if (setdone == 0) setdone = 1;

	/* Enforce limits so we don't run over bounds */
	if (setinit > nitems) setinit = nitems;
	if (setdone > nitems) setdone = nitems;

	/* Set the direction of flow (incrementing vs. decrementing) */
	if (setinit > setdone) nextset = -1;

	/*
	 * Loop over each `set' in the configured direction until we are done
	 */
	for (curset = setinit;
	    nextset > 0 ? curset <= setdone : curset >= setdone;
	    curset += nextset) {
		uint64_t n;
		uint64_t nsubsets;

		/*
		 * Calculate number of subsets, based on the number of items in
		 * the current set we're working on.
		 */
		nsubsets = nitems - curset + 1;

		/*
		 * Calculate number of combinations based on number of subsets
		 */
		z = d = 1;
		for (n = 0; n < curset; n++) {
			z *= nsubsets + n;
			d *= n + 1;
		}
		ncombos = z / d;

		/*
		 * Add the number of combinations in this set to the total
		 */
		count += ncombos;
	}

	return (count);
}

/*
 * Takes pointer to `struct cmb_config' options, number of items, and array of
 * `char *' items. Calculates combinations according to options and either
 * prints combinations to stdout (default) or runs `action' if passed-in as
 * function pointer member of `config' argument.
 */
int
cmb(struct cmb_config *config, uint64_t nitems, char *items[])
{
	uint8_t docount = FALSE;
	uint8_t doseek = FALSE;
	int8_t nextset = 1;
	int retval = 0;
	uint64_t count = 0;
	uint64_t curset;
	uint64_t seek = 0;
	uint64_t setdone = nitems;
	uint64_t setinit = 1;
	uint64_t setmax;
	char **curitems;
	uint64_t *setnums;
	uint64_t *setnums_backend;
	int (*action)(uint64_t nitems, char *items[]) = cmb_print;

	if (nitems == 0) return (0);
	if (config != NULL) {
		cmb_print_nul = config->nul_terminate;
		if (config->action != NULL) action = config->action;
		if (config->count != 0) {
			docount = TRUE;
			count = config->count;
		}
		if (config->delimiter != NULL)
			cmb_print_delimiter = config->delimiter;
		if (config->prefix != NULL) cmb_print_prefix = config->prefix;
		if (config->range_min != 0 || config->range_max != 0) {
			setinit = config->range_min;
			setdone = config->range_max;
		}
		if (config->start > 1) {
			doseek = TRUE;
			seek = config->start;
		}
		if (config->suffix != NULL) cmb_print_suffix = config->suffix;
	}

	/* Adjust values to be non-zero (mathematical constraint) */
	if (setinit == 0) setinit = 1;
	if (setdone == 0) setdone = 1;

	/* Enforce limits so we don't run over bounds */
	if (setinit > nitems) setinit = nitems;
	if (setdone > nitems) setdone = nitems;

	/* Set the direction of flow (incrementing vs. decrementing) */
	if (setinit > setdone) nextset = -1;

	/*
	 * Allocate memory
	 */
	setmax = setdone > setinit ? setdone : setinit;
	if ((curitems = (char **)malloc(sizeof(char *) * setmax)) == NULL)
		errx(EXIT_FAILURE, "Out of memory?!");
	if ((setnums = (uint64_t *)malloc(sizeof(uint64_t) * setmax)) == NULL)
		errx(EXIT_FAILURE, "Out of memory?!");
	if ((setnums_backend =
	    (uint64_t *)malloc(sizeof(uint64_t) * setmax)) == NULL)
		errx(EXIT_FAILURE, "Out of memory?!");

	/*
	 * Loop over each `set' in the configured direction until we are done.
	 * NB: Each `set' can represent a single item or multiple items.
	 */
	for (curset = setinit;
	    nextset > 0 ? curset <= setdone : curset >= setdone;
	    curset += nextset)
	{
		uint64_t combo;
		uint64_t d, z;
		uint64_t i;
		uint64_t n;
		uint64_t ncombos;
		uint64_t nsubsets;
		uint64_t setpos;
		uint64_t setpos_backend;

		/*
		 * Calculate number of subsets, based on the number of items in
		 * the current set we're working on.
		 */
		nsubsets = nitems - curset + 1;

		/*
		 * Calculate number of combinations based on number of subsets.
		 */
		z = d = 1;
		for (n = 0; n < curset; n++) {
			z *= nsubsets + n;
			d *= n + 1;
		}
		ncombos = z / d;

		/*
		 * Jump to the next set if requested start is beyond this one.
		 */
		if (doseek) {
			if (seek > ncombos) {
				seek -= ncombos;
				continue;
			} else if (seek == 1) {
				doseek = FALSE;
			}
		}

		/*
		 * Fill array with the initial positional arguments
		 */
		for (n = 0; n < curset; n++) curitems[n] = items[n];

		/*
		 * Produce results with the first set of items.
		 */
		if (!doseek) {
			if ((retval = action(curset, curitems)) != 0) break;
			if (docount && --count == 0) break;
		}

		/*
		 * Prefill two arrays used for matrix calculations.
		 *
		 * The first array (setnums) is a linear sequence starting at
		 * one (1) and ending at N (where N is the same integer as the
		 * current set we're operating on). For example, if we are
		 * currently on a set-of-2, setnums is 1, 2.
		 *
		 * The second array (setnums_backend) is a linear sequence
		 * starting at nitems-N and ending at nitems (again, N is the
		 * same integer as the current set we are operating on; nitems
		 * is the total number of items). For example, if we are
		 * operating on a set-of-2, and nitems is 8, setnums_backend is
		 * set to 7, 8.
		 */
		i = 0;
		for (n = 0; n < curset; n++) setnums[n] = n;
		for (n = curset; n > 0; n--) setnums_backend[i++] = nitems - n;

		/*
		 * Process remaining self-similar combinations in the set.
		 */
		for (combo = 1; combo < ncombos; combo++) {
			uint64_t seed;
			uint64_t setnums_last = curset;

			/*
			 * Using self-similarity (matrix) theorem, determine
			 * (by comparing the [sliding] setnums to the stored
			 * setnums_backend) the number of arguments that remain
			 * available for shifting into a new setnums value
			 * (for later mapping into curitems).
			 *
			 * In essence, determine when setnums has slid into
			 * setnums_backend in which case we can mathematically
			 * use the last item to find the next-new item.
			 */
			for (n = curset; n > 0; n--) {
				setpos = setnums[n - 1];
				setpos_backend = setnums_backend[n - 1];
				/*
				 * If setpos is equal to or greater than
				 * setpos_backend then we keep iterating over
				 * the current set's list of argument positions
				 * until otherwise; each time incrementing the
				 * amount of numbers we must produce from
				 * formulae rather than stored position.
				 */
				setnums_last = n - 1;
				if (setpos < setpos_backend) break;
			}

			/*
			 * The next few stanzas are dedicated to rebuilding the
			 * setnums array for mapping positional items
			 * [immediately following] into curitems.
			 */

			/*
			 * Get the generator number used to populate unknown
			 * positions in the matrix (using self-similarity).
			 */
			seed = setnums[setnums_last];

			/*
			 * Use the generator number to populate any position
			 * numbers that weren't carried over from previous
			 * combination run -- using self-similarity theorem.
			 */
			for (n = setnums_last; n <= curset; n++)
				setnums[n] = seed + n - setnums_last + 1;

			/*
			 * Now map the new setnums into values stored in items
			 */
			for (n = 0; n < curset; n++)
				curitems[n] = items[setnums[n]];

			/*
			 * Produce results with this set of items
			 */
			if (doseek) {
				seek--;
				if (seek == 1) doseek = FALSE;
			}
			if (!doseek || seek == 1) {
				doseek = FALSE;
				if ((retval = action(curset, curitems)) != 0)
					goto cmb_return;
				if (docount && --count == 0) goto cmb_return;
			}

		} /* for combo */

	} /* for curset */

cmb_return:
	free(curitems);
	free(setnums);
	free(setnums_backend);

	return (retval);
}

int
cmb_print(uint64_t nitems, char *items[])
{
	uint64_t n;

	if (cmb_print_prefix != NULL) printf("%s", cmb_print_prefix);
	for (n = 0; n < nitems; n++) {
		printf("%s", items[n]);
		if (n < nitems - 1 && cmb_print_delimiter != NULL)
			printf("%s", cmb_print_delimiter);
	}
	if (cmb_print_suffix != NULL) printf("%s", cmb_print_suffix);
	if (cmb_print_nul)
		printf("%c", 0);
	else
		printf("\n");

	return (0);
}

#ifdef HAVE_OPENSSL_BN_H
/*
 * Takes pointer to `struct cmb_config_bn' options and number of items. Returns
 * total number of combinations according to config options. Numbers formatted
 * as openssl bn(3) BIGNUM type.
 */
BIGNUM *
cmb_count_bn(struct cmb_config *config, uint64_t nitems)
{
	int8_t nextset = 1;
	uint64_t curset;
	uint64_t setinit = 1;
	uint64_t setdone = nitems;
	BIGNUM *count = NULL;
	BIGNUM *d = NULL, *z = NULL;
	BIGNUM *ncombos = NULL;
	BN_CTX *ctx = NULL;

	if (nitems == 0) return (NULL);
	if (config != NULL) {
		if (config->range_min != 0 || config->range_max != 0) {
			setinit = config->range_min;
			setdone = config->range_max;
		}
	}

	/* Adjust values to be non-zero (mathematical constraint) */
	if (setinit == 0) setinit = 1;
	if (setdone == 0) setdone = 1;

	/* Enforce limits so we don't run over bounds */
	if (setinit > nitems) setinit = nitems;
	if (setdone > nitems) setdone = nitems;

	/* Set the direction of flow (incrementing vs decrementing) */
	if (setinit > setdone) nextset = -1;

	/*
	 * Initialize count
	 */
	if ((count = BN_new()) == NULL)	return (NULL);
	if (!BN_zero(count)) goto cmb_count_bn_return;

	/*
	 * Allocate memory
	 */
	if ((ctx =	BN_CTX_new()) == NULL)	goto cmb_count_bn_return;
	if ((d =	BN_new()) == NULL)	goto cmb_count_bn_return;
	if ((ncombos =	BN_new()) == NULL)	goto cmb_count_bn_return;
	if ((z =	BN_new()) == NULL)	goto cmb_count_bn_return;

	/*
	 * Loop over each `set' in the configured direction until we are done
	 */
	for (curset = setinit;
	    nextset > 0 ? curset <= setdone : curset >= setdone;
	    curset += nextset) {
		uint64_t n;
		uint64_t nsubsets;

		/*
		 * Calculate number of subsets, based on the number of items in
		 * the current set we're working on.
		 */
		nsubsets = nitems - curset + 1;

		/*
		 * Calculate number of combinations based on number of subsets
		 */
		if (!BN_one(z) || !BN_one(d)) break;
		for (n = 0; n < curset; n++) {
			if (!BN_mul_word(z, nsubsets + n)) break;
			if (!BN_mul_word(d, n + 1)) break;
		}
		if (!BN_div(ncombos, NULL, z, d, ctx)) break;

		/*
		 * Add the number of combinations in this set to the total
		 */
		if (!BN_add(count, count, ncombos)) break;

	} /* curset */

cmb_count_bn_return:
	BN_CTX_free(ctx);
	BN_free(d);
	BN_free(ncombos);
	BN_free(z);

	return (count);
}

/*
 * Takes pointer to `struct cmb_config_bn' options, number of items, and array
 * of `char *' items. Calculates combinations according to options and either
 * prints combinations to stdout (default) or runs `action' if passed-in as
 * function pointer member of `config' argument. Numbers formatted as openssl
 * bn(3) BIGNUM type.
 */
int
cmb_bn(struct cmb_config *config, uint64_t nitems, char *items[])
{
	uint8_t docount = FALSE;
	uint8_t doseek = FALSE;
	int8_t nextset = 1;
	int retval = 0;
	uint64_t curset;
	uint64_t setdone = nitems;
	uint64_t setinit = 1;
	uint64_t setmax;
	char **curitems;
	uint64_t *setnums;
	uint64_t *setnums_backend;
	BIGNUM *combo = NULL;
	BIGNUM *count = NULL;
	BIGNUM *d = NULL, *z = NULL;
	BIGNUM *ncombos = NULL;
	BIGNUM *seek = NULL;
	BN_CTX *ctx = NULL;
	int (*action)(uint64_t nitems, char *items[]) = cmb_print;

	if (nitems == 0) return (0);
	if (config != NULL) {
		cmb_print_nul = config->nul_terminate;
		if (config->action != NULL) action = config->action;
		if (config->count_bn != NULL) {
			docount = TRUE;
			if ((count = BN_dup(config->count_bn)) == NULL)
				goto cmb_bn_return;
		}
		if (config->delimiter != NULL)
			cmb_print_delimiter = config->delimiter;
		if (config->prefix != NULL) cmb_print_prefix = config->prefix;
		if (config->range_min != 0 || config->range_max != 0) {
			setinit = config->range_min;
			setdone = config->range_max;
		}
		if (config->start_bn != NULL) {
			doseek = TRUE;
			if ((seek = BN_dup(config->start_bn)) == NULL)
				goto cmb_bn_return;
		}
		if (config->suffix != NULL) cmb_print_suffix = config->suffix;
	}

	/* Adjust values to be non-zero (mathematical constraint) */
	if (setinit == 0) setinit = 1;
	if (setdone == 0) setdone = 1;

	/* Enforce limits so we don't run over bounds */
	if (setinit > nitems) setinit = nitems;
	if (setdone > nitems) setdone = nitems;

	/* Set the direction of flow (incrementing vs. decrementing) */
	if (setinit > setdone) nextset = -1;

	/*
	 * Allocate memory
	 */
	if ((combo =	BN_new()) == NULL)	goto cmb_bn_return;
	if ((ctx =	BN_CTX_new()) == NULL)	goto cmb_bn_return;
	if ((d =	BN_new()) == NULL)	goto cmb_bn_return;
	if ((ncombos =	BN_new()) == NULL)	goto cmb_bn_return;
	if ((z =	BN_new()) == NULL)	goto cmb_bn_return;
	setmax = setdone > setinit ? setdone : setinit;
	if ((curitems = (char **)malloc(sizeof(char *) * setmax)) == NULL)
		errx(EXIT_FAILURE, "Out of memory?!");
	if ((setnums = (uint64_t *)malloc(sizeof(uint64_t) * setmax)) == NULL)
		errx(EXIT_FAILURE, "Out of memory?!");
	if ((setnums_backend =
	    (uint64_t *)malloc(sizeof(uint64_t) * setmax)) == NULL)
		errx(EXIT_FAILURE, "Out of memory?!");

	/*
	 * Loop over each `set' in the configured direction until we are done.
	 * NB: Each `set' can represent a single item or multiple items.
	 */
	for (curset = setinit;
	    nextset > 0 ? curset <= setdone : curset >= setdone;
	    curset += nextset)
	{
		uint64_t i;
		uint64_t n;
		uint64_t nsubsets;
		uint64_t setpos;
		uint64_t setpos_backend;

		/*
		 * Calculate number of subsets, based on the number of items in
		 * the current set we're working on.
		 */
		nsubsets = nitems - curset + 1;

		/*
		 * Calculate number of combinations based on number of subsets.
		 */
		if (!BN_one(z) || !BN_one(d)) break;
		for (n = 0; n < curset; n++) {
			if (!BN_mul_word(z, nsubsets + n)) break;
			if (!BN_mul_word(d, n + 1)) break;
		}
		if (!BN_div(ncombos, NULL, z, d, ctx)) break;

		/*
		 * Jump to the next set if requested start is beyond this one.
		 */
		if (doseek) {
			if (BN_ucmp(seek, ncombos) > 0) {
				if (!BN_sub(seek, seek, ncombos)) break;
				continue;
			} else if (BN_is_one(seek)) {
				doseek = FALSE;
			}
		}

		/*
		 * Fill array with the initial positional arguments
		 */
		for (n = 0; n < curset; n++) curitems[n] = items[n];

		/*
		 * Produce results with the first set of items.
		 */
		if (!doseek) {
			if ((retval = action(curset, curitems)) != 0) break;
			if (docount) {
				if (!BN_sub_word(count, 1)) break;
				if (BN_is_zero(count)) break;
			}
		}

		/*
		 * Prefill two arrays used for matrix calculations.
		 *
		 * The first array (setnums) is a linear sequence starting at
		 * one (1) and ending at N (where N is the same integer as the
		 * current set we're operating on). For example, if we are
		 * currently on a set-of-2, setnums is 1, 2.
		 *
		 * The second array (setnums_backend) is a linear sequence
		 * starting at nitems-N and ending at nitems (again, N is the
		 * same integer as the current set we are operating on; nitems
		 * is the total number of items). For example, if we are
		 * operating on a set-of-2, and nitems is 8, setnums_backend is
		 * set to 7, 8.
		 */
		i = 0;
		for (n = 0; n < curset; n++) setnums[n] = n;
		for (n = curset; n > 0; n--) setnums_backend[i++] = nitems - n;

		/*
		 * Process remaining self-similar combinations in the set.
		 */
		if (!BN_one(combo)) break;
		for (; BN_ucmp(combo, ncombos) < 0; ) {
			uint64_t seed;
			uint64_t setnums_last = curset;

			/*
			 * Using self-similarity (matrix) theorem, determine
			 * (by comparing the [sliding] setnums to the stored
			 * setnums_backend) the number of arguments that remain
			 * available for shifting into a new setnums value
			 * (for later mapping into curitems).
			 *
			 * In essence, determine when setnums has slid into
			 * setnums_backend in which case we can mathematically
			 * use the last item to find the next-new item.
			 */
			for (n = curset; n > 0; n--) {
				setpos = setnums[n - 1];
				setpos_backend = setnums_backend[n - 1];
				/*
				 * If setpos is equal to or greater than
				 * setpos_backend then we keep iterating over
				 * the current set's list of argument positions
				 * until otherwise; each time incrementing the
				 * amount of numbers we must produce from
				 * formulae rather than stored position.
				 */
				setnums_last = n - 1;
				if (setpos < setpos_backend) break;
			}

			/*
			 * The next few stanzas are dedicated to rebuilding the
			 * setnums array for mapping positional items
			 * [immediately following] into curitems.
			 */

			/*
			 * Get the generator number used to populate unknown
			 * positions in the matrix (using self-similarity).
			 */
			seed = setnums[setnums_last];

			/*
			 * Use the generator number to populate any position
			 * numbers that weren't carried over from previous
			 * combination run -- using self-similarity theorem.
			 */
			for (n = setnums_last; n <= curset; n++)
				setnums[n] = seed + n - setnums_last + 1;

			/*
			 * Now map the new setnums into values stored in items
			 */
			for (n = 0; n < curset; n++)
				curitems[n] = items[setnums[n]];

			/*
			 * Produce results with this set of items
			 */
			if (doseek) {
				if (!BN_sub_word(seek, 1)) goto cmb_bn_return;
				if (BN_is_one(seek)) doseek = FALSE;
			}
			if (!doseek || BN_is_one(seek)) {
				doseek = FALSE;
				if ((retval = action(curset, curitems)) != 0)
					goto cmb_bn_return;
				if (docount) {
					if (!BN_sub_word(count, 1))
						goto cmb_bn_return;
					if (BN_is_zero(count))
						goto cmb_bn_return;
				}
			}

			if (!BN_add_word(combo, 1)) goto cmb_bn_return;

		} /* for combo */

	} /* for curset */

cmb_bn_return:
	BN_CTX_free(ctx);
	BN_free(combo);
	BN_free(count);
	BN_free(d);
	BN_free(ncombos);
	BN_free(seek);
	BN_free(z);

	return (retval);
}
#endif /* HAVE_OPENSSL_BN_H */
