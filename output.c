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
       default:
          fputc (*s, fp);
          break;
      }
      s++;
   }
}

/* create a sanitized string with html entities for special chars */
static void
clean_output_replacing (char *input, char *output)
{
   strncpy(output, "\0", 1);
   while (*input) {
      switch (*input) {
       case '\'':
          strncat(output, "&#39;", 5);
          break;
       case '"':
          strncat(output, "&#34;", 5);
          break;
       case '&':
          strncat(output, "&amp;", 5);
          break;
       case '<':
          strncat(output, "&lt;", 4);
          break;
       case '>':
          strncat(output, "&gt;", 4);
          break;
       case ' ':
          strncat(output, "&nbsp;", 6);
          break;
       default:
          strncat(output, input, 1);
          break;
      }
      input++;
   }
   strncat(output, "\0", 1);
}

static char *
nbsp_replace (char *input)
{
   return replace_str (input, " ", "&nbsp;");
}

static void
print_html_header (FILE * fp, char *now)
{
   fprintf (fp, "<!DOCTYPE html>\n");
   fprintf (fp, "<html lang=\"en\"><head>\n");
   fprintf (fp, "<title>Server Statistics - %s</title>\n", now);
   fprintf (fp, "<meta charset=\"UTF-8\" />");
   fprintf (fp, "<meta name=\"robots\" content=\"noindex, nofollow\" />");
   fprintf (fp, "<link rel=\"stylesheet\" href=\"http://yui.yahooapis.com/pure/0.3.0/pure-min.css\">");

   fprintf (fp, "<script type=\"text/javascript\">\n");
   fprintf (fp, "function t(c){for(var ");
   fprintf (fp, "b=c.parentNode.parentNode.parentNode.parentNode.");
   fprintf (fp, "getElementsByTagName('tr'),a=0;a<b.length;a++)");
   fprintf (fp, "'hide'==b[a].className?(b[a].className='show',");
   fprintf (fp, "c.innerHTML='▼'):'show'==b[a].className&&");
   fprintf (fp, "(b[a].className='hide',c.innerHTML='◀')};");
   fprintf (fp, "function a(c){var b=c.parentNode.parentNode.nextSibling;");
   fprintf (fp, "while(b && b.nodeType != 1) b=b.nextSibling;");
   fprintf (fp, "'a-hide'==b.className?(b.className='a-show',");
   fprintf (fp, "c.innerHTML='▼'):'a-show'==b.className&&");
   fprintf (fp, "(b.className='a-hide',c.innerHTML='▶')};");
   fprintf (fp, "</script>\n");

   fprintf (fp, "<style type=\"text/css\">\n");
   fprintf (fp, "body {");
   fprintf (fp, "   font-size: 11px;");
   fprintf (fp, "   color: #777;");
   fprintf (fp, "}");

   fprintf (fp, "h1, h2, h3, h4, h5, h6 {");
   fprintf (fp, "   font-weight: bold;");
   fprintf (fp, "   color: rgb(75, 75, 75);");
   fprintf (fp, "}");

   fprintf (fp, ".a-hide, .hide { display: none }");
   fprintf (fp, ".r, .s {");
   fprintf (fp, "   cursor: pointer;");
   fprintf (fp, "}");
   fprintf (fp, ".r { float:right; }");

   fprintf (fp, "thead th {");
   fprintf (fp, "   text-align:center;");
   fprintf (fp, "}");

   fprintf (fp, ".max {");
   fprintf (fp, "    color: #D20B2C;");
   fprintf (fp, "    font-weight: 700;");
   fprintf (fp, "}");
   fprintf (fp, "#layout {");
   fprintf (fp, "   padding-left: 150px;");
   fprintf (fp, "   left: 0;");
   fprintf (fp, "}");
   fprintf (fp, ".l-box {");
   fprintf (fp, "   padding: 1.3em;");
   fprintf (fp, "}");
   fprintf (fp, "#menu {");
   fprintf (fp, "   margin-left: -150px;");
   fprintf (fp, "   width: 150px;");
   fprintf (fp, "   position: fixed;");
   fprintf (fp, "   top:0;");
   fprintf (fp, "   left:150px;");
   fprintf (fp, "   bottom:0;");
   fprintf (fp, "   z-index: 1000;");
   fprintf (fp, "   background: #191818;");
   fprintf (fp, "   overflow-y: auto;");
   fprintf (fp, "   -webkit-overflow-scroll: touch;");
   fprintf (fp, "}");

   fprintf (fp, "#menu a {");
   fprintf (fp, "   color: #999;");
   fprintf (fp, "   border: none;");
   fprintf (fp, "   white-space: normal;");
   fprintf (fp, "   padding: 0.6em 0 0.6em 0.6em;");
   fprintf (fp, "}");

   fprintf (fp, "#menu p {");
   fprintf (fp, "   color: #eee;");
   fprintf (fp, "   padding: 0.6em;");
   fprintf (fp, "   text-align:center;");
   fprintf (fp, "}");

   fprintf (fp, "#menu .pure-menu-open {");
   fprintf (fp, "   background: transparent;");
   fprintf (fp, "   border: 0;");
   fprintf (fp, "}");

   fprintf (fp, "#menu .pure-menu ul {");
   fprintf (fp, "   border: none;");
   fprintf (fp, "   background: transparent;");
   fprintf (fp, "}");

   fprintf (fp, "#menu .pure-menu ul,");
   fprintf (fp, "#menu .pure-menu .menu-item-divided {");
   fprintf (fp, "   border-top: 1px solid #333;");
   fprintf (fp, "}");

   fprintf (fp, "#menu .pure-menu li a:hover,");
   fprintf (fp, "#menu .pure-menu li a:focus {");
   fprintf (fp, "    background: #333;");
   fprintf (fp, "}");

   fprintf (fp, "#menu .pure-menu-heading:hover, #menu .pure-menu-heading:focus {");
   fprintf (fp, "   color: rgb(153, 153, 153);");
   fprintf (fp, "}");

   fprintf (fp, "#menu .pure-menu-heading {");
   fprintf (fp, "   font-size: 110%%;");
   fprintf (fp, "   color: #eee;");
   fprintf (fp, "}");

   fprintf (fp, ".graph{");
   fprintf (fp, "    height:1.529411765em;");
   fprintf (fp, "    margin-bottom:.470588235em;");
   fprintf (fp, "    overflow:hidden;");
   fprintf (fp, "    background-color:#e5e5e5;");
   fprintf (fp, "    border-radius:.071428571em;");
   fprintf (fp, "    text-align:center;");
   fprintf (fp, "}");
   fprintf (fp, ".graph-bar{");
   fprintf (fp, "    float:left;");
   fprintf (fp, "    width:0;");
   fprintf (fp, "    height:100%%;");
   fprintf (fp, "    color:#ffffff;");
   fprintf (fp, "    background-color:#777;");
   fprintf (fp, "    -webkit-box-sizing:border-box;");
   fprintf (fp, "    -moz-box-sizing:border-box;");
   fprintf (fp, "    box-sizing:border-box;");
   fprintf (fp, "}");
   fprintf (fp, ".graph-light{");
   fprintf (fp, "    background-color:#BBB;");
   fprintf (fp, "}");

   fprintf (fp, "</style>\n");
   fprintf (fp, "</head>\n");
   fprintf (fp, "<body>\n");

   fprintf (fp, "<div class=\"pure-g-r\" id=\"layout\">");
}

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
print_html_h2 (FILE* fp, char *title, char *id)
{
   if (id)
      fprintf (fp, "<h2 id=\"%s\">%s</h2>", id, title);
   else
      fprintf (fp, "<h2>%s</h2>", title);
}

static void
print_p (FILE* fp, char *paragraph)
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
   int hits;
   float percent;
   GSubItem *iter;

   for (iter = sub_list->head; iter; iter = iter->next) {
      hits = iter->hits;
      data = (char *) iter->data;
      percent = get_percentage (process, hits);
      percent = percent < 0 ? 0 : percent;

      char cleaned_data[6*strlen(data)]; // maximum possible size of the cleaned string
      clean_output_replacing (data, cleaned_data);
      char *format = "—&nbsp;%s";
      name = xmalloc (snprintf (NULL, 0, format, data) + 1);
      sprintf (name, format, data);

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

   if (h->idx == 0)
      return;

   print_html_h2 (fp, CODES_HEAD, CODES_ID);
   print_p (fp, CODES_DESC);
   print_html_begin_table (fp);
   print_html_begin_thead (fp);
   fprintf (fp, "<tr><th>Hits</th><th>%%</th><th>Code<span class=\"r\" onclick=\"t(this)\">◀</span></th></tr>");
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

      GSubList *sub_list = h->items[i].sub_list;
      print_html_sub_status (fp, sub_list, process);
   }
   print_html_end_tbody (fp);
   print_html_end_table (fp);
}

static void
print_html_generic (FILE * fp, GHolder * h, int process)
{
   char *data;
   float percent;
   int hits;
   int i, until = 0;

   if (h->idx == 0)
      return;

   char *head = REFER_HEAD;
   char *id = REFER_ID;
   char *desc = REFER_DESC;
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
   fprintf (fp, "<tr><th>Hits</th><th>%%</th><th>URL<span class=\"r\" onclick=\"t(this)\">◀</span></th></tr>");
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

      char cleaned_data[6*strlen(data)]; // maximum possible size of the cleaned string
      clean_output_replacing (data, cleaned_data);
      char *format = "—&nbsp;%s";
      name = xmalloc (snprintf (NULL, 0, format, data) + 1);
      sprintf (name, format, data);

      print_html_begin_tr (fp, 1);
      fprintf (fp, "<td>%d</td>", hits);
      fprintf (fp, "<td>%4.2f%%</td>", percent);

      fprintf (fp, "<td>%s</td>", name);

      fprintf (fp, "<td class=\"graph\">");
      fprintf (fp, "<div class=\"graph-bar graph-light\" style=\"width:%f%%\"></div>", l);
      fprintf (fp, "</td>");
      print_html_end_tr (fp);
      free (name);
   }
}

static void
print_html_browser_os (FILE * fp, GHolder * h)
{
   char *data;
   int hits;
   float percent, l;
   int i, max, process = g_hash_table_size (ht_unique_visitors);

   if (h->idx == 0)
      return;

   char *head = OPERA_HEAD;
   char *id = OPERA_ID;
   char *desc = OPERA_DESC;
   if (h->module == BROWSERS) {
      head = BROWS_HEAD;
      id = BROWS_ID;
      desc = BROWS_DESC;
   }

   print_html_h2 (fp, head, id);
   print_p (fp, desc);
   print_html_begin_table (fp);
   print_html_begin_thead (fp);
   fprintf (fp, "<tr><th>Hits</th><th>%%</th><th>Name</th><th style=\"width:100%%;text-align:right;\"><span class=\"r\" onclick=\"t(this)\">◀</span></th></tr>");
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
//       if (hits == max)
//          fprintf (fp, "<td class=\"max\">%4.2f%%</td>", percent);
//       else
      fprintf (fp, "<td>%4.2f%%</td>", percent);

      /* data */
      fprintf (fp, "<td>");
      clean_output (fp, data);
      fprintf (fp, "</td>");

      fprintf (fp, "<td class=\"graph\">");
      fprintf (fp, "<div class=\"graph-bar\" style=\"width:%f%%\"></div>", l);
      fprintf (fp, "</td>");
      print_html_end_tr (fp);

      GSubList *sub_list = h->items[i].sub_list;
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
   fprintf (fp, "<tr><th></th><th>Hits</th><th>%%</th><th>Bandwidth</th><th>Time&nbsp;served</th><th>IP</th><th style=\"width:100%%;text-align:right;\"><span class=\"r\" onclick=\"t(this)\">◀</span></th></tr>");
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
      fprintf (fp, "<td>%s</td>", bandwidth);
      /* usecs */
      usecs = usecs_to_str (h->items[i].usecs);
      fprintf (fp, "<td>%s</td>", usecs);
      fprintf (fp, "<td>%s</td>", data);

      fprintf (fp, "<td class=\"graph\">");
      fprintf (fp, "<div class=\"graph-bar\" style=\"width:%f%%\"></div>", l);
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

         fprintf (fp, "<tr class=\"a-hide\">\n");
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
   float percent;
   int hits;
   int i, until = 0;

   if (h->idx == 0)
      return;

   char *head = REQUE_HEAD;
   char *id = REQUE_ID;
   char *desc = REQUE_DESC;
   if (ht == ht_requests_static) {
      head = STATI_HEAD;
      id = STATI_ID;
      desc = STATI_DESC;
   }
   else if (ht == ht_not_found_requests) {
      head = FOUND_HEAD;
      id = FOUND_ID;
      desc = FOUND_DESC;
   }

   print_html_h2 (fp, head, id);
   print_p (fp, desc);
   print_html_begin_table (fp);
   print_html_begin_thead (fp);
   fprintf (fp, "<tr><th>Hits</th><th>%%</th><th>Bandwidth</th><th>Time&nbsp;served</th><th>URL<span class=\"r\" onclick=\"t(this)\">◀</span></th></tr>");
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
      fprintf (fp, "<td>%s</td>", bandwidth);

      /* usecs */
      usecs = usecs_to_str (h->items[i].usecs);
      fprintf (fp, "<td>%s</td>", usecs);

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
   char buf[DATE_LEN];
   /* make compiler happy */
   memset (buf, 0, sizeof (buf));

   char *data, *bandwidth;
   int hits;
   float percent, l;
   int i, max, process = g_hash_table_size (ht_unique_visitors);

   print_html_h2 (fp, VISIT_HEAD, VISIT_ID);
   print_p (fp, VISIT_DESC);
   print_html_begin_table (fp);
   print_html_begin_thead (fp);
   fprintf (fp, "<tr><th>Hits</th><th>%%</th><th>Date</th><th>Size</th>");
   fprintf (fp, "<th style=\"width:100%%;text-align:right;\"><span class=\"r\" onclick=\"t(this)\">◀</span></th></tr>");
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
      bandwidth = nbsp_replace (bandwidth);

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
      fprintf (fp, "<td>%s</td>\n", bandwidth);

      /* bars */
      fprintf (fp, "<td class=\"graph\">");
      fprintf (fp, "<div class=\"graph-bar\" style=\"width:%f%%\"></div>", l);
      fprintf (fp, "</td>\n");

      print_html_end_tr (fp);

      free (bandwidth);
   }

   print_html_end_tbody (fp);
   print_html_end_table (fp);
}

static void
print_html_summary_field (FILE * fp, int hits, char *field)
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
   } else
      size = alloc_string ("N/A");

   bw = filesize_str ((float) logger->resp_size);
   if (conf.ifile == NULL)
      conf.ifile = "STDIN";

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
    fprintf (fp, "<a class=\"pure-menu-heading\" href=\"http://goaccess.prosoftcorp.com/\">GoAccess</a>");
    fprintf (fp, "<ul>");
    fprintf (fp, "<li><a href=\"#\">Overall</a></li>");
    fprintf (fp, "<li><a href=\"#%s\">Unique visitors</a></li>", VISIT_ID);
    fprintf (fp, "<li><a href=\"#%s\">Requested files</a></li>", REQUE_ID);
    fprintf (fp, "<li><a href=\"#%s\">Requested static files</a></li>", STATI_ID);
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
    fprintf (fp, "<p>Generated by<br />GoAccess %s<br />—<br />%s</p>", GO_VERSION, now);
    fprintf (fp, "</div>");
    fprintf (fp, "</div> <!-- menu -->");

    fprintf (fp, "<div id=\"main\" class=\"pure-u-1\">");
    fprintf (fp, "<div class=\"l-box\">");
}

/* entry point to generate a report writing it to the fp */
void
output_html (GLog * logger, GHolder * holder)
{
   generate_time ();

   FILE *fp = stdout;
   char now[20];
   strftime(now, 20, "%Y-%m-%d %H:%M:%S", now_tm);

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
