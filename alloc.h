/**
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 * @version 0.1
 * Last Modified: Wednesday, July 07, 2010
 * Path:  /alloc.h
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * GoAccess is released under the GNU/GPL License.
 * Copy of the GNU General Public License is attached to this source 
 * distribution for its full text.
 *
 * Visit http://goaccess.prosoftcorp.com for new releases.
 */

#ifndef ALLOC_H_INCLUDED
#define ALLOC_H_INCLUDED

/* macro for easily allocating memory */
#define ALLOCATE_STRUCT(target, size)				\
do {												\
	target = malloc(sizeof * target * size);		\
	if (target) {									\
		size_t i;									\
		for (i = 0; i < size; i++) {				\
			target[i] = malloc(sizeof * target[i]);	\
		}											\
	}												\
} while (0);

#endif

