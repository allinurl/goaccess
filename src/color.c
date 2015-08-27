/**
 * color.c -- functions related to custom color
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "color.h"

#include "commons.h"
#include "error.h"
#include "util.h"
#include "xmalloc.h"

static GSLList *color_list = NULL;
static GSLList *pair_list = NULL;

/* *INDENT-OFF* */
static GEnum CSTM_COLORS[] = {
  {"COLOR_MTRC_HITS"     , COLOR_MTRC_HITS}     ,
  {"COLOR_MTRC_VISITORS" , COLOR_MTRC_VISITORS} ,
  {"COLOR_MTRC_PERC"     , COLOR_MTRC_PERC}     ,
  {"COLOR_MTRC_BW"       , COLOR_MTRC_BW}       ,
  {"COLOR_MTRC_AVGTS"    , COLOR_MTRC_AVGTS}    ,
  {"COLOR_MTRC_CUMTS"    , COLOR_MTRC_CUMTS}    ,
  {"COLOR_MTRC_MAXTS"    , COLOR_MTRC_MAXTS}    ,
  {"COLOR_MTRC_PROT"     , COLOR_MTRC_PROT}     ,
  {"COLOR_MTRC_MTHD"     , COLOR_MTRC_MTHD}     ,
  {"COLOR_MTRC_DATA"     , COLOR_MTRC_DATA}     ,
  {"COLOR_MTRC_PERC_MAX" , COLOR_MTRC_PERC_MAX} ,
  {"COLOR_PANEL_COLS"    , COLOR_PANEL_COLS}    ,
  {"COLOR_BARS"          , COLOR_BARS}          ,
  {"COLOR_ERROR"         , COLOR_ERROR}         ,
  {"COLOR_SELECTED"      , COLOR_SELECTED}      ,
  {"COLOR_PANEL_ACTIVE"  , COLOR_PANEL_ACTIVE}  ,
  {"COLOR_PANEL_HEADER"  , COLOR_PANEL_HEADER}  ,
  {"COLOR_PANEL_DESC"    , COLOR_PANEL_DESC}  ,
  {"COLOR_OVERALL_LBLS"  , COLOR_OVERALL_LBLS}  ,
  {"COLOR_OVERALL_VALS"  , COLOR_OVERALL_VALS}  ,
  {"COLOR_OVERALL_PATH"  , COLOR_OVERALL_PATH}  ,
  {"COLOR_ACTIVE_LABEL"  , COLOR_ACTIVE_LABEL}  ,
  {"COLOR_BG"            , COLOR_BG}            ,
  {"COLOR_DEFAULT"       , COLOR_DEFAULT}       ,
  {"COLOR_PROGRESS"      , COLOR_PROGRESS}      ,
};

static const char *colors256_mono[] = {
  "COLOR_MTRC_HITS     color7:color-1",
  "COLOR_MTRC_VISITORS color8:color-1",
  "COLOR_MTRC_DATA     color7:color-1",
  "COLOR_MTRC_BW       color8:color-1",
  "COLOR_MTRC_AVGTS    color8:color-1",
  "COLOR_MTRC_CUMTS    color8:color-1",
  "COLOR_MTRC_MAXTS    color8:color-1",
  "COLOR_MTRC_PROT     color8:color-1",
  "COLOR_MTRC_MTHD     color7:color-1",
  "COLOR_MTRC_PERC     color0:color-1 bold",
  "COLOR_MTRC_PERC     color1:color-1 bold VISITORS",
  "COLOR_MTRC_PERC     color1:color-1 bold OS",
  "COLOR_MTRC_PERC     color1:color-1 bold BROWSERS",
  "COLOR_MTRC_PERC     color1:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_PERC_MAX color0:color-1 bold",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold VISITORS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold OS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold BROWSERS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold VISIT_TIMES",
  "COLOR_PANEL_COLS    color7:color-1",
  "COLOR_BARS          color7:color-1",
  "COLOR_ERROR         color7:color1",
  "COLOR_SELECTED      color7:color8",
  "COLOR_PANEL_ACTIVE  color0:color3",
  "COLOR_PANEL_HEADER  color0:color7",
  "COLOR_PANEL_DESC    color7:color-1",
  "COLOR_OVERALL_LBLS  color7:color-1 bold",
  "COLOR_OVERALL_VALS  color6:color-1 bold",
  "COLOR_OVERALL_PATH  color3:color-1",
  "COLOR_ACTIVE_LABEL  color4:color7",
  "COLOR_BG            color7:color-1",
  "COLOR_DEFAULT       color7:color-1",
  "COLOR_PROGRESS      color0:color6",
};

static const char *colors256_green[] = {
  "COLOR_MTRC_HITS     color7:color-1",
  "COLOR_MTRC_VISITORS color8:color-1",
  "COLOR_MTRC_DATA     color7:color-1",
  "COLOR_MTRC_BW       color8:color-1",
  "COLOR_MTRC_AVGTS    color8:color-1",
  "COLOR_MTRC_CUMTS    color8:color-1",
  "COLOR_MTRC_MAXTS    color8:color-1",
  "COLOR_MTRC_PROT     color8:color-1",
  "COLOR_MTRC_MTHD     color7:color-1",
  "COLOR_MTRC_PERC     color0:color-1 bold",
  "COLOR_MTRC_PERC     color1:color-1 bold VISITORS",
  "COLOR_MTRC_PERC     color1:color-1 bold OS",
  "COLOR_MTRC_PERC     color1:color-1 bold BROWSERS",
  "COLOR_MTRC_PERC     color1:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_PERC_MAX color0:color-1 bold",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold VISITORS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold OS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold BROWSERS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold VISIT_TIMES",
  "COLOR_PANEL_COLS    color7:color-1",
  "COLOR_BARS          color7:color-1",
  "COLOR_ERROR         color7:color1",
  "COLOR_SELECTED      color7:color8",
  "COLOR_PANEL_ACTIVE  color0:color3",
  "COLOR_PANEL_HEADER  color0:color35",
  "COLOR_PANEL_DESC    color7:color-1",
  "COLOR_OVERALL_LBLS  color7:color-1 bold",
  "COLOR_OVERALL_VALS  color6:color-1 bold",
  "COLOR_OVERALL_PATH  color3:color-1",
  "COLOR_ACTIVE_LABEL  color7:color35",
  "COLOR_BG            color7:color-1",
  "COLOR_DEFAULT       color7:color-1",
  "COLOR_PROGRESS      color0:color6",
};

static const char *colors8_mono[] = {
  "COLOR_MTRC_HITS     color7:color-1",
  "COLOR_MTRC_VISITORS color0:color-1 bold",
  "COLOR_MTRC_DATA     color7:color-1",
  "COLOR_MTRC_BW       color0:color-1 bold",
  "COLOR_MTRC_AVGTS    color0:color-1 bold",
  "COLOR_MTRC_CUMTS    color0:color-1 bold",
  "COLOR_MTRC_MAXTS    color0:color-1 bold",
  "COLOR_MTRC_PROT     color0:color-1 bold",
  "COLOR_MTRC_MTHD     color7:color-1 ",
  "COLOR_MTRC_PERC     color0:color-1 bold",
  "COLOR_MTRC_PERC     color1:color-1 bold VISITORS",
  "COLOR_MTRC_PERC     color1:color-1 bold OS",
  "COLOR_MTRC_PERC     color1:color-1 bold BROWSERS",
  "COLOR_MTRC_PERC     color1:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_PERC_MAX color0:color-1 bold",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold VISITORS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold OS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold BROWSERS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold VISIT_TIMES",
  "COLOR_PANEL_COLS    color7:color-1",
  "COLOR_BARS          color7:color-1",
  "COLOR_ERROR         color7:color1",
  "COLOR_SELECTED      color0:color7",
  "COLOR_PANEL_ACTIVE  color0:color3",
  "COLOR_PANEL_HEADER  color0:color7",
  "COLOR_PANEL_DESC    color7:color-1",
  "COLOR_OVERALL_LBLS  color7:color-1 bold",
  "COLOR_OVERALL_VALS  color6:color-1",
  "COLOR_OVERALL_PATH  color3:color-1",
  "COLOR_ACTIVE_LABEL  color4:color7",
  "COLOR_BG            color7:color-1",
  "COLOR_DEFAULT       color7:color-1",
  "COLOR_PROGRESS      color0:color6",
};

static const char *colors8_green[] = {
  "COLOR_MTRC_HITS     color7:color-1",
  "COLOR_MTRC_VISITORS color0:color-1 bold",
  "COLOR_MTRC_DATA     color7:color-1",
  "COLOR_MTRC_BW       color0:color-1 bold",
  "COLOR_MTRC_AVGTS    color0:color-1 bold",
  "COLOR_MTRC_CUMTS    color0:color-1 bold",
  "COLOR_MTRC_MAXTS    color0:color-1 bold",
  "COLOR_MTRC_PROT     color0:color-1 bold",
  "COLOR_MTRC_MTHD     color7:color-1 ",
  "COLOR_MTRC_PERC     color0:color-1 bold",
  "COLOR_MTRC_PERC     color1:color-1 bold VISITORS",
  "COLOR_MTRC_PERC     color1:color-1 bold OS",
  "COLOR_MTRC_PERC     color1:color-1 bold BROWSERS",
  "COLOR_MTRC_PERC     color1:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_PERC_MAX color0:color-1 bold",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold VISITORS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold OS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold BROWSERS",
  "COLOR_MTRC_PERC_MAX color3:color-1 bold VISIT_TIMES",
  "COLOR_PANEL_COLS    color7:color-1",
  "COLOR_BARS          color2:color-1",
  "COLOR_ERROR         color7:color1",
  "COLOR_SELECTED      color0:color7",
  "COLOR_PANEL_ACTIVE  color0:color3",
  "COLOR_PANEL_HEADER  color0:color2",
  "COLOR_PANEL_DESC    color7:color-1",
  "COLOR_OVERALL_LBLS  color7:color-1 bold",
  "COLOR_OVERALL_VALS  color6:color-1",
  "COLOR_OVERALL_PATH  color3:color-1",
  "COLOR_ACTIVE_LABEL  color0:color2",
  "COLOR_BG            color7:color-1",
  "COLOR_DEFAULT       color7:color-1",
  "COLOR_PROGRESS      color0:color6",
};

static const char *nocolors[] = {
  "COLOR_MTRC_HITS     color0:color-1",
  "COLOR_MTRC_VISITORS color0:color-1",
  "COLOR_MTRC_DATA     color0:color-1",
  "COLOR_MTRC_BW       color0:color-1",
  "COLOR_MTRC_AVGTS    color0:color-1",
  "COLOR_MTRC_CUMTS    color0:color-1",
  "COLOR_MTRC_MAXTS    color0:color-1",
  "COLOR_MTRC_PROT     color0:color-1",
  "COLOR_MTRC_MTHD     color0:color-1",
  "COLOR_MTRC_PERC     color0:color-1",
  "COLOR_MTRC_PERC_MAX color0:color-1",
  "COLOR_PANEL_COLS    color0:color-1",
  "COLOR_BARS          color0:color-1",
  "COLOR_ERROR         color0:color-1",
  "COLOR_SELECTED      color0:color-1 reverse",
  "COLOR_PANEL_ACTIVE  color0:color-1 reverse",
  "COLOR_PANEL_HEADER  color0:color-1 reverse",
  "COLOR_PANEL_DESC    color0:color-1",
  "COLOR_OVERALL_LBLS  color0:color-1",
  "COLOR_OVERALL_VALS  color0:color-1",
  "COLOR_OVERALL_PATH  color0:color-1",
  "COLOR_ACTIVE_LABEL  color0:color-1 reverse",
  "COLOR_BG            color0:color-1",
  "COLOR_DEFAULT       color0:color-1",
  "COLOR_PROGRESS      color0:color-1 reverse",
};

/* *INDENT-ON* */

/* Allocate memory for color elements */
static GColors *
new_gcolors (void)
{
  GColors *color = xcalloc (1, sizeof (GColors));
  color->module = -1;

  return color;
}

static GColorPair *
new_gcolorpair (void)
{
  GColorPair *pair = xcalloc (1, sizeof (GColorPair));
  /* Must be between 2 and COLOR_PAIRS-1.
   * Starts at 2 since COLOR_NORMAL has already been set */
  pair->idx = 2;

  return pair;
}

void
free_color_lists (void)
{
  if (pair_list)
    list_remove_nodes (pair_list);
  if (color_list)
    list_remove_nodes (color_list);
  color_list = NULL;
  pair_list = NULL;
}

void
set_normal_color (void)
{
  GColorPair *pair = new_gcolorpair ();
  GColors *color = new_gcolors ();

  pair->idx = 1;
  pair->fg = COLOR_WHITE;
  pair->bg = -1;

  color->pair = pair;
  color->item = COLOR_NORMAL;

  pair_list = list_create (pair);
  color_list = list_create (color);

  init_pair (pair->idx, pair->fg, pair->bg);
}

GColors *
color_overall_lbls (void)
{
  return get_color (COLOR_OVERALL_LBLS);
}

GColors *
color_overall_vals (void)
{
  return get_color (COLOR_OVERALL_VALS);
}

GColors *
color_overall_path (void)
{
  return get_color (COLOR_OVERALL_PATH);
}

GColors *
color_panel_header (void)
{
  return get_color (COLOR_PANEL_HEADER);
}

GColors *
color_panel_desc (void)
{
  return get_color (COLOR_PANEL_DESC);
}

GColors *
color_panel_active (void)
{
  return get_color (COLOR_PANEL_ACTIVE);
}

GColors *
color_selected (void)
{
  return get_color (COLOR_SELECTED);
}

GColors *
color_progress (void)
{
  return get_color (COLOR_PROGRESS);
}

GColors *
color_default (void)
{
  return get_color (COLOR_DEFAULT);
}

GColors *
color_error (void)
{
  return get_color (COLOR_ERROR);
}

static int
get_color_item_enum (const char *str)
{
  return str2enum (CSTM_COLORS, ARRAY_SIZE (CSTM_COLORS), str);
}

/* Extract color from the given config string.
 * On success, returns a color between -1 (terminal default
 * foreground/background) to 255.
 * On error, returns -2 on error */
static int
extract_color (char *color)
{
  char *sEnd;
  int col = 0;

  if (strncasecmp (color, "color", 5) != 0)
    return -2;

  color += 5;
  col = strtol (color, &sEnd, 10);
  if (color == sEnd || *sEnd != '\0' || errno == ERANGE)
    return -2;
  /* ensure used color is supported by the terminal */
  if (col > COLORS)
    FATAL ("Terminal doesn't support color: %d - max colors: %d", col, COLORS);

  return col;
}

static int
parse_bg_fg_color (GColorPair * pair, const char *value)
{
  char bgcolor[COLOR_STR_LEN] = "", fgcolor[COLOR_STR_LEN] = "";
  int ret = 0;

  if (sscanf (value, "%8[^:]:%8[^ ]", fgcolor, bgcolor) != 2)
    return 1;

  if ((pair->bg = extract_color (bgcolor)) == -2)
    ret = 1;

  if ((pair->fg = extract_color (fgcolor)) == -2)
    ret = 1;

  return ret;
}

static void
locate_attr_color (GColors * color, const char *attr)
{
  if (strstr (attr, "bold"))
    color->attr |= A_BOLD;
  if (strstr (attr, "underline"))
    color->attr |= A_UNDERLINE;
  if (strstr (attr, "normal"))
    color->attr |= A_NORMAL;
  if (strstr (attr, "reverse"))
    color->attr |= A_REVERSE;
  if (strstr (attr, "standout"))
    color->attr |= A_REVERSE;
  if (strstr (attr, "blink"))
    color->attr |= A_BLINK;
}

static int
parse_attr_color (GColors * color, const char *value)
{
  char *line, *ptr, *start;
  int ret = 0;

  line = xstrdup (value);

  start = strchr (line, ' ');
  if ((!start) || (!*(start + 1))) {
    LOG_DEBUG (("attempted to parse color attr: %s\n", value));
    goto clean;
  }

  start++;
  while (1) {
    if ((ptr = strpbrk (start, ", ")) != NULL)
      *ptr = 0;
    locate_attr_color (color, start);
    if (ptr == NULL)
      break;
    start = ptr + 1;
  }

clean:
  free (line);

  return ret;
}

static int
parse_module_color (GColors * color, const char *value)
{
  char *line = xstrdup (value), *p;

  p = strrchr (line, ' ');
  if (!p || !*(p + 1)) {
    LOG_DEBUG (("attempted to parse color module: %s\n", value));
    goto clean;
  }

  if ((color->module = get_module_enum (p + 1)) == -1)
    LOG_DEBUG (("attempted to parse color module: %s\n", value));

clean:
  free (line);

  return 0;
}

static int
find_color_in_list (void *data, void *color)
{
  GColors *new_color = color;
  GColors *old_color = data;

  if (old_color->item != new_color->item)
    return 0;
  if (old_color->module != new_color->module)
    return 0;
  return 1;
}

static int
find_pair_in_list (void *data, void *color)
{
  GColorPair *new_color = color;
  GColorPair *old_color = data;

  if (old_color->fg != new_color->fg)
    return 0;
  if (old_color->bg != new_color->bg)
    return 0;
  return 1;
}

static int
find_color_item_in_list (void *data, void *needle)
{
  GColors *color = data;
  GColorItem *item = needle;

  return color->item == (GColorItem) (*(int *) item) && color->module == -1;
}

static int
find_color_item_module_in_list (void *data, void *needle)
{
  GColors *color = data;
  GColors *item = needle;

  return color->item == item->item && color->module == item->module;
}

GColors *
get_color (GColorItem item)
{
  GColorItem normal = COLOR_NORMAL;
  GSLList *match = NULL;

  if ((match = list_find (color_list, find_color_item_in_list, &item)))
    return (GColors *) match->data;

  if ((match = list_find (color_list, find_color_item_in_list, &normal)))
    return (GColors *) match->data;

  /* should not get here */
  FATAL ("Unable to find color item %d", item);
}

GColors *
get_color_by_item_module (GColorItem item, GModule module)
{
  GColors *needle = new_gcolors (), *color = NULL;
  GSLList *match = NULL;

  needle->module = module;
  needle->item = item;

  /* find color for specific item/module pair */
  if ((match = list_find (color_list, find_color_item_module_in_list, needle)))
    color = match->data;

  /* attempt to find color by item (fallback) */
  if (!color)
    color = get_color (item);
  free (needle);

  return color;
}

static void
parse_color_line (GColorPair * pair, GColors * color, char *line)
{
  char *val;
  int item = 0;
  size_t idx;

  /* key */
  idx = strcspn (line, " \t");
  if (strlen (line) == idx)
    FATAL ("Malformed color key at line: %s", line);

  line[idx] = '\0';
  if ((item = get_color_item_enum (line)) == -1)
    FATAL ("Unable to find color key: %s", line);

  /* value */
  val = line + (idx + 1);
  idx = strspn (val, " \t");
  if (strlen (line) == idx)
    FATAL ("Malformed color value at line: %s", line);
  val = val + idx;

  /* get background/foreground color */
  if (parse_bg_fg_color (pair, val) == 1)
    FATAL ("Invalid bg/fg color pairs at: %s %s", line, val);

  if (parse_attr_color (color, val) == 1)
    FATAL ("Invalid color attrs at: %s %s", line, val);

  if (parse_module_color (color, val) == 1)
    FATAL ("Invalid color module at: %s %s", line, val);

  color->item = item;
}

static void
parse_color (char *line)
{
  GSLList *match = NULL;
  GColors *color = NULL;
  GColorPair *pair = NULL;

  color = new_gcolors ();
  pair = new_gcolorpair ();

  parse_color_line (pair, color, line);

  if (pair_list == NULL) {
    pair_list = list_create (pair);
  } else if ((match = list_find (pair_list, find_pair_in_list, pair))) {
    free (pair);
    pair = (GColorPair *) match->data;
  } else {
    pair->idx += list_count (pair_list);
    pair_list = list_insert_prepend (pair_list, pair);
  }
  color->pair = pair;

  if (color_list == NULL)
    color_list = list_create (color);
  else if (list_find (color_list, find_color_in_list, color))
    free (color);
  else
    color_list = list_insert_prepend (color_list, color);

  if (!match) {
    init_pair (color->pair->idx, color->pair->fg, color->pair->bg);
  }

  free (line);
}

static void
parse_colors (const char *colors[], size_t n)
{
  char *line;
  size_t i;

  for (i = 0; i < n; ++i) {
    line = strdup (colors[i]);
    /* did not find a valid format */
    if (strchr (line, ':') == NULL) {
      free (line);
      continue;
    }
    parse_color (line);
  }
}

static void
add_default_colors (void)
{
  if (COLORS < 8)
    parse_colors (nocolors, ARRAY_SIZE (nocolors));

  if (COLORS == 8 && COLORS <= 16 && conf.color_scheme == STD_GREEN)
    parse_colors (colors8_green, ARRAY_SIZE (colors8_green));
  else if (COLORS >= 8 && COLORS <= 16)
    parse_colors (colors8_mono, ARRAY_SIZE (colors8_mono));

  if (COLORS > 16 && conf.color_scheme == STD_GREEN)
    parse_colors (colors256_green, ARRAY_SIZE (colors256_green));
  else if (COLORS > 16)
    parse_colors (colors256_mono, ARRAY_SIZE (colors256_mono));
}

void
set_colors (int force)
{
  if (conf.color_idx > 0 && !force)
    parse_colors (conf.colors, conf.color_idx);
  else
    add_default_colors ();
}
