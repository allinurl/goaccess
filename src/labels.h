/**
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2016 Gerardo Orellana <hello @ goaccess.io>
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

#ifndef LABELS_H_INCLUDED
#define LABELS_H_INCLUDED

#include <libintl.h>

#define _(String) dgettext (PACKAGE , String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

/* Overall Stats Labels */
#define T_DASH                    _("Dashboard")
#define T_HEAD                    _("Overall Analyzed Requests")
#define T_DASH_HEAD               _("Dashboard - Overall Analyzed Requests")

#define T_DATETIME                _("Date/Time")
#define T_REQUESTS                _("Total Requests")
#define T_GEN_TIME                _("Init. Proc. Time")
#define T_FAILED                  _("Failed Requests")
#define T_VALID                   _("Valid Requests")
#define T_UNIQUE_VIS              _("Unique Visitors")
#define T_UNIQUE_FIL              _("Unique Files")
#define T_EXCLUDE_IP              _("Excl. IP Hits")
#define T_REFERRER                _("Referrers")
#define T_UNIQUE404               _("Unique 404")
#define T_STATIC_FIL              _("Static Files")
#define T_LOG                     _("Log Size")
#define T_BW                      _("Bandwidth")
#define T_LOG_PATH                _("Log Source")

/* Module Labels and Descriptions */
#define VISIT_HEAD                N_("Unique visitors per day")
#define VISIT_HBOTS               N_("Unique visitors per day - Including spiders")
#define VISIT_DESC                N_("Hits having the same IP, date and agent are a unique visit.")
#define VISIT_LABEL               N_("Visitors")

#define REQUE_HEAD                N_("Requested Files (URLs)")
#define REQUE_DESC                N_("Top requests sorted by hits [, avgts, cumts, maxts, mthd, proto]")
#define REQUE_LABEL               N_("Requests")

#define STATI_HEAD                N_("Static Requests")
#define STATI_DESC                N_("Top static requests sorted by hits [, avgts, cumts, maxts, mthd, proto]")
#define STATI_LABEL               N_("Static Requests")

#define VTIME_HEAD                N_("Time Distribution")
#define VTIME_DESC                N_("Data sorted by hour [, avgts, cumts, maxts]")
#define VTIME_LABEL               N_("Time")

#define VHOST_HEAD                N_("Virtual Hosts")
#define VHOST_DESC                N_("Data sorted by hits [, avgts, cumts, maxts]")
#define VHOST_LABEL               N_("Virtual Hosts")

#define RUSER_HEAD                N_("Remote User (HTTP authentication)")
#define RUSER_DESC                N_("Data sorted by hits [, avgts, cumts, maxts]")
#define RUSER_LABEL               N_("Remote User")

#define FOUND_HEAD                N_("Not Found URLs (404s)")
#define FOUND_DESC                N_("Top not found URLs sorted by hits [, avgts, cumts, maxts, mthd, proto]")
#define FOUND_LABEL               N_("Not Found")

#define HOSTS_HEAD                N_("Visitor Hostnames and IPs")
#define HOSTS_DESC                N_("Top visitor hosts sorted by hits [, avgts, cumts, maxts]")
#define HOSTS_LABEL               N_("Hosts")

#define OPERA_HEAD                N_("Operating Systems")
#define OPERA_DESC                N_("Top Operating Systems sorted by hits [, avgts, cumts, maxts]")
#define OPERA_LABEL               N_("OS")

#define BROWS_HEAD                N_("Browsers")
#define BROWS_DESC                N_("Top Browsers sorted by hits [, avgts, cumts, maxts]")
#define BROWS_LABEL               N_("Browsers")

#define REFER_HEAD                N_("Referrers URLs")
#define REFER_DESC                N_("Top Requested Referrers sorted by hits [, avgts, cumts, maxts]")
#define REFER_LABEL               N_("Referrers")

#define SITES_HEAD                N_("Referring Sites")
#define SITES_DESC                N_("Top Referring Sites sorted by hits [, avgts, cumts, maxts]")
#define SITES_LABEL               N_("Referring Sites")

#define KEYPH_HEAD                N_("Keyphrases from Google's search engine")
#define KEYPH_DESC                N_("Top Keyphrases sorted by hits [, avgts, cumts, maxts]")
#define KEYPH_LABEL               N_("Keyphrases")

#define GEOLO_HEAD                N_("Geo Location")
#define GEOLO_DESC                N_("Continent > Country sorted by unique hits [, avgts, cumts, maxts]")
#define GEOLO_LABEL               N_("Geo Location")

#define CODES_HEAD                N_("HTTP Status Codes")
#define CODES_DESC                N_("Top HTTP Status Codes sorted by hits [, avgts, cumts, maxts]")
#define CODES_LABEL               N_("Status Codes")

/* Metric Labels */
#define MTRC_HITS_LBL             _("Hits")
#define MTRC_VISITORS_LBL         _("Visitors")
#define MTRC_VISITORS_SHORT_LBL   _("Vis.")
#define MTRC_BW_LBL               _("Bandwidth")
#define MTRC_AVGTS_LBL            _("Avg. T.S.")
#define MTRC_CUMTS_LBL            _("Cum. T.S.")
#define MTRC_MAXTS_LBL            _("Max. T.S.")
#define MTRC_METHODS_LBL          _("Method")
#define MTRC_METHODS_SHORT_LBL    _("Mtd")
#define MTRC_PROTOCOLS_LBL        _("Protocol")
#define MTRC_PROTOCOLS_SHORT_LBL  _("Proto")
#define MTRC_CITY_LBL             _("City")
#define MTRC_COUNTRY_LBL          _("Country")
#define MTRC_HOSTNAME_LBL         _("Hostname")
#define MTRC_DATA_LBL             _("Data")

/* Find Labels */
#define FIND_HEAD                 _("Find pattern in all views")
#define FIND_DESC                 _("Regex allowed - ^g to cancel - TAB switch case")

/* Sort Labels */
#define SORT_ASC_SEL              _("[x] ASC [ ] DESC")
#define SORT_DESC_SEL             _("[ ] ASC [x] DESC")

#define ERR_HEADER                _("Format Errors - Verify your log/date/time format")
#define CSENSITIVE                _("[x] case sensitive")
#define CISENSITIVE               _("[ ] case sensitive")

#endif // for #ifndef LABELS_H
