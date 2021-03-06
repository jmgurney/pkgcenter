#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <cmb.h>

#include "const-c.inc"

SV *g_action = NULL;
AV *g_args = NULL;

int
g_callback(struct cmb_config *config, uint64_t seq, uint32_t nitems,
    char *items[])
{
	uint32_t i;
	int result = 0;
	dTHX;
	dSP;
	dMULTICALL;
	HV *stash;
	GV *gv;
	U8 gimme = G_SCALAR;
	CV *cv;

#ifdef __linux__
	(void)newsp;
#endif

	cv = sv_2cv(g_action, &stash, &gv, 0);
	PUSH_MULTICALL(cv);

	av_clear(g_args);
	if (config != NULL && config->show_numbers)
		av_push(g_args, (SV *)newSViv(seq));
	for (i = 1; i <= nitems; i++) {
		av_push(g_args, (SV *)items[i-1]);
		SvREFCNT_inc((SV *)items[i-1]);
	}

	{
		GvAV(PL_defgv) = g_args;
		MULTICALL;
	}

	result = SvIV(*PL_stack_sp);
	POP_MULTICALL;

	return (result);
}

void
_config(struct cmb_config *c, SV *hash)
{
	HV *config = (HV *)SvRV(hash);
	HE *entry;
	char *k;
	SV *v;
	I32 len;
	dTHX;

	(void)hv_iterinit(config);
	while ((entry = hv_iternext(config))) {
		k = hv_iterkey(entry, &len);
		v = hv_iterval(config, entry);
		if (strEQ(k, "debug")) {
			c->debug = SvIV(v);
		} else if (strEQ(k, "nul_terminate")) {
			c->nul_terminate = SvIV(v);
		} else if (strEQ(k, "show_empty")) {
			c->show_empty = SvIV(v);
		} else if (strEQ(k, "show_numbers")) {
			c->show_numbers = SvIV(v);
		} else if (strEQ(k, "delimiter")) {
			c->delimiter = SvPV(v, PL_na);
		} else if (strEQ(k, "prefix")) {
			c->prefix = SvPV(v, PL_na);
		} else if (strEQ(k, "suffix")) {
			c->suffix = SvPV(v, PL_na);
		} else if (strEQ(k, "size_min")) {
			c->size_min = SvIV(v);
		} else if (strEQ(k, "size_max")) {
			c->size_max = SvIV(v);
		} else if (strEQ(k, "count")) {
			c->count = SvIV(v);
		} else if (strEQ(k, "start")) {
			c->start = SvIV(v);
		}
	}
}

typedef struct cmb_config * Cmb;

MODULE = Cmb		PACKAGE = Cmb		

INCLUDE: const-xs.inc

TYPEMAP: <<EOF
struct cmb_config *	T_PTROBJ
Cmb			T_PTROBJ
uint32_t		T_U_LONG
unsigned long long	T_U_LONG_LONG
uint64_t		T_U_LONG_LONG

INPUT
T_U_LONG_LONG
	$var = (unsigned long long)SvUv($arg)

OUTPUT
T_U_LONG_LONG
	sv_setuv($arg, (UV)$var);
EOF

Cmb
new(char *class, ...)
CODE:
	(void)class;
	struct cmb_config *c = calloc(1, sizeof(struct cmb_config));
	if (items > 1)
		_config(c, ST(1));
	RETVAL = c;
OUTPUT:
	RETVAL

void
DESTROY(c)
	Cmb c;
CODE:
	free(c);

void
config(c, hash)
	Cmb c
	SV *hash
CODE:
	_config(c, hash);

const char *
cmb_version(...)
CODE:
	RETVAL = cmb_version(items > 0 ? SvIV(ST(0)) : 0);
OUTPUT:
	RETVAL

const char *
version(c, ...)
	Cmb c
CODE:
	(void)c;
	RETVAL = cmb_version(items > 1 ? SvIV(ST(1)) : 0);
OUTPUT:
	RETVAL

uint64_t
cmb_count(c, nitems)
	Cmb c
	uint32_t nitems

uint64_t
count(c, nitems)
	Cmb c
	uint32_t nitems
CODE:
	RETVAL = cmb_count(c, nitems);
OUTPUT:
	RETVAL

int
print(c, seq, nitems, arrayref)
PREINIT:
	char **array;
	int inum;
	SV **item;
	int i;
INPUT:
	Cmb c;
	IV seq;
	uint32_t nitems;
	AV *arrayref;
CODE:
	inum = av_len(arrayref);
	array = (char **)calloc(inum + 1, sizeof(char *));
	for (i = 0; i <= inum; i++) {
		if ((item = av_fetch(arrayref, i, 0)) && SvOK(*item)) {
			array[i] = SvPV(*item, PL_na);
		}
	}
	RETVAL = cmb_print(c, seq, nitems, array);
OUTPUT:
	RETVAL

int
cmb(c, nitems, arrayref)
PREINIT:
	char **array;
	int inum;
	SV **item;
	int i;
INPUT:
	Cmb c;
	uint32_t nitems;
	AV *arrayref;
CODE:
	inum = av_len(arrayref);
	array = (char **)calloc(inum + 1, sizeof(char *));
	for (i = 0; i <= inum; i++) {
		if ((item = av_fetch(arrayref, i, 0)) && SvOK(*item)) {
			array[i] = SvPV(*item, PL_na);
		}
	}
	RETVAL = cmb(c, nitems, array);
OUTPUT:
	RETVAL

int
cmb_callback(c, nitems, arrayref, name)
PREINIT:
	int inum;
	char **array;
	int i;
	SV **item;
	int (*_action)(struct cmb_config *c, uint64_t seq, uint32_t nitems,
	    char *array[]);
INPUT:
	Cmb c;
	uint32_t nitems;
	AV *arrayref;
	SV *name;
CODE:
	/* Set custom callback to call referenced subroutine */
	_action = c->action; /* save current callback */
	c->action = g_callback;

	/* Configure which Perl subroutine g_callback() should call */
	g_action = name;

	/* Initialize globals used by g_callback() */
	if (g_args == NULL)
		g_args = (AV *)sv_2mortal((SV *)newAV());

	/* Copy elements from arrayref to C array of SV pointers */
	inum = av_len(arrayref);
	array = (char **)calloc(inum + 1, sizeof(char *));
	for (i = 0; i <= inum; i++) {
		if ((item = av_fetch(arrayref, i, 0)) && SvOK(*item)) {
			array[i] = (char *)*item;
		}
	}

	/* Combine scalar value reference pointers using g_callback() */
	RETVAL = cmb(c, nitems, array);
	c->action = _action; /* restore previous callback */
OUTPUT:
	RETVAL
