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

#ifndef LABLES_H_INCLUDED
#define LABLES_H_INCLUDED

/* Overall Stats Labels */
#define T_DASH       "Dashboard"
#define T_HEAD       "Overall Analyzed Requests"

#define T_DATETIME   "Date/Time"
#define T_REQUESTS   "Total Requests"
#define T_GEN_TIME   "Processed Time"
#define T_FAILED     "Failed Requests"
#define T_VALID      "Valid Requests"
#define T_UNIQUE_VIS "Unique Visitors"
#define T_UNIQUE_FIL "Unique Files"
#define T_EXCLUDE_IP "Excl. IP Hits"
#define T_REFERRER   "Referrers"
#define T_UNIQUE404  "Unique 404"
#define T_STATIC_FIL "Static Files"
#define T_LOG        "Log Size"
#define T_BW         "Bandwidth"
#define T_LOG_PATH   "Log File"

/* Spinner Label Format */
#define SPIN_FMT "%s"
#define SPIN_FMTM "%s [%'d] [%'lld/s]"
#define SPIN_LBL 50     /* max length of the progress spinner */

#define INCLUDE_BOTS " - Including spiders"

/* Module Labels and Descriptions */
#define VISIT_HEAD  "Unique visitors per day"
#define VISIT_DESC  "Hits having the same IP, date and agent are a unique visit."
#define VISIT_ID    "visitors"
#define VISIT_LABEL "Visitors"

#define REQUE_HEAD  "Requested Files (URLs)"
#define REQUE_DESC  "Top requests sorted by hits [, avgts, cumts, maxts, mthd, proto]"
#define REQUE_ID    "requests"
#define REQUE_LABEL "Requests"

#define STATI_HEAD  "Static Requests"
#define STATI_DESC  "Top static requests sorted by hits [, avgts, cumts, maxts, mthd, proto]"
#define STATI_ID    "static_requests"
#define STATI_LABEL "Static Requests"

#define VTIME_HEAD  "Time Distribution"
#define VTIME_DESC  "Data sorted by hour [, avgts, cumts, maxts]"
#define VTIME_ID    "visit_time"
#define VTIME_LABEL "Time"

#define VHOST_HEAD  "Virtual Hosts"
#define VHOST_DESC  "Data sorted by hits [, avgts, cumts, maxts]"
#define VHOST_ID    "vhosts"
#define VHOST_LABEL "Virtual Hosts"

#define FOUND_HEAD  "Not Found URLs (404s)"
#define FOUND_DESC  "Top not found URLs sorted by hits [, avgts, cumts, maxts, mthd, proto]"
#define FOUND_ID    "not_found"
#define FOUND_LABEL "Not Found"

#define HOSTS_HEAD  "Visitor Hostnames and IPs"
#define HOSTS_DESC  "Top visitor hosts sorted by hits [, avgts, cumts, maxts]"
#define HOSTS_ID    "hosts"
#define HOSTS_LABEL "Hosts"

#define OPERA_HEAD  "Operating Systems"
#define OPERA_DESC  "Top Operating Systems sorted by hits [, avgts, cumts, maxts]"
#define OPERA_ID    "os"
#define OPERA_LABEL "OS"

#define BROWS_HEAD  "Browsers"
#define BROWS_DESC  "Top Browsers sorted by hits [, avgts, cumts, maxts]"
#define BROWS_ID    "browsers"
#define BROWS_LABEL "Browsers"

#define REFER_HEAD  "Referrers URLs"
#define REFER_DESC  "Top Requested Referrers sorted by hits [, avgts, cumts, maxts]"
#define REFER_ID    "referrers"
#define REFER_LABEL "Referrers"

#define SITES_HEAD  "Referring Sites"
#define SITES_DESC  "Top Referring Sites sorted by hits [, avgts, cumts, maxts]"
#define SITES_ID    "referring_sites"
#define SITES_LABEL "Referring Sites"

#define KEYPH_HEAD  "Keyphrases from Google's search engine"
#define KEYPH_DESC  "Top Keyphrases sorted by hits [, avgts, cumts, maxts]"
#define KEYPH_ID    "keyphrases"
#define KEYPH_LABEL "Keyphrases"

#define GEOLO_HEAD  "Geo Location"
#define GEOLO_DESC  "Continent > Country sorted by unique hits [, avgts, cumts, maxts]"
#define GEOLO_ID    "geolocation"
#define GEOLO_LABEL "Geo Location"

#define CODES_HEAD  "HTTP Status Codes"
#define CODES_DESC  "Top HTTP Status Codes sorted by hits [, avgts, cumts, maxts]"
#define CODES_ID    "status_codes"
#define CODES_LABEL "Status Codes"

#define GENER_ID   "general"

/* Overall Statistics CSV/JSON Keys */
#define OVERALL_DATETIME  "date_time"
#define OVERALL_REQ       "total_requests"
#define OVERALL_VALID     "valid_requests"
#define OVERALL_GENTIME   "generation_time"
#define OVERALL_FAILED    "failed_requests"
#define OVERALL_VISITORS  "unique_visitors"
#define OVERALL_FILES     "unique_files"
#define OVERALL_EXCL_HITS "excluded_hits"
#define OVERALL_REF       "unique_referrers"
#define OVERALL_NOTFOUND  "unique_not_found"
#define OVERALL_STATIC    "unique_static_files"
#define OVERALL_LOGSIZE   "log_size"
#define OVERALL_BANDWIDTH "bandwidth"
#define OVERALL_LOG       "log_path"

/* Metric Labels */
#define MTRC_HITS_LBL            "Hits"
#define MTRC_VISITORS_LBL        "Visitors"
#define MTRC_VISITORS_SHORT_LBL  "Vis."
#define MTRC_BW_LBL              "Bandwidth"
#define MTRC_AVGTS_LBL           "Avg. T.S."
#define MTRC_CUMTS_LBL           "Cum. T.S."
#define MTRC_MAXTS_LBL           "Max. T.S."
#define MTRC_METHODS_LBL         "Method"
#define MTRC_METHODS_SHORT_LBL   "Mtd"
#define MTRC_PROTOCOLS_LBL       "Protocol"
#define MTRC_PROTOCOLS_SHORT_LBL "Proto"
#define MTRC_CITY_LBL            "City"
#define MTRC_COUNTRY_LBL         "Country"
#define MTRC_HOSTNAME_LBL        "Hostname"
#define MTRC_DATA_LBL            "Data"

/* Find Labels */
#define FIND_HEAD    "Find pattern in all views"
#define FIND_DESC    "Regex allowed - ^g to cancel - TAB switch case"

#endif
