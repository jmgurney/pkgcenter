/*-
 * Copyright (c) 2018 Devin Teske <dteske@FreeBSD.org>
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
__FBSDID("$FrauBSD: pkgcenter/depend/cmb/cmb.c 2018-12-17 02:23:26 -0800 freebsdfrau $");
__FBSDID("$FreeBSD$");
#endif

#include <sys/param.h>
#include <sys/time.h>

#include <cmb.h>
#include <err.h>
#include <errno.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#elif defined(__FreeBSD_version)
#ifdef HAVE_LIBCRYPTO
#define HAVE_OPENSSL_BN_H 1
#define HAVE_OPENSSL_CRYPTO_H 1
#else
#undef HAVE_OPENSSL_BN_H
#undef HAVE_OPENSSL_CRYPTO_H
#endif
#endif

#ifdef HAVE_OPENSSL_BN_H
#include <openssl/bn.h>
#endif
#ifdef HAVE_OPENSSL_CRYPTO_H
#include <openssl/crypto.h>
#endif

static char version[] = "$Version: 2.0 $";

/* Environment */
static char *pgm; /* set to argv[0] by main() */

/* Function prototypes */
static void	_Noreturn usage(void);
static uint64_t	rand_range(uint64_t range);
static int	nop(struct cmb_config *config, uint64_t seq, uint32_t nitems, char *items[]);
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
static int	nop_bn(struct cmb_config *config, BIGNUM *seq, uint32_t nitems, char *items[]);
#endif

/* Inline functions */
static inline uint8_t	p2(uint64_t x) { return (x == (x & -x)); }
static inline uint64_t	urand64(void) { return (((uint64_t)lrand48() << 42)
    + ((uint64_t)lrand48() << 21) + (uint64_t)lrand48());
}

int
main(int argc, char *argv[])
{
	uint8_t opt_empty = FALSE;
#ifdef HAVE_LIBCRYPTO
	uint8_t opt_nossl = FALSE;
#endif
	uint8_t opt_randi = FALSE;
	uint8_t opt_silent = FALSE;
	uint8_t opt_total = FALSE;
	uint8_t opt_total_num = FALSE;
	uint8_t opt_version = FALSE;
	char *cp;
	char *cmdver = version;
	char **items = argv;
	const char *libver = cmb_version(CMB_VERSION);
	char *opt_file = NULL;
	int ch;
	int retval = EXIT_SUCCESS;
	uint32_t nitems = 0;
	uint32_t titems = 0;
	size_t config_size = sizeof(struct cmb_config);
	size_t optlen;
	struct cmb_config *config = NULL;
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
	BIGNUM *count_bn;
#endif
	uint64_t count;
	uint64_t nstart = 0; /* negative start */
	struct timeval tv;

	pgm = argv[0]; /* store a copy of invocation name */

	/* Allocate config structure */
	if ((config = malloc(config_size)) == NULL)
		errx(EXIT_FAILURE, "Out of memory?!");
	bzero(config, sizeof(struct cmb_config));

	/*
	 * Process command-line options
	 */
#define OPTSTRING1 "0c:d:ef:i:k:Nn:op:s:StT:v"
#if CMB_DEBUG
#define OPTSTRING2 OPTSTRING1 "D"
#else
#define OPTSTRING2 OPTSTRING1
#endif
#define OPTSTRING OPTSTRING2
	while ((ch = getopt(argc, argv, OPTSTRING)) != -1) {
		switch(ch) {
		case '0': /* NUL terminate */
			config->nul_terminate = TRUE;
			break;
		case 'c': /* count */
			if (*optarg < 48 || *optarg > 57) {
				errno = EINVAL;
				err(EXIT_FAILURE, "-c");
				/* NOTREACHED */
			}
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
			if (!opt_nossl &&
			    BN_dec2bn(&(config->count_bn), optarg) == 0)
				errx(EXIT_FAILURE, "OpenSSL Error?!");
			if (opt_nossl) {
#endif
				errno = 0;
				config->count = strtoull(optarg,
				    (char **)NULL, 10);
				if (errno != 0) {
					err(EXIT_FAILURE, "-c");
					/* NOTREACHED */
				}
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
			}
#endif
			break;
		case 'd': /* delimiter */
			config->delimiter = optarg;
			break;
#if CMB_DEBUG
		case 'D': /* debug */
			config->debug = TRUE;
			break;
#endif
		case 'e': /* empty */
			opt_empty = TRUE;
			config->show_empty = opt_empty;
			break;
		case 'f': /* file */
			opt_file = optarg;
			break;
		case 'i': /* start */
			if ((optlen = strlen(optarg)) > 0 &&
			    strncmp("random", optarg, optlen) == 0) {
				opt_randi = TRUE;
			} else if (*optarg < 48 || *optarg > 57) {
				if (*optarg != '-' ||
				    optarg[1] < 48 || optarg[1] > 57) {
					errno = EINVAL;
					err(EXIT_FAILURE, "-i");
					/* NOTREACHED */
				}
			}
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
			if (!opt_randi && !opt_nossl &&
			    BN_dec2bn(&(config->start_bn), optarg) == 0)
				errx(EXIT_FAILURE, "OpenSSL Error?!");
			if (!opt_randi && opt_nossl) {
#else
			if (!opt_randi) {
#endif
				errno = 0;
				if (*optarg == '-') {
					nstart = strtoull(&optarg[1],
					    (char **)NULL, 10);
				} else {
					config->start = strtoull(optarg,
					    (char **)NULL, 10);
				}
				if (errno != 0) {
					err(EXIT_FAILURE, "-i");
					/* NOTREACHED */
				}
			}
			break;
		case 'k': /* size */
			if (*optarg < 48 || *optarg > 57) {
				if (*optarg != '-' ||
				    optarg[1] < 48 || optarg[1] > 57) {
					errno = EINVAL;
					err(EXIT_FAILURE, "-k");
					/* NOTREACHED */
				}
			}
			errno = 0;
			config->size_min = (uint32_t)strtoul(optarg,
			    (char **)NULL, 10);
			if (errno != 0) {
				err(EXIT_FAILURE, "-k");
				/* NOTREACHED */
			}
			if ((cp = strstr(optarg, "..")) != NULL) {
				errno = 0;
				config->size_max = (uint32_t)strtoul(cp + 2,
				    (char **)NULL, 10);
				if (errno != 0) {
					err(EXIT_FAILURE, "-k");
					/* NOTREACHED */
				}
			} else if ((cp = strstr(optarg, "-")) != NULL) {
				errno = 0;
				config->size_max = (uint32_t)strtoul(cp + 1,
				    (char **)NULL, 10);
				if (errno != 0) {
					err(EXIT_FAILURE, "-k");
					/* NOTREACHED */
				}
			} else {
				config->size_max = config->size_min;
			}
			break;
		case 'N': /* numbers */
			config->show_numbers = TRUE;
			break;
		case 'n': /* args */
			if (*optarg < 48 || *optarg > 57) {
				errno = EINVAL;
				err(EXIT_FAILURE, "-n");
				/* NOTREACHED */
			}
			errno = 0;
			nitems = (uint32_t)strtoul(optarg, (char **)NULL, 10);
			if (errno != 0) {
				err(EXIT_FAILURE, "-n");
				/* NOTREACHED */
			}
			break;
		case 'o': /* disable openssl */
#ifdef HAVE_LIBCRYPTO
			opt_nossl = TRUE;
#endif
			break;
		case 'p': /* prefix */
			config->prefix = optarg;
			break;
		case 'S': /* silent */
			opt_silent = TRUE;
			break;
		case 's': /* suffix */
			config->suffix = optarg;
			break;
		case 't': /* total */
			opt_total = TRUE;
			break;
		case 'T': /* total w/ num items */
			if (*optarg < 48 || *optarg > 57) {
				errno = EINVAL;
				err(EXIT_FAILURE, "-T");
				/* NOTREACHED */
			}
			opt_total_num = TRUE;
			errno = 0;
			titems = (uint32_t)strtoul(optarg, (char **)NULL, 10);
			if (errno != 0) {
				err(EXIT_FAILURE, "-T");
				/* NOTREACHED */
			}
			break;
		case 'v': /* version */
			opt_version = TRUE;
			break;
		default: /* unhandled argument (based on switch) */
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;
	items = argv;

	if (opt_version) {
		cmdver += 10; /* Seek past "$Version: " */
		cmdver[strlen(cmdver)-2] = '\0'; /* Place NUL before "$" */
#ifdef HAVE_OPENSSL_CRYPTO_H
		printf("%s: %s (%s; %s)\n", pgm, cmdver, libver,
		    SSLeay_version(SSLEAY_VERSION));
#else
		printf("%s: %s (%s)\n", pgm, cmdver, libver);
#endif
		exit(EXIT_FAILURE);
	}

	/* Print total for num items if given `-T num' */
	if (opt_total_num) {
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
		if (!opt_nossl) {
			char *count_str;

			if ((count_bn =
			    cmb_count_bn(config, titems)) != NULL) {
				count_str = BN_bn2dec(count_bn);
				printf("%s\n", count_str);
#ifdef HAVE_OPENSSL_CRYPTO_H
				OPENSSL_free(count_str);
#endif
				BN_free(count_bn);
			} else
				printf("0\n");
		} else {
#endif
			count = cmb_count(config, titems);
			if (errno) {
				err(EXIT_FAILURE, NULL);
				/* NOTREACHED */
			}
			printf("%"PRIu64"\n", count);
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
		}
#endif
		exit(EXIT_SUCCESS);
	}

	/* At least one non-option argument is required */
	if (argc == 0 && !opt_empty && opt_file == NULL) {
		usage();
		/* NOTREACHED */
	}

	/* Read arguments from file if give `-f file' */
	if (opt_file != NULL) {
		if (argc > 0)
			warnx("arguments ignored when `-f file' given");
		if ((items = malloc(sizeof(char *) * 0xffffffff)) == NULL)
			errx(EXIT_FAILURE, "Out of memory?!");
		items = cmb_parse_file(config, opt_file, &nitems, nitems);
		if (items == NULL && errno != 0) {
			err(EXIT_FAILURE, NULL);
			/* NOTREACHED */
		}
	} else if (nitems == 0 || nitems > (uint32_t)argc)
		nitems = (uint32_t)argc;

	/*
	 * Time-based benchmarking option (-S for silent).
	 *
	 * NB: The call-stack is still incremented into the action, while using
	 *     a nop function allows us to benchmark various action overhead.
	 */
	if (opt_silent) {
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
		if (opt_nossl)
#endif
			config->action = nop;
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
		else
			config->action_bn = nop_bn;
#endif
		config->show_numbers = FALSE;
	}

	/*
	 * Calculate combinations
	 */
	if (opt_total) {
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
		if (!opt_nossl) {
			char *count_str;

			if ((count_bn =
			    cmb_count_bn(config, nitems)) != NULL) {
				count_str = BN_bn2dec(count_bn);
				printf("%s\n", count_str);
#ifdef HAVE_OPENSSL_CRYPTO_H
				OPENSSL_free(count_str);
#endif
				BN_free(count_bn);
			} else
				printf("0\n");
		} else {
#endif
			count = cmb_count(config, nitems);
			if (errno) {
				err(EXIT_FAILURE, NULL);
				/* NOTREACHED */
			}
			printf("%"PRIu64"\n", count);
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
		}
	} else if (!opt_nossl) {
		if (opt_randi) {
			if ((count_bn =
			    cmb_count_bn(config, nitems)) != NULL) {
				if (config->start_bn == NULL)
					config->start_bn = BN_new();
				if (BN_rand_range(config->start_bn, count_bn))
					BN_add_word(config->start_bn, 1);
			}
		} else if (config->start_bn != NULL &&
		    BN_is_negative(config->start_bn)) {
			if ((count_bn =
			    cmb_count_bn(config, nitems)) != NULL) {
				BN_add(config->start_bn,
				    count_bn, config->start_bn);
				BN_add_word(config->start_bn, 1);
				if (BN_is_negative(config->start_bn))
					BN_zero(config->start_bn);
				BN_free(count_bn);
			}
		}
		retval = cmb_bn(config, nitems, items);
#endif
	} else {
		if (opt_randi) {
			count = cmb_count(config, nitems);
			if (errno) {
				err(EXIT_FAILURE, NULL);
				/* NOTREACHED */
			}
			if (gettimeofday(&tv,NULL) == 0) {
				srand48((long)tv.tv_usec);
				config->start = rand_range(count) + 1;
			}
		} else if (nstart != 0) {
			count = cmb_count(config, nitems);
			if (errno) {
				err(EXIT_FAILURE, NULL);
				/* NOTREACHED */
			}
			if (count > nstart)
				config->start = count - nstart + 1;
			else
				config->start = 0;
		}
		retval = cmb(config, nitems, items);
		if (errno) {
			err(EXIT_FAILURE, NULL);
			/* NOTREACHED */
		}
	}

	return (retval);
}

/*
 * Print short usage statement to stderr and exit with error status.
 */
static void
usage(void)
{
	fprintf(stderr, "usage: %s [options] item1 [item2 ...]\n", pgm);
#define OPTFMT "\t%-10s %s\n"
	fprintf(stderr, "OPTIONS:\n");
	fprintf(stderr, OPTFMT, "-0",
	    "NUL terminate combinations (use with `xargs -0').");
	fprintf(stderr, OPTFMT, "-c num",
	    "Produce num combinations (default `0' for all).");
#if CMB_DEBUG
	fprintf(stderr, OPTFMT, "-D",
	    "Enable debugging information on stderr."
	);
#endif
	fprintf(stderr, OPTFMT, "-d str", "Item delimiter (default is ` ').");
	fprintf(stderr, OPTFMT, "-e", "Show empty set with no items.");
	fprintf(stderr, OPTFMT, "-i num",
	    "Skip the first num-1 combinations.");
	fprintf(stderr, OPTFMT, "-k size",
	    "Number or range (`min..max' or `min-max') of items.");
	fprintf(stderr, OPTFMT, "-N", "Show combination sequence numbers.");
	fprintf(stderr, OPTFMT, "-n num",
	    "Limit arguments taken from the command-line.");
	fprintf(stderr, OPTFMT, "-o",
		"Disable OpenSSL (limits calculations to 64-bits).");
	fprintf(stderr, OPTFMT, "-p str", "Prefix text for each line.");
	fprintf(stderr, OPTFMT, "-S", "Silent (for performance benchmarks).");
	fprintf(stderr, OPTFMT, "-s str", "Suffix text for each line.");
	fprintf(stderr, OPTFMT, "-t",
	    "Print number of combinations and exit.");
	fprintf(stderr, OPTFMT, "-T num",
	    "Print number of combinations for num items and exit.");
	fprintf(stderr, OPTFMT, "-v",
	    "Print version info to stdout and exit.");
	exit(EXIT_FAILURE);
}

/*
 * Return pseudo-random 64-bit unsigned integer in range 0 <= return <= range.
 */
static uint64_t
rand_range(uint64_t range)
{
	static const uint64_t M = (uint64_t)~0;
	uint64_t exclusion = p2(range) ? 0 : M % range + 1;
	uint64_t res = 0;

	while ((res = urand64()) < exclusion) {}
	return (res % range);
}

/*
 * For performance benchmarking
 */
static int
nop(struct cmb_config *config, uint64_t seq, uint32_t nitems, char *items[])
{
	(void)config;
	(void)seq;
	(void)nitems;
	(void)items;
	return (0);
}
#if defined(HAVE_LIBCRYPTO) && defined(HAVE_OPENSSL_BN_H)
static int
nop_bn(struct cmb_config *config, BIGNUM *seq, uint32_t nitems, char *items[])
{
	(void)config;
	(void)seq;
	(void)nitems;
	(void)items;
	return (0);
}
#endif
