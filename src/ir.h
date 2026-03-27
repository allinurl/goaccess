#ifndef IR_H_INCLUDED
#define IR_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#include "parser.h"

typedef enum GProfile_ {
  GO_PROFILE_CLASSIC = 0,
  GO_PROFILE_IR_HTTP,
} GProfile;

typedef enum IREventCategory_ {
  IR_EVT_BENIGN = 0,
  IR_EVT_RECON,
  IR_EVT_AUTH_FAIL,
  IR_EVT_AUTH_SUCCESS,
  IR_EVT_EXPLOIT_ATTEMPT,
  IR_EVT_EXFIL_CANDIDATE,
  IR_EVT_APP_ERROR,
} IREventCategory;

typedef enum IRAssetClass_ {
  IR_ASSET_PUBLIC = 0,
  IR_ASSET_AUTH,
  IR_ASSET_ADMIN,
  IR_ASSET_API,
  IR_ASSET_INTERNAL,
  IR_ASSET_SENSITIVE,
} IRAssetClass;

typedef struct IRCaseSummaryItem_ {
  char *metric;
  char *value;
  char *details;
} IRCaseSummaryItem;

typedef struct IRSuspiciousSourceItem_ {
  char *source;
  uint64_t hits;
  uint64_t burst_max;
  uint64_t status_4xx;
  uint64_t status_5xx;
  uint64_t sensitive_targets;
  char *user_agent;
  char *asn;
  char *country;
  uint32_t suspicion_score;
  uint64_t distinct_paths;
  uint64_t ratio_404;
  uint64_t scanner_patterns;
  uint64_t rare_methods;
  uint64_t auth_401;
  uint64_t auth_403;
  uint64_t success_after_failures;
} IRSuspiciousSourceItem;

typedef struct IRSensitiveTargetItem_ {
  char *resource;
  char *asset_class;
  uint64_t hits;
  uint64_t unique_sources;
  uint64_t suspicious_sources;
  char *dominant_status;
  uint32_t impact_score;
} IRSensitiveTargetItem;

typedef struct IRTimelineItem_ {
  uint64_t minute_ts;
  char *minute_label;
  uint64_t hits;
  uint64_t status_4xx;
  uint64_t status_5xx;
  uint64_t new_scanners;
  uint64_t burst_peak;
} IRTimelineItem;

typedef struct IRReconItem_ {
  char *source;
  uint64_t distinct_paths;
  uint64_t ratio_404;
  uint64_t sensitive_patterns;
  uint64_t extensions_targeted;
  uint32_t score_recon;
} IRReconItem;

typedef struct IRAuthAbuseItem_ {
  char *source;
  uint64_t auth_hits;
  uint64_t status_401;
  uint64_t status_403;
  uint64_t success_after_failures;
  uint32_t score_auth_abuse;
} IRAuthAbuseItem;

int ir_profile_enabled (void);
void ir_apply_profile_defaults (void);
void ir_enrich_logitem (GLogItem *logitem);
void ir_init (void);
void ir_process_log (GLogItem *logitem);
void ir_finalize (void);
void ir_free (void);

const IRCaseSummaryItem *ir_case_summary (size_t *size);
const IRSuspiciousSourceItem *ir_suspicious_sources (size_t *size);
const IRSensitiveTargetItem *ir_sensitive_targets (size_t *size);
const IRTimelineItem *ir_incident_timeline (size_t *size);
const IRReconItem *ir_recon_activity (size_t *size);
const IRAuthAbuseItem *ir_auth_abuse (size_t *size);

#endif
