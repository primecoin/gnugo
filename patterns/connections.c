/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *\
 * This is GNU Go, a Go program. Contact gnugo@gnu.org, or see       *
 * http://www.gnu.org/software/gnugo/ for more information.          *
 *                                                                   *
 * Copyright 1999, 2000, 2001, 2002 and 2003                         *
 * by the Free Software Foundation.                                  *
 *                                                                   *
 * This program is free software; you can redistribute it and/or     *
 * modify it under the terms of the GNU General Public License as    *
 * published by the Free Software Foundation - version 2             *
 *                                                                   *
 * This program is distributed in the hope that it will be useful,   *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the     *
 * GNU General Public License in file COPYING for more details.      *
 *                                                                   *
 * You should have received a copy of the GNU General Public         *
 * License along with this program; if not, write to the Free        *
 * Software Foundation, Inc., 59 Temple Place - Suite 330,           *
 * Boston, MA 02111, USA.                                            *
\* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include "liberty.h"
#include "patterns.h"


/* Try to match all (permutations of) connection patterns at (m,n).
 * For each match, if it is a B pattern, set cutting point in worm
 * data structure and make eye space marginal for the connection
 * inhibiting entries of the pattern. If it is a C pattern, amalgamate
 * the dragons in the pattern.
 */

static void
cut_connect_callback(int anchor, int color, struct pattern *pattern,
		     int ll, void *data)
{
  int move;
  int k;
  int first_dragon  = NO_MOVE;
  int second_dragon = NO_MOVE;

  int other = OTHER_COLOR(color);
  UNUSED(data);

  move = AFFINE_TRANSFORM(pattern->move_offset, ll, anchor);
  
  if ((pattern->class & CLASS_B) && !safe_move(move, other))
    return;

  /* Reject C patterns in which an INHIBIT_CONNECTION was set
   * previously during find_cuts.
   */

  if (pattern->class & CLASS_C) {
    /* If C pattern, test if there are more than one dragon in this
     * pattern so that there is something to connect, before doing any
     * expensive reading.
     */

    for (k = 0; k < pattern->patlen; ++k) { /* match each point */
      /* transform pattern real coordinate */
      int pos = AFFINE_TRANSFORM(pattern->patn[k].offset, ll, anchor);
      
      /* Look for distinct dragons. */
      if (pattern->patn[k].att == ATT_O) {
	if (first_dragon == NO_MOVE)
	  first_dragon = dragon[pos].origin;
	else if (second_dragon == NO_MOVE
		 && dragon[pos].origin != first_dragon) {
	  second_dragon = dragon[pos].origin;
	  /* A second dragon found, no need to continue looping. */
	  break;
	}
      }
    }
    if (second_dragon == NO_MOVE)
      return; /* Nothing to amalgamate. */
  }
    
  /* If the pattern has a constraint, call the autohelper to see
   * if the pattern must be rejected.
   */
  if (pattern->autohelper_flag & HAVE_CONSTRAINT) {
    if (!pattern->autohelper(ll, move, color, 0))
      return;
  }

  /* If the pattern has a helper, call it to see if the pattern must
   * be rejected.
   */
  if (pattern->helper) {
    if (!pattern->helper(pattern, ll, move, color))
      return;
  }

  if ((pattern->class & CLASS_B)
      && !(pattern->class & CLASS_s)) {
    /* Require that the X stones in the pattern are tactically safe. */
    for (k = 0; k < pattern->patlen; ++k) { /* match each point */
      if (pattern->patn[k].att == ATT_X) {
	/* transform pattern real coordinate */
	int pos = AFFINE_TRANSFORM(pattern->patn[k].offset, ll, anchor);

	if (worm[pos].attack_codes[0] == WIN
	  && (move == NO_MOVE
	      || !does_defend(move, pos)))
	  return; /* Match failed */
      }
    }
  }

  /* Get here => Pattern matches. */
  if (pattern->class & CLASS_B) {
    DEBUG(DEBUG_DRAGONS, "Cutting pattern %s+%d found at %1m\n",
	  pattern->name, ll, anchor);
    DEBUG(DEBUG_DRAGONS, "cutting point %1m\n", move);
  }
  else if (pattern->class & CLASS_C)
    DEBUG(DEBUG_DRAGONS, "Connecting pattern %s+%d found at %1m\n",
	  pattern->name, ll, anchor);

  /* does the pattern have an action? */
  if (pattern->autohelper_flag & HAVE_ACTION) {
    pattern->autohelper(ll, move, color, 1);
  }

  /* If it is a B pattern, set cutting point in eye data and make eye
   * space marginal. Also set the connection inhibition property.
   */
  
  if (pattern->class & CLASS_B) {
    if (color == WHITE) {
      white_eye[move].cut = 1;
      white_eye[move].type |= INHIBIT_CONNECTION;
    }
    else {
      black_eye[move].cut = 1;
      black_eye[move].type |= INHIBIT_CONNECTION;
    }
    if (color == WHITE && white_eye[move].color == WHITE)
      white_eye[move].marginal = 1;
    else if (color == BLACK && black_eye[move].color == BLACK)
      black_eye[move].marginal = 1;
  }
  else if (!(pattern->class & CLASS_C))
    return; /* Nothing more to do, up to the helper or autohelper
	       to amalgamate dragons or modify eye space. */

  /* If it is a C pattern, find the dragons to connect.
   * If it is a B pattern, find eye space points to inhibit connection
   * through.
   */
  first_dragon  = NO_MOVE;
  second_dragon = NO_MOVE;
  for (k = 0; k < pattern->patlen; ++k) { /* match each point */
    /* transform pattern real coordinate */
    int pos = AFFINE_TRANSFORM(pattern->patn[k].offset, ll, anchor);

    /* Look for dragons to amalgamate. Never amalgamate stones which
     * can be attacked.
     */
    if ((pattern->class & CLASS_C)
	&& board[pos] == color
	&& pattern->patn[k].att == ATT_O
	&& ((pattern->class & CLASS_s) || worm[pos].attack_codes[0] == 0)) {
      if (first_dragon == NO_MOVE)
	first_dragon = dragon[pos].origin;
      else if (second_dragon == NO_MOVE
	       && dragon[pos].origin != first_dragon) {
	second_dragon = dragon[pos].origin;
	/* A second dragon found, we amalgamate them at once. */
	/* Want this output if verbose or DEBUG_DRAGONS is on. */
	if (verbose || (debug & DEBUG_DRAGONS))
	  gprintf("Pattern %s joins %C dragons %1m, %1m\n",
		  pattern->name, color, first_dragon, second_dragon);
	join_dragons(second_dragon, first_dragon);
	/* Now look for another second dragon. */
	second_dragon = NO_MOVE;
	first_dragon = dragon[pos].origin;
      }
    }
    
    /* Inhibit connections */
    if (pattern->class & CLASS_B) {
      if (pattern->patn[k].att != ATT_not)
	break; /* The inhibition points are guaranteed to come first. */
      if (color == WHITE && white_eye[pos].color == WHITE) {
	white_eye[pos].type |= INHIBIT_CONNECTION;
	DEBUG(DEBUG_DRAGONS, "inhibiting connection at %1m\n", pos);
      }
      else if (color == BLACK && black_eye[pos].color == BLACK) {
	black_eye[pos].type |= INHIBIT_CONNECTION;
	DEBUG(DEBUG_DRAGONS, "inhibiting connection at %1m\n", pos);
      }
    }
  } /* loop over elements */
}


/* Only consider B patterns. */
static void
cut_callback(int anchor, int color, struct pattern *pattern, int ll,
	     void *data)
{
  if (pattern->class & CLASS_B)
    cut_connect_callback(anchor, color, pattern, ll, data);
}
  

/* Consider C patterns and those without classification. */
static void
conn_callback(int anchor, int color, struct pattern *pattern, int ll,
	      void *data)
{
  if (!(pattern->class & CLASS_B))
    cut_connect_callback(anchor, color, pattern, ll, data);
}
  
/* Find cutting points which should inhibit amalgamations and sever
 * the adjacent eye space.
 */
void
find_cuts(void)
{
  matchpat(cut_callback, ANCHOR_COLOR, &conn_db, NULL, NULL);
}

/* Find explicit connection patterns and amalgamate the involved dragons. */
void
find_connections(void)
{
  matchpat(conn_callback, ANCHOR_COLOR, &conn_db, NULL, NULL);
}


/*
 * Local Variables:
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */
