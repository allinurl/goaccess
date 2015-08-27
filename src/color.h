/**
 * Copyright (C) 2009-2015 by Gerardo Orellana <goaccess@prosoftcorp.com>
 * GoAccess - An Ncurses apache weblog analyzer & interactive viewer
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

#ifndef COLOR_H_INCLUDED
#define COLOR_H_INCLUDED

#define COLOR_STR_LEN 9

typedef enum CSTM_COLORS
{
  COLOR_NORMAL,
  COLOR_MTRC_HITS,
  COLOR_MTRC_VISITORS,
  COLOR_MTRC_DATA,
  COLOR_MTRC_BW,
  COLOR_MTRC_AVGTS,
  COLOR_MTRC_CUMTS,
  COLOR_MTRC_MAXTS,
  COLOR_MTRC_PROT,
  COLOR_MTRC_MTHD,
  COLOR_MTRC_PERC,
  COLOR_MTRC_PERC_MAX,
  COLOR_PANEL_COLS,
  COLOR_BARS,
  COLOR_ERROR,
  COLOR_SELECTED,
  COLOR_PANEL_ACTIVE,
  COLOR_PANEL_HEADER,
  COLOR_PANEL_DESC,
  COLOR_OVERALL_LBLS,
  COLOR_OVERALL_VALS,
  COLOR_OVERALL_PATH,
  COLOR_ACTIVE_LABEL,
  COLOR_BG,
  COLOR_DEFAULT,
  COLOR_PROGRESS,
} GColorItem;

typedef enum SCHEMES
{
  NO_COLOR,
  MONOCHROME,
  STD_GREEN
} GShemes;

#include "commons.h"

typedef struct GColorPair_
{
  short idx;
  short fg;
  short bg;
} GColorPair;

typedef struct GColors_
{
  GColorItem item;
  GColorPair *pair;
  int attr;
  short module;
} GColors;

GColors *color_default (void);
GColors *color_error (void);
GColors *color_overall_lbls (void);
GColors *color_overall_path (void);
GColors *color_overall_vals (void);
GColors *color_panel_active (void);
GColors *color_panel_desc (void);
GColors *color_panel_header (void);
GColors *color_progress (void);
GColors *color_selected (void);
GColors *get_color_by_item_module (GColorItem item, GModule module);
GColors *get_color (GColorItem item);
GColors *get_color_normal (void);
void free_color_lists (void);
void set_colors (int force);
void set_normal_color (void);

#endif // for #ifndef COLOR_H
