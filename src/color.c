/**
 * color.c -- functions related to custom color
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2020 Gerardo Orellana <hello @ goaccess.io>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

#include "error.h"
#include "gslist.h"
#include "util.h"
#include "xmalloc.h"

static GSLList *color_list = NULL;
static GSLList *pair_list = NULL;

/* *INDENT-OFF* */
static GEnum CSTM_COLORS[] = {
  {"COLOR_MTRC_HITS"              , COLOR_MTRC_HITS},
  {"COLOR_MTRC_VISITORS"          , COLOR_MTRC_VISITORS},
  {"COLOR_MTRC_HITS_PERC"         , COLOR_MTRC_HITS_PERC},
  {"COLOR_MTRC_VISITORS_PERC"     , COLOR_MTRC_VISITORS_PERC},
  {"COLOR_MTRC_BW"                , COLOR_MTRC_BW},
  {"COLOR_MTRC_AVGTS"             , COLOR_MTRC_AVGTS},
  {"COLOR_MTRC_CUMTS"             , COLOR_MTRC_CUMTS},
  {"COLOR_MTRC_MAXTS"             , COLOR_MTRC_MAXTS},
  {"COLOR_MTRC_PROT"              , COLOR_MTRC_PROT},
  {"COLOR_MTRC_MTHD"              , COLOR_MTRC_MTHD},
  {"COLOR_MTRC_DATA"              , COLOR_MTRC_DATA},
  {"COLOR_MTRC_HITS_PERC_MAX"     , COLOR_MTRC_HITS_PERC_MAX},
  {"COLOR_MTRC_VISITORS_PERC_MAX" , COLOR_MTRC_VISITORS_PERC_MAX},
  {"COLOR_PANEL_COLS"             , COLOR_PANEL_COLS},
  {"COLOR_BARS"                   , COLOR_BARS},
  {"COLOR_ERROR"                  , COLOR_ERROR},
  {"COLOR_SELECTED"               , COLOR_SELECTED},
  {"COLOR_PANEL_ACTIVE"           , COLOR_PANEL_ACTIVE},
  {"COLOR_PANEL_HEADER"           , COLOR_PANEL_HEADER},
  {"COLOR_PANEL_DESC"             , COLOR_PANEL_DESC},
  {"COLOR_OVERALL_LBLS"           , COLOR_OVERALL_LBLS},
  {"COLOR_OVERALL_VALS"           , COLOR_OVERALL_VALS},
  {"COLOR_OVERALL_PATH"           , COLOR_OVERALL_PATH},
  {"COLOR_ACTIVE_LABEL"           , COLOR_ACTIVE_LABEL},
  {"COLOR_BG"                     , COLOR_BG},
  {"COLOR_DEFAULT"                , COLOR_DEFAULT},
  {"COLOR_PROGRESS"               , COLOR_PROGRESS},
};

static const char *colors256_mono[] = {
  "COLOR_MTRC_HITS              color7:color-1",
  "COLOR_MTRC_VISITORS          color8:color-1",
  "COLOR_MTRC_DATA              color7:color-1",
  "COLOR_MTRC_BW                color8:color-1",
  "COLOR_MTRC_AVGTS             color8:color-1",
  "COLOR_MTRC_CUMTS             color8:color-1",
  "COLOR_MTRC_MAXTS             color8:color-1",
  "COLOR_MTRC_PROT              color8:color-1",
  "COLOR_MTRC_MTHD              color7:color-1",
  "COLOR_MTRC_HITS_PERC         color0:color-1 bold",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold VISITORS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold OS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold BROWSERS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_HITS_PERC_MAX     color0:color-1 bold",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold VISITORS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold OS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold BROWSERS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_VISITORS_PERC     color0:color-1 bold",
  "COLOR_MTRC_VISITORS_PERC_MAX color0:color-1 bold",
  "COLOR_PANEL_COLS             color7:color-1",
  "COLOR_BARS                   color7:color-1",
  "COLOR_ERROR                  color7:color1",
  "COLOR_SELECTED               color7:color8",
  "COLOR_PANEL_ACTIVE           color0:color3",
  "COLOR_PANEL_HEADER           color0:color7",
  "COLOR_PANEL_DESC             color7:color-1",
  "COLOR_OVERALL_LBLS           color7:color-1 bold",
  "COLOR_OVERALL_VALS           color6:color-1 bold",
  "COLOR_OVERALL_PATH           color3:color-1",
  "COLOR_ACTIVE_LABEL           color4:color7",
  "COLOR_BG                     color7:color-1",
  "COLOR_DEFAULT                color7:color-1",
  "COLOR_PROGRESS               color0:color6",
};

static const char *colors256_green[] = {
  "COLOR_MTRC_HITS              color7:color-1",
  "COLOR_MTRC_VISITORS          color8:color-1",
  "COLOR_MTRC_DATA              color7:color-1",
  "COLOR_MTRC_BW                color8:color-1",
  "COLOR_MTRC_AVGTS             color8:color-1",
  "COLOR_MTRC_CUMTS             color8:color-1",
  "COLOR_MTRC_MAXTS             color8:color-1",
  "COLOR_MTRC_PROT              color8:color-1",
  "COLOR_MTRC_MTHD              color7:color-1",
  "COLOR_MTRC_HITS_PERC         color0:color-1 bold",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold VISITORS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold OS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold BROWSERS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_HITS_PERC_MAX     color0:color-1 bold",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold VISITORS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold OS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold BROWSERS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_VISITORS_PERC     color0:color-1 bold",
  "COLOR_MTRC_VISITORS_PERC_MAX color0:color-1 bold",
  "COLOR_PANEL_COLS             color7:color-1",
  "COLOR_BARS                   color7:color-1",
  "COLOR_ERROR                  color7:color1",
  "COLOR_SELECTED               color7:color8",
  "COLOR_PANEL_ACTIVE           color0:color3",
  "COLOR_PANEL_HEADER           color0:color35",
  "COLOR_PANEL_DESC             color7:color-1",
  "COLOR_OVERALL_LBLS           color7:color-1 bold",
  "COLOR_OVERALL_VALS           color6:color-1 bold",
  "COLOR_OVERALL_PATH           color3:color-1",
  "COLOR_ACTIVE_LABEL           color7:color35",
  "COLOR_BG                     color7:color-1",
  "COLOR_DEFAULT                color7:color-1",
  "COLOR_PROGRESS               color0:color6",
};

static const char *colors256_monokai[] = {
  "COLOR_MTRC_HITS              color197:color-1",
  "COLOR_MTRC_VISITORS          color148:color-1",
  "COLOR_MTRC_DATA              color7:color-1",
  "COLOR_MTRC_BW                color81:color-1",
  "COLOR_MTRC_AVGTS             color247:color-1",
  "COLOR_MTRC_CUMTS             color95:color-1",
  "COLOR_MTRC_MAXTS             color186:color-1",
  "COLOR_MTRC_PROT              color141:color-1",
  "COLOR_MTRC_MTHD              color81:color-1",
  "COLOR_MTRC_HITS_PERC         color186:color-1",
  "COLOR_MTRC_HITS_PERC         color186:color-1 VISITORS",
  "COLOR_MTRC_HITS_PERC         color186:color-1 OS",
  "COLOR_MTRC_HITS_PERC         color186:color-1 BROWSERS",
  "COLOR_MTRC_HITS_PERC         color186:color-1 VISIT_TIMES",
  "COLOR_MTRC_HITS_PERC_MAX     color208:color-1",
  "COLOR_MTRC_HITS_PERC_MAX     color208:color-1 VISITORS",
  "COLOR_MTRC_HITS_PERC_MAX     color208:color-1 OS",
  "COLOR_MTRC_HITS_PERC_MAX     color208:color-1 BROWSERS",
  "COLOR_MTRC_HITS_PERC_MAX     color208:color-1 VISIT_TIMES",
  "COLOR_MTRC_VISITORS_PERC     color187:color-1",
  "COLOR_MTRC_VISITORS_PERC_MAX color208:color-1",
  "COLOR_PANEL_COLS             color242:color-1",
  "COLOR_BARS                   color186:color-1",
  "COLOR_ERROR                  color231:color197",
  "COLOR_SELECTED               color0:color215",
  "COLOR_PANEL_ACTIVE           color7:color240",
  "COLOR_PANEL_HEADER           color7:color237",
  "COLOR_PANEL_DESC             color242:color-1",
  "COLOR_OVERALL_LBLS           color251:color-1",
  "COLOR_OVERALL_VALS           color148:color-1",
  "COLOR_OVERALL_PATH           color186:color-1",
  "COLOR_ACTIVE_LABEL           color7:color237",
  "COLOR_BG                     color7:color-1",
  "COLOR_DEFAULT                color7:color-1",
  "COLOR_PROGRESS               color7:color141",
};

static const char *colors8_mono[] = {
  "COLOR_MTRC_HITS              color7:color-1",
  "COLOR_MTRC_VISITORS          color0:color-1 bold",
  "COLOR_MTRC_DATA              color7:color-1",
  "COLOR_MTRC_BW                color0:color-1 bold",
  "COLOR_MTRC_AVGTS             color0:color-1 bold",
  "COLOR_MTRC_CUMTS             color0:color-1 bold",
  "COLOR_MTRC_MAXTS             color0:color-1 bold",
  "COLOR_MTRC_PROT              color0:color-1 bold",
  "COLOR_MTRC_MTHD              color7:color-1 ",
  "COLOR_MTRC_HITS_PERC         color0:color-1 bold",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold VISITORS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold OS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold BROWSERS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_HITS_PERC_MAX     color0:color-1 bold",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold VISITORS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold OS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold BROWSERS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_VISITORS_PERC     color0:color-1 bold",
  "COLOR_MTRC_VISITORS_PERC_MAX color0:color-1 bold",
  "COLOR_PANEL_COLS             color7:color-1",
  "COLOR_BARS                   color7:color-1",
  "COLOR_ERROR                  color7:color1",
  "COLOR_SELECTED               color0:color7",
  "COLOR_PANEL_ACTIVE           color0:color3",
  "COLOR_PANEL_HEADER           color0:color7",
  "COLOR_PANEL_DESC             color7:color-1",
  "COLOR_OVERALL_LBLS           color7:color-1 bold",
  "COLOR_OVERALL_VALS           color6:color-1",
  "COLOR_OVERALL_PATH           color3:color-1",
  "COLOR_ACTIVE_LABEL           color4:color7",
  "COLOR_BG                     color7:color-1",
  "COLOR_DEFAULT                color7:color-1",
  "COLOR_PROGRESS               color0:color6",
};

static const char *colors8_green[] = {
  "COLOR_MTRC_HITS              color7:color-1",
  "COLOR_MTRC_VISITORS          color0:color-1 bold",
  "COLOR_MTRC_DATA              color7:color-1",
  "COLOR_MTRC_BW                color0:color-1 bold",
  "COLOR_MTRC_AVGTS             color0:color-1 bold",
  "COLOR_MTRC_CUMTS             color0:color-1 bold",
  "COLOR_MTRC_MAXTS             color0:color-1 bold",
  "COLOR_MTRC_PROT              color0:color-1 bold",
  "COLOR_MTRC_MTHD              color7:color-1 ",
  "COLOR_MTRC_HITS_PERC         color0:color-1 bold",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold VISITORS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold OS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold BROWSERS",
  "COLOR_MTRC_HITS_PERC         color1:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_HITS_PERC_MAX     color0:color-1 bold",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold VISITORS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold OS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold BROWSERS",
  "COLOR_MTRC_HITS_PERC_MAX     color3:color-1 bold VISIT_TIMES",
  "COLOR_MTRC_VISITORS_PERC     color0:color-1 bold",
  "COLOR_MTRC_VISITORS_PERC_MAX color0:color-1 bold",
  "COLOR_PANEL_COLS             color7:color-1",
  "COLOR_BARS                   color2:color-1",
  "COLOR_ERROR                  color7:color1",
  "COLOR_SELECTED               color0:color7",
  "COLOR_PANEL_ACTIVE           color0:color3",
  "COLOR_PANEL_HEADER           color0:color2",
  "COLOR_PANEL_DESC             color7:color-1",
  "COLOR_OVERALL_LBLS           color7:color-1 bold",
  "COLOR_OVERALL_VALS           color6:color-1",
  "COLOR_OVERALL_PATH           color3:color-1",
  "COLOR_ACTIVE_LABEL           color0:color2",
  "COLOR_BG                     color7:color-1",
  "COLOR_DEFAULT                color7:color-1",
  "COLOR_PROGRESS               color0:color6",
};

static const char *nocolors[] = {
  "COLOR_MTRC_HITS              color0:color-1",
  "COLOR_MTRC_VISITORS          color0:color-1",
  "COLOR_MTRC_DATA              color0:color-1",
  "COLOR_MTRC_BW                color0:color-1",
  "COLOR_MTRC_AVGTS             color0:color-1",
  "COLOR_MTRC_CUMTS             color0:color-1",
  "COLOR_MTRC_MAXTS             color0:color-1",
  "COLOR_MTRC_PROT              color0:color-1",
  "COLOR_MTRC_MTHD              color0:color-1",
  "COLOR_MTRC_HITS_PERC         color0:color-1",
  "COLOR_MTRC_HITS_PERC_MAX     color0:color-1",
  "COLOR_MTRC_VISITORS_PERC     color0:color-1",
  "COLOR_MTRC_VISITORS_PERC_MAX color0:color-1",
  "COLOR_PANEL_COLS             color0:color-1",
  "COLOR_BARS                   color0:color-1",
  "COLOR_ERROR                  color0:color-1",
  "COLOR_SELECTED               color0:color-1 reverse",
  "COLOR_PANEL_ACTIVE           color0:color-1 reverse",
  "COLOR_PANEL_HEADER           color0:color-1 reverse",
  "COLOR_PANEL_DESC             color0:color-1",
  "COLOR_OVERALL_LBLS           color0:color-1",
  "COLOR_OVERALL_VALS           color0:color-1",
  "COLOR_OVERALL_PATH           color0:color-1",
  "COLOR_ACTIVE_LABEL           color0:color-1 reverse",
  "COLOR_BG                     color0:color-1",
  "COLOR_DEFAULT                color0:color-1",
  "COLOR_PROGRESS               color0:color-1 reverse",
};

/* *INDENT-ON* */

/* Allocate memory for color elements */
static GColors *
new_gcolors (void) {
  GColors *color = xcalloc (1, sizeof (GColors));
  color->module = -1;

  return color;
}

/* Allocate memory for a color element properties */
static GColorPair *
new_gcolorpair (void) {
  GColorPair *pair = xcalloc (1, sizeof (GColorPair));
  /* Must be between 2 and COLOR_PAIRS-1.
   * Starts at 2 since COLOR_NORMAL has already been set */
  pair->idx = 2;

  return pair;
}

/* Free malloc'd memory for color elements */
void
free_color_lists (void) {
  if (pair_list)
    list_remove_nodes (pair_list);
  if (color_list)
    list_remove_nodes (color_list);
  color_list = NULL;
  pair_list = NULL;
}

/* Set a default color - COLOR_NORMAL, this will be used if
 * no colors are supported by the terminal */
void
set_normal_color (void) {
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

/* Get color properties for COLOR_OVERALL_LBLS */
GColors *
color_overall_lbls (void) {
  return get_color (COLOR_OVERALL_LBLS);
}

/* Get color properties for COLOR_OVERALL_VALS */
GColors *
color_overall_vals (void) {
  return get_color (COLOR_OVERALL_VALS);
}

/* Get color properties for COLOR_OVERALL_PATH */
GColors *
color_overall_path (void) {
  return get_color (COLOR_OVERALL_PATH);
}

/* Get color properties for COLOR_PANEL_HEADER */
GColors *
color_panel_header (void) {
  return get_color (COLOR_PANEL_HEADER);
}

/* Get color properties for COLOR_PANEL_DESC */
GColors *
color_panel_desc (void) {
  return get_color (COLOR_PANEL_DESC);
}

/* Get color properties for COLOR_PANEL_ACTIVE*/
GColors *
color_panel_active (void) {
  return get_color (COLOR_PANEL_ACTIVE);
}

/* Get color properties for COLOR_SELECTED */
GColors *
color_selected (void) {
  return get_color (COLOR_SELECTED);
}

/* Get color properties for COLOR_PROGRESS */
GColors *
color_progress (void) {
  return get_color (COLOR_PROGRESS);
}

/* Get color properties for COLOR_DEFAULT */
GColors *
color_default (void) {
  return get_color (COLOR_DEFAULT);
}

/* Get color properties for COLOR_ERROR */
GColors *
color_error (void) {
  return get_color (COLOR_ERROR);
}

/* Get the enumerated color given its equivalent color string.
 *
 * On error, -1 is returned.
 * On success, the enumerated color is returned. */
static int
get_color_item_enum (const char *str) {
  return str2enum (CSTM_COLORS, ARRAY_SIZE (CSTM_COLORS), str);
}

/* Extract color number from the given config string.
 *
 * On error, -2 is returned. If color is greater than max colors, it aborts.
 * On success, the color number is returned. */
static int
extract_color (char *color) {
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

/* Assign the background and foreground color number from the given
 * config string to GColorPair.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
parse_bg_fg_color (GColorPair * pair, const char *value) {
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

/* Assign color attributes from the given config string to GColors. */
static void
locate_attr_color (GColors * color, const char *attr) {
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

/* Parse color attributes from the given config string.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
parse_attr_color (GColors * color, const char *value) {
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

/* Parse color module from the given config string.
 *
 * On error, 1 is returned.
 * On success, 0 is returned. */
static int
parse_module_color (GColors * color, const char *value) {
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

/* Find a color by item and module attributes on the list of already
 * parsed colors.
 *
 * If color exists, 1 is returned.
 * If color does not exist, 1 is returned. */
static int
find_color_in_list (void *data, void *color) {
  GColors *new_color = color;
  GColors *old_color = data;

  if (old_color->item != new_color->item)
    return 0;
  if (old_color->module != new_color->module)
    return 0;
  return 1;
}

/* Find a color by foreground and background attributes on the list of
 * already parsed colors.
 *
 * If color exists, 1 is returned.
 * If color does not exist, 1 is returned. */
static int
find_pair_in_list (void *data, void *color) {
  GColorPair *new_color = color;
  GColorPair *old_color = data;

  if (old_color->fg != new_color->fg)
    return 0;
  if (old_color->bg != new_color->bg)
    return 0;
  return 1;
}

/* Compare a color item (GColorItem) that has no module with the given needle
 * item.
 *
 * If the items match and with no module, 1 is returned.
 * If condition is not satisfied, 0 is returned. */
static int
find_color_item_in_list (void *data, void *needle) {
  GColors *color = data;
  GColorItem *item = needle;

  return color->item == (GColorItem) (*(int *) item) && color->module == -1;
}

/* Compare a color item (GColorItem) and module with the given needle item.
 *
 * If the items match and with no module, 1 is returned.
 * If condition is not satisfied, 0 is returned. */
static int
find_color_item_module_in_list (void *data, void *needle) {
  GColors *color = data;
  GColors *item = needle;

  return color->item == item->item && color->module == item->module;
}

/* Get color item properties given an item (enumerated).
 *
 * On error, it aborts.
 * On success, the color item properties are returned, or NULL if no match
 * found. */
GColors *
get_color (GColorItem item) {
  GColorItem normal = COLOR_NORMAL;
  GSLList *match = NULL;

  if ((match = list_find (color_list, find_color_item_in_list, &item)))
    return (GColors *) match->data;

  if ((match = list_find (color_list, find_color_item_in_list, &normal)))
    return (GColors *) match->data;

  /* should not get here */
  FATAL ("Unable to find color item %d", item);
}

/* Get color item properties given an item (enumerated) and its module.
 *
 * On error, it aborts.
 * On success, the color item properties are returned, or NULL if no match
 * found. */
GColors *
get_color_by_item_module (GColorItem item, GModule module) {
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

/* Parse a color definition line from the config file.
 *
 * On error, it aborts.
 * On success, the color properties are assigned */
static void
parse_color_line (GColorPair * pair, GColors * color, char *line) {
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
  if (strlen (val) == idx)
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

/* Attempt to prepend the given color on our color linked list.
 *
 * On error, or if color already exists, the given color is freed.
 * On success, or if not color found, store color properties */
static void
prepend_color (GColors ** color) {
  /* create a list of colors if one does not exist */
  if (color_list == NULL) {
    color_list = list_create (*color);
  }
  /* attempt to find the given color data type (by item and attributes) in
   * our color list */
  else if (list_find (color_list, find_color_in_list, *color)) {
    /* if found, free the recently malloc'd color data type and use
     * existing color */
    free (*color);
    *color = NULL;
  } else {
    /* not a dup, so insert the new color in our color list */
    color_list = list_insert_prepend (color_list, *color);
  }
}

/* Parse a color definition line from the config file and store it on a signle
 * linked-list.
 *
 * On error, it aborts.
 * On success, the color properties are stored */
static void
parse_color (char *line) {
  GSLList *match = NULL;
  GColors *color = NULL;
  GColorPair *pair = NULL;

  color = new_gcolors ();
  pair = new_gcolorpair ();

  /* extract a color pair and color attributes from the given config line */
  parse_color_line (pair, color, line);

  /* create a pair color list if one doesn't exist */
  if (pair_list == NULL) {
    pair_list = list_create (pair);
  }
  /* attempt to find the given color pair in our pair list */
  else if ((match = list_find (pair_list, find_pair_in_list, pair))) {
    /* pair found, use new pair and free existing one */
    free (pair);
    pair = (GColorPair *) match->data;
  }
  /* pair not found, use it then */
  else {
    pair->idx += list_count (pair_list);
    pair_list = list_insert_prepend (pair_list, pair);
  }
  /* set color pair */
  color->pair = pair;
  prepend_color (&color);

  /* if no color pair was found, then we init the color pair */
  if (!match && color)
    init_pair (color->pair->idx, color->pair->fg, color->pair->bg);

  free (line);
}

/* Iterate over all color definitions in the config file.
 *
 * On error, it aborts.
 * On success, the color properties are parsed and stored */
static void
parse_colors (const char *colors[], size_t n) {
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

/* Use default color definitions if necessary. */
static void
add_default_colors (void) {
  /* no colors */
  if (COLORS < 8)
    parse_colors (nocolors, ARRAY_SIZE (nocolors));

  /* 256 colors, and no color scheme set or set to monokai */
  if (COLORS == 256 && (!conf.color_scheme || conf.color_scheme == MONOKAI))
    parse_colors (colors256_monokai, ARRAY_SIZE (colors256_monokai));
  /* otherwise use 16 colors scheme */
  else if (COLORS > 16) {
    if (conf.color_scheme == STD_GREEN)
      parse_colors (colors256_green, ARRAY_SIZE (colors256_green));
    else
      parse_colors (colors256_mono, ARRAY_SIZE (colors256_mono));
  }

  /* 8 colors */
  if (COLORS >= 8 && COLORS <= 16) {
    if (conf.color_scheme == STD_GREEN)
      parse_colors (colors8_green, ARRAY_SIZE (colors8_green));
    else
      parse_colors (colors8_mono, ARRAY_SIZE (colors8_mono));
  }
}

/* Entry point to parse color definitions or use default colors */
void
set_colors (int force) {
  errno = 0;
  if (conf.color_idx > 0 && !force)
    parse_colors (conf.colors, conf.color_idx);
  else
    add_default_colors ();
}
