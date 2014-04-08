/**
 * output.c -- output to the standard output stream
 * Copyright (C) 2009-2014 by Gerardo Orellana <goaccess@prosoftcorp.com>
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

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <ctype.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include "output.h"

#ifdef HAVE_LIBTOKYOCABINET
#include "tcabinet.h"
#elif HAVE_LIBGLIB_2_0
#include "glibht.h"
#endif

#include "commons.h"
#include "error.h"
#include "settings.h"
#include "ui.h"
#include "util.h"
#include "xmalloc.h"

/* sanitize output with html entities for special chars */
static void
clean_output (FILE * fp, char *s)
{
  while (*s) {
    switch (*s) {
     case '\'':
       fprintf (fp, "&#39;");
       break;
     case '"':
       fprintf (fp, "&#34;");
       break;
     case '&':
       fprintf (fp, "&amp;");
       break;
     case '<':
       fprintf (fp, "&lt;");
       break;
     case '>':
       fprintf (fp, "&gt;");
       break;
     case ' ':
       fprintf (fp, "&nbsp;");
       break;
     default:
       fputc (*s, fp);
       break;
    }
    s++;
  }
}

/* *INDENT-OFF* */
static void
print_html_header (FILE * fp, char *now)
{
  fprintf (fp, "<!DOCTYPE html>\n");
  fprintf (fp, "<html lang=\"en\"><head>\n");
  fprintf (fp, "<title>Server Statistics - %s</title>\n", now);
  fprintf (fp, "<meta charset=\"UTF-8\" />");
  fprintf (fp, "<meta name=\"robots\" content=\"noindex, nofollow\" />");

  fprintf (fp, "<script type=\"text/javascript\">\n");
  fprintf (fp, "function t(c){for(var ");
  fprintf (fp, "b=c.parentNode.parentNode.parentNode.parentNode.");
  fprintf (fp, "getElementsByTagName('tr'),a=0;a<b.length;a++)");
  fprintf (fp, "'hide'==b[a].className?(b[a].className='show',");
  fprintf (fp, "c.innerHTML='▼'):'show'==b[a].className&&");
  fprintf (fp, "(b[a].className='hide',c.innerHTML='◀')};");
  fprintf (fp, "function a(c){var b=c.parentNode.parentNode.nextSibling;");
  fprintf (fp, "while(b && b.nodeType != 1) b=b.nextSibling;");
  fprintf (fp, "'agent-hide'==b.className?(b.className='a-show',");
  fprintf (fp, "c.innerHTML='▼'):'a-show'==b.className&&");
  fprintf (fp, "(b.className='agent-hide',c.innerHTML='▶')};");
  fprintf (fp, "</script>\n");

  fprintf (fp, "<style type=\"text/css\">");
  fprintf (fp,
  "html {"
  "    font-size: 100%%;"
  "    -ms-text-size-adjust: 100%%;"
  "    -webkit-text-size-adjust: 100%%;"
  "}"
  "html {"
  "    font-family: sans-serif"
  "}"
  "body {"
  "    font-size: 80%%;"
  "    color: #777;"
  "    margin: 0;"
  "}"
  "a:focus {"
  "    outline: thin dotted"
  "}"
  "a:active,"
  "a:hover {"
  "    outline: 0"
  "}"
  "p {"
  "    margin: 0 0 1em 0"
  "}"
  "ul {"
  "    margin: 1em 0"
  "}"
  "ul {"
  "    padding: 0 0 0 40px"
  "}"
  "table {"
  "    border-collapse: collapse;"
  "    border-spacing: 0;"
  "}"
  "h2 {"
  "    font-weight: 700;"
  "    color: #4b4b4b;"
  "    font-size: 1.2em;"
  "    margin: .83em 0 .20em 0;"
  "}"
  ".agent-hide,"
  ".hide {"
  "    display: none"
  "}"
  ".r,"
  ".s {"
  "    cursor: pointer"
  "}"
  ".r {"
  "    float: right"
  "}"
  "thead th {"
  "    text-align: center"
  "}"
  ".max {"
  "    color: #D20B2C;"
  "    font-weight: 700;"
  "}"
  "#layout {"
  "    padding-left: 200px;"
  "    left: 0;"
  "}"
  ".l-box {"
  "    padding: 0 1.3em 1.3em 1.3em"
  "}"
  ".graph {"
  "    height: 1.529411765em;"
  "    margin-bottom: .470588235em;"
  "    overflow: hidden;"
  "    background-color: #e5e5e5;"
  "    border-radius: .071428571em;"
  "    text-align: center;"
  "}"
  ".graph .bar {"
  "    -moz-box-sizing: border-box;"
  "    -webkit-box-sizing: border-box;"
  "    background-color: #777;"
  "    border: 1px solid #FFF;"
  "    box-sizing: border-box;"
  "    color: #fff;"
  "    float: left;"
  "    height: 100%%;"
  "    outline: 1px solid #777;"
  "    width: 0;"
  "}"
  ".graph .light {"
  "    background-color: #BBB"
  "}"
  "#menu {"
  "    -webkit-overflow-scroll: touch;"
  "    -webkit-transition: left 0.75s, -webkit-transform 0.75s;"
  "    background: #242424;"
  "    border-right: 1px solid #3E444C;"
  "    bottom: 0;"
  "    left: 200px;"
  "    margin-left: -200px;"
  "    outline: 1px solid #101214;"
  "    overflow-y: auto;"
  "    position: fixed;"
  "    text-shadow: 0px -1px 0px #000;"
  "    top: 0;"
  "    transition: left 0.75s, -webkit-transform 0.75s, transform 0.75s;"
  "    width: 200px;"
  "    z-index: 1000;"
  "}"
  "#menu a {"
  "    border: 0;"
  "    border-bottom: 1px solid #111;"
  "    box-shadow: 0 1px 0 #383838;"
  "    color: #999;"
  "    padding: .6em 0 .6em .6em;"
  "    white-space: normal;"
  "}"
  "#menu p {"
  "    color: #eee;"
  "    padding: .6em;"
  "    font-size: 85%%;"
  "}"
  "#menu .pure-menu-open {"
  "    background: transparent;"
  "    border: 0;"
  "}"
  "#menu .pure-menu ul {"
  "    border: 0;"
  "    background: transparent;"
  "}"
  "#menu .pure-menu li a:hover,"
  "#menu .pure-menu li a:focus {"
  "    background: #333"
  "}"
  "#menu .pure-menu-heading:hover,"
  "#menu .pure-menu-heading:focus {"
  "    color: #999"
  "}"
  "#menu .pure-menu-heading {"
  "    color: #FFF;"
  "    font-size: 110%%;"
  "    font-weight: bold;"
  "}"
  ".pure-u {"
  "    display: inline-block;"
  "    *display: inline;"
  "    zoom: 1;"
  "    letter-spacing: normal;"
  "    word-spacing: normal;"
  "    vertical-align: top;"
  "    text-rendering: auto;"
  "}"
  ".pure-u-1 {"
  "    display: inline-block;"
  "    *display: inline;"
  "    zoom: 1;"
  "    letter-spacing: normal;"
  "    word-spacing: normal;"
  "    vertical-align: top;"
  "    text-rendering: auto;"
  "}"
  ".pure-u-1 {"
  "    width: 100%%"
  "}"
  ".pure-g-r {"
  "    letter-spacing: -.31em;"
  "    *letter-spacing: normal;"
  "    *word-spacing: -.43em;"
  "    font-family: sans-serif;"
  "    display: -webkit-flex;"
  "    -webkit-flex-flow: row wrap;"
  "    display: -ms-flexbox;"
  "    -ms-flex-flow: row wrap;"
  "}"
  ".pure-g-r {"
  "    word-spacing: -.43em"
  "}"
  ".pure-g-r [class *=pure-u] {"
  "    font-family: sans-serif"
  "}"
  "@media (max-width:480px) { "
  "    .pure-g-r>.pure-u,"
  "    .pure-g-r>[class *=pure-u-] {"
  "        width: 100%%"
  "    }"
  "}"
  "@media (max-width:767px) { "
  "    .pure-g-r>.pure-u,"
  "    .pure-g-r>[class *=pure-u-] {"
  "        width: 100%%"
  "    }"
  "}"
  ".pure-menu ul {"
  "    position: absolute;"
  "    visibility: hidden;"
  "}"
  ".pure-menu.pure-menu-open {"
  "    visibility: visible;"
  "    z-index: 2;"
  "    width: 100%%;"
  "}"
  ".pure-menu ul {"
  "    left: -10000px;"
  "    list-style: none;"
  "    margin: 0;"
  "    padding: 0;"
  "    top: -10000px;"
  "    z-index: 1;"
  "}"
  ".pure-menu>ul {"
  "    position: relative"
  "}"
  ".pure-menu-open>ul {"
  "    left: 0;"
  "    top: 0;"
  "    visibility: visible;"
  "}"
  ".pure-menu-open>ul:focus {"
  "    outline: 0"
  "}"
  ".pure-menu li {"
  "    position: relative"
  "}"
  ".pure-menu a,"
  ".pure-menu .pure-menu-heading {"
  "    display: block;"
  "    color: inherit;"
  "    line-height: 1.5em;"
  "    padding: 5px 20px;"
  "    text-decoration: none;"
  "    white-space: nowrap;"
  "}"
  ".pure-menu li a {"
  "    padding: 5px 20px"
  "}"
  ".pure-menu.pure-menu-open {"
  "    background: #fff;"
  "    border: 1px solid #b7b7b7;"
  "}"
  ".pure-menu a {"
  "    border: 1px solid transparent;"
  "    border-left: 0;"
  "    border-right: 0;"
  "}"
  ".pure-menu a {"
  "    color: #777"
  "}"
  ".pure-menu li a:hover,"
  ".pure-menu li a:focus {"
  "    background: #eee"
  "}"
  ".pure-menu .pure-menu-heading {"
  "    color: #565d64;"
  "    font-size: 90%%;"
  "    margin-top: .5em;"
  "    border-bottom-width: 1px;"
  "    border-bottom-style: solid;"
  "    border-bottom-color: #dfdfdf;"
  "}"
  ".pure-table {"
  "    border-collapse: collapse;"
  "    border-spacing: 0;"
  "    empty-cells: show;"
  "    border: 1px solid #cbcbcb;"
  "}"
  ".pure-table td,"
  ".pure-table th {"
  "    border-left: 1px solid #cbcbcb;"
  "    border-width: 0 0 0 1px;"
  "    font-size: inherit;"
  "    margin: 0;"
  "    overflow: visible;"
  "    padding: 6px 12px;"
  "}"
  ".pure-table td:first-child,"
  ".pure-table th:first-child {"
  "    border-left-width: 0"
  "}"
  ".pure-table td:last-child {"
  "    white-space: normal;"
  "    width: auto;"
  "    word-break: break-all;"
  "    word-wrap: break-word;"
  "}"
  ".pure-table thead {"
  "    background: #242424;"
  "    color: #FFF;"
  "    text-align: left;"
  "    text-shadow: 0px -1px 0px #000;"
  "    vertical-align: bottom;"
  "}"
  ".pure-table td {"
  "    background-color: transparent"
  "}"
  ".pure-table tbody tr:hover,"
  ".pure-table-striped tr:nth-child(2n-1) td {"
  "    background-color: #f2f2f2"
  "}"
  "@media (max-width: 974px) {"
  "    #layout {"
  "        position: relative;"
  "        padding-left: 0;"
  "    }"
  "    #layout.active {"
  "        position: relative;"
  "        left: 200px;"
  "    }"
  "    #layout.active #menu {"
  "        left: 200px;"
  "        width: 200px;"
  "    }"
  "    #menu {"
  "        left: 0"
  "    }"
  "    .pure-menu-link {"
  "        position: fixed;"
  "        left: 0;"
  "        display: block;"
  "    }"
  "    #layout.active .pure-menu-link {"
  "        left: 200px"
  "    }"
  "}"
  );

  fprintf (fp, "</style>\n");
  fprintf (fp, "</head>\n");
  fprintf (fp, "<body>\n");

  fprintf (fp, "<div class=\"pure-g-r\" id=\"layout\">");
}
/* *INDENT-ON* */

static void
print_html_footer (FILE * fp)
{
  fprintf (fp, "</div> <!-- l-box -->\n");
  fprintf (fp, "</div> <!-- main -->\n");
  fprintf (fp, "</div> <!-- layout -->\n");
  fprintf (fp, "</body>\n");
  fprintf (fp, "</html>");
}

static void
print_html_h2 (FILE * fp, const char *title, const char *id)
{
  if (id)
    fprintf (fp, "<h2 id=\"%s\">%s</h2>", id, title);
  else
    fprintf (fp, "<h2>%s</h2>", title);
}

static void
print_p (FILE * fp, const char *paragraph)
{
  fprintf (fp, "<p>%s</p>", paragraph);
}

static void
print_html_begin_table (FILE * fp)
{
  fprintf (fp, "<table class=\"pure-table\">\n");
}

static void
print_html_end_table (FILE * fp)
{
  fprintf (fp, "</table>\n");
}

static void
print_html_begin_thead (FILE * fp)
{
  fprintf (fp, "<thead>\n");
}

static void
print_html_end_thead (FILE * fp)
{
  fprintf (fp, "</thead>\n");
}

static void
print_html_begin_tbody (FILE * fp)
{
  fprintf (fp, "<tbody>\n");
}

static void
print_html_end_tbody (FILE * fp)
{
  fprintf (fp, "</tbody>\n");
}

static void
print_html_begin_tr (FILE * fp, int hide)
{
  if (hide)
    fprintf (fp, "<tr class=\"hide\">");
  else
    fprintf (fp, "<tr>");
}

static void
print_html_end_tr (FILE * fp)
{
  fprintf (fp, "</tr>");
}

#ifdef HAVE_LIBGEOIP
static void
print_html_sub_geolocation (FILE * fp, GSubList * sub_list, int process)
{
  char *data, *name = NULL;
  float percent;
  GSubItem *iter;
  int hits;

  for (iter = sub_list->head; iter; iter = iter->next) {
    hits = iter->hits;
    data = (char *) iter->data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    name = xmalloc (snprintf (NULL, 0, "—&nbsp;%s", data) + 1);
    sprintf (name, "—&nbsp;%s", data);

    print_html_begin_tr (fp, 1);
    fprintf (fp, "<td>%d</td>", hits);
    fprintf (fp, "<td>%4.2f%%</td>", percent);

    fprintf (fp, "<td>%s</td>", name);

    print_html_end_tr (fp);
    free (name);
  }
}
#endif

#ifdef HAVE_LIBGEOIP
static void
print_html_geolocation (FILE * fp, GHolder * h, int process)
{
  char *data;
  float percent;
  int hits;
  int i;
  GSubList *sub_list;

  if (h->idx == 0)
    return;

  print_html_h2 (fp, GEOLO_HEAD, GEOLO_ID);
  print_p (fp, GEOLO_DESC);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Visitors</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>");
  fprintf (fp, "Location");
  fprintf (fp, "<span class=\"r\" onclick=\"t(this)\">◀</span>");
  fprintf (fp, "</th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  for (i = 0; i < h->idx; i++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    print_html_begin_tr (fp, 0);
    fprintf (fp, "<td>%d</td>", hits);
    fprintf (fp, "<td>%4.2f%%</td>", percent);
    fprintf (fp, "<td>%s</td>", data);
    print_html_end_tr (fp);

    sub_list = h->items[i].sub_list;
    print_html_sub_geolocation (fp, sub_list, process);
  }
  print_html_end_tbody (fp);
  print_html_end_table (fp);
}
#endif

static void
print_html_sub_status (FILE * fp, GSubList * sub_list, int process)
{
  char *data, *name = NULL;
  float percent;
  GSubItem *iter;
  int hits;

  for (iter = sub_list->head; iter; iter = iter->next) {
    hits = iter->hits;
    data = (char *) iter->data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    name = xmalloc (snprintf (NULL, 0, "—&nbsp;%s", data) + 1);
    sprintf (name, "—&nbsp;%s", data);

    print_html_begin_tr (fp, 1);
    fprintf (fp, "<td>%d</td>", hits);
    fprintf (fp, "<td>%4.2f%%</td>", percent);

    fprintf (fp, "<td>%s</td>", name);

    print_html_end_tr (fp);
    free (name);
  }
}

static void
print_html_status (FILE * fp, GHolder * h, int process)
{
  char *data;
  float percent;
  int hits;
  int i;
  GSubList *sub_list;

  if (h->idx == 0)
    return;

  print_html_h2 (fp, CODES_HEAD, CODES_ID);
  print_p (fp, CODES_DESC);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Hits</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp,
           "<th>Code<span class=\"r\" onclick=\"t(this)\">◀</span></th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  for (i = 0; i < h->idx; i++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    print_html_begin_tr (fp, 0);
    fprintf (fp, "<td>%d</td>", hits);
    fprintf (fp, "<td>%4.2f%%</td>", percent);
    fprintf (fp, "<td>%s</td>", data);
    print_html_end_tr (fp);

    sub_list = h->items[i].sub_list;
    print_html_sub_status (fp, sub_list, process);
  }
  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_generic (FILE * fp, GHolder * h, int process)
{
  char *data;
  const char *desc = REFER_DESC;
  const char *head = REFER_HEAD;
  const char *id = REFER_ID;
  float percent;
  int hits, i, until = 0;

  if (h->idx == 0)
    return;

  if (h->module == NOT_FOUND) {
    head = FOUND_HEAD;
    id = FOUND_ID;
    desc = FOUND_DESC;
  } else if (h->module == REFERRING_SITES) {
    head = SITES_HEAD;
    id = SITES_ID;
    desc = SITES_DESC;
  } else if (h->module == KEYPHRASES) {
    head = KEYPH_HEAD;
    id = KEYPH_ID;
    desc = KEYPH_DESC;
  }

  print_html_h2 (fp, head, id);
  print_p (fp, desc);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Hits</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp,
           "<th>URL<span class=\"r\" onclick=\"t(this)\">◀</span></th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  until = h->idx < MAX_CHOICES ? h->idx : MAX_CHOICES;
  for (i = 0; i < until; i++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    print_html_begin_tr (fp, i > OUTPUT_N ? 1 : 0);

    fprintf (fp, "<td>%d</td>", hits);
    fprintf (fp, "<td>%4.2f%%</td>", percent);
    fprintf (fp, "<td>");
    clean_output (fp, data);
    fprintf (fp, "</td>");

    print_html_end_tr (fp);
  }

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_sub_browser_os (FILE * fp, GSubList * sub_list, int process)
{
  char *data, *name = NULL;
  int hits;
  float percent, l;
  GSubItem *iter;

  for (iter = sub_list->head; iter; iter = iter->next) {
    hits = iter->hits;
    data = (char *) iter->data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    l = get_percentage (process, hits);
    l = l < 1 ? 1 : l;

    name = xmalloc (snprintf (NULL, 0, "—&nbsp;%s", data) + 1);
    sprintf (name, "—&nbsp;%s", data);

    print_html_begin_tr (fp, 1);
    fprintf (fp, "<td>%d</td>", hits);
    fprintf (fp, "<td>%4.2f%%</td>", percent);
    fprintf (fp, "<td  style=\"white-space:nowrap;\">%s</td>", name);
    fprintf (fp, "<td class=\"graph\">");
    fprintf (fp, "<div class=\"bar light\" style=\"width:%f%%\"></div>", l);
    fprintf (fp, "</td>");
    print_html_end_tr (fp);
    free (name);
  }
}

static void
print_html_browser_os (FILE * fp, GHolder * h)
{
  char *data;
  const char *desc = OPERA_DESC;
  const char *head = OPERA_HEAD;
  const char *id = OPERA_ID;
  float percent, l;
  GSubList *sub_list;
  int hits, i, max, process = get_ht_size (ht_unique_visitors);

  if (h->idx == 0)
    return;

  if (h->module == BROWSERS) {
    head = BROWS_HEAD;
    id = BROWS_ID;
    desc = BROWS_DESC;
  }

  print_html_h2 (fp, head, id);
  print_p (fp, desc);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Visitors</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>Name</th>");
  fprintf (fp, "<th style=\"width:100%%;text-align:right;\">");
  fprintf (fp, "<span class=\"r\" onclick=\"t(this)\">◀</span>");
  fprintf (fp, "</th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  max = 0;
  for (i = 0; i < h->idx; i++) {
    if (h->items[i].hits > max)
      max = h->items[i].hits;
  }

  for (i = 0; i < h->idx; i++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;

    l = get_percentage (max, hits);
    l = l < 1 ? 1 : l;

    print_html_begin_tr (fp, 0);
    fprintf (fp, "<td>%d</td>", hits);
    fprintf (fp, "<td>%4.2f%%</td>", percent);

    /* data */
    fprintf (fp, "<td>");
    clean_output (fp, data);
    fprintf (fp, "</td>");

    fprintf (fp, "<td class=\"graph\">");
    fprintf (fp, "<div class=\"bar\" style=\"width:%f%%\"></div>", l);
    fprintf (fp, "</td>");
    print_html_end_tr (fp);

    sub_list = h->items[i].sub_list;
    print_html_sub_browser_os (fp, sub_list, process);
  }

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_hosts (FILE * fp, GHolder * h, int process)
{
  GAgents *agents;

  char *data, *bandwidth, *usecs, *ag, *ptr_value, *host;
  float percent, l;
  int hits;
  int i, j, max, until = 0, delims = 0, colspan = 6;
  size_t alloc = 0;

#ifdef HAVE_LIBGEOIP
  const char *location = NULL;
  colspan++;
#endif

  if (h->idx == 0)
    return;

  print_html_h2 (fp, HOSTS_HEAD, HOSTS_ID);
  print_p (fp, HOSTS_DESC);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th></th>");
  fprintf (fp, "<th>Hits</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>Bandwidth</th>");
  if (conf.serve_usecs) {
    colspan++;
    fprintf (fp, "<th>Time&nbsp;served</th>");
  }
  fprintf (fp, "<th>IP</th>");
#ifdef HAVE_LIBGEOIP
  fprintf (fp, "<th>Country</th>");
#endif
  if (conf.enable_html_resolver) {
    colspan++;
    fprintf (fp, "<th>Hostname</th>");
  }

  fprintf (fp, "<th style=\"width:100%%;text-align:right;\">");
  fprintf (fp, "<span class=\"r\" onclick=\"t(this)\">◀</span>");
  fprintf (fp, "</th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  until = h->idx < MAX_CHOICES ? h->idx : MAX_CHOICES;
  max = 0;
  for (i = 0; i < until; i++) {
    if (h->items[i].hits > max)
      max = h->items[i].hits;
  }

  for (i = 0; i < until; i++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;
    bandwidth = filesize_str (h->items[i].bw);
    l = get_percentage (max, hits);
    l = l < 1 ? 1 : l;

#ifdef HAVE_LIBTOKYOCABINET
    ag = tcbdbget2 (ht_hosts_agents, data);
#elif HAVE_LIBGLIB_2_0
    ag = g_hash_table_lookup (ht_hosts_agents, data);
#endif

    print_html_begin_tr (fp, i > OUTPUT_N ? 1 : 0);
    fprintf (fp, "<td>");
    if (ag != NULL)
      fprintf (fp, "<span class=\"s\" onclick=\"a(this)\">▶</span>");
    else
      fprintf (fp, "<span class=\"s\">-</span>");
    fprintf (fp, "</td>");

    fprintf (fp, "<td>%d</td>", hits);
    fprintf (fp, "<td>%4.2f%%</td>", percent);

    fprintf (fp, "<td>");
    clean_output (fp, bandwidth);
    fprintf (fp, "</td>");

    /* usecs */
    if (conf.serve_usecs) {
      usecs = usecs_to_str (h->items[i].usecs);
      fprintf (fp, "<td>");
      clean_output (fp, usecs);
      fprintf (fp, "</td>");
      free (usecs);
    }

    fprintf (fp, "<td>%s</td>", data);

#ifdef HAVE_LIBGEOIP
    location = get_geoip_data (data);
    fprintf (fp, "<td style=\"white-space:nowrap;\">%s</td>", location);
#endif

    if (conf.enable_html_resolver) {
      host = reverse_ip (data);
      fprintf (fp, "<td style=\"white-space:nowrap;\">%s</td>", host);
      free (host);
    }

    fprintf (fp, "<td class=\"graph\">");
    fprintf (fp, "<div class=\"bar\" style=\"width:%f%%\"></div>", l);
    fprintf (fp, "</td>");
    print_html_end_tr (fp);

    /* render agents for each host */
    if (ag != NULL) {
      ptr_value = (char *) ag;

      delims = count_occurrences (ptr_value, '|');
      /* round-up + padding */
      alloc = ((strlen (ptr_value) + 300 - 1) / 300) + delims + 1;
      agents = xmalloc (alloc * sizeof (GAgents));
      memset (agents, 0, alloc * sizeof (GAgents));

      /* split agents into struct */
      split_agent_str (ptr_value, agents, 300);

      fprintf (fp, "<tr class=\"agent-hide\">\n");
      fprintf (fp, "<td colspan=\"%d\">\n", colspan);
      fprintf (fp, "<div>");
      fprintf (fp, "<table class=\"pure-table-striped\">");

      /* output agents from struct */
      for (j = 0; (j < 10) && (agents[j].agents != NULL); j++) {
        print_html_begin_tr (fp, 0);
        fprintf (fp, "<td>");
        clean_output (fp, agents[j].agents);
        fprintf (fp, "</td>");
        print_html_end_tr (fp);
      }

      fprintf (fp, "</table>\n");
      fprintf (fp, "</div>\n");
      fprintf (fp, "</td>\n");
      print_html_end_tr (fp);

      for (j = 0; (agents[j].agents != NULL); j++)
        free (agents[j].agents);
      free (agents);
#ifdef HAVE_LIBTOKYOCABINET
      if (ag)
        free (ag);
#endif
    }

    free (bandwidth);
  }

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_request_report (FILE * fp, GHolder * h, int process)
{
#ifdef HAVE_LIBTOKYOCABINET
  TCBDB *ht;
#elif HAVE_LIBGLIB_2_0
  GHashTable *ht;
#endif

  char *data, *bandwidth, *usecs;
  const char *desc = REQUE_DESC;
  const char *head = REQUE_HEAD;
  const char *id = REQUE_ID;
  float percent;
  int hits;
  int i, until = 0;

  if (h->idx == 0)
    return;

  ht = get_ht_by_module (h->module);

  if (ht == ht_requests_static) {
    head = STATI_HEAD;
    id = STATI_ID;
    desc = STATI_DESC;
  } else if (ht == ht_not_found_requests) {
    head = FOUND_HEAD;
    id = FOUND_ID;
    desc = FOUND_DESC;
  }

  print_html_h2 (fp, head, id);
  print_p (fp, desc);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Hits</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>Bandwidth</th>");
  if (conf.serve_usecs)
    fprintf (fp, "<th>Time&nbsp;served</th>");
  if (conf.append_protocol)
    fprintf (fp, "<th>Protocol</th>");
  if (conf.append_method)
    fprintf (fp, "<th>Method</th>");
  fprintf (fp, "<th>URL<span class=\"r\" onclick=\"t(this)\">◀</span>");
  fprintf (fp, "</th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);
  print_html_begin_tbody (fp);

  until = h->idx < MAX_CHOICES ? h->idx : MAX_CHOICES;
  for (i = 0; i < until; i++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;
    bandwidth = filesize_str (h->items[i].bw);

    print_html_begin_tr (fp, i > OUTPUT_N ? 1 : 0);

    /* hits */
    fprintf (fp, "<td>%d</td>", hits);
    /* percent */
    fprintf (fp, "<td>%4.2f%%</td>", percent);
    /* bandwidth */
    fprintf (fp, "<td>");
    clean_output (fp, bandwidth);
    fprintf (fp, "</td>");

    /* usecs */
    if (conf.serve_usecs) {
      usecs = usecs_to_str (h->items[i].usecs);
      fprintf (fp, "<td>");
      clean_output (fp, usecs);
      fprintf (fp, "</td>");
      free (usecs);
    }
    /* protocol */
    if (conf.append_protocol) {
      fprintf (fp, "<td>");
      clean_output (fp, h->items[i].protocol);
      fprintf (fp, "</td>");
    }
    /* method */
    if (conf.append_method) {
      fprintf (fp, "<td>");
      clean_output (fp, h->items[i].method);
      fprintf (fp, "</td>");
    }

    /* data */
    fprintf (fp, "<td>");
    clean_output (fp, data);
    fprintf (fp, "</td>");

    print_html_end_tr (fp);

    free (bandwidth);
  }

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_visitors_report (FILE * fp, GHolder * h)
{
  char *data, *bandwidth, buf[DATE_LEN];
  float percent, l;
  int hits, i, max, process = get_ht_size (ht_unique_visitors);

  /* make compiler happy */
  memset (buf, 0, sizeof (buf));

  print_html_h2 (fp, VISIT_HEAD, VISIT_ID);
  print_p (fp, VISIT_DESC);
  print_html_begin_table (fp);
  print_html_begin_thead (fp);

  fprintf (fp, "<tr>");
  fprintf (fp, "<th>Visitors</th>");
  fprintf (fp, "<th>%%</th>");
  fprintf (fp, "<th>Date</th>");
  fprintf (fp, "<th>Bandwidth</th>");
  fprintf (fp, "<th style=\"width:100%%;text-align:right;\">");
  fprintf (fp, "<span class=\"r\" onclick=\"t(this)\">◀</span>");
  fprintf (fp, "</th>");
  fprintf (fp, "</tr>");

  print_html_end_thead (fp);

  print_html_begin_tbody (fp);

  max = 0;
  for (i = 0; i < h->idx; i++) {
    if (h->items[i].hits > max)
      max = h->items[i].hits;
  }
  for (i = 0; i < h->idx; i++) {
    hits = h->items[i].hits;
    data = h->items[i].data;
    percent = get_percentage (process, hits);
    percent = percent < 0 ? 0 : percent;
    bandwidth = filesize_str (h->items[i].bw);

    l = get_percentage (max, hits);
    l = l < 1 ? 1 : l;

    print_html_begin_tr (fp, i > OUTPUT_N ? 1 : 0);

    /* hits */
    fprintf (fp, "<td>%d</td>", hits);
    if (hits == max)
      fprintf (fp, "<td class=\"max\">%4.2f%%</td>", percent);
    else
      fprintf (fp, "<td>%4.2f%%</td>", percent);

    /* date */
    convert_date (buf, data, "%Y%m%d", "%d/%b/%Y", DATE_LEN);
    fprintf (fp, "<td>%s</td>", buf);

    /* bandwidth */
    fprintf (fp, "<td>");
    clean_output (fp, bandwidth);
    fprintf (fp, "</td>");

    /* bars */
    fprintf (fp, "<td class=\"graph\">");
    fprintf (fp, "<div class=\"bar\" style=\"width:%f%%\"></div>", l);
    fprintf (fp, "</td>\n");

    print_html_end_tr (fp);

    free (bandwidth);
  }

  print_html_end_tbody (fp);
  print_html_end_table (fp);
}

static void
print_html_summary_field (FILE * fp, int hits, const char *field)
{
  fprintf (fp, "<td>%s</td><td>%d</td>", field, hits);
}

static void
print_html_summary (FILE * fp, GLog * logger)
{
  char *bw, *size;
  off_t log_size;

  print_html_h2 (fp, T_HEAD, 0);
  print_html_begin_table (fp);
  print_html_begin_tbody (fp);

  print_html_begin_tr (fp, 0);
  print_html_summary_field (fp, logger->process, T_REQUESTS);
  print_html_summary_field (fp, get_ht_size (ht_unique_visitors),
                            T_UNIQUE_VIS);
  print_html_summary_field (fp, get_ht_size (ht_referrers), T_REFERRER);

  if (!logger->piping) {
    log_size = file_size (conf.ifile);
    size = filesize_str (log_size);
  } else {
    size = alloc_string ("N/A");
  }

  bw = filesize_str ((float) logger->resp_size);
  if (conf.ifile == NULL)
    conf.ifile = (char *) "STDIN";

  fprintf (fp, "<td>%s</td><td>%s</td>", T_LOG, size);
  print_html_end_tr (fp);

  print_html_begin_tr (fp, 0);
  print_html_summary_field (fp, logger->invalid, T_F_REQUESTS);
  print_html_summary_field (fp, get_ht_size (ht_requests), T_UNIQUE_FIL);
  print_html_summary_field (fp, get_ht_size (ht_not_found_requests),
                            T_UNIQUE404);
  fprintf (fp, "<td>%s</td><td>%s</td>", T_BW, bw);
  print_html_end_tr (fp);

  print_html_begin_tr (fp, 0);
  fprintf (fp, "<td>%s</td>", T_GEN_TIME);
  fprintf (fp, "<td>%llu</td>", ((long long) end_proc - start_proc));

  print_html_summary_field (fp, get_ht_size (ht_requests_static),
                            T_STATIC_FIL);
  fprintf (fp, "<td colspan=\"4\">%s</td>", conf.ifile);

  print_html_end_tr (fp);
  print_html_end_tbody (fp);
  print_html_end_table (fp);
  free (bw);
  free (size);
}

static void
print_pure_menu (FILE * fp, char *now)
{
  fprintf (fp, "<div id=\"menu\" class=\"pure-u\">");
  fprintf (fp, "<div class=\"pure-menu pure-menu-open\">");
  fprintf (fp, "<a class=\"pure-menu-heading\" href=\"%s\">", GO_WEBSITE);
  fprintf (fp, "«./GoAccess»");
  fprintf (fp, "</a>");
  fprintf (fp, "<ul>");
  fprintf (fp, "<li><a href=\"#\">Overall</a></li>");
  fprintf (fp, "<li><a href=\"#%s\">Unique visitors</a></li>", VISIT_ID);
  fprintf (fp, "<li><a href=\"#%s\">Requested files</a></li>", REQUE_ID);
  fprintf (fp, "<li><a href=\"#%s\">Requested static files</a></li>",
           STATI_ID);
  fprintf (fp, "<li><a href=\"#%s\">Not found URLs</a></li>", FOUND_ID);
  fprintf (fp, "<li><a href=\"#%s\">Hosts</a></li>", HOSTS_ID);
  fprintf (fp, "<li><a href=\"#%s\">Operating Systems</a></li>", OPERA_ID);
  fprintf (fp, "<li><a href=\"#%s\">Browsers</a></li>", BROWS_ID);
  fprintf (fp, "<li><a href=\"#%s\">Referrers URLs</a></li>", REFER_ID);
  fprintf (fp, "<li><a href=\"#%s\">Referring sites</a></li>", SITES_ID);
  fprintf (fp, "<li><a href=\"#%s\">Keyphrases</a></li>", KEYPH_ID);
#ifdef HAVE_LIBGEOIP
  fprintf (fp, "<li><a href=\"#%s\">Geo Location</a></li>", GEOLO_ID);
#endif
  fprintf (fp, "<li><a href=\"#%s\">Status codes</a></li>", CODES_ID);
  fprintf (fp, "<li class=\"menu-item-divided\"></li>");

  fprintf (fp, "</ul>");
  fprintf (fp, "<p>Generated by<br />GoAccess %s<br />—<br />%s</p>",
           GO_VERSION, now);
  fprintf (fp, "</div>");
  fprintf (fp, "</div> <!-- menu -->");

  fprintf (fp, "<div id=\"main\" class=\"pure-u-1\">");
  fprintf (fp, "<div class=\"l-box\">");
}

/* entry point to generate a report writing it to the fp */
void
output_html (GLog * logger, GHolder * holder)
{
  FILE *fp = stdout;
  char now[DATE_TIME];

  generate_time ();
  strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

  print_html_header (fp, now);
  print_pure_menu (fp, now);

  print_html_summary (fp, logger);
  print_html_visitors_report (fp, holder + VISITORS);
  print_html_request_report (fp, holder + REQUESTS, logger->process);
  print_html_request_report (fp, holder + REQUESTS_STATIC, logger->process);
  print_html_request_report (fp, holder + NOT_FOUND, logger->process);
  print_html_hosts (fp, holder + HOSTS, logger->process);
  print_html_browser_os (fp, holder + OS);
  print_html_browser_os (fp, holder + BROWSERS);
  print_html_generic (fp, holder + REFERRERS, logger->process);
  print_html_generic (fp, holder + REFERRING_SITES, logger->process);
  print_html_generic (fp, holder + KEYPHRASES, logger->process);
#ifdef HAVE_LIBGEOIP
  print_html_geolocation (fp, holder + GEO_LOCATION, logger->process);
#endif
  print_html_status (fp, holder + STATUS_CODES, logger->process);

  print_html_footer (fp);
}
