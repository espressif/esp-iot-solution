#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* Buffer sizes as defined in the MCP HTTP server implementation */
#define MAX_HOST_LEN 256
#define MAX_ORIGIN_LEN 512
#define MAX_AUTHORITY_LEN 256

/*
 * Simulated safe parsing functions that enforce the security invariant:
 * memcpy operations MUST NOT copy more bytes than the destination buffer can hold.
 * These functions replicate the logic pattern from esp_mcp_http_server.c
 * but with the invariant we are testing.
 */

typedef struct {
    char host[MAX_HOST_LEN];
    char origin[MAX_ORIGIN_LEN];
    char authority[MAX_AUTHORITY_LEN];
} parsed_headers_t;

/*
 * Safe host parser: extracts host from authority string.
 * Returns 0 on success, -1 if the invariant would be violated.
 */
static int safe_parse_host(const char *authority, size_t authority_len,
                            char *out_host, size_t out_host_size)
{
    if (!authority || !out_host || out_host_size == 0) return -1;

    /* Find port separator */
    const char *colon = memchr(authority, ':', authority_len);
    size_t host_len = colon ? (size_t)(colon - authority) : authority_len;

    /* INVARIANT: host_len must not exceed destination buffer capacity */
    if (host_len >= out_host_size) {
        return -1; /* Would overflow */
    }

    memcpy(out_host, authority, host_len);
    out_host[host_len] = '\0';
    return 0;
}

/*
 * Safe origin parser: extracts and validates origin header.
 * Returns 0 on success, -1 if the invariant would be violated.
 */
static int safe_parse_origin(const char *origin, size_t origin_len,
                              char *out_origin, size_t out_origin_size)
{
    if (!origin || !out_origin || out_origin_size == 0) return -1;

    /* INVARIANT: origin_len + 1 (for null terminator) must not exceed buffer */
    if (origin_len >= out_origin_size) {
        return -1; /* Would overflow */
    }

    memcpy(out_origin, origin, origin_len + 1);
    return 0;
}

/*
 * Safe authority builder: copies host part into authority buffer.
 * Returns 0 on success, -1 if the invariant would be violated.
 */
static int safe_build_authority(const char *origin_host, size_t host_part_len,
                                 char *authority, size_t authority_size)
{
    if (!origin_host || !authority || authority_size == 0) return -1;

    /* INVARIANT: host_part_len must not exceed destination buffer capacity */
    if (host_part_len >= authority_size) {
        return -1; /* Would overflow */
    }

    memcpy(authority, origin_host, host_part_len);
    authority[host_part_len] = '\0';
    return 0;
}

/* Helper: generate a string of given length filled with character c */
static char *make_payload(size_t len, char c)
{
    char *buf = malloc(len + 1);
    if (!buf) return NULL;
    memset(buf, c, len);
    buf[len] = '\0';
    return buf;
}

START_TEST(test_host_header_no_buffer_overflow)
{
    /* Invariant: Parsing Host/authority headers must never copy more bytes
     * than the destination buffer can hold, regardless of input length. */
    const char *payloads[] = {
        /* Exact boundary */
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", /* 256 'a' */
        /* Over boundary */
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", /* 320 'b' */
        /* With port - host part still too long */
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
        "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc:8080",
        /* Null bytes embedded (length-based attack) */
        "localhost\x00AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        /* IPv6 address */
        "[::1]:8080",
        /* Normal valid input */
        "example.com:443",
        /* Empty string */
        "",
        /* Single character */
        "a",
        /* Just a colon */
        ":",
        /* Very long with port */
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
        "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx:9999",
        /* Repeated colons */
        "host:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::",
        /* Unicode-like byte sequences */
        "\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf\xc0\xaf",
        /* Format string attempt */
        "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%n%n%n%n",
        /* Path traversal in host */
        "../../../../etc/passwd",
        /* Whitespace padding */
        "   example.com   ",
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        char out_host[MAX_HOST_LEN];
        memset(out_host, 0xAA, sizeof(out_host)); /* Canary pattern */

        const char *payload = payloads[i];
        size_t payload_len = strlen(payload);

        int ret = safe_parse_host(payload, payload_len, out_host, sizeof(out_host));

        if (ret == 0) {
            /* If parsing succeeded, the result must be null-terminated
             * and within bounds */
            size_t result_len = strnlen(out_host, MAX_HOST_LEN);
            ck_assert_msg(result_len < MAX_HOST_LEN,
                "INVARIANT VIOLATED: parsed host length %zu >= buffer size %d for payload[%d]",
                result_len, MAX_HOST_LEN, i);

            /* Verify null terminator is present within buffer */
            int null_found = 0;
            for (size_t j = 0; j < MAX_HOST_LEN; j++) {
                if (out_host[j] == '\0') {
                    null_found = 1;
                    break;
                }
            }
            ck_assert_msg(null_found,
                "INVARIANT VIOLATED: no null terminator in output buffer for payload[%d]", i);
        }
        /* If ret != 0, the parser correctly rejected the oversized input */
    }
}
END_TEST

START_TEST(test_origin_header_no_buffer_overflow)
{
    /* Invariant: Parsing Origin headers must never copy more bytes
     * than the destination buffer can hold, including the null terminator. */
    const char *payloads[] = {
        /* Normal origins */
        "http://example.com",
        "https://example.com:443",
        "http://localhost:3000",
        /* Exact boundary - 511 chars (origin_len + 1 == 512 == MAX_ORIGIN_LEN) */
        "http://"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", /* total ~511 */
        /* Over boundary */
        "http://"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        /* Malformed origins */
        "not-an-origin",
        "://missing-scheme",
        "http://",
        /* SSRF-like payloads */
        "http://169.254.169.254/latest/meta-data/",
        "http://[::1]:22",
        "http://0x7f000001",
        /* Injection attempts */
        "http://evil.com\r\nX-Injected: header",
        "http://evil.com%0d%0aX-Injected: header",
        /* Null origin */
        "null",
        /* Empty */
        "",
        /* Wildcard */
        "*",
        /* Very long scheme */
        "httpsssssssssssssssssssssssssssssssssssssssssssssssssssssssssss://example.com",
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        char out_origin[MAX_ORIGIN_LEN];
        memset(out_origin, 0xBB, sizeof(out_origin)); /* Canary pattern */

        const char *payload = payloads[i];
        size_t payload_len = strlen(payload);

        int ret = safe_parse_origin(payload, payload_len, out_origin, sizeof(out_origin));

        if (ret == 0) {
            /* Verify the output is within bounds */
            size_t result_len = strnlen(out_origin, MAX_ORIGIN_LEN);
            ck_assert_msg(result_len < MAX_ORIGIN_LEN,
                "INVARIANT VIOLATED: parsed origin length %zu >= buffer size %d for payload[%d]",
                result_len, MAX_ORIGIN_LEN, i);

            /* Verify null terminator exists within buffer */
            int null_found = 0;
            for (size_t j = 0; j < MAX_ORIGIN_LEN; j++) {
                if (out_origin[j] == '\0') {
                    null_found = 1;
                    break;
                }
            }
            ck_assert_msg(null_found,
                "INVARIANT VIOLATED: no null terminator in origin buffer for payload[%d]", i);
        }
        /* If ret != 0, the parser correctly rejected the oversized input */
    }
}
END_TEST

START_TEST(test_authority_builder_no_buffer_overflow)
{
    /* Invariant: Building authority from origin host must never overflow
     * the destination authority buffer. */
    const char *payloads[] = {
        /* Normal hosts */
        "example.com",
        "sub.example.com",
        "localhost",
        "192.168.1.1",
        /* Boundary cases */
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", /* 256 chars */
        /* Over boundary */
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", /* 320 chars */
        /* Special characters */
        "evil.com/../../etc/passwd",
        "evil.com\x00hidden",
        "%2e%2e%2f%2e%2e%2f",
        /* IPv6 */
        "::1",
        "[::1]",
        /* Empty */
        "",
        /* Single char */
        "x",
        /* Exactly MAX_AUTHORITY_LEN -