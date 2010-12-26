/**
 * Copyright (C) 2010 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An ncurses apache weblog analyzer & interactive viewer
 *
 * This program is free software; you can redistribute it and/or    
 * modify it under the terms of the GNU General Public License as   
 * published by the Free Software Foundation; either version 2 of   
 * the License, or (at your option) any later version.              
 *                                                                  
 * This program is distributed in the hope that it will be useful,  
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    
 * GNU General Public License for more details.                     
 *                                                                  
 * A copy of the GNU General Public License is attached to this 
 * source distribution for its full text.
 *
 * Visit http://goaccess.prosoftcorp.com for new releases.
 */

#ifndef ALLOC_H_INCLUDED
#define ALLOC_H_INCLUDED

/* macro for easily allocating memory */
#define MALLOC_STRUCT(target, size)                 \
do {                                                \
	target = malloc(sizeof * target * size);         \
	if (target) {                                    \
		size_t i;                                     \
		for (i = 0; i < size; i++) {                  \
			target[i] = malloc(sizeof * target[i]);	 \
		}                                             \
	}                                                \
} while (0);
#endif
