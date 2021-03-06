/* $Id$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <build-defs.h>

/*
 * SHIX  -  Scan for Harassment and Intercept eXpress messages.
 *
 * SHIX was developed from the MASH (Monolith Against Sexual Harassment)
 * system, which scans eXpress messages for keywords and intercepts the
 * message if a keyword is found.
 * SHIX also scans for keywords, but does not intercept on the basis of
 * just one match. Instead it calculates a score for the messages, which
 * is based on the values that are assigned to strings in its datafile(s).
 *
 * In its latest revision SHIX uses regular expression matching, thus
 * increasing the chance of preventing harassing x-es getting through
 * without blocking normal x-es.
 *
 * MASH  -  Copyright (c) Monolith Community Development 1995.
 * SHIX  -  Copyright (c) Monolith Community Development 1996 - 1999.
 *
 * Both MASH and SHIX may _not_ be used or distributed before obtaining
 * permission from the developers of the system or the founders of Monolith
 * BBS.
 */

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include "monolith.h"

#define extern
#include "libshix.h"
#undef extern

#include "routines.h"


/*
 * on TRUE the x will be killed, on FALSE it will not be killed.
 * scoring per word should be installed instead of a one-hit evaluation.
 */

int
shix(const char *message)
{

    shix_t entry;
    char nocase[X_BUFFER], text[100];
    FILE *fp;
    int i, hiscore = 0;
    regexp *expr = NULL;

    strcpy(nocase, "");
    strcpy(text, "");

    for (i = 0; message[i] != '\0' && i < X_BUFFER; i++) {
	nocase[i] = tolower(message[i]);
    }

    fp = xfopen(SHIX_SCOREFILE, "r", FALSE);
    if (fp == NULL)
	return FALSE;

    while (fgets(text, 99, fp) != NULL) {

	/* this should remove the '\n' the fgets() adds */
	if (strlen(text) == 0)
	    continue;
	text[strlen(text) - 1] = '\0';

	strcpy(entry.word, strtok(text, DELIM));
	if (strlen(entry.word) == 0)
	    continue;

	/* compile string into a regexp */
	expr = regcomp(entry.word);

	/* look for a regexp match */
	if (regexec(expr, nocase) != 0)
	    hiscore += atoi(strtok(NULL, DELIM));

	xfree(expr);
        expr = NULL;
    }

    fclose(fp);

    if (hiscore >= 100)
	return TRUE;
    return FALSE;
}

int
shix_strmatch( char *input, char *match )
{
    regexp *expr = NULL;

    expr = regcomp(match);

    if( regexec(expr, input) != 0 ) {
        xfree(expr);
#ifdef DEBUG
        fprintf( stdout, "\n'%s' -> '%s': YES\n", input, match);
        fflush(stdout);
#endif
        return TRUE;
    }
    xfree(expr);
#ifdef DEBUG
    printf("\n'%s' -> '%s': YES\n", input, match);
    fflush(stdout);
#endif
    return FALSE;
}

/*
 * Check if input is a valid regular expression.
 */
int
shix_valid( char *input)
{
    regexp *expr = NULL;
    regexp *expr1 = NULL;
    regexp *expr2 = NULL;

    expr = regcomp("^[*?]");
    if(regexec(expr,input)) {
        xfree(expr);
        return FALSE;
    }
    xfree(expr);

    expr1 = regcomp("^.*[^\\]\\[.*$");
    expr2 = regcomp("^.*\\].*$");
    if(regexec(expr1,input) && (!regexec(expr2,input))) {
        xfree(expr1);
        xfree(expr2);
        return FALSE;
    }
    xfree(expr1);
    xfree(expr2);

    return TRUE;
}
