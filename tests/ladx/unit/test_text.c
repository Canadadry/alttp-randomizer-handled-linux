#include "ladx_sha1.h"
#include "ladx_text.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_text"
#define OUT_MAX 512

static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static void to_hex(const uint8_t *bytes, size_t n, char *hex)
{
	size_t i;

	for (i = 0; i < n; i++)
		sprintf(hex + 2 * i, "%02x", bytes[i]);
	hex[2 * n] = '\0';
}

/* The SHA-1 of the bytes LADXR's utils.formatText returned at the pin. */
static const struct {
	const char *name, *text;
	int center;
	const char *ask, *sha1;
} vectors[] = {
	{"plain", "Hqzlo", 0, NULL,
	 "d734e4fccbc829250a85db73681b72c898846e32"},
	{"names", "Qx zv {BAD_HEART_CONTAINER} kw {BAD_HEART_CONTAINER}!", 0, NULL,
	 "649d660ef6638dc0e4566cb6e98bbc43c52582b3"},
	{"braces", "a {{b}} c", 0, NULL,
	 "12c65ed5c3d79f7a5f815c50c3969c252383aaff"},
	{"apostrophe and tags", "Zq's <link> <bigA><stick>!", 0, NULL,
	 "ddf73aba6a77aec0fa1d7b783b131ae8e0ec0459"},
	{"wrap", "aaaa bbbbbbbbbbb cc dddddddddddddddddd e", 0, NULL,
	 "b3a8edd322284c4f09a9caeb703030af02b30454"},
	{"spaces", "a  b   c   ", 0, NULL,
	 "b3a1ee424fb66d84529682ebf714855877a6c43b"},
	{"newline", "fqzst\nsxcqnd lqne hxre kq zv", 0, NULL,
	 "794172d6284ca40ab08d6a40e600255e3d22de80"},
	{"underscore", "a_b __", 0, NULL,
	 "a16c5cc2bb8935f2e31d838f2993d5162885036f"},
	{"center", "Qx 2 {BAD_HEART_CONTAINER} ZQX!", 1, NULL,
	 "128070a1abe789f21c3561f27408dd105657dbed"},
	{"ask", "{BAD_HEART_CONTAINER}\n20 {BAD_HEART_CONTAINER}!", 0, "Qzv  Xw'k",
	 "2f80263e366e67e61f1cfb7731c55564644c35b7"},
	{"ask after a 16-byte word", "q9z8x7w6v5k4j3m2", 0, "Qz Xw",
	 "c47307d6007adcf41c992c9c51e1e3c50130a323"},
	{"empty", "", 0, NULL,
	 "85e53271e14006f0265921d02d4d736cdc580b0b"},
	{"long word centred", "qzxwvkjqzxwvkjqzxwvk x", 1, NULL,
	 "010ba5165d14323b77d3d787685b9a588d529072"},
};

static void test_vectors(void)
{
	uint8_t out[OUT_MAX], digest[LADX_SHA1_SIZE];
	char hex[2 * OUT_MAX + 1], sha1[2 * LADX_SHA1_SIZE + 1];
	LadxOutcome raised;
	size_t i, n;

	for (i = 0; i < sizeof vectors / sizeof vectors[0]; i++) {
		check(ladx_format_text(vectors[i].text, strlen(vectors[i].text), vectors[i].center, vectors[i].ask, out,
				       sizeof out, &n, &raised) == LADX_TEXT_OK,
		      vectors[i].name);
		ladx_sha1(out, n, digest);
		to_hex(digest, LADX_SHA1_SIZE, sha1);
		if (strcmp(sha1, vectors[i].sha1) != 0) {
			to_hex(out, n, hex);
			test_fail(TEST, "%s: not LADXR's bytes, got %s", vectors[i].name, hex);
		}
		checks++;
	}
}

static void check_raise(const char *text, const char *type, const char *message)
{
	uint8_t out[OUT_MAX];
	LadxOutcome raised;
	size_t n = 7;

	check(ladx_format_text(text, strlen(text), 0, NULL, out, sizeof out, &n, &raised) == LADX_TEXT_RAISED,
	      "raises");
	check(n == 7, "a raise writes no length");
	check(raised.kind == LADX_OUTCOME_CRASH && strcmp(raised.type, type) == 0, type);
	if (strcmp(raised.message, message) != 0)
		test_fail(TEST, "%s: message %s, expected %s", type, raised.message, message);
}

static void test_raises(void)
{
	check_raise("Damp\xe9!", "UnicodeEncodeError",
		    "'ascii' codec can't encode character '\\xe9' in position 4: ordinal not in range(128)");
	check_raise("{RUPEES} ain\xe2\x80\xb2t", "UnicodeEncodeError",
		    "'ascii' codec can't encode characters in position 10-12: ordinal not in range(128)");
	check_raise("{NOPE}", "KeyError", "'NOPE'");
}

static void test_limits(void)
{
	uint8_t out[4];
	LadxOutcome raised;
	size_t n;

	check(ladx_format_text("Hello", 5, 0, NULL, out, sizeof out, &n, &raised) == LADX_TEXT_OVERFLOW,
	      "output over capacity");
	check(ladx_format_text("a {0} b", 7, 0, NULL, out, sizeof out, &n, &raised) == LADX_TEXT_UNSUPPORTED,
	      "a positional field is not ported");
	check(ladx_format_text("a } b", 5, 0, NULL, out, sizeof out, &n, &raised) == LADX_TEXT_UNSUPPORTED,
	      "a single } is not ported");
}

int main(void)
{
	test_vectors();
	test_raises();
	test_limits();
	printf("%s: formatText's names, tags, wrap, centring and ask as LADXR, its two raises (%d checks)\n", TEST,
	       checks);
	return 0;
}
