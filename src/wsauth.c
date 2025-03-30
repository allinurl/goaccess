/**
 * wsauth.c - web socket authentication
 *    ______      ___
 *   / ____/___  /   | _____________  __________
 *  / / __/ __ \/ /| |/ ___/ ___/ _ \/ ___/ ___/
 * / /_/ / /_/ / ___ / /__/ /__/  __(__  |__  )
 * \____/\____/_/  |_\___/\___/\___/____/____/
 *
 * The MIT License (MIT)
 * Copyright (c) 2009-2024 Gerardo Orellana <hello @ goaccess.io>
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

static char *
create_jwt_payload (const char *sub, long iat, long exp) {
  const char *aud = "goaccess_ws";
  const char *scope = "report_access";
  char *payload = NULL;
  char hostname[HOST_NAME_MAX];

  if (gethostname (hostname, sizeof (hostname)) != 0) {
    perror ("gethostname");
    // Fallback to a default issuer value if hostname retrieval fails.
    strcpy (hostname, "goaccess");
  }
  // Allocate a buffer for the payload JSON.
  // Adjust the size if you plan on including more data.
  payload = xcalloc (1, MAX_JWT_PAYLOAD);
  if (!payload)
    return NULL;

  // Build the JSON payload.
  snprintf (payload, MAX_JWT_PAYLOAD,
            "{\"iss\":\"%s\",\"sub\":\"%s\",\"iat\":%ld,\"exp\":%ld,\"aud\":\"%s\",\"scope\":\"%s\"}",
            hostname, sub, iat, exp, aud, scope);

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

static int
validate_jwt_claims (const char *payload_json) {
  json_stream json;
  enum json_type t = JSON_ERROR;
  size_t len = 0, level = 0;
  enum json_type ctx = JSON_ERROR;
  char hostname[HOST_NAME_MAX] = { 0 };
  char *curr_key = NULL;

  /* Validation flags/values. */
  int valid_iss = 0, valid_sub = 0, valid_aud = 0, valid_scope = 0;
  long iat = 0, exp = 0;
  time_t now = time (NULL);

  /* Get hostname for the issuer check. */
  if (gethostname (hostname, sizeof (hostname)) != 0) {
    perror ("gethostname");
    strcpy (hostname, "goaccess");
  }

  /* Open JSON payload as a stream and disable streaming mode. */
  json_open_string (&json, payload_json);
  json_set_streaming (&json, false);

  /* The payload should be a JSON object. */
  t = json_next (&json);
  if (t != JSON_OBJECT) {
    json_close (&json);
    return 0;
  }

  /* Iterate over each token (key or value) in the JSON object. */
  while ((t = json_next (&json)) != JSON_DONE && t != JSON_ERROR) {
    ctx = json_get_context (&json, &level);
    /* keys typically appear when (level % 2) != 0 and not inside an array.
     * otherwise, the token is a value. */
    if ((level % 2) != 0 && ctx != JSON_ARRAY) {
      /* This token is a key. Duplicate it to use for matching. */
      if (curr_key)
        free (curr_key);
      curr_key = xstrdup (json_get_string (&json, &len));
    } else {
      /* Assume this token is the value for the last encountered key. */
      if (curr_key) {
        char *val = xstrdup (json_get_string (&json, &len));
        if (strcmp (curr_key, "iss") == 0) {
          /* "iss" must equal the hostname. */
          valid_iss = (strcmp (val, hostname) == 0);
        } else if (strcmp (curr_key, "sub") == 0) {
          /* "sub" must be non-empty. */
          valid_sub = (val[0] != '\0');
        } else if (strcmp (curr_key, "aud") == 0) {
          valid_aud = (strcmp (val, "goaccess_ws") == 0);
        } else if (strcmp (curr_key, "scope") == 0) {
          valid_scope = (strcmp (val, "report_access") == 0);
        } else if (strcmp (curr_key, "iat") == 0) {
          iat = strtol (val, NULL, 10);
        } else if (strcmp (curr_key, "exp") == 0) {
          exp = strtol (val, NULL, 10);
        }
        free (val);
        free (curr_key);
        curr_key = NULL;
      }
    }
  }
  if (curr_key)
    free (curr_key);
  json_close (&json);

  /* Final validation */
  if (conf.ws_auth_verify_only) {
    if (iat > 0 && exp > iat && now >= iat && now <= exp)
      return 1;
    else
      return 0;
  } else {
    if (valid_iss && valid_sub && valid_aud && valid_scope &&
        iat > 0 && exp > iat && now >= iat && now <= exp)
      return 1; // Valid JWT claims.
    else
      return 0; // One or more claim validations failed.
  }
}

/* verifies the JWT signature.
 * Returns 1 if valid, 0 if not.
 */
int
verify_jwt_token (const char *jwt, const char *secret) {
  char *payload_part = NULL, *payload_json = NULL, *token_dup = NULL, *std_payload = NULL;
  size_t payload_len = 0;
  int valid_signature = 0, valid_claims = 0;

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
  valid_claims = validate_jwt_claims (payload_json);

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
