# IR HTTP Profile

This fork adds a batch-oriented DFIR profile for cold HTTP log analysis:

```bash
./goaccess client_access.log --log-format=COMBINED --profile ir-http -o report.html
```

The `ir-http` profile is intended for:

* exported access logs
* offline triage
* static HTML reports
* JSON export for external post-processing

It is not intended to be a SIEM replacement or a real-time detection engine.

## What The Profile Adds

When `--profile ir-http` is enabled, GoAccess keeps the legacy parsing and
aggregation pipeline, and adds a second IR-oriented batch layer that:

* classifies interesting HTTP requests
* scores suspicious sources
* scores sensitive targets
* tracks a minute-level incident timeline
* exposes DFIR-oriented panels in JSON and HTML

The profile currently adds these synthetic panels:

* `case_summary`
* `suspicious_sources`
* `incident_timeline`
* `recon_activity`
* `auth_abuse`
* `sensitive_targets`

## Current MVP Heuristics

The MVP uses hardcoded path and behavior rules. It looks for patterns such as:

* `/.env`
* `/.git`
* `/wp-login.php`
* `/xmlrpc.php`
* `/admin`
* `/actuator`
* `/api/`
* `/internal`

The scoring is intentionally simple and triage-oriented. It currently favors:

* scanner-like paths
* repeated 4xx responses
* auth failures
* success after auth failures
* multiple sensitive targets
* bursty activity
* unusual HTTP methods

## Example Commands

Generate a static HTML report:

```bash
./goaccess access.log --log-format=COMBINED --profile ir-http -o report.html
```

Generate JSON only:

```bash
./goaccess access.log --log-format=COMBINED --profile ir-http -o report.json
```

Generate both HTML and JSON with two runs:

```bash
./goaccess access.log access.log.1 \
  --log-format=COMBINED \
  --profile ir-http \
  -o report.html

./goaccess access.log access.log.1 \
  --log-format=COMBINED \
  --profile ir-http \
  -o report.json
```

With gzip support enabled at build time:

```bash
./goaccess access.log access.log.1.gz \
  --log-format=COMBINED \
  --profile ir-http \
  -o report.html
```

## Output Notes

The generated JSON keeps the legacy GoAccess sections and appends IR sections.

The generated HTML exposes the same IR panels through `user_interface` and
`json_data`, so the existing report frontend can render them without a separate
frontend implementation.

## Current Limitations

The current implementation is MVP-only:

* rules are hardcoded and not loaded from `rules/*.yml` yet
* the ncurses terminal workflow is not adapted to the IR profile
* correlation is lightweight and source-centric
* non-HTTP multi-source correlation is out of scope

## Quick Test

If you already built the binary, this is the shortest validation path:

```bash
./goaccess /path/to/access.log \
  --log-format=COMBINED \
  --profile ir-http \
  -o /tmp/report.html
```

Then open `/tmp/report.html` and confirm that the report contains:

* `Case Summary`
* `Suspicious Sources`
* `Incident Timeline`
* `Recon Activity`
* `Auth Abuse`
* `Sensitive Targets`
