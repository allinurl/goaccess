#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "ir.h"

#include "khash.h"
#include "settings.h"
#include "util.h"
#include "xmalloc.h"

KHASH_MAP_INIT_STR (ir_strmap, uint32_t)
KHASH_MAP_INIT_INT64 (ir_u64map, uint32_t)

typedef struct IRSourceAgg_ {
  char *source;
  uint64_t hits;
  uint64_t status_2xx;
  uint64_t status_3xx;
  uint64_t status_4xx;
  uint64_t status_5xx;
  uint64_t auth_hits;
  uint64_t auth_401;
  uint64_t auth_403;
  uint64_t auth_success;
  uint64_t success_after_failures;
  uint64_t distinct_paths;
  uint64_t distinct_404_paths;
  uint64_t scanner_patterns;
  uint64_t sensitive_targets;
  uint64_t rare_methods;
  uint64_t max_burst;
  uint64_t extensions_targeted;
  uint8_t rare_ua;
  uint32_t suspicion_score;
  char *user_agent;
  char *country;
  char *asn;
} IRSourceAgg;

typedef struct IRTargetAgg_ {
  char *resource;
  IRAssetClass asset_class;
  uint64_t hits;
  uint64_t unique_sources;
  uint64_t suspicious_sources;
  uint64_t status_2xx;
  uint64_t status_3xx;
  uint64_t status_4xx;
  uint64_t status_5xx;
  uint32_t impact_score;
} IRTargetAgg;

typedef struct IRTimelineAgg_ {
  uint64_t minute_ts;
  uint64_t hits;
  uint64_t status_4xx;
  uint64_t status_5xx;
  uint64_t new_scanners;
  uint64_t burst_peak;
} IRTimelineAgg;

typedef struct IRAuthState_ {
  uint64_t failures;
} IRAuthState;

typedef struct IRUACount_ {
  char *user_agent;
  uint64_t hits;
} IRUACount;

typedef struct IREdge_ {
  uint32_t source_idx;
  uint32_t target_idx;
} IREdge;

typedef struct IRCaseSummary_ {
  uint64_t total_events;
  uint64_t unique_sources;
  uint64_t approx_bytes;
  uint64_t start_ts;
  uint64_t end_ts;
  uint64_t peak_ts;
  uint64_t peak_hits;
  uint64_t status_2xx;
  uint64_t status_3xx;
  uint64_t status_4xx;
  uint64_t status_5xx;
} IRCaseSummary;

typedef struct IRContext_ {
  IRCaseSummary summary;

  IRSourceAgg *sources;
  size_t sources_size;
  size_t sources_cap;
  khash_t (ir_strmap) *source_map;

  IRTargetAgg *targets;
  size_t targets_size;
  size_t targets_cap;
  khash_t (ir_strmap) *target_map;

  IRTimelineAgg *timeline;
  size_t timeline_size;
  size_t timeline_cap;
  khash_t (ir_u64map) *timeline_map;

  IRUACount *ua_counts;
  size_t ua_size;
  size_t ua_cap;
  khash_t (ir_strmap) *ua_map;

  IREdge *edges;
  size_t edges_size;
  size_t edges_cap;
  khash_t (ir_strmap) *edge_map;

  khash_t (ir_strmap) *source_path_seen;
  khash_t (ir_strmap) *source_404_seen;
  khash_t (ir_strmap) *source_target_seen;
  khash_t (ir_strmap) *auth_state_map;
  khash_t (ir_strmap) *burst_map;

  IRAuthState *auth_states;
  size_t auth_states_size;
  size_t auth_states_cap;

  IRCaseSummaryItem *case_items;
  size_t case_items_size;
  IRSuspiciousSourceItem *source_items;
  size_t source_items_size;
  IRSensitiveTargetItem *target_items;
  size_t target_items_size;
  IRTimelineItem *timeline_items;
  size_t timeline_items_size;
  IRReconItem *recon_items;
  size_t recon_items_size;
  IRAuthAbuseItem *auth_items;
  size_t auth_items_size;

  int initialized;
  int finalized;
} IRContext;

static IRContext irctx;

typedef struct IRRule_ {
  const char *pattern;
  IRAssetClass asset_class;
  int is_auth;
  int is_scanner;
} IRRule;

static const IRRule ir_rules[] = {
  {"/wp-login.php", IR_ASSET_AUTH, 1, 1},
  {"/xmlrpc.php", IR_ASSET_AUTH, 0, 1},
  {"/login", IR_ASSET_AUTH, 1, 0},
  {"/signin", IR_ASSET_AUTH, 1, 0},
  {"/auth", IR_ASSET_AUTH, 1, 0},
  {"/oauth", IR_ASSET_AUTH, 1, 0},
  {"/session", IR_ASSET_AUTH, 1, 0},
  {"/sso", IR_ASSET_AUTH, 1, 0},
  {"/admin", IR_ASSET_ADMIN, 0, 0},
  {"/wp-admin", IR_ASSET_ADMIN, 0, 0},
  {"/manage", IR_ASSET_ADMIN, 0, 0},
  {"/console", IR_ASSET_ADMIN, 0, 0},
  {"/api/", IR_ASSET_API, 0, 0},
  {"/internal", IR_ASSET_INTERNAL, 0, 0},
  {"/actuator", IR_ASSET_SENSITIVE, 0, 1},
  {"/.env", IR_ASSET_SENSITIVE, 0, 1},
  {"/.git", IR_ASSET_SENSITIVE, 0, 1},
  {"/vendor/phpunit", IR_ASSET_SENSITIVE, 0, 1},
  {"/cgi-bin", IR_ASSET_SENSITIVE, 0, 1},
  {"/phpinfo.php", IR_ASSET_SENSITIVE, 0, 1},
  {"/config", IR_ASSET_SENSITIVE, 0, 0},
  {"/backup", IR_ASSET_SENSITIVE, 0, 0},
  {"/export", IR_ASSET_SENSITIVE, 0, 0},
};

static void
free_khash_keys (khash_t (ir_strmap) *hash) {
  khiter_t k;

  if (!hash)
    return;
  for (k = kh_begin (hash); k != kh_end (hash); ++k) {
    if (!kh_exist (hash, k))
      continue;
    free ((char *) kh_key (hash, k));
  }
  kh_destroy (ir_strmap, hash);
}

static void
free_owned_string (char **ptr) {
  if (*ptr) {
    free (*ptr);
    *ptr = NULL;
  }
}

static char *
fmt_u64 (uint64_t value) {
  return u642str (value, 0);
}

static char *
fmt_bytes (uint64_t value) {
  return filesize_str (value);
}

static char *
fmt_percent (uint64_t num, uint64_t den) {
  char buf[64];

  if (!den)
    return xstrdup ("0%");
  snprintf (buf, sizeof buf, "%.2f%%", (100.0 * (double) num) / (double) den);
  return xstrdup (buf);
}

static char *
fmt_ts (uint64_t ts) {
  char buf[64];
  time_t raw = (time_t) ts;
  struct tm tm;

  if (!ts)
    return xstrdup ("-");

  localtime_r (&raw, &tm);
  strftime (buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
  return xstrdup (buf);
}

static char *
fmt_range (uint64_t start_ts, uint64_t end_ts) {
  char *start = fmt_ts (start_ts);
  char *end = fmt_ts (end_ts);
  char *out = xmalloc (strlen (start) + strlen (end) + 5);

  sprintf (out, "%s -> %s", start, end);
  free (start);
  free (end);

  return out;
}

static int
status_bucket (int status) {
  if (status >= 200 && status < 300)
    return 2;
  if (status >= 300 && status < 400)
    return 3;
  if (status >= 400 && status < 500)
    return 4;
  if (status >= 500 && status < 600)
    return 5;
  return 0;
}

static int
is_rare_method (const char *method) {
  if (!method)
    return 0;
  return strcmp (method, "GET") != 0 &&
         strcmp (method, "POST") != 0 &&
         strcmp (method, "HEAD") != 0;
}

static int
looks_like_extension_target (const char *req) {
  const char *dot;

  if (!req)
    return 0;
  dot = strrchr (req, '.');
  if (!dot)
    return 0;
  return strchr (dot, '/') == NULL;
}

static char *
to_lower_dup (const char *str) {
  char *dup;
  size_t i, len;

  if (!str)
    return xstrdup ("");
  len = strlen (str);
  dup = xmalloc (len + 1);
  for (i = 0; i < len; ++i)
    dup[i] = (char) tolower ((unsigned char) str[i]);
  dup[len] = '\0';
  return dup;
}

static void
ensure_cap (void **ptr, size_t *cap, size_t elem_size, size_t need) {
  size_t next = *cap ? *cap : 16;

  if (need <= *cap)
    return;
  while (next < need)
    next *= 2;
  *ptr = xrealloc (*ptr, elem_size * next);
  *cap = next;
}

static uint32_t
intern_string_index (khash_t (ir_strmap) *hash, const char *key, uint32_t next_idx) {
  khiter_t k;
  int ret;
  char *dupkey;

  k = kh_get (ir_strmap, hash, key);
  if (k != kh_end (hash))
    return kh_val (hash, k);

  dupkey = xstrdup (key);
  k = kh_put (ir_strmap, hash, dupkey, &ret);
  if (ret < 0) {
    free (dupkey);
    return next_idx;
  }
  kh_val (hash, k) = next_idx;

  return next_idx;
}

static uint32_t
get_source_idx (const char *source) {
  uint32_t idx = irctx.sources_size;

  idx = intern_string_index (irctx.source_map, source, idx);
  if (idx != irctx.sources_size)
    return idx;

  ensure_cap ((void **) &irctx.sources, &irctx.sources_cap, sizeof (*irctx.sources), irctx.sources_size + 1);
  memset (&irctx.sources[irctx.sources_size], 0, sizeof (*irctx.sources));
  irctx.sources[irctx.sources_size].source = xstrdup (source);
  irctx.sources_size++;
  irctx.summary.unique_sources = irctx.sources_size;

  return idx;
}

static uint32_t
get_target_idx (const char *target, IRAssetClass asset_class) {
  uint32_t idx = irctx.targets_size;

  idx = intern_string_index (irctx.target_map, target, idx);
  if (idx != irctx.targets_size) {
    if (asset_class > irctx.targets[idx].asset_class)
      irctx.targets[idx].asset_class = asset_class;
    return idx;
  }

  ensure_cap ((void **) &irctx.targets, &irctx.targets_cap, sizeof (*irctx.targets), irctx.targets_size + 1);
  memset (&irctx.targets[irctx.targets_size], 0, sizeof (*irctx.targets));
  irctx.targets[irctx.targets_size].resource = xstrdup (target);
  irctx.targets[irctx.targets_size].asset_class = asset_class;
  irctx.targets_size++;

  return idx;
}

static uint32_t
get_ua_idx (const char *user_agent) {
  uint32_t idx = irctx.ua_size;

  idx = intern_string_index (irctx.ua_map, user_agent, idx);
  if (idx != irctx.ua_size)
    return idx;

  ensure_cap ((void **) &irctx.ua_counts, &irctx.ua_cap, sizeof (*irctx.ua_counts), irctx.ua_size + 1);
  memset (&irctx.ua_counts[irctx.ua_size], 0, sizeof (*irctx.ua_counts));
  irctx.ua_counts[irctx.ua_size].user_agent = xstrdup (user_agent);
  irctx.ua_size++;

  return idx;
}

static uint32_t
get_timeline_idx (uint64_t minute_ts) {
  khiter_t k;
  int ret;

  k = kh_get (ir_u64map, irctx.timeline_map, minute_ts);
  if (k != kh_end (irctx.timeline_map))
    return kh_val (irctx.timeline_map, k);

  ensure_cap ((void **) &irctx.timeline, &irctx.timeline_cap, sizeof (*irctx.timeline), irctx.timeline_size + 1);
  memset (&irctx.timeline[irctx.timeline_size], 0, sizeof (*irctx.timeline));
  irctx.timeline[irctx.timeline_size].minute_ts = minute_ts;

  k = kh_put (ir_u64map, irctx.timeline_map, minute_ts, &ret);
  if (ret >= 0)
    kh_val (irctx.timeline_map, k) = irctx.timeline_size;

  irctx.timeline_size++;

  return irctx.timeline_size - 1;
}

static void
remember_edge (uint32_t source_idx, uint32_t target_idx) {
  char key[64];
  khiter_t k;
  int ret;
  char *dupkey;

  snprintf (key, sizeof key, "%u|%u", source_idx, target_idx);
  k = kh_get (ir_strmap, irctx.edge_map, key);
  if (k != kh_end (irctx.edge_map))
    return;

  ensure_cap ((void **) &irctx.edges, &irctx.edges_cap, sizeof (*irctx.edges), irctx.edges_size + 1);
  irctx.edges[irctx.edges_size].source_idx = source_idx;
  irctx.edges[irctx.edges_size].target_idx = target_idx;

  dupkey = xstrdup (key);
  k = kh_put (ir_strmap, irctx.edge_map, dupkey, &ret);
  if (ret >= 0)
    kh_val (irctx.edge_map, k) = irctx.edges_size;
  else
    free (dupkey);

  irctx.edges_size++;
}

static int
mark_seen (khash_t (ir_strmap) *hash, const char *key) {
  khiter_t k;
  int ret;
  char *dupkey;

  k = kh_get (ir_strmap, hash, key);
  if (k != kh_end (hash))
    return 0;

  dupkey = xstrdup (key);
  k = kh_put (ir_strmap, hash, dupkey, &ret);
  if (ret < 0) {
    free (dupkey);
    return 0;
  }
  kh_val (hash, k) = 1;

  return 1;
}

static uint32_t
get_auth_state_idx (const char *key) {
  uint32_t idx = irctx.auth_states_size;

  idx = intern_string_index (irctx.auth_state_map, key, idx);
  if (idx != irctx.auth_states_size)
    return idx;

  ensure_cap ((void **) &irctx.auth_states, &irctx.auth_states_cap, sizeof (*irctx.auth_states), irctx.auth_states_size + 1);
  memset (&irctx.auth_states[irctx.auth_states_size], 0, sizeof (*irctx.auth_states));
  irctx.auth_states_size++;

  return idx;
}

static uint64_t
inc_burst_count (const char *key) {
  khiter_t k;
  int ret;
  char *dupkey;

  k = kh_get (ir_strmap, irctx.burst_map, key);
  if (k != kh_end (irctx.burst_map)) {
    kh_val (irctx.burst_map, k) += 1;
    return kh_val (irctx.burst_map, k);
  }

  dupkey = xstrdup (key);
  k = kh_put (ir_strmap, irctx.burst_map, dupkey, &ret);
  if (ret < 0) {
    free (dupkey);
    return 1;
  }
  kh_val (irctx.burst_map, k) = 1;
  return 1;
}

static const char *
asset_class_str (IRAssetClass asset_class) {
  switch (asset_class) {
  case IR_ASSET_AUTH:
    return "auth";
  case IR_ASSET_ADMIN:
    return "admin";
  case IR_ASSET_API:
    return "api";
  case IR_ASSET_INTERNAL:
    return "internal";
  case IR_ASSET_SENSITIVE:
    return "sensitive";
  default:
    return "public";
  }
}

static char *
dominant_status_str (const IRTargetAgg *target) {
  uint64_t max = target->status_2xx;
  const char *label = "2xx";

  if (target->status_3xx > max)
    max = target->status_3xx, label = "3xx";
  if (target->status_4xx > max)
    max = target->status_4xx, label = "4xx";
  if (target->status_5xx > max)
    label = "5xx";

  return xstrdup (label);
}

static int
contains_pattern (const char *lower_req, const char *pattern) {
  char *lower_pat;
  int found;

  lower_pat = to_lower_dup (pattern);
  found = strstr (lower_req, lower_pat) != NULL;
  free (lower_pat);

  return found;
}

static void
set_string_once (char **dst, const char *value) {
  if (*dst || !value || *value == '\0')
    return;
  *dst = xstrdup (value);
}

int
ir_profile_enabled (void) {
  return conf.profile == GO_PROFILE_IR_HTTP;
}

void
ir_apply_profile_defaults (void) {
  if (!ir_profile_enabled ())
    return;

  conf.ir_enabled = 1;
  conf.html_report_title = conf.html_report_title ? conf.html_report_title : "GoAccess DFIR Report";
  conf.no_progress = 1;
  conf.no_parsing_spinner = 1;

  if (!conf.html_prefs) {
    conf.html_prefs =
      "{"
      "\"autoHideTables\":false,"
      "\"hiddenPanels\":[],"
      "\"panelOrder\":["
      "\"case_summary\","
      "\"suspicious_sources\","
      "\"incident_timeline\","
      "\"recon_activity\","
      "\"auth_abuse\","
      "\"sensitive_targets\","
      "\"status_codes\","
      "\"hosts\","
      "\"asn\","
      "\"geolocation\","
      "\"tls_type\""
      "]"
      "}";
  }
}

void
ir_enrich_logitem (GLogItem *logitem) {
  char *lower_req;
  size_t i;
  time_t ts;

  if (!conf.ir_enabled)
    return;

  logitem->ir_event_category = IR_EVT_BENIGN;
  logitem->ir_asset_class = IR_ASSET_PUBLIC;
  logitem->ir_is_sensitive_target = 0;
  logitem->ir_is_scanner_pattern = 0;
  logitem->ir_is_auth_endpoint = 0;
  logitem->ir_suspicion_score = 0;
  logitem->ir_rarity_score = 0;
  logitem->ir_burst_score = 0;

  ts = mktime (&logitem->dt);
  if (ts > 0)
    logitem->epoch_minute = ((uint64_t) ts / 60) * 60;

  lower_req = to_lower_dup (logitem->req ? logitem->req : "");
  for (i = 0; i < ARRAY_SIZE (ir_rules); ++i) {
    if (!contains_pattern (lower_req, ir_rules[i].pattern))
      continue;
    if (ir_rules[i].asset_class > logitem->ir_asset_class)
      logitem->ir_asset_class = ir_rules[i].asset_class;
    if (ir_rules[i].is_auth)
      logitem->ir_is_auth_endpoint = 1;
    if (ir_rules[i].is_scanner) {
      logitem->ir_is_scanner_pattern = 1;
      logitem->ir_is_sensitive_target = 1;
    }
  }

  if (logitem->ir_asset_class != IR_ASSET_PUBLIC)
    logitem->ir_is_sensitive_target = 1;

  if (logitem->ir_is_auth_endpoint) {
    if (logitem->status == 401 || logitem->status == 403)
      logitem->ir_event_category = IR_EVT_AUTH_FAIL;
    else if (logitem->status >= 200 && logitem->status < 400)
      logitem->ir_event_category = IR_EVT_AUTH_SUCCESS;
  } else if (logitem->ir_is_scanner_pattern) {
    logitem->ir_event_category = IR_EVT_RECON;
  } else if (logitem->status >= 500) {
    logitem->ir_event_category = IR_EVT_APP_ERROR;
  }

  free (lower_req);
}

void
ir_init (void) {
  memset (&irctx, 0, sizeof irctx);
  irctx.source_map = kh_init (ir_strmap);
  irctx.target_map = kh_init (ir_strmap);
  irctx.timeline_map = kh_init (ir_u64map);
  irctx.ua_map = kh_init (ir_strmap);
  irctx.edge_map = kh_init (ir_strmap);
  irctx.source_path_seen = kh_init (ir_strmap);
  irctx.source_404_seen = kh_init (ir_strmap);
  irctx.source_target_seen = kh_init (ir_strmap);
  irctx.auth_state_map = kh_init (ir_strmap);
  irctx.burst_map = kh_init (ir_strmap);
  irctx.initialized = 1;
}

void
ir_process_log (GLogItem *logitem) {
  IRSourceAgg *source;
  IRTargetAgg *target;
  IRTimelineAgg *bucket;
  uint32_t source_idx, target_idx, timeline_idx, ua_idx;
  int sbucket;
  char key[4096];
  uint64_t burst;

  if (!conf.ir_enabled || !irctx.initialized || logitem->ignorelevel == IGNORE_LEVEL_REQ)
    return;

  source_idx = get_source_idx (logitem->host ? logitem->host : "-");
  target_idx = get_target_idx (logitem->req ? logitem->req : "-", logitem->ir_asset_class);
  timeline_idx = get_timeline_idx (logitem->epoch_minute);

  source = &irctx.sources[source_idx];
  target = &irctx.targets[target_idx];
  bucket = &irctx.timeline[timeline_idx];

  source->hits++;
  target->hits++;
  bucket->hits++;

  irctx.summary.total_events++;
  irctx.summary.approx_bytes += logitem->resp_size;
  if (!irctx.summary.start_ts || logitem->epoch_minute < irctx.summary.start_ts)
    irctx.summary.start_ts = logitem->epoch_minute;
  if (!irctx.summary.end_ts || logitem->epoch_minute > irctx.summary.end_ts)
    irctx.summary.end_ts = logitem->epoch_minute;

  sbucket = status_bucket (logitem->status);
  switch (sbucket) {
  case 2:
    source->status_2xx++;
    target->status_2xx++;
    irctx.summary.status_2xx++;
    break;
  case 3:
    source->status_3xx++;
    target->status_3xx++;
    irctx.summary.status_3xx++;
    break;
  case 4:
    source->status_4xx++;
    target->status_4xx++;
    bucket->status_4xx++;
    irctx.summary.status_4xx++;
    break;
  case 5:
    source->status_5xx++;
    target->status_5xx++;
    bucket->status_5xx++;
    irctx.summary.status_5xx++;
    break;
  }

  if (!irctx.summary.peak_hits || bucket->hits >= irctx.summary.peak_hits) {
    irctx.summary.peak_hits = bucket->hits;
    irctx.summary.peak_ts = bucket->minute_ts;
  }

  set_string_once (&source->user_agent, logitem->agent);
  set_string_once (&source->country, logitem->country);
  set_string_once (&source->asn, logitem->asn);

  ua_idx = get_ua_idx (logitem->agent ? logitem->agent : "-");
  irctx.ua_counts[ua_idx].hits++;

  snprintf (key, sizeof key, "%s|%s", source->source, target->resource);
  if (mark_seen (irctx.source_path_seen, key))
    source->distinct_paths++;

  if (logitem->is_404) {
    snprintf (key, sizeof key, "%s|404|%s", source->source, target->resource);
    if (mark_seen (irctx.source_404_seen, key))
      source->distinct_404_paths++;
  }

  if (logitem->ir_is_sensitive_target) {
    snprintf (key, sizeof key, "%s|sensitive|%s", source->source, target->resource);
    if (mark_seen (irctx.source_target_seen, key))
      source->sensitive_targets++;
  }

  remember_edge (source_idx, target_idx);
  snprintf (key, sizeof key, "%s|%s", target->resource, source->source);
  if (mark_seen (irctx.source_target_seen, key))
    target->unique_sources++;

  snprintf (key, sizeof key, "%s|%" PRIu64, source->source, bucket->minute_ts);
  burst = inc_burst_count (key);
  if (burst > source->max_burst)
    source->max_burst = burst;
  if (burst > bucket->burst_peak)
    bucket->burst_peak = burst;

  if (logitem->ir_is_scanner_pattern) {
    if (source->scanner_patterns == 0)
      bucket->new_scanners++;
    source->scanner_patterns++;
  }

  if (looks_like_extension_target (target->resource))
    source->extensions_targeted++;
  if (is_rare_method (logitem->method))
    source->rare_methods++;

  if (logitem->ir_is_auth_endpoint) {
    uint32_t auth_idx;

    source->auth_hits++;
    if (logitem->status == 401)
      source->auth_401++;
    if (logitem->status == 403)
      source->auth_403++;
    if (logitem->status >= 200 && logitem->status < 400)
      source->auth_success++;

    snprintf (key, sizeof key, "%s|%s", source->source, target->resource);
    auth_idx = get_auth_state_idx (key);
    if (logitem->status == 401 || logitem->status == 403)
      irctx.auth_states[auth_idx].failures++;
    else if (logitem->status >= 200 && logitem->status < 400 && irctx.auth_states[auth_idx].failures > 0) {
      source->success_after_failures++;
      irctx.auth_states[auth_idx].failures = 0;
    }
  }
}

static int
cmp_sources_desc (const void *a, const void *b) {
  const IRSuspiciousSourceItem *sa = a;
  const IRSuspiciousSourceItem *sb = b;

  if (sa->suspicion_score != sb->suspicion_score)
    return (sa->suspicion_score < sb->suspicion_score) - (sa->suspicion_score > sb->suspicion_score);
  return (sa->hits < sb->hits) - (sa->hits > sb->hits);
}

static int
cmp_targets_desc (const void *a, const void *b) {
  const IRSensitiveTargetItem *ta = a;
  const IRSensitiveTargetItem *tb = b;

  if (ta->impact_score != tb->impact_score)
    return (ta->impact_score < tb->impact_score) - (ta->impact_score > tb->impact_score);
  return (ta->hits < tb->hits) - (ta->hits > tb->hits);
}

static int
cmp_timeline_asc (const void *a, const void *b) {
  const IRTimelineItem *ta = a;
  const IRTimelineItem *tb = b;
  return (ta->minute_ts > tb->minute_ts) - (ta->minute_ts < tb->minute_ts);
}

static int
cmp_recon_desc (const void *a, const void *b) {
  const IRReconItem *ra = a;
  const IRReconItem *rb = b;
  return (ra->score_recon < rb->score_recon) - (ra->score_recon > rb->score_recon);
}

static int
cmp_auth_desc (const void *a, const void *b) {
  const IRAuthAbuseItem *aa = a;
  const IRAuthAbuseItem *ab = b;
  return (aa->score_auth_abuse < ab->score_auth_abuse) - (aa->score_auth_abuse > ab->score_auth_abuse);
}

static uint32_t
score_source (const IRSourceAgg *source) {
  uint32_t score = 0;

  if (source->max_burst >= 20)
    score += 3;
  if (source->distinct_404_paths >= 10)
    score += 3;
  if (source->scanner_patterns > 0)
    score += 4;
  if ((source->auth_401 + source->auth_403) >= 10)
    score += 4;
  if (source->success_after_failures > 0)
    score += 5;
  if (source->rare_ua)
    score += 2;
  if (source->sensitive_targets >= 2)
    score += 2;
  if (source->rare_methods > 0)
    score += 2;

  return score;
}

static uint32_t
score_target (const IRTargetAgg *target) {
  uint32_t score = 0;

  if (target->asset_class != IR_ASSET_PUBLIC)
    score += 4;
  if (target->suspicious_sources >= 2)
    score += 3;
  if ((target->status_2xx + target->status_3xx) > 0 && target->suspicious_sources > 0)
    score += 3;
  if (target->hits >= 20)
    score += 2;
  if (target->status_5xx > 0)
    score += 2;

  return score;
}

static char *
join_top_sources (void) {
  size_t i, limit;
  char *out = xstrdup ("");

  limit = MIN ((size_t) 5, irctx.source_items_size);
  for (i = 0; i < limit; ++i) {
    append_str (&out, irctx.source_items[i].source);
    if (i + 1 < limit)
      append_str (&out, ", ");
  }
  if (*out == '\0')
    append_str (&out, "-");

  return out;
}

static char *
join_top_targets (void) {
  size_t i, limit;
  char *out = xstrdup ("");

  limit = MIN ((size_t) 5, irctx.target_items_size);
  for (i = 0; i < limit; ++i) {
    append_str (&out, irctx.target_items[i].resource);
    if (i + 1 < limit)
      append_str (&out, ", ");
  }
  if (*out == '\0')
    append_str (&out, "-");

  return out;
}

static void
append_case_item (const char *metric, char *value, char *details) {
  irctx.case_items = xrealloc (irctx.case_items, (irctx.case_items_size + 1) * sizeof (*irctx.case_items));
  irctx.case_items[irctx.case_items_size].metric = xstrdup (metric);
  irctx.case_items[irctx.case_items_size].value = value;
  irctx.case_items[irctx.case_items_size].details = details;
  irctx.case_items_size++;
}

void
ir_finalize (void) {
  size_t i;

  if (!conf.ir_enabled || irctx.finalized)
    return;

  irctx.source_items = xcalloc (irctx.sources_size, sizeof (*irctx.source_items));
  for (i = 0; i < irctx.sources_size; ++i) {
    IRSourceAgg *source = &irctx.sources[i];
    IRSuspiciousSourceItem *item = &irctx.source_items[irctx.source_items_size++];
    uint32_t ua_idx;

    if (source->user_agent) {
      ua_idx = kh_val (irctx.ua_map, kh_get (ir_strmap, irctx.ua_map, source->user_agent));
      if (irctx.ua_counts[ua_idx].hits <= 2)
        source->rare_ua = 1;
    }
    source->suspicion_score = score_source (source);

    item->source = xstrdup (source->source);
    item->hits = source->hits;
    item->burst_max = source->max_burst;
    item->status_4xx = source->status_4xx;
    item->status_5xx = source->status_5xx;
    item->sensitive_targets = source->sensitive_targets;
    item->user_agent = xstrdup (source->user_agent ? source->user_agent : "-");
    item->asn = xstrdup (source->asn ? source->asn : "-");
    item->country = xstrdup (source->country ? source->country : "-");
    item->suspicion_score = source->suspicion_score;
    item->distinct_paths = source->distinct_paths;
    item->ratio_404 = source->hits ? (source->distinct_404_paths * 100) / source->hits : 0;
    item->scanner_patterns = source->scanner_patterns;
    item->rare_methods = source->rare_methods;
    item->auth_401 = source->auth_401;
    item->auth_403 = source->auth_403;
    item->success_after_failures = source->success_after_failures;
  }
  qsort (irctx.source_items, irctx.source_items_size, sizeof (*irctx.source_items), cmp_sources_desc);

  for (i = 0; i < irctx.edges_size; ++i) {
    IREdge *edge = &irctx.edges[i];
    IRSourceAgg *source = &irctx.sources[edge->source_idx];
    IRTargetAgg *target = &irctx.targets[edge->target_idx];

    if (source->suspicion_score > 0)
      target->suspicious_sources++;
  }

  irctx.target_items = xcalloc (irctx.targets_size, sizeof (*irctx.target_items));
  for (i = 0; i < irctx.targets_size; ++i) {
    IRTargetAgg *target = &irctx.targets[i];
    IRSensitiveTargetItem *item = &irctx.target_items[irctx.target_items_size++];

    target->impact_score = score_target (target);
    item->resource = xstrdup (target->resource);
    item->asset_class = xstrdup (asset_class_str (target->asset_class));
    item->hits = target->hits;
    item->unique_sources = target->unique_sources;
    item->suspicious_sources = target->suspicious_sources;
    item->dominant_status = dominant_status_str (target);
    item->impact_score = target->impact_score;
  }
  qsort (irctx.target_items, irctx.target_items_size, sizeof (*irctx.target_items), cmp_targets_desc);

  irctx.timeline_items = xcalloc (irctx.timeline_size, sizeof (*irctx.timeline_items));
  for (i = 0; i < irctx.timeline_size; ++i) {
    IRTimelineAgg *bucket = &irctx.timeline[i];
    IRTimelineItem *item = &irctx.timeline_items[irctx.timeline_items_size++];

    item->minute_ts = bucket->minute_ts;
    item->minute_label = fmt_ts (bucket->minute_ts);
    item->hits = bucket->hits;
    item->status_4xx = bucket->status_4xx;
    item->status_5xx = bucket->status_5xx;
    item->new_scanners = bucket->new_scanners;
    item->burst_peak = bucket->burst_peak;
  }
  qsort (irctx.timeline_items, irctx.timeline_items_size, sizeof (*irctx.timeline_items), cmp_timeline_asc);

  irctx.recon_items = xcalloc (irctx.sources_size, sizeof (*irctx.recon_items));
  for (i = 0; i < irctx.sources_size; ++i) {
    IRSourceAgg *source = &irctx.sources[i];
    IRReconItem *item;
    uint32_t score;

    score = 0;
    if (source->distinct_paths >= 5)
      score += 3;
    if (source->distinct_404_paths >= 5)
      score += 3;
    if (source->scanner_patterns > 0)
      score += 4;
    if (source->rare_methods > 0)
      score += 2;
    if (source->sensitive_targets > 0)
      score += 2;
    if (score == 0)
      continue;

    item = &irctx.recon_items[irctx.recon_items_size++];
    item->source = xstrdup (source->source);
    item->distinct_paths = source->distinct_paths;
    item->ratio_404 = source->hits ? (source->distinct_404_paths * 100) / source->hits : 0;
    item->sensitive_patterns = source->scanner_patterns + source->sensitive_targets;
    item->extensions_targeted = source->extensions_targeted;
    item->score_recon = score;
  }
  qsort (irctx.recon_items, irctx.recon_items_size, sizeof (*irctx.recon_items), cmp_recon_desc);

  irctx.auth_items = xcalloc (irctx.sources_size, sizeof (*irctx.auth_items));
  for (i = 0; i < irctx.sources_size; ++i) {
    IRSourceAgg *source = &irctx.sources[i];
    IRAuthAbuseItem *item;
    uint32_t score;

    if (source->auth_hits == 0)
      continue;

    score = 0;
    if ((source->auth_401 + source->auth_403) >= 5)
      score += 4;
    if (source->success_after_failures > 0)
      score += 5;
    if (source->auth_hits >= 10)
      score += 2;

    item = &irctx.auth_items[irctx.auth_items_size++];
    item->source = xstrdup (source->source);
    item->auth_hits = source->auth_hits;
    item->status_401 = source->auth_401;
    item->status_403 = source->auth_403;
    item->success_after_failures = source->success_after_failures;
    item->score_auth_abuse = score;
  }
  qsort (irctx.auth_items, irctx.auth_items_size, sizeof (*irctx.auth_items), cmp_auth_desc);

  append_case_item ("period", fmt_range (irctx.summary.start_ts, irctx.summary.end_ts), xstrdup ("coverage"));
  append_case_item ("total_events", fmt_u64 (irctx.summary.total_events), xstrdup ("parsed events"));
  append_case_item ("unique_sources", fmt_u64 (irctx.summary.unique_sources), xstrdup ("distinct source IPs"));
  append_case_item ("top_suspicious_sources", join_top_sources (), xstrdup ("top 5 by suspicion score"));
  append_case_item ("top_sensitive_targets", join_top_targets (), xstrdup ("top 5 by impact score"));
  append_case_item ("peak_activity", fmt_ts (irctx.summary.peak_ts), fmt_u64 (irctx.summary.peak_hits));
  append_case_item ("status_2xx", fmt_u64 (irctx.summary.status_2xx), fmt_percent (irctx.summary.status_2xx, irctx.summary.total_events));
  append_case_item ("status_3xx", fmt_u64 (irctx.summary.status_3xx), fmt_percent (irctx.summary.status_3xx, irctx.summary.total_events));
  append_case_item ("status_4xx", fmt_u64 (irctx.summary.status_4xx), fmt_percent (irctx.summary.status_4xx, irctx.summary.total_events));
  append_case_item ("status_5xx", fmt_u64 (irctx.summary.status_5xx), fmt_percent (irctx.summary.status_5xx, irctx.summary.total_events));
  append_case_item ("approx_bytes", fmt_bytes (irctx.summary.approx_bytes), xstrdup ("response volume"));

  irctx.finalized = 1;
}

static void
free_case_items (void) {
  size_t i;

  for (i = 0; i < irctx.case_items_size; ++i) {
    free_owned_string (&irctx.case_items[i].metric);
    free_owned_string (&irctx.case_items[i].value);
    free_owned_string (&irctx.case_items[i].details);
  }
  free (irctx.case_items);
}

static void
free_source_items (void) {
  size_t i;

  for (i = 0; i < irctx.source_items_size; ++i) {
    free_owned_string (&irctx.source_items[i].source);
    free_owned_string (&irctx.source_items[i].user_agent);
    free_owned_string (&irctx.source_items[i].asn);
    free_owned_string (&irctx.source_items[i].country);
  }
  free (irctx.source_items);
}

static void
free_target_items (void) {
  size_t i;

  for (i = 0; i < irctx.target_items_size; ++i) {
    free_owned_string (&irctx.target_items[i].resource);
    free_owned_string (&irctx.target_items[i].asset_class);
    free_owned_string (&irctx.target_items[i].dominant_status);
  }
  free (irctx.target_items);
}

static void
free_timeline_items (void) {
  size_t i;

  for (i = 0; i < irctx.timeline_items_size; ++i)
    free_owned_string (&irctx.timeline_items[i].minute_label);
  free (irctx.timeline_items);
}

static void
free_recon_items (void) {
  size_t i;

  for (i = 0; i < irctx.recon_items_size; ++i)
    free_owned_string (&irctx.recon_items[i].source);
  free (irctx.recon_items);
}

static void
free_auth_items (void) {
  size_t i;

  for (i = 0; i < irctx.auth_items_size; ++i)
    free_owned_string (&irctx.auth_items[i].source);
  free (irctx.auth_items);
}

void
ir_free (void) {
  size_t i;

  for (i = 0; i < irctx.sources_size; ++i) {
    free (irctx.sources[i].source);
    free (irctx.sources[i].user_agent);
    free (irctx.sources[i].country);
    free (irctx.sources[i].asn);
  }
  for (i = 0; i < irctx.targets_size; ++i)
    free (irctx.targets[i].resource);
  for (i = 0; i < irctx.ua_size; ++i)
    free (irctx.ua_counts[i].user_agent);

  free (irctx.sources);
  free (irctx.targets);
  free (irctx.timeline);
  free (irctx.ua_counts);
  free (irctx.edges);
  free (irctx.auth_states);

  free_case_items ();
  free_source_items ();
  free_target_items ();
  free_timeline_items ();
  free_recon_items ();
  free_auth_items ();

  free_khash_keys (irctx.source_map);
  free_khash_keys (irctx.target_map);
  if (irctx.timeline_map)
    kh_destroy (ir_u64map, irctx.timeline_map);
  free_khash_keys (irctx.ua_map);
  free_khash_keys (irctx.edge_map);
  free_khash_keys (irctx.source_path_seen);
  free_khash_keys (irctx.source_404_seen);
  free_khash_keys (irctx.source_target_seen);
  free_khash_keys (irctx.auth_state_map);
  free_khash_keys (irctx.burst_map);

  memset (&irctx, 0, sizeof irctx);
}

const IRCaseSummaryItem *
ir_case_summary (size_t *size) {
  *size = irctx.case_items_size;
  return irctx.case_items;
}

const IRSuspiciousSourceItem *
ir_suspicious_sources (size_t *size) {
  *size = irctx.source_items_size;
  return irctx.source_items;
}

const IRSensitiveTargetItem *
ir_sensitive_targets (size_t *size) {
  *size = irctx.target_items_size;
  return irctx.target_items;
}

const IRTimelineItem *
ir_incident_timeline (size_t *size) {
  *size = irctx.timeline_items_size;
  return irctx.timeline_items;
}

const IRReconItem *
ir_recon_activity (size_t *size) {
  *size = irctx.recon_items_size;
  return irctx.recon_items;
}

const IRAuthAbuseItem *
ir_auth_abuse (size_t *size) {
  *size = irctx.auth_items_size;
  return irctx.auth_items;
}
