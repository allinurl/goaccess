/**
 * wsauth.c - web socket authentication
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2026 Gerardo Orellana <hello @ goaccess.io>
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

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>
#include <limits.h>

#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

#include "wsauth.h"

#include "base64.h"
#include "util.h"
#include "pdjson.h"
#include "settings.h"
#include "xmalloc.h"

/* JWT claim names and values issued by GoAccess. */
#define JWT_CLAIM_ISSUER "iss"
#define JWT_CLAIM_SUBJECT "sub"
#define JWT_CLAIM_ISSUED_AT "iat"
#define JWT_CLAIM_EXPIRATION "exp"
#define JWT_CLAIM_AUDIENCE "aud"
#define JWT_CLAIM_SCOPE "scope"
#define JWT_EXPECTED_AUDIENCE "goaccess_ws"
#define JWT_EXPECTED_SCOPE "report_access"
#define JWT_FALLBACK_ISSUER "goaccess"

typedef enum JWTClaim_ {
  JWT_CLAIM_UNKNOWN = 0,
  JWT_CLAIM_KIND_ISSUER,
  JWT_CLAIM_KIND_SUBJECT,
  JWT_CLAIM_KIND_ISSUED_AT,
  JWT_CLAIM_KIND_EXPIRATION,
  JWT_CLAIM_KIND_AUDIENCE,
  JWT_CLAIM_KIND_SCOPE
} JWTClaim;

typedef struct JWTClaims_ {
  time_t issued_at;
  time_t expiration;
  int has_issuer;
  int has_subject;
  int has_issued_at;
  int has_expiration;
  int has_audience;
  int has_scope;
} JWTClaims;

char *
read_secret_from_file (const char *path) {
  FILE *file = fopen (path, "r");
  char *secret = xcalloc (1, MAX_SECRET_SIZE);

  if (!file) {
    perror ("Error opening secret file");
    return NULL;
  }

  if (!secret) {
    perror ("Error allocating memory");
    fclose (file);
    return NULL;
  }

  if (!fgets (secret, MAX_SECRET_SIZE, file)) {
    perror ("Error reading secret file");
    free (secret);
    fclose (file);
    return NULL;
  }
  fclose (file);

  // Remove trailing newline, if present.
  secret[strcspn (secret, "\n")] = '\0';

  return secret;
}

/* Generate a new secret (HS256-compatible) and return it as a hex string. */
char *
generate_ws_auth_secret (void) {
  char *secret_hex = NULL;
  int secret_len = 32;          // 256 bits
  unsigned char secret_bytes[32];

  if (RAND_bytes (secret_bytes, secret_len) != 1) {
    fprintf (stderr, "Error generating random bytes\n");
    return NULL;
  }
  secret_hex = xmalloc (secret_len * 2 + 1);
  if (!secret_hex)
    return NULL;
  for (int i = 0; i < secret_len; i++) {
    sprintf (&secret_hex[i * 2], "%02x", secret_bytes[i]);
  }
  secret_hex[secret_len * 2] = '\0';
  return secret_hex;
}

/* Create the JSON claims payload used for a locally issued JWT.
 *
 * On success, a newly allocated payload is returned.
 * On failure, NULL is returned. */
static char *
create_jwt_payload (const char *sub, long iat, long exp) {
  char *payload = NULL;
  char hostname[HOST_NAME_MAX + 1] = { 0 };

  if (gethostname (hostname, sizeof (hostname)) != 0) {
    perror ("gethostname");
    // Fallback to a default issuer value if hostname retrieval fails.
    strcpy (hostname, JWT_FALLBACK_ISSUER);
  }
  hostname[sizeof (hostname) - 1] = '\0';
  // Allocate a buffer for the payload JSON.
  // Adjust the size if you plan on including more data.
  payload = xcalloc (1, MAX_JWT_PAYLOAD);
  if (!payload)
    return NULL;

  // Build the JSON payload.
  snprintf (payload, MAX_JWT_PAYLOAD,
            "{\"iss\":\"%s\",\"sub\":\"%s\",\"iat\":%ld,\"exp\":%ld,\"aud\":\"%s\",\"scope\":\"%s\"}",
            hostname, sub, iat, exp, JWT_EXPECTED_AUDIENCE, JWT_EXPECTED_SCOPE);

  return payload;
}

char *
create_jwt_token (void) {
  char *jwt = NULL, *payload = NULL, report_id[50];
  time_t now = time (NULL);
  struct tm *jwt_now_tm = localtime (&now);
  // Configure token lifetime in seconds
  long token_lifetime = conf.ws_auth_expire > 0 ? conf.ws_auth_expire : DEFAULT_EXPIRE_TIME;
  long iat = now;
  long exp = now + token_lifetime;

  // Format the date as "YYYYMMDD" for the report ID
  snprintf (report_id, sizeof (report_id), "goaccess_report_%04d%02d%02d",
            jwt_now_tm->tm_year + 1900, jwt_now_tm->tm_mon + 1, jwt_now_tm->tm_mday);

  payload = create_jwt_payload (report_id, iat, exp);
  if (!payload) {
    fprintf (stderr, "Failed to create JWT payload\n");
    return NULL;
  }
  jwt = generate_jwt (conf.ws_auth_secret, payload);
  free (payload);
  return jwt;
}

static int
verify_jwt_signature (const char *jwt, const char *secret) {
  char *token_dup = NULL, *header_part = NULL, *payload_part = NULL, *signature_part = NULL,
    *signing_input = NULL, *computed_signature = NULL, *computed_signature_url = NULL;
  unsigned char *hmac_result = NULL;
  unsigned int hmac_len = 0;
  int valid = 0;
  size_t signing_input_len = 0;

  if (!jwt || !secret) {
    return 0;
  }

  token_dup = strdup (jwt);
  if (!token_dup) {
    return 0;
  }

  header_part = strtok (token_dup, ".");
  payload_part = strtok (NULL, ".");
  signature_part = strtok (NULL, ".");
  if (!header_part || !payload_part || !signature_part) {
    free (token_dup);
    return 0;
  }

  signing_input_len = strlen (header_part) + 1 + strlen (payload_part) + 1;
  signing_input = malloc (signing_input_len);
  if (!signing_input) {
    free (token_dup);
    return 0;
  }
  snprintf (signing_input, signing_input_len, "%s.%s", header_part, payload_part);

  hmac_result =
    HMAC (EVP_sha256 (), secret, strlen (secret), (unsigned char *) signing_input,
          strlen (signing_input), NULL, &hmac_len);
  free (signing_input);
  if (!hmac_result) {
    free (token_dup);
    return 0;
  }

  computed_signature = base64_encode (hmac_result, hmac_len);
  if (!computed_signature) {
    free (token_dup);
    return 0;
  }
  // Convert computed_signature to Base64Url format
  computed_signature_url = base64UrlEncode (computed_signature);
  free (computed_signature);

  if (!computed_signature_url) {
    free (token_dup);
    return 0;
  }

  valid = (strcmp (computed_signature_url, signature_part) == 0);

  free (computed_signature_url);
  free (token_dup);

  return valid;
}

/* Check whether the current JSON string exactly matches an expected value.
 *
 * On success, non-zero is returned.
 * On failure, including strings with embedded NUL bytes, 0 is returned. */
static int
jwt_json_string_equals (json_stream *json, const char *expected) {
  const char *value = NULL;
  size_t expected_len = 0, value_len = 0;

  value = json_get_string (json, &value_len);
  expected_len = strlen (expected);

  if (value_len != expected_len + 1)
    return 0;

  return memcmp (value, expected, expected_len + 1) == 0;
}

/* Check whether the current JSON string is non-empty and contains no embedded NUL.
 *
 * On success, non-zero is returned.
 * On failure, 0 is returned. */
static int
jwt_json_string_is_nonempty (json_stream *json) {
  const char *value = NULL;
  size_t value_len = 0;

  value = json_get_string (json, &value_len);
  if (value_len <= 1 || value[value_len - 1] != '\0')
    return 0;

  return memchr (value, '\0', value_len - 1) == NULL;
}

/* Identify a supported top-level JWT claim from the current JSON member name.
 *
 * On success, the matching claim kind is returned.
 * For an unknown or intentionally ignored claim, JWT_CLAIM_UNKNOWN is returned. */
static JWTClaim
get_jwt_claim (json_stream *json, int verify_only) {
  if (jwt_json_string_equals (json, JWT_CLAIM_ISSUED_AT))
    return JWT_CLAIM_KIND_ISSUED_AT;
  if (jwt_json_string_equals (json, JWT_CLAIM_EXPIRATION))
    return JWT_CLAIM_KIND_EXPIRATION;

  if (verify_only)
    return JWT_CLAIM_UNKNOWN;

  if (jwt_json_string_equals (json, JWT_CLAIM_ISSUER))
    return JWT_CLAIM_KIND_ISSUER;
  if (jwt_json_string_equals (json, JWT_CLAIM_SUBJECT))
    return JWT_CLAIM_KIND_SUBJECT;
  if (jwt_json_string_equals (json, JWT_CLAIM_AUDIENCE))
    return JWT_CLAIM_KIND_AUDIENCE;
  if (jwt_json_string_equals (json, JWT_CLAIM_SCOPE))
    return JWT_CLAIM_KIND_SCOPE;

  return JWT_CLAIM_UNKNOWN;
}

/* Parse the current JSON number as a positive time_t value.
 *
 * On success, non-zero is returned and timestamp is updated.
 * On failure, 0 is returned and timestamp is left unchanged. */
static int
parse_jwt_numeric_date (json_stream *json, time_t *numeric_date) {
  const char *value = NULL;
  char *end = NULL;
  intmax_t whole_seconds = 0;
  long double parsed = 0.0;
  size_t value_len = 0;
  time_t converted = 0;

  value = json_get_string (json, &value_len);
  if (value_len <= 1 || value[value_len - 1] != '\0')
    return 0;

  errno = 0;
  parsed = strtold (value, &end);
  if (errno == ERANGE || parsed <= 0.0 || parsed > (long double) INTMAX_MAX ||
      end != value + value_len - 1)
    return 0;

  whole_seconds = (intmax_t) parsed;
  converted = (time_t) whole_seconds;
  if (whole_seconds <= 0 || (intmax_t) converted != whole_seconds)
    return 0;

  *numeric_date = converted;
  return 1;
}

/* Parse and validate one recognized JWT claim value.
 *
 * On success, non-zero is returned and claims is updated.
 * On a duplicate, mistyped, or invalid claim, 0 is returned. */
static int
parse_jwt_claim_value (json_stream *json, JWTClaim claim, const char *hostname, JWTClaims *claims) {
  enum json_type type = JSON_ERROR;

  type = json_next (json);
  if (type == JSON_ERROR || type == JSON_DONE)
    return 0;

  switch (claim) {
  case JWT_CLAIM_KIND_ISSUER:
    if (claims->has_issuer || type != JSON_STRING || !jwt_json_string_equals (json, hostname))
      return 0;
    claims->has_issuer = 1;
    return 1;
  case JWT_CLAIM_KIND_SUBJECT:
    if (claims->has_subject || type != JSON_STRING || !jwt_json_string_is_nonempty (json))
      return 0;
    claims->has_subject = 1;
    return 1;
  case JWT_CLAIM_KIND_ISSUED_AT:
    if (claims->has_issued_at || type != JSON_NUMBER ||
        !parse_jwt_numeric_date (json, &claims->issued_at))
      return 0;
    claims->has_issued_at = 1;
    return 1;
  case JWT_CLAIM_KIND_EXPIRATION:
    if (claims->has_expiration || type != JSON_NUMBER ||
        !parse_jwt_numeric_date (json, &claims->expiration))
      return 0;
    claims->has_expiration = 1;
    return 1;
  case JWT_CLAIM_KIND_AUDIENCE:
    if (claims->has_audience || type != JSON_STRING ||
        !jwt_json_string_equals (json, JWT_EXPECTED_AUDIENCE))
      return 0;
    claims->has_audience = 1;
    return 1;
  case JWT_CLAIM_KIND_SCOPE:
    if (claims->has_scope || type != JSON_STRING ||
        !jwt_json_string_equals (json, JWT_EXPECTED_SCOPE))
      return 0;
    claims->has_scope = 1;
    return 1;
  case JWT_CLAIM_UNKNOWN:
    return 0;
  }

  return 0;
}

/* Parse a complete JWT claims object without confusing nested values for members.
 *
 * On success, non-zero is returned and claims is updated.
 * On malformed input or an invalid recognized claim, 0 is returned. */
static int
parse_jwt_claims_object (json_stream *json, const char *hostname, int verify_only,
                         JWTClaims *claims) {
  enum json_type type = JSON_ERROR;
  JWTClaim claim = JWT_CLAIM_UNKNOWN;

  if (json_next (json) != JSON_OBJECT)
    return 0;

  while ((type = json_next (json)) == JSON_STRING) {
    claim = get_jwt_claim (json, verify_only);
    if (claim == JWT_CLAIM_UNKNOWN) {
      type = json_skip (json);
      if (type == JSON_ERROR || type == JSON_DONE)
        return 0;
      continue;
    }

    if (!parse_jwt_claim_value (json, claim, hostname, claims))
      return 0;
  }

  if (type != JSON_OBJECT_END)
    return 0;

  return json_next (json) == JSON_DONE;
}

/* Validate JWT claims and return the token's absolute expiration time.
 *
 * On success, 1 is returned and expires_at is updated.
 * On failure, 0 is returned and expires_at is left unchanged. */
static int
validate_jwt_claims (const char *payload_json, size_t payload_len, time_t *expires_at) {
  json_stream json;
  JWTClaims claims = { 0 };
  char hostname[HOST_NAME_MAX + 1] = { 0 };
  time_t now = 0;
  int parsed = 0, verify_only = 0;

  if (payload_json == NULL || expires_at == NULL)
    return 0;

  verify_only = conf.ws_auth_verify_only;
  if (!verify_only && gethostname (hostname, sizeof (hostname)) != 0) {
    perror ("gethostname");
    strcpy (hostname, JWT_FALLBACK_ISSUER);
  }
  hostname[sizeof (hostname) - 1] = '\0';

  json_open_buffer (&json, payload_json, payload_len);
  json_set_streaming (&json, false);
  parsed = parse_jwt_claims_object (&json, hostname, verify_only, &claims);
  json_close (&json);

  if (!parsed || !claims.has_issued_at || !claims.has_expiration)
    return 0;

  now = time (NULL);
  if (now == (time_t) - 1 || claims.expiration <= claims.issued_at ||
      now < claims.issued_at || now >= claims.expiration)
    return 0;

  if (!verify_only && (!claims.has_issuer || !claims.has_subject || !claims.has_audience ||
                       !claims.has_scope))
    return 0;

  *expires_at = claims.expiration;
  return 1;
}

/* Verify a JWT and return the token's absolute expiration time.
 *
 * On success, 1 is returned and expires_at is updated.
 * On failure, 0 is returned and expires_at is set to zero. */
int
verify_jwt_token (const char *jwt, const char *secret, time_t *expires_at) {
  char *payload_part = NULL, *payload_json = NULL, *token_dup = NULL, *std_payload = NULL;
  size_t payload_len = 0;
  int valid_signature = 0, valid_claims = 0;

  if (expires_at == NULL)
    return 0;

  *expires_at = 0;

  /* Step 1: Verify the signature */
  valid_signature = verify_jwt_signature (jwt, secret);
  if (!valid_signature) {
    return 0;
  }

  /* Step 2: Extract the payload part from the JWT */
  token_dup = strdup (jwt);
  if (!token_dup) {
    return 0;
  }
  strtok (token_dup, "."); // Skip header
  payload_part = strtok (NULL, "."); // Get payload
  if (!payload_part) {
    free (token_dup);
    return 0;
  }

  /* Convert Base64Url to standard Base64 before decoding */
  std_payload = base64UrlDecode (payload_part);
  if (!std_payload) {
    free (token_dup);
    return 0;
  }

  /* Step 3: Decode the base64url-encoded payload */
  payload_json = base64_decode (std_payload, &payload_len);
  free (std_payload);
  free (token_dup);
  if (!payload_json) {
    return 0;
  }

  /* Null-terminate the payload JSON string for parsing */
  payload_json = realloc (payload_json, payload_len + 1);
  if (!payload_json) {
    return 0;
  }
  payload_json[payload_len] = '\0';

  /* Step 4: Validate the claims */
  valid_claims = validate_jwt_claims (payload_json, payload_len, expires_at);

  /* Clean up */
  free (payload_json);

  return valid_claims;

}

/* Generate a JWT signed with HMAC-SHA256.
 * - secret: the secret key as a string (from conf.ws_auth_secret)
 * - payload: a JSON string to be used as the JWT payload.
 *
 * The JWT header is fixed to {"alg":"HS256","typ":"JWT"}.
 *
 * The returned JWT is dynamically allocated and must be freed by the caller.
 */
char *
generate_jwt (const char *secret, const char *payload) {
  char *encoded_payload = NULL, *encoded_header = NULL, *encoded_signature = NULL;
  char *signing_input = NULL;
  const char *header = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
  unsigned char *hmac_result = NULL;
  unsigned int hmac_len = 0;
  size_t jwt_len = 0, signing_input_len = 0;
  char *jwt = NULL, *tmp = NULL;

  /* Encode header and convert to base64url */
  tmp = base64_encode ((const unsigned char *) header, strlen (header));
  if (!tmp)
    return NULL;
  encoded_header = base64UrlEncode (tmp);
  free (tmp);
  if (!encoded_header)
    return NULL;

  /* Encode payload and convert to base64url */
  tmp = base64_encode ((const unsigned char *) payload, strlen (payload));
  if (!tmp) {
    free (encoded_header);
    return NULL;
  }
  encoded_payload = base64UrlEncode (tmp);
  free (tmp);
  if (!encoded_payload) {
    free (encoded_header);
    return NULL;
  }

  /* Create the signing input: "<encoded_header>.<encoded_payload>" */
  signing_input_len = strlen (encoded_header) + 1 + strlen (encoded_payload) + 1;
  signing_input = malloc (signing_input_len);
  if (!signing_input) {
    free (encoded_header);
    free (encoded_payload);
    return NULL;
  }
  snprintf (signing_input, signing_input_len, "%s.%s", encoded_header, encoded_payload);

  /* Compute HMAC-SHA256 signature */
  hmac_result =
    HMAC (EVP_sha256 (), secret, strlen (secret), (unsigned char *) signing_input,
          strlen (signing_input), NULL, &hmac_len);
  if (!hmac_result) {
    free (encoded_header);
    free (encoded_payload);
    free (signing_input);
    return NULL;
  }

  /* Base64url-encode the signature */
  tmp = base64_encode (hmac_result, hmac_len);
  if (!tmp) {
    free (encoded_header);
    free (encoded_payload);
    free (signing_input);
    return NULL;
  }
  encoded_signature = base64UrlEncode (tmp);
  free (tmp);
  if (!encoded_signature) {
    free (encoded_header);
    free (encoded_payload);
    free (signing_input);
    return NULL;
  }

  /* Build the final JWT: "<encoded_header>.<encoded_payload>.<encoded_signature>" */
  jwt_len =
    strlen (encoded_header) + 1 + strlen (encoded_payload) + 1 + strlen (encoded_signature) + 1;
  jwt = malloc (jwt_len);
  if (!jwt) {
    free (encoded_header);
    free (encoded_payload);
    free (signing_input);
    free (encoded_signature);
    return NULL;
  }
  snprintf (jwt, jwt_len, "%s.%s.%s", encoded_header, encoded_payload, encoded_signature);

  free (encoded_header);
  free (encoded_payload);
  free (signing_input);
  free (encoded_signature);

  return jwt;
}
