/**
 * output.c -- output to the standard output stream
 * Copyright (C) 2009-2013 by Gerardo Orellana <goaccess@prosoftcorp.com>
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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>

#include "output.h"

#include "commons.h"
#include "error.h"
#include "xmalloc.h"
#include "ui.h"
#include "gdashboard.h"
#include "util.h"

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

static void
escape_cvs_output (FILE * fp, char *s)
{
   while (*s) {
      switch (*s) {
       case '"':
          fprintf (fp, "\\\"");
          break;
       default:
          fputc (*s, fp);
          break;
      }
      s++;
   }
}

static void
escape_json_output (FILE * fp, char *s)
{
   while (*s) {
      switch (*s) {
       case '"':
          fprintf (fp, "\\\"");
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
   "    -webkit-overflow-scroll: touch;"
   "    width: 200px;"
   "    z-index: 1000;"
   "}"
   "#menu a {"
   "    border: 0;"
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
   "#menu .pure-menu ul,"
   "#menu .pure-menu .menu-item-divided {"
   "    border-top: 1px solid #333;"
   "    box-shadow: 0px 0 1px #000;"
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
   fprintf (fp, "<th>URL<span class=\"r\" onclick=\"t(this)\">◀</span></th>");
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
print_html_sub_os (FILE * fp, GSubList * sub_list, int process)
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

      name = xmalloc (snprintf (NULL, 0, "`-&nbsp;%s", data) + 1);
      sprintf (name, "`-&nbsp;%s", data);

      print_html_begin_tr (fp, 1);
      fprintf (fp, "<td>%d</td>", hits);
      fprintf (fp, "<td>%4.2f%%</td>", percent);
      fprintf (fp, "<td>%s</td>", name);
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
   int hits, i, max, process = g_hash_table_size (ht_unique_visitors);

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
      print_html_sub_os (fp, sub_list, process);
   }

   print_html_end_table (fp);
}

static void
print_html_hosts (FILE * fp, GHolder * h, int process)
{
   GAgents *agents;

   char *data, *bandwidth, *usecs, *ag, *ptr_value;
   float percent, l;
   int hits;
   int i, j, max, until = 0, delims = 0;
   size_t alloc = 0;

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
   fprintf (fp, "<th>Time&nbsp;served</th>");
   fprintf (fp, "<th>IP</th>");
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

      ag = g_hash_table_lookup (ht_hosts_agents, data);

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
      usecs = usecs_to_str (h->items[i].usecs);
      fprintf (fp, "<td>");
      clean_output (fp, usecs);
      fprintf (fp, "</td>");
      fprintf (fp, "<td>%s</td>", data);

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
         fprintf (fp, "<td colspan=\"7\">\n");
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
      }
      free (usecs);
      free (bandwidth);
   }

   print_html_end_tbody (fp);
   print_html_end_table (fp);
}

static void
print_html_request_report (FILE * fp, GHolder * h, GHashTable * ht, int process)
{
   char *data, *bandwidth, *usecs;
   const char *desc = REQUE_DESC;
   const char *head = REQUE_HEAD;
   const char *id = REQUE_ID;
   float percent;
   int hits;
   int i, until = 0;

   if (h->idx == 0)
      return;

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
   fprintf (fp, "<th>Time&nbsp;served</th>");
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
      usecs = usecs_to_str (h->items[i].usecs);
      fprintf (fp, "<td>");
      clean_output (fp, usecs);
      fprintf (fp, "</td>");

      /* data */
      fprintf (fp, "<td>");
      clean_output (fp, data);
      fprintf (fp, "</td>");

      print_html_end_tr (fp);

      free (usecs);
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
   int hits, i, max, process = g_hash_table_size (ht_unique_visitors);

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
   fprintf (fp, "<th>Size</th>");
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
   print_html_summary_field (fp, g_hash_table_size (ht_unique_visitors),
                             T_UNIQUE_VIS);
   print_html_summary_field (fp, g_hash_table_size (ht_referrers), T_REFERRER);

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
   print_html_summary_field (fp, g_hash_table_size (ht_requests), T_UNIQUE_FIL);
   print_html_summary_field (fp, g_hash_table_size (ht_not_found_requests),
                             T_UNIQUE404);
   fprintf (fp, "<td>%s</td><td>%s</td>", T_BW, bw);
   print_html_end_tr (fp);

   print_html_begin_tr (fp, 0);
   fprintf (fp, "<td>%s</td>", T_GEN_TIME);
   fprintf (fp, "<td>%lu</td>", ((int) end_proc - start_proc));

   print_html_summary_field (fp, g_hash_table_size (ht_requests_static),
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
   fprintf (fp,
            "<a class=\"pure-menu-heading\" href=\"http://goaccess.prosoftcorp.com/\">GoAccess</a>");
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
   print_html_request_report (fp, holder + REQUESTS, ht_requests,
                              logger->process);
   print_html_request_report (fp, holder + REQUESTS_STATIC, ht_requests_static,
                              logger->process);
   print_html_request_report (fp, holder + NOT_FOUND, ht_not_found_requests,
                              logger->process);
   print_html_hosts (fp, holder + HOSTS, logger->process);
   print_html_browser_os (fp, holder + OS);
   print_html_browser_os (fp, holder + BROWSERS);
   print_html_generic (fp, holder + REFERRERS, logger->process);
   print_html_generic (fp, holder + REFERRING_SITES, logger->process);
   print_html_generic (fp, holder + KEYPHRASES, logger->process);
   print_html_status (fp, holder + STATUS_CODES, logger->process);

   print_html_footer (fp);
}

/**
 * Generate CSV on partial fields for the following modules:
 * OS, BROWSERS, REFERRERS, REFERRING_SITES, KEYPHRASES, STATUS_CODES
 */
static void
print_csv_generic (FILE * fp, GHolder * holder, int process)
{
   char id[REF_LEN], *data;
   float percent;
   GHolder *h;
   int i, j, hits;

   /* os, browsers, referrers, referring sites, keyphrases & status code report */
   for (i = 0; i < 6; i++) {
      switch (i) {
       case 0:
          process = g_hash_table_size (ht_unique_visitors);
          h = holder + OS;
          strcpy (id, "os");
          break;
       case 1:
          process = g_hash_table_size (ht_unique_visitors);
          h = holder + BROWSERS;
          strcpy (id, "browser");
          break;
       case 2:
          h = holder + REFERRERS;
          strcpy (id, "ref");
          break;
       case 3:
          h = holder + REFERRING_SITES;
          strcpy (id, "refsite");
          break;
       case 4:
          h = holder + KEYPHRASES;
          strcpy (id, "keyphrase");
          break;
       case 5:
          h = holder + STATUS_CODES;
          strcpy (id, "status");
          break;
      }

      for (j = 0; j < h->idx; j++) {
         hits = h->items[j].hits;
         data = h->items[j].data;
         percent = get_percentage (process, hits);
         percent = percent < 0 ? 0 : percent;

         fprintf (fp, "\"%s\",\"%d\",\"%4.2f%%\",\"", id, hits, percent);
         escape_cvs_output (fp, data);
         fprintf (fp, "\"\r\n");
      }
   }
}

/**
 * Generate CSV on complete fields for the following modules:
 * REQUESTS, REQUESTS_STATIC, NOT_FOUND, HOSTS
 */
static void
print_csv_complete (FILE * fp, GHolder * holder, int process)
{
   char id[REF_LEN], *data;
   float percent;
   GHolder *h;
   int i, j, hits;
   unsigned long long bw, usecs;

   for (i = 0; i < 4; i++) {
      switch (i) {
       case 0:
          h = holder + REQUESTS;
          strcpy (id, "req");
          break;
       case 1:
          h = holder + REQUESTS_STATIC;
          strcpy (id, "static");
          break;
       case 2:
          h = holder + NOT_FOUND;
          strcpy (id, "notfound");
          break;
       case 3:
          h = holder + HOSTS;
          strcpy (id, "host");
          break;
      }

      for (j = 0; j < h->idx; j++) {
         hits = h->items[j].hits;
         data = h->items[j].data;
         percent = get_percentage (process, hits);
         percent = percent < 0 ? 0 : percent;
         bw = h->items[j].bw;
         usecs = h->items[j].usecs;

         fprintf (fp, "\"%s\",\"%d\",\"%4.2f%%\",\"", id, hits, percent);
         escape_cvs_output (fp, data);
         fprintf (fp, "\",\"%lld\",\"%lld\"\r\n", bw, usecs);
      }
   }
}

/* generate CSV unique visitors stats */
static void
print_csv_visitors (FILE * fp, GHolder * h)
{
   char *data, buf[DATE_LEN];
   float percent;
   int hits, bw, i, process = g_hash_table_size (ht_unique_visitors);

   /* make compiler happy */
   memset (buf, 0, sizeof (buf));

   for (i = 0; i < h->idx; i++) {
      hits = h->items[i].hits;
      data = h->items[i].data;
      percent = get_percentage (process, hits);
      percent = percent < 0 ? 0 : percent;
      bw = h->items[i].bw;
      convert_date (buf, data, "%Y%m%d", "%d/%b/%Y", DATE_LEN);

      fprintf (fp, "\"visitors\",\"%d\",\"%4.2f%%\",\"%s\",\"%d\"\r\n", hits,
               percent, buf, bw);
   }
}

/* generate overview stats */
static void
print_csv_summary (FILE * fp, GLog * logger)
{
   off_t log_size = 0;
   char now[DATE_TIME];

   generate_time ();
   strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

   /* general statistics info */
   fprintf (fp, "\"general\",\"date_time\",\"%s\"\r\n", now);
   fprintf (fp, "\"general\",\"total_requests\",\"%d\"\r\n", logger->process);
   fprintf (fp, "\"general\",\"unique_visitors\",\"%d\"\r\n",
            g_hash_table_size (ht_unique_visitors));
   fprintf (fp, "\"general\",\"referrers\",\"%d\"\r\n",
            g_hash_table_size (ht_referrers));

   if (!logger->piping)
      log_size = file_size (conf.ifile);

   fprintf (fp, "\"general\",\"log_size\",\"%jd\"\r\n", (intmax_t) log_size);
   fprintf (fp, "\"general\",\"failed_requests\",\"%d\"\r\n", logger->invalid);
   fprintf (fp, "\"general\",\"unique_files\",\"%d\"\r\n",
            g_hash_table_size (ht_requests));
   fprintf (fp, "\"general\",\"unique_404\",\"%d\"\r\n",
            g_hash_table_size (ht_not_found_requests));

   fprintf (fp, "\"general\",\"bandwidth\",\"%lld\"\r\n", logger->resp_size);
   fprintf (fp, "\"general\",\"generation_time\",\"%lu\"\r\n",
            ((int) end_proc - start_proc));
   fprintf (fp, "\"general\",\"static_files\",\"%d\"\r\n",
            g_hash_table_size (ht_requests_static));

   if (conf.ifile == NULL)
      conf.ifile = (char *) "STDIN";

   fprintf (fp, "\"general\",\"filename\",\"%s\"\r\n", conf.ifile);
}

/* entry point to generate a a csv report writing it to the fp */
void
output_csv (GLog * logger, GHolder * holder)
{
   FILE *fp = stdout;

   print_csv_summary (fp, logger);
   print_csv_visitors (fp, holder + VISITORS);
   print_csv_complete (fp, holder, logger->process);
   print_csv_generic (fp, holder, logger->process);

   fclose (fp);
}

/**
 * Generate JSON on partial fields for the following modules:
 * OS, BROWSERS, REFERRERS, REFERRING_SITES, KEYPHRASES, STATUS_CODES
 */
static void
print_json_generic (FILE * fp, GHolder * holder, int process)
{
   char *data;
   float percent;
   GHolder *h;
   int i, j, hits;

   fprintf (fp, "\t\"generic\": {\n");
   /* os, browsers, referrers, referring sites, keyphrases & status code report */
   for (i = 0; i < 6; i++) {
      switch (i) {
       case 0:
          process = g_hash_table_size (ht_unique_visitors);
          h = holder + OS;
          fprintf (fp, "\t\t\"os\": [\n");
          break;
       case 1:
          process = g_hash_table_size (ht_unique_visitors);
          h = holder + BROWSERS;
          fprintf (fp, "\t\t\"browser\": [\n");
          break;
       case 2:
          h = holder + REFERRERS;
          fprintf (fp, "\t\t\"ref\": [\n");
          break;
       case 3:
          h = holder + REFERRING_SITES;
          fprintf (fp, "\t\t\"refsite\": [\n");
          break;
       case 4:
          h = holder + KEYPHRASES;
          fprintf (fp, "\t\t\"keyphrase\": [\n");
          break;
       case 5:
          h = holder + STATUS_CODES;
          fprintf (fp, "\t\t\"status\": [\n");
          break;
      }

      for (j = 0; j < h->idx; j++) {
         hits = h->items[j].hits;
         data = h->items[j].data;
         percent = get_percentage (process, hits);
         percent = percent < 0 ? 0 : percent;

         fprintf (fp,
                  "\t\t\t{\n\t\t\t\t\"hits\": \"%d\",\n\t\t\t\t\"percent\": \"%4.2f%%\",\n\t\t\t\t\"data\": \"",
                  hits, percent);
         escape_json_output (fp, data);
         fprintf (fp, "\"\n\t\t\t}");
         if (j != h->idx - 1)
            fprintf (fp, ",\n");
         else
            fprintf (fp, "\n");
      }
      if (i != 5)
         fprintf (fp, "\t\t],\n");
      else
         fprintf (fp, "\t\t]\n");
   }
   fprintf (fp, "\t}");
}

/**
 * Generate JSON on complete fields for the following modules:
 * REQUESTS, REQUESTS_STATIC, NOT_FOUND, HOSTS
 */
static void
print_json_complete (FILE * fp, GHolder * holder, int process)
{
   char *data;
   float percent;
   GHolder *h;
   int i, j, hits;
   unsigned long long bw, usecs;

   fprintf (fp, "\t\"complete\": {\n");
   for (i = 0; i < 4; i++) {
      switch (i) {
       case 0:
          h = holder + REQUESTS;
          fprintf (fp, "\t\t\"req\": [\n");
          break;
       case 1:
          h = holder + REQUESTS_STATIC;
          fprintf (fp, "\t\t\"static\": [\n");
          break;
       case 2:
          h = holder + NOT_FOUND;
          fprintf (fp, "\t\t\"notfound\": [\n");
          break;
       case 3:
          h = holder + HOSTS;
          fprintf (fp, "\t\t\"host\": [\n");
          break;
      }

      for (j = 0; j < h->idx; j++) {
         hits = h->items[j].hits;
         data = h->items[j].data;
         percent = get_percentage (process, hits);
         percent = percent < 0 ? 0 : percent;
         bw = h->items[j].bw;
         usecs = h->items[j].usecs;
         fprintf (fp,
                  "\t\t\t{\n\t\t\t\t\"hits\": \"%d\",\n\t\t\t\t\"percent\": \"%4.2f%%\",\n\t\t\t\t\"url: \":\"",
                  hits, percent);
         escape_cvs_output (fp, data);
         fprintf (fp,
                  "\",\n\t\t\t\t\"bytes\": \"%lld\",\n\t\t\t\t\"usecs\": \"%lld\"\n\t\t\t}",
                  bw, usecs);
         if (j != h->idx - 1)
            fprintf (fp, ",\n");
         else
            fprintf (fp, "\n");
      }
      if (i != 3)
         fprintf (fp, "\t\t],\n");
      else
         fprintf (fp, "\t\t]\n");
   }
   fprintf (fp, "\t}");
}

/* generate JSON unique visitors stats */
static void
print_json_visitors (FILE * fp, GHolder * h)
{
   char *data, buf[DATE_LEN];
   float percent;
   int hits, bw, i, process = g_hash_table_size (ht_unique_visitors);

   /* make compiler happy */
   memset (buf, 0, sizeof (buf));
   fprintf (fp, "\t\"visitors\": [\n");
   for (i = 0; i < h->idx; i++) {
      hits = h->items[i].hits;
      data = h->items[i].data;
      percent = get_percentage (process, hits);
      percent = percent < 0 ? 0 : percent;
      bw = h->items[i].bw;
      convert_date (buf, data, "%Y%m%d", "%d/%b/%Y", DATE_LEN);
      fprintf (fp,
               "\t\t{\n\t\t\t\"hits\": \"%d\",\n\t\t\t\"percent\": \"%4.2f%%\",\n\t\t\t\"date\": \"%s\",\n\t\t\t\"bytes\": \"%d\"\n\t\t}",
               hits, percent, buf, bw);
      if (i != h->idx - 1)
         fprintf (fp, ",\n");
      else
         fprintf (fp, "\n");
   }
   fprintf (fp, "\t]");
}

/* generate overview stats */
static void
print_json_summary (FILE * fp, GLog * logger)
{
   off_t log_size = 0;
   char now[DATE_TIME];

   generate_time ();
   strftime (now, DATE_TIME, "%Y-%m-%d %H:%M:%S", now_tm);

   fprintf (fp, "\t\"general\": {\n");
   /* general statistics info */
   fprintf (fp, "\t\t\"date_time\": \"%s\",\n", now);
   fprintf (fp, "\t\t\"total_requests\": %d,\n", logger->process);
   fprintf (fp, "\t\t\"unique_visitors\": %d,\n",
            g_hash_table_size (ht_unique_visitors));
   fprintf (fp, "\t\t\"referrers\": %d,\n", g_hash_table_size (ht_referrers));

   if (!logger->piping)
      log_size = file_size (conf.ifile);

   fprintf (fp, "\t\t\"log_size\": %jd,\n", (intmax_t) log_size);
   fprintf (fp, "\t\t\"failed_requests\": %d,\n", logger->invalid);
   fprintf (fp, "\t\t\"unique_files\": %d,\n", g_hash_table_size (ht_requests));
   fprintf (fp, "\t\t\"unique_404\": %d,\n",
            g_hash_table_size (ht_not_found_requests));

   fprintf (fp, "\t\t\"bandwidth\": %lld,\n", logger->resp_size);
   fprintf (fp, "\t\t\"generation_time\": %lu,\n",
            ((int) end_proc - start_proc));
   fprintf (fp, "\t\t\"static_files\": %d,\n",
            g_hash_table_size (ht_requests_static));

   if (conf.ifile == NULL)
      conf.ifile = (char *) "STDIN";

   fprintf (fp, "\t\t\"filename\": \"%s\"\n", conf.ifile);
   fprintf (fp, "\t}");
}

/* entry point to generate a a json report writing it to the fp */
/* follow the JSON style similar to http://developer.github.com/v3/ */
void
output_json (GLog * logger, GHolder * holder)
{
   FILE *fp = stdout;
   fprintf (fp, "{\n");
   print_json_summary (fp, logger);
   fprintf (fp, ",\n");
   print_json_visitors (fp, holder + VISITORS);
   fprintf (fp, ",\n");
   print_json_complete (fp, holder, logger->process);
   fprintf (fp, ",\n");
   print_json_generic (fp, holder, logger->process);
   fprintf (fp, "\n}\n");

   fclose (fp);
}
