#include "system.h"
#include <stdarg.h>
#include <stdbool.h>
#include <curl/curl.h>

#include <rpmutil.h>

#include "debug.h"

#include "jsmn.c"

/*==============================================================*/
/* --- buf.h */

typedef struct {
    size_t len;			// current length of buffer (used bytes)
    size_t limit;		// maximum length of buffer (allocated)
    uint8_t *data;		// insert bytes here
} buf_t;


buf_t *buf_size(buf_t * buf, size_t len);
void buf_push(buf_t * buf, uint8_t c);
void buf_concat(buf_t * buf, uint8_t * data, size_t len);
char *buf_tostr(buf_t * buf);
/*==============================================================*/
/* --- json.h */

char *json_fetch(char *url);
jsmntok_t *json_tokenise(char *js);
bool json_token_streq(char *js, jsmntok_t * t, char *s)
	RPM_GNUC_PURE;
char *json_token_tostr(char *js, jsmntok_t * t);
/*==============================================================*/
/* --- log.h */

#define STR(x) #x
#define STRINGIFY(x) STR(x)
#define LINESTR STRINGIFY(__LINE__)

#define log_assert(x) if (!(x)) log_die("%s: assertion failed: " #x \
                                        " (" __FILE__ ", line " LINESTR \
                                        ")", __func__)

#define log_null(x) log_assert(x != NULL)

#define log_debug(msg, ...) log_info("%s: " msg, __func__, ##__VA_ARGS__)

#define log_syserr(msg) log_die("%s: %s: %s", __func__, msg, strerror(errno))

void log_die(char *msg, ...)
	RPM_GNUC_NORETURN;
void log_info(char *msg, ...);
/*==============================================================*/
/* --- buf.c */
/* buf: a sized buffer type. */

buf_t *buf_size(buf_t * buf, size_t len)
{
    if (buf == NULL) {
	buf = malloc(sizeof(buf_t));
	log_null(buf);
	buf->data = NULL;
	buf->len = 0;
    }

    buf->data = realloc(buf->data, len);
    log_null(buf->data);

    if (buf->len > len)
	buf->len = len;
    buf->limit = len;

    return buf;
}

void buf_push(buf_t * buf, uint8_t c)
{
    log_null(buf);

    log_assert(buf->len < buf->limit);

    buf->data[buf->len++] = c;
}

void buf_concat(buf_t * dst, uint8_t * src, size_t len)
{
    log_null(dst);
    log_null(src);

    log_assert(dst->len + len <= dst->limit);

    for (size_t i = 0; i < len; i++)
	dst->data[dst->len++] = src[i];
}

char *buf_tostr(buf_t * buf)
{
    log_null(buf);

    char *str = malloc(buf->len + 1);

    memcpy(str, buf->data, buf->len);
    str[buf->len] = '\0';

    return str;
}

/*==============================================================*/
/* --- json.c */

#define BUFFER_SIZE 32768
#define JSON_TOKENS 256

static size_t fetch_data(void *buffer, size_t size, size_t nmemb,
			 void *userp)
{
    buf_t *buf = (buf_t *) userp;
    size_t total = size * nmemb;

    if (buf->limit - buf->len < total) {
	buf = buf_size(buf, buf->limit + total);
	log_null(buf);
    }

    buf_concat(buf, buffer, total);

    return total;
}

char *json_fetch(char *url)
{
    CURL *curl = curl_easy_init();
    log_null(curl);

    curl_easy_setopt(curl, CURLOPT_URL, url);

    buf_t *buf = buf_size(NULL, BUFFER_SIZE);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fetch_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,
		     "jsmn-example (https://github.com/alisdair/jsmn-example, alisdair@mcdiarmid.org)");

    struct curl_slist *hs =
	curl_slist_append(NULL, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
	log_die("curl_easy_perform failed: %s", curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    curl_slist_free_all(hs);

    char *js = buf_tostr(buf);
    free(buf->data);
    free(buf);

fprintf(stderr, "<-- %s(%s)\n%s\n", __FUNCTION__, url, js);
    return js;
}

jsmntok_t *json_tokenise(char *js)
{
    jsmn_parser parser;
    jsmn_init(&parser);

    unsigned int n = JSON_TOKENS;
    jsmntok_t *tokens = malloc(sizeof(jsmntok_t) * n);
    log_null(tokens);

    int ret = jsmn_parse(&parser, js, strlen(js), tokens, n);

    while (ret == JSMN_ERROR_NOMEM) {
	n = n * 2 + 1;
	tokens = realloc(tokens, sizeof(jsmntok_t) * n);
	log_null(tokens);
	ret = jsmn_parse(&parser, js, strlen(js), tokens, n);
    }

    if (ret == JSMN_ERROR_INVAL)
	log_die("jsmn_parse: invalid JSON string");
    if (ret == JSMN_ERROR_PART)
	log_die("jsmn_parse: truncated JSON string");

    return tokens;
}

bool json_token_streq(char *js, jsmntok_t * t, char *s)
{
    return (strncmp(js + t->start, s, t->end - t->start) == 0
	    && strlen(s) == (size_t) (t->end - t->start));
}

char *json_token_tostr(char *js, jsmntok_t * t)
{
    js[t->end] = '\0';
    return js + t->start;
}

/*==============================================================*/
/* --- log.c */
/* log: stderr logging information. */

void log_die(char *msg, ...)
{
    va_list argp;

    log_null(msg);

    va_start(argp, msg);
    vfprintf(stderr, msg, argp);
    va_end(argp);

    fprintf(stderr, "\n");
    abort();
}

void log_info(char *msg, ...)
{
    va_list argp;

    log_null(msg);

    va_start(argp, msg);
    vfprintf(stderr, msg, argp);
    va_end(argp);

    fprintf(stderr, "\n");
}

/*==============================================================*/
/* --- twitter.c */

#if defined(JSMN_TWITTER)
char URL[] = "https://api.twitter.com/1.1/trends/1.json";

int main(void)
{
    char *js = json_fetch(URL);

    jsmntok_t *tokens = json_tokenise(js);

    /* The Twitter trends API response is in this format:
     *
     * [
     *   {
     *      ...,
     *      "trends": [
     *          {
     *              ...,
     *              "name": "TwitterChillers",
     *          },
     *          ...,
     *      ],
     *      ...,
     *   }
     * ]
     *
     */
    typedef enum {
	START,
	WRAPPER, OBJECT,
	TRENDS, ARRAY,
	TREND, NAME,
	SKIP,
	STOP
    } parse_state;

    // state is the current state of the parser
    parse_state state = START;

    // stack is the state we return to when reaching the end of an object
    parse_state stack = STOP;

    // Counters to keep track of how far through parsing we are
    size_t object_tokens = 0;
    size_t skip_tokens = 0;
    size_t trends = 0;
    size_t trend_tokens = 0;

    for (size_t i = 0, j = 1; j > 0; i++, j--) {
	jsmntok_t *t = &tokens[i];

	// Should never reach uninitialized tokens
	log_assert(t->start != -1 && t->end != -1);

	if (t->type == JSMN_ARRAY || t->type == JSMN_OBJECT)
	    j += t->size;

	switch (state) {
	case START:
	    if (t->type != JSMN_ARRAY)
		log_die("Invalid response: root element must be array.");
	    if (t->size != 1)
		log_die("Invalid response: array must have one element.");

	    state = WRAPPER;
	    break;

	case WRAPPER:
	    if (t->type != JSMN_OBJECT)
		log_die("Invalid response: wrapper must be an object.");

	    state = OBJECT;
	    object_tokens = t->size;
	    break;

	case OBJECT:
	    object_tokens--;

	    // Keys are odd-numbered tokens within the object
	    if (object_tokens % 2 == 1) {
		if (t->type == JSMN_STRING
		    && json_token_streq(js, t, "trends"))
		    state = TRENDS;
	    } else if (t->type == JSMN_ARRAY || t->type == JSMN_OBJECT) {
		state = SKIP;
		stack = OBJECT;
		skip_tokens = t->size;
	    }
	    // Last object value
	    if (object_tokens == 0)
		state = STOP;

	    break;

	case SKIP:
	    skip_tokens--;

	    if (t->type == JSMN_ARRAY || t->type == JSMN_OBJECT)
		skip_tokens += t->size;

	    if (skip_tokens == 0)
		state = stack;

	    break;

	case TRENDS:
	    if (t->type != JSMN_ARRAY)
		log_die("Unknown trends value: expected array.");

	    trends = t->size;
	    state = ARRAY;
	    stack = ARRAY;

	    // No trends found
	    if (trends == 0)
		state = STOP;

	    break;

	case ARRAY:
	    trends--;

	    trend_tokens = t->size;
	    state = TREND;

	    // Empty trend object
	    if (trend_tokens == 0)
		state = STOP;

	    // Last trend object
	    if (trends == 0)
		stack = STOP;

	    break;

	case TREND:
	case NAME:
	    trend_tokens--;

	    // Keys are odd-numbered tokens within the object
	    if (trend_tokens % 2 == 1) {
		if (t->type == JSMN_STRING
		    && json_token_streq(js, t, "name"))
		    state = NAME;
	    }
	    // Only care about values in the NAME state
	    else if (state == NAME) {
		if (t->type != JSMN_STRING)
		    log_die("Invalid trend name.");

		char *str = json_token_tostr(js, t);
		puts(str);

		state = TREND;
	    } else if (t->type == JSMN_ARRAY || t->type == JSMN_OBJECT) {
		state = SKIP;
		stack = TREND;
		skip_tokens = t->size;
	    }
	    // Last object value
	    if (trend_tokens == 0)
		state = stack;

	    break;

	case STOP:
	    // Just consume the tokens
	    break;

	default:
	    log_die("Invalid state %u", state);
	}
    }

    return 0;
}
#endif	/* JSMN_TWITTER */

/*==============================================================*/
/* --- github.c */

#if defined(JSMN_GITHUB)

char URL[] = "https://api.github.com/users/alisdair";
char *KEYS[] = { "name", "location", "public_repos", "hireable" };

int main(void)
{
    char *js = json_fetch(URL);

    jsmntok_t *tokens = json_tokenise(js);

    /* The GitHub user API response is a single object. States required to
     * parse this are simple: start of the object, keys, values we want to
     * print, values we want to skip, and then a marker state for the end. */

    typedef enum { START, KEY, PRINT, SKIP, STOP } parse_state;
    parse_state state = START;

    size_t object_tokens = 0;

    for (size_t i = 0, j = 1; j > 0; i++, j--) {
	jsmntok_t *t = &tokens[i];

	// Should never reach uninitialized tokens
	log_assert(t->start != -1 && t->end != -1);

	if (t->type == JSMN_ARRAY || t->type == JSMN_OBJECT)
	    j += 2 * t->size;

	switch (state) {
	case START:
	    if (t->type != JSMN_OBJECT)
		log_die
		    ("Invalid response: root element must be an object.");

	    state = KEY;
	    object_tokens = 2 * t->size;

	    if (object_tokens == 0)
		state = STOP;

	    if (object_tokens % 2 != 0)
		log_die
		    ("Invalid response: object must have even number of children.");

	    break;

	case KEY:
	    object_tokens--;

	    if (t->type != JSMN_STRING)
		log_die("Invalid response: object keys must be strings.");

	    state = SKIP;

	    for (size_t i = 0; i < sizeof(KEYS) / sizeof(char *); i++) {
		if (json_token_streq(js, t, KEYS[i])) {
		    printf("%s: ", KEYS[i]);
		    state = PRINT;
		    break;
		}
	    }

	    break;

	case SKIP:
	    if (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE)
		log_die
		    ("Invalid response: object values must be strings or primitives.");

	    object_tokens--;
	    state = KEY;

	    if (object_tokens == 0)
		state = STOP;

	    break;

	case PRINT:
	    if (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE)
		log_die
		    ("Invalid response: object values must be strings or primitives.");

	    char *str = json_token_tostr(js, t);
	    puts(str);

	    object_tokens--;
	    state = KEY;

	    if (object_tokens == 0)
		state = STOP;

	    break;

	case STOP:
	    // Just consume the tokens
	    break;

	default:
	    log_die("Invalid state %u", state);
	}
    }

    return 0;
}
#endif	/* JSMN_GITHUB */
