
/*****************************************************************************
 * iview    -    SDL based image viewer for linux and fbsd. (X and console)  *
 * Copyright (C) 2001 Erik Greenwald <erik@smluc.org>                        *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation; either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program; if not, write to the Free Software               *
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA *
 ****************************************************************************/

#include <stdio.h>		/* for NULL */
#include <stdlib.h>
#include <string.h>
#include "ll.h"


typedef struct anode {
    char *line;
    struct anode *prev;
    struct anode *next;
} node;
typedef struct alist {
    node *head;
    node *current;
    node *last;
} list;

void *
ll_newlist ()
{
    list *x;

    x = (list *) malloc (sizeof (list));
    if (x != NULL)
    {
	x->head = NULL;
	x->current = NULL;
	x->last = NULL;
	return x;
    }
    printf ("Failed to gnerate new list");
    exit (-1);
    return NULL;
}

int
ll_next (void *this)
{
    list *x;

    x = (list *) this;
    if (x == NULL || x->current == NULL || x->current->next == NULL)
	return 0;

/*
    printf ("nextA: \"%s\" [\"%s\"] \"%s\"\n",
	x->current->prev ? x->current->prev->line : "(null)", x->current->line,
	x->current->next ? x->current->next->line : "(null)");
*/
    x->current = x->current->next;

/*
    printf ("nextB: \"%s\" [\"%s\"] \"%s\"\n",
	x->current->prev ? x->current->prev->line : "(null)", x->current->line,
	x->current->next ? x->current->next->line : "(null)");
*/
    return 1;
}

int
ll_prev (void *this)
{
    list *x;

    x = (list *) this;
    if (x == NULL || x->current == NULL || x->current->prev == NULL)
	return 0;

/*
    printf ("prevA: \"%s\" [\"%s\"] \"%s\"\n",
	x->current->prev ? x->current->prev->line : "(null)", 
	x->current->line,
	x->current->next ? x->current->next->line : "(null)");
*/
    x->current = x->current->prev;

/*
    printf ("prevB: \"%s\" [\"%s\"] \"%s\"\n",
	x->current->prev ? x->current->prev->line : "(null)", 
	x->current->line,
	x->current->next ? x->current->next->line : "(null)");
*/
    return 1;
}

int
ll_rewind (void *this)
{
    list *x;

    if (this == NULL)
	return 0;
    x = (list *) this;
    x->current = x->head;
    return 1;
}

int
ll_addatend (void *this, char *line)
{
    node *x;
    list *l;

    if (this == NULL || line == NULL)	/* bad list */
	return 0;

    l = (list *) this;

    /*
     * generate node 
     */
    x = (node *) malloc (sizeof (node));
    if (x == NULL)
	return 0;

    x->line = (char *)strdup (line);
    x->next = NULL;
    x->prev = l->last;

    /*
     * attach node to list 
     */
    if (l->head == NULL)
	l->head = l->current = l->last = x;
    else
    {
	l->last->next = x;
    }
    l->last = x;
    return 1;
}

char *
ll_showline (void *this)
{
    list *l;

    l = (list *) this;
    if (l == NULL || l->current == NULL || l->current->line == NULL)
	return NULL;
    return l->current->line;
}

static void
ll_rec_clearlist (node * this)
{
    if (this == NULL)
	return;
    if (this->next != NULL)
	ll_rec_clearlist (this->next);
    free (this->line);
    free (this);
    return;
}


int
ll_clearlist (void *this)
{

    /*
     * node *x, *y; 
     */
    if (this == NULL || ((list *) this)->head == NULL)
	return 0;

    /*
     * x = ((list *) this)->head; while (x->next != NULL) { y = x->next; if
     * (x->line != NULL) free (x->line); free (x); x = y; } 
     */
    ll_rec_clearlist (((list *) this)->head);
    free (this);
    return 1;
}

int
ll_empty (void *v)
{
    list *l;

    l = (list *) v;
    if (l == NULL || l->head == NULL)
	return 1;
    return 0;
}

int
ll_deletenode (void *v)
{
    list *l;
    node *n, *t;

    l = (list *) v;
    if (l == NULL || l->head == NULL)
	return 0;
    n = l->head;

    while (n->next != NULL && n->next != l->current)
	n = n->next;

    if (n->next == NULL)
	return 0;

    t = n->next;
    n->next = t->next;
    free (t);
    l->current = n->next;

    return 1;
}

void
ll_showall (void *imglist)
{
    list *l;
    node *n;

    l = (list *) imglist;
    n = l->head;
    printf
	("==============================================================================\n");
    while (n != NULL)
    {
	printf (n == l->current ? "[%s]\n" : "%s\n", n->line);
	fflush (stdout);
	n = n->next;
    }
    printf
	("==============================================================================\n");
    return;
}
