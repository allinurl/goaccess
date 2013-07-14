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

static void
print_html_header (FILE * fp, char *now)
{
   fprintf (fp, "<html>\n");
   fprintf (fp, "<head>\n");
   fprintf (fp, "<title>Server Statistics - %s</title>\n", now);
   fprintf (fp, "<meta charset=\"UTF-8\" />");
   fprintf (fp, "<meta name=\"robots\" content=\"noindex, nofollow\" />");
   fprintf (fp, "<script type=\"text/javascript\">\n");

   fprintf (fp, "function t(c){for(var ");
   fprintf (fp, "b=c.parentNode.parentNode.parentNode.");
   fprintf (fp, "getElementsByTagName('tr'),a=0;a<b.length;a++)");
   fprintf (fp, "'hide'==b[a].className?(b[a].className='show',");
   fprintf (fp, "c.innerHTML='[-] Collapse'):'show'==b[a].className&&");
   fprintf (fp, "(b[a].className='hide',c.innerHTML='[+] Expand')};");

   fprintf (fp, "function a(c){var b=c.parentNode.parentNode.nextSibling;");
   fprintf (fp, "while(b && b.nodeType != 1) b=b.nextSibling;");
   fprintf (fp, "'a-hide'==b.className?(b.className='a-show',");
   fprintf (fp, "c.innerHTML='[-]'):'a-show'==b.className&&");
   fprintf (fp, "(b.className='a-hide',c.innerHTML='[+]')};");

   fprintf (fp, "</script>\n");
   fprintf (fp, "<style type=\"text/css\">\n");

   fprintf (fp, "body {");
   fprintf (fp, "    font-family: Verdana;");
   fprintf (fp, "    font-size: 11px;");
   fprintf (fp, "}");
   fprintf (fp, "table.a1, table.a2 {");
   fprintf (fp, "    border-spacing: 0;");
   fprintf (fp, "    font-size: 11px;");
   fprintf (fp, "    margin: 5px;");
   fprintf (fp, "    table-layout: fixed;");
   fprintf (fp, "    white-space: nowrap;");
   fprintf (fp, "}");
   fprintf (fp, "table.a1 { width: 600px }");
   fprintf (fp, "table.a2 {");
   fprintf (fp, "    background-color: #EEE;");
   fprintf (fp, "    padding: 0 5px;");
   fprintf (fp, "    width: 590px;");
   fprintf (fp, "}");
   fprintf (fp, ".head {");
   fprintf (fp, "    background-color: #222;");
   fprintf (fp, "    color: #FFF;");
   fprintf (fp, "    padding: 5px;");
   fprintf (fp, "}");
   fprintf (fp, ".r, .s {");
   fprintf (fp, "    cursor: pointer;");
   fprintf (fp, "    font-size: 9px;");
   fprintf (fp, "}");
   fprintf (fp, ".r { float: right }");
   fprintf (fp, ".red {");
   fprintf (fp, "    color: #D20B2C;");
   fprintf (fp, "    font-weight: 700;");
   fprintf (fp, "}");
   fprintf (fp, ".lnk {");
   fprintf (fp, "    font-size: 10px;");
   fprintf (fp, "    font-weight: 700;");
   fprintf (fp, "}");
   fprintf (fp, "a { color: #222 }");
   fprintf (fp, ".desc {");
   fprintf (fp, "    background-color: #EEE;");
   fprintf (fp, "    color: #222;");
   fprintf (fp, "    padding: 5px;");
   fprintf (fp, "}");
   fprintf (fp, ".d1, .d2 {");
   fprintf (fp, "    -ms-text-overflow: ellipsis;");
   fprintf (fp, "    overflow: hidden;");
   fprintf (fp, "    text-overflow: ellipsis;");
   fprintf (fp, "    white-space: nowrap;");
   fprintf (fp, "}");
   fprintf (fp, "td.d1 { border-bottom: 1px dotted #eee }");
   fprintf (fp, ".d2 { border-bottom: 1px dotted #ccc }");
   fprintf (fp, ".bar {");
   fprintf (fp, "    background-color: #777;");
   fprintf (fp, "    border-right: 1px #FFF solid;");
   fprintf (fp, "    height: 10px;");
   fprintf (fp, "}");
   fprintf (fp, ".a-hide, .hide { display: none }");

   fprintf (fp, "</style>\n");
   fprintf (fp, "</head>\n");
   fprintf (fp, "<body>\n");
}

static void
print_html_footer (FILE * fp, char *now)
{
   fprintf (fp, "<p class=\"lnk\">Generated by: ");
   fprintf (fp, "<a href=\"http://goaccess.prosoftcorp.com/\">GoAccess</a> ");
   fprintf (fp, "%s on %s</p>", GO_VERSION, now);
   fprintf (fp, "</body>\n");
   fprintf (fp, "</html>");
}

static void
print_html_begin_table (FILE * fp)
{
   fprintf (fp, "<table class=\"a1\">\n");
}

static void
print_html_end_table (FILE * fp)
{
   fprintf (fp, "</table>\n");
}

static void
print_html_head_top (FILE * fp, const char *s, int span, int exp)
{
   fprintf (fp, "<tr>");
   if (exp) {
      fprintf (fp, "<td colspan=\"%d\" class=\"head\">", span);
      fprintf (fp, "<span class=\"r\" onclick=\"t(this)\">Expand [+]</span>");
      fprintf (fp, "<span>%s</span>", s);
      fprintf (fp, "</td>");
   } else
      fprintf (fp, "<td class=\"head\" colspan=\"%d\">%s</td>", span, s);
   fprintf (fp, "</tr>\n");
}

static void
print_html_head_bottom (FILE * fp, const char *s, int colspan)
{
   fprintf (fp, "<tr>");
   fprintf (fp, "<td class=\"desc\" colspan=\"%d\">%s</td>", colspan, s);
   fprintf (fp, "</tr>\n");
}

static void
print_html_col (FILE * fp, int w)
{
   fprintf (fp, "<col style=\"width:%dpx\">\n", w);
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
   char *data, *val = NULL;
   int hits;
   float percent;
   GSubItem *iter;

   for (iter = sub_list->head; iter; iter = iter->next) {
      hits = iter->hits;
      data = (char *) iter->data;
      percent = get_percentage (process, hits);
      percent = percent < 0 ? 0 : percent;

      val = xmalloc (snprintf (NULL, 0, "|`- %s", data) + 1);
      sprintf (val, "|`- %s", data);

      print_html_begin_tr (fp, 1);
      fprintf (fp, "<td class=\"d1\">%d</td>", hits);
      fprintf (fp, "<td class=\"d1\">%4.2f%%</td>", percent);

      fprintf (fp, "<td class=\"d1\">");
      clean_output (fp, val);
      fprintf (fp, "</td>");

      print_html_end_tr (fp);
      free (val);
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

   print_html_begin_table (fp);

   print_html_col (fp, 60);
   print_html_col (fp, 80);
   print_html_col (fp, 460);

   print_html_head_top (fp, CODES_HEAD, 3, 1);
   print_html_head_bottom (fp, CODES_DESC, 3);

   for (i = 0; i < h->idx; i++) {
      hits = h->items[i].hits;
      data = h->items[i].data;
      percent = get_percentage (process, hits);
      percent = percent < 0 ? 0 : percent;

      print_html_begin_tr (fp, 0);
      fprintf (fp, "<td class=\"d1\">%d</td>", hits);
      fprintf (fp, "<td class=\"d1\">%4.2f%%</td>", percent);
      fprintf (fp, "<td class=\"d1\">%s</td>", data);
      print_html_end_tr (fp);

      GSubList *sub_list = h->items[i].sub_list;
      print_html_sub_status (fp, sub_list, process);
   }
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

   print_html_begin_table (fp);

   print_html_col (fp, 60);
   print_html_col (fp, 80);
   print_html_col (fp, 460);

   switch (h->module) {
    case REFERRERS:
       print_html_head_top (fp, REFER_HEAD, 3, h->idx > OUTPUT_N ? 1 : 0);
       print_html_head_bottom (fp, REFER_DESC, 3);
       break;
    case NOT_FOUND:
       print_html_head_top (fp, FOUND_HEAD, 3, h->idx > OUTPUT_N ? 1 : 0);
       print_html_head_bottom (fp, FOUND_DESC, 3);
       break;
    case REFERRING_SITES:
       print_html_head_top (fp, SITES_HEAD, 3, h->idx > OUTPUT_N ? 1 : 0);
       print_html_head_bottom (fp, SITES_DESC, 3);
       break;
    case KEYPHRASES:
       print_html_head_top (fp, KEYPH_HEAD, 3, h->idx > OUTPUT_N ? 1 : 0);
       print_html_head_bottom (fp, KEYPH_DESC, 3);
       break;
    default:
       break;
   }

   until = h->idx < MAX_CHOICES ? h->idx : MAX_CHOICES;
   for (i = 0; i < until; i++) {
      hits = h->items[i].hits;
      data = h->items[i].data;
      percent = get_percentage (process, hits);
      percent = percent < 0 ? 0 : percent;

      print_html_begin_tr (fp, i > OUTPUT_N ? 1 : 0);
      fprintf (fp, "<td class=\"d1\">%d</td>", hits);
      fprintf (fp, "<td class=\"d1\">%4.2f%%</td>", percent);
      fprintf (fp, "<td class=\"d1\">");
      fprintf (fp, "<span class=\"d1\">");
      clean_output (fp, data);
      fprintf (fp, "</span>");
      fprintf (fp, "</td>");
      print_html_end_tr (fp);
   }

   print_html_end_table (fp);
}

static void
print_html_sub_os (FILE * fp, GSubList * sub_list, int process)
{
   char *data, *val = NULL;
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

      val = xmalloc (snprintf (NULL, 0, "|`- %s", data) + 1);
      sprintf (val, "|`- %s", data);

      print_html_begin_tr (fp, 1);
      fprintf (fp, "<td class=\"d1\">%d</td>", hits);
      fprintf (fp, "<td class=\"d1\">%4.2f%%</td>", percent);

      fprintf (fp, "<td class=\"d1\">");
      clean_output (fp, val);
      fprintf (fp, "</td>");

      fprintf (fp, "<td class=\"d1\">");
      fprintf (fp, "<div class=\"bar\" style=\"width:%f%%\"></div>", l);
      fprintf (fp, "</td>");
      print_html_end_tr (fp);
      free (val);
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

   print_html_begin_table (fp);

   print_html_col (fp, 60);
   print_html_col (fp, 80);
   print_html_col (fp, 220);
   print_html_col (fp, 240);

   /* *INDENT-OFF* */
   if (h->module == OS) {
      print_html_head_top (fp, OPERA_HEAD, 4, 1);
      print_html_head_bottom (fp, OPERA_DESC, 4);
   }
   else if (h->module == BROWSERS) {
      print_html_head_top (fp, BROWS_HEAD, 4, 1);
      print_html_head_bottom (fp, BROWS_DESC, 4);
   }
   /* *INDENT-ON* */

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
      fprintf (fp, "<td class=\"d1\">%d</td>", hits);
      if (hits == max)
         fprintf (fp, "<td class=\"d1 red\">%4.2f%%</td>", percent);
      else
         fprintf (fp, "<td class=\"d1\">%4.2f%%</td>", percent);

      /* data */
      fprintf (fp, "<td class=\"d1\">");
      clean_output (fp, data);
      fprintf (fp, "</td>");

      fprintf (fp, "<td class=\"d1\">");
      fprintf (fp, "<div class=\"bar\" style=\"width:%f%%\"></div>", l);
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

   print_html_begin_table (fp);

   print_html_col (fp, 20);
   print_html_col (fp, 60);
   print_html_col (fp, 80);
   print_html_col (fp, 80);
   print_html_col (fp, 80);
   print_html_col (fp, 120);
   print_html_col (fp, 160);

   print_html_head_top (fp, HOSTS_HEAD, 7, h->idx > OUTPUT_N ? 1 : 0);
   print_html_head_bottom (fp, HOSTS_DESC, 7);

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
         fprintf (fp, "<span class=\"s\" onclick=\"a(this)\">[+]</span>");
      else
         fprintf (fp, "<span class=\"s\">-</span>");
      fprintf (fp, "</td>");

      fprintf (fp, "<td class=\"d1\">%d</td>", hits);
      fprintf (fp, "<td class=\"d1\">%4.2f%%</td>", percent);
      fprintf (fp, "<td class=\"d1\">%s</td>", bandwidth);
      /* usecs */
      usecs = usecs_to_str (h->items[i].usecs);
      fprintf (fp, "<td class=\"d1\">%s</td>", usecs);
      fprintf (fp, "<td class=\"d1\">%s</td>", data);

      fprintf (fp, "<td class=\"d1\">");
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

         fprintf (fp, "<tr class=\"a-hide\">\n");
         fprintf (fp, "<td colspan=\"6\">\n");
         fprintf (fp, "<div style=\"padding:10px 0;\">");
         fprintf (fp, "<table class=\"a2\">");

         /* output agents from struct */
         for (j = 0; (j < 10) && (agents[j].agents != NULL); j++) {
            print_html_begin_tr (fp, 0);
            fprintf (fp, "<td class=\"d2\">");
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

   print_html_begin_table (fp);

   print_html_col (fp, 60);
   print_html_col (fp, 80);
   print_html_col (fp, 100);
   print_html_col (fp, 100);
   print_html_col (fp, 260);

   /* *INDENT-OFF* */
   if (ht == ht_requests) {
      print_html_head_top (fp, REQUE_HEAD, 5, h->idx > OUTPUT_N ? 1 : 0);
      print_html_head_bottom (fp, REQUE_DESC, 5);
   }
   else if (ht == ht_requests_static) {
      print_html_head_top (fp, STATI_HEAD, 5, h->idx > OUTPUT_N ? 1 : 0);
      print_html_head_bottom (fp, STATI_DESC, 5);
   }
   else if (ht == ht_not_found_requests) {
      print_html_head_top (fp, FOUND_HEAD, 5, h->idx > OUTPUT_N ? 1 : 0);
      print_html_head_bottom (fp, FOUND_DESC, 5);
   }
   /* *INDENT-ON* */

   until = h->idx < MAX_CHOICES ? h->idx : MAX_CHOICES;
   for (i = 0; i < until; i++) {
      hits = h->items[i].hits;
      data = h->items[i].data;
      percent = get_percentage (process, hits);
      percent = percent < 0 ? 0 : percent;
      bandwidth = filesize_str (h->items[i].bw);

      print_html_begin_tr (fp, i > OUTPUT_N ? 1 : 0);

      /* hits */
      fprintf (fp, "<td class=\"d1\">%d</td>", hits);
      /* percent */
      fprintf (fp, "<td class=\"d1\">%4.2f%%</td>", percent);
      /* bandwidth */
      fprintf (fp, "<td class=\"d1\">%s</td>", bandwidth);

      /* usecs */
      usecs = usecs_to_str (h->items[i].usecs);
      fprintf (fp, "<td class=\"d1\">%s</td>", usecs);

      /* data */
      fprintf (fp, "<td class=\"d1\">");
      fprintf (fp, "<span class=\"d1\">");
      clean_output (fp, data);
      fprintf (fp, "</span>");
      fprintf (fp, "</td>");

      print_html_end_tr (fp);

      free (usecs);
      free (bandwidth);
   }

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

   print_html_begin_table (fp);

   print_html_col (fp, 60);
   print_html_col (fp, 80);
   print_html_col (fp, 100);
   print_html_col (fp, 100);
   print_html_col (fp, 260);

   print_html_head_top (fp, VISIT_HEAD, 5, h->idx > OUTPUT_N ? 1 : 0);
   print_html_head_bottom (fp, VISIT_DESC, 5);

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
      fprintf (fp, "<td class=\"d1\">%d</td>", hits);
      if (hits == max)
         fprintf (fp, "<td class=\"d1 red\">%4.2f%%</td>", percent);
      else
         fprintf (fp, "<td class=\"d1\">%4.2f%%</td>", percent);

      /* date */
      convert_date (buf, data, "%Y%m%d", "%d/%b/%Y", DATE_LEN);
      fprintf (fp, "<td class=\"d1\">%s</td>", buf);

      /* bandwidth */
      fprintf (fp, "<td class=\"d1\">%s</td>", bandwidth);

      /* bars */
      fprintf (fp, "<td class=\"d1\">");
      fprintf (fp, "<div class=\"bar\" style=\"width:%f%%\"></div>", l);
      fprintf (fp, "</td>");

      print_html_end_tr (fp);

      free (bandwidth);
   }

   print_html_end_table (fp);
}

static void
print_html_summary_field (FILE * fp, int hits, char *field)
{
   fprintf (fp, "<td class=\"d1\"><span class=\"d1\">%s</span></td>", field);
   fprintf (fp, "<td class=\"d1\"><span class=\"d1\">%d</span></td>", hits);
}

static void
print_html_summary (FILE * fp, GLog * logger)
{
   char *bw, *size;
   off_t log_size;
   print_html_begin_table (fp);

   print_html_col (fp, 100);
   print_html_col (fp, 75);
   print_html_col (fp, 100);
   print_html_col (fp, 60);
   print_html_col (fp, 80);
   print_html_col (fp, 60);
   print_html_col (fp, 40);
   print_html_col (fp, 80);

   print_html_head_top (fp, T_HEAD, 8, 0);

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

   fprintf (fp, "<td class=\"d1\"><span class=\"d1\">%s</span></td>", T_LOG);
   fprintf (fp, "<td class=\"d1\"><span class=\"d1\">%s</span></td>", size);
   print_html_end_tr (fp);

   print_html_begin_tr (fp, 0);
   print_html_summary_field (fp, logger->invalid, T_F_REQUESTS);
   print_html_summary_field (fp, g_hash_table_size (ht_requests), T_UNIQUE_FIL);
   print_html_summary_field (fp, g_hash_table_size (ht_not_found_requests),
                             T_UNIQUE404);
   fprintf (fp, "<td class=\"d1\"><span class=\"d1\">%s</span></td>", T_BW);
   fprintf (fp, "<td class=\"d1\"><span class=\"d1\">%s</span></td>", bw);
   print_html_end_tr (fp);

   print_html_begin_tr (fp, 0);
   fprintf (fp, "<td class=\"d1\">");
   fprintf (fp, "<span class=\"d1\">%s</span>", T_GEN_TIME);
   fprintf (fp, "</td>");

   fprintf (fp, "<td class=\"d1\">");
   fprintf (fp, "<span class=\"d1\">%lu</span>", ((int) end_proc - start_proc));
   fprintf (fp, "</td>");

   print_html_summary_field (fp, g_hash_table_size (ht_requests_static),
                             T_STATIC_FIL);
   fprintf (fp, "<td class=\"d1\" colspan=\"4\">");
   fprintf (fp, "<span class=\"d1\">%s</span>", conf.ifile);
   fprintf (fp, "</td>");

   print_html_end_tr (fp);
   print_html_end_table (fp);
   free (bw);
   free (size);
}

void
output_html (GLog * logger, GHolder * holder)
{
   generate_time ();

   FILE *fp = stdout;

   print_html_header (fp, asctime (now_tm));
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

   print_html_footer (fp, asctime (now_tm));
}
