/*==============================================================*/
/*
 * Copyright (c) 2007-2014, Lloyd Hilaiel <me@lloyd.io>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*==============================================================*/
/* --- yajl_common.h */

#ifdef __cplusplus
extern "C" {
#endif

#define YAJL_MAX_DEPTH 128

/* msft dll export gunk.  To build a DLL on windows, you
 * must define WIN32, YAJL_SHARED, and YAJL_BUILD.  To use a shared
 * DLL, you must define YAJL_SHARED and WIN32 */
#if (defined(_WIN32) || defined(WIN32)) && defined(YAJL_SHARED)
#  ifdef YAJL_BUILD
#    define YAJL_API __declspec(dllexport)
#  else
#    define YAJL_API __declspec(dllimport)
#  endif
#else
#  if defined(__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__) >= 303
#    define YAJL_API __attribute__ ((visibility("default")))
#  else
#    define YAJL_API
#  endif
#endif

/** pointer to a malloc function, supporting client overriding memory
 *  allocation routines */
typedef void * (*yajl_malloc_func)(void *ctx, size_t sz);

/** pointer to a free function, supporting client overriding memory
 *  allocation routines */
typedef void (*yajl_free_func)(void *ctx, void * ptr);

/** pointer to a realloc function which can resize an allocation. */
typedef void * (*yajl_realloc_func)(void *ctx, void * ptr, size_t sz);

/** A structure which can be passed to yajl_*_alloc routines to allow the
 *  client to specify memory allocation functions to be used. */
typedef struct
{
    /** pointer to a function that can allocate uninitialized memory */
    yajl_malloc_func malloc;
    /** pointer to a function that can resize memory allocations */
    yajl_realloc_func realloc;
    /** pointer to a function that can free memory allocated using
     *  reallocFunction or mallocFunction */
    yajl_free_func free;
    /** a context pointer that will be passed to above allocation routines */
    void * ctx;
} yajl_alloc_funcs;

#ifdef __cplusplus
}
#endif

/*==============================================================*/
/* --- yajl_version.h */

#define YAJL_MAJOR 2
#define YAJL_MINOR 1
#define YAJL_MICRO 1

#define YAJL_VERSION ((YAJL_MAJOR * 10000) + (YAJL_MINOR * 100) + YAJL_MICRO)

#ifdef __cplusplus
extern "C" {
#endif

extern int YAJL_API yajl_version(void);

#ifdef __cplusplus
}
#endif

/*==============================================================*/
/* --- yajl_alloc.h */

/**
 * default memory allocation routines for yajl which use malloc/realloc and
 * free
 */

#define YA_MALLOC(afs, sz) (afs)->malloc((afs)->ctx, (sz))
#define YA_FREE(afs, ptr) (afs)->free((afs)->ctx, (ptr))
#define YA_REALLOC(afs, ptr, sz) (afs)->realloc((afs)->ctx, (ptr), (sz))

void yajl_set_default_alloc_funcs(yajl_alloc_funcs * yaf);

/*==============================================================*/
/* --- yajl_buf.h */

/*
 * Implementation/performance notes.  If this were moved to a header
 * only implementation using #define's where possible we might be 
 * able to sqeeze a little performance out of the guy by killing function
 * call overhead.  YMMV.
 */

/**
 * yajl_buf is a buffer with exponential growth.  the buffer ensures that
 * you are always null padded.
 */
typedef struct yajl_buf_t * yajl_buf;

/* allocate a new buffer */
yajl_buf yajl_buf_alloc(yajl_alloc_funcs * alloc);

/* free the buffer */
void yajl_buf_free(yajl_buf buf);

/* append a number of bytes to the buffer */
void yajl_buf_append(yajl_buf buf, const void * data, size_t len);

/* empty the buffer */
void yajl_buf_clear(yajl_buf buf);

/* get a pointer to the beginning of the buffer */
const unsigned char * yajl_buf_data(yajl_buf buf)
	BSON_GNUC_PURE;

/* get the length of the buffer */
size_t yajl_buf_len(yajl_buf buf)
	BSON_GNUC_PURE;

/* truncate the buffer */
void yajl_buf_truncate(yajl_buf buf, size_t len);

/*==============================================================*/
/* --- yajl_gen.h */

/*
 * Interface to YAJL's JSON generation facilities.
 */

#ifdef __cplusplus
extern "C" {
#endif
    /** generator status codes */
    typedef enum {
        /** no error */
        yajl_gen_status_ok = 0,
        /** at a point where a map key is generated, a function other than
         *  yajl_gen_string was called */
        yajl_gen_keys_must_be_strings,
        /** YAJL's maximum generation depth was exceeded.  see
         *  YAJL_MAX_DEPTH */
        yajl_max_depth_exceeded,
        /** A generator function (yajl_gen_XXX) was called while in an error
         *  state */
        yajl_gen_in_error_state,
        /** A complete JSON document has been generated */
        yajl_gen_generation_complete,
        /** yajl_gen_double was passed an invalid floating point value
         *  (infinity or NaN). */
        yajl_gen_invalid_number,
        /** A print callback was passed in, so there is no internal
         * buffer to get from */
        yajl_gen_no_buf,
        /** returned from yajl_gen_string() when the yajl_gen_validate_utf8
         *  option is enabled and an invalid was passed by client code.
         */
        yajl_gen_invalid_string
    } yajl_gen_status;

    /** an opaque handle to a generator */
    typedef struct yajl_gen_t * yajl_gen;

    /** a callback used for "printing" the results. */
    typedef void (*yajl_print_t)(void * ctx,
                                 const char * str,
                                 size_t len);

    /** configuration parameters for the parser, these may be passed to
     *  yajl_gen_config() along with option specific argument(s).  In general,
     *  all configuration parameters default to *off*. */
    typedef enum {
        /** generate indented (beautiful) output */
        yajl_gen_beautify = 0x01,
        /**
         * Set an indent string which is used when yajl_gen_beautify
         * is enabled.  Maybe something like \\t or some number of
         * spaces.  The default is four spaces ' '.
         */
        yajl_gen_indent_string = 0x02,
        /**
         * Set a function and context argument that should be used to
         * output generated json.  the function should conform to the
         * yajl_print_t prototype while the context argument is a
         * void * of your choosing.
         *
         * example:
         *   yajl_gen_config(g, yajl_gen_print_callback, myFunc, myVoidPtr);
         */
        yajl_gen_print_callback = 0x04,
        /**
         * Normally the generator does not validate that strings you
         * pass to it via yajl_gen_string() are valid UTF8.  Enabling
         * this option will cause it to do so.
         */
        yajl_gen_validate_utf8 = 0x08,
        /**
         * the forward solidus (slash or '/' in human) is not required to be
         * escaped in json text.  By default, YAJL will not escape it in the
         * iterest of saving bytes.  Setting this flag will cause YAJL to
         * always escape '/' in generated JSON strings.
         */
        yajl_gen_escape_solidus = 0x10
    } yajl_gen_option;

    /** allow the modification of generator options subsequent to handle
     *  allocation (via yajl_alloc)
     *  \returns zero in case of errors, non-zero otherwise
     */
    YAJL_API int yajl_gen_config(yajl_gen g, yajl_gen_option opt, ...);

    /** allocate a generator handle
     *  \param allocFuncs an optional pointer to a structure which allows
     *                    the client to overide the memory allocation
     *                    used by yajl.  May be NULL, in which case
     *                    malloc/free/realloc will be used.
     *
     *  \returns an allocated handle on success, NULL on failure (bad params)
     */
    YAJL_API yajl_gen yajl_gen_alloc(const yajl_alloc_funcs * allocFuncs);

    /** free a generator handle */
    YAJL_API void yajl_gen_free(yajl_gen handle);

    YAJL_API yajl_gen_status yajl_gen_integer(yajl_gen hand, int64_t number);
    /** generate a floating point number.  number may not be infinity or
     *  NaN, as these have no representation in JSON.  In these cases the
     *  generator will return 'yajl_gen_invalid_number' */
    YAJL_API yajl_gen_status yajl_gen_double(yajl_gen hand, double number);
    YAJL_API yajl_gen_status yajl_gen_number(yajl_gen hand,
                                             const char * num,
                                             size_t len);
    YAJL_API yajl_gen_status yajl_gen_string(yajl_gen hand,
                                             const unsigned char * str,
                                             size_t len);
    YAJL_API yajl_gen_status yajl_gen_null(yajl_gen hand);
    YAJL_API yajl_gen_status yajl_gen_bool(yajl_gen hand, int boolean);
    YAJL_API yajl_gen_status yajl_gen_map_open(yajl_gen hand);
    YAJL_API yajl_gen_status yajl_gen_map_close(yajl_gen hand);
    YAJL_API yajl_gen_status yajl_gen_array_open(yajl_gen hand);
    YAJL_API yajl_gen_status yajl_gen_array_close(yajl_gen hand);

    /** access the null terminated generator buffer.  If incrementally
     *  outputing JSON, one should call yajl_gen_clear to clear the
     *  buffer.  This allows stream generation. */
    YAJL_API yajl_gen_status yajl_gen_get_buf(yajl_gen hand,
                                              const unsigned char ** buf,
                                              size_t * len);

    /** clear yajl's output buffer, but maintain all internal generation
     *  state.  This function will not "reset" the generator state, and is
     *  intended to enable incremental JSON outputing. */
    YAJL_API void yajl_gen_clear(yajl_gen hand);

    /** Reset the generator state.  Allows a client to generate multiple
     *  json entities in a stream. The "sep" string will be inserted to
     *  separate the previously generated entity from the current,
     *  NULL means *no separation* of entites (clients beware, generating
     *  multiple JSON numbers without a separator, for instance, will result in ambiguous output)
     *
     *  Note: this call will not clear yajl's output buffer.  This
     *  may be accomplished explicitly by calling yajl_gen_clear() */
    YAJL_API void yajl_gen_reset(yajl_gen hand, const char * sep);

#ifdef __cplusplus
}
#endif

/*==============================================================*/
/* --- yajl_encode.h */

#ifdef __cplusplus
extern "C" {
#endif    

void yajl_string_encode(const yajl_print_t printer,
                        void * ctx,
                        const unsigned char * str,
                        size_t length,
                        int escape_solidus);

void yajl_string_decode(yajl_buf buf, const unsigned char * str,
                        size_t length);

int yajl_string_validate_utf8(const unsigned char * s, size_t len)
	BSON_GNUC_PURE;

#ifdef __cplusplus
}
#endif    

/*==============================================================*/
/* --- yajl_bytestack.h */

/*
 * A header only implementation of a simple stack of bytes, used in YAJL
 * to maintain parse state.
 */

#define YAJL_BS_INC 128

typedef struct yajl_bytestack_t
{
    unsigned char * stack;
    size_t size;
    size_t used;
    yajl_alloc_funcs * yaf;
} yajl_bytestack;

/* initialize a bytestack */
#define yajl_bs_init(obs, _yaf) {               \
        (obs).stack = NULL;                     \
        (obs).size = 0;                         \
        (obs).used = 0;                         \
        (obs).yaf = (_yaf);                     \
    }                                           \


/* initialize a bytestack */
#define yajl_bs_free(obs)                 \
    if ((obs).stack) (obs).yaf->free((obs).yaf->ctx, (obs).stack);

#define yajl_bs_current(obs)               \
    (assert((obs).used > 0), (obs).stack[(obs).used - 1])

#define yajl_bs_push(obs, byte) {                       \
    if (((obs).size - (obs).used) == 0) {               \
        (obs).size += YAJL_BS_INC;                      \
        (obs).stack = (obs).yaf->realloc((obs).yaf->ctx,\
                                         (void *) (obs).stack, (obs).size);\
    }                                                   \
    (obs).stack[((obs).used)++] = (byte);               \
}

/* removes the top item of the stack, returns nothing */
#define yajl_bs_pop(obs) { ((obs).used)--; }

#define yajl_bs_set(obs, byte)                          \
    (obs).stack[((obs).used) - 1] = (byte);

/*==============================================================*/
/* --- yajl_lex.h */

typedef enum {
    yajl_tok_bool,
    yajl_tok_colon,
    yajl_tok_comma,
    yajl_tok_eof,
    yajl_tok_error,
    yajl_tok_left_brace,
    yajl_tok_left_bracket,
    yajl_tok_null,
    yajl_tok_right_brace,
    yajl_tok_right_bracket,

    /* we differentiate between integers and doubles to allow the
     * parser to interpret the number without re-scanning */
    yajl_tok_integer,
    yajl_tok_double,

    /* we differentiate between strings which require further processing,
     * and strings that do not */
    yajl_tok_string,
    yajl_tok_string_with_escapes,

    /* comment tokens are not currently returned to the parser, ever */
    yajl_tok_comment
} yajl_tok;

typedef struct yajl_lexer_t * yajl_lexer;

yajl_lexer yajl_lex_alloc(yajl_alloc_funcs * alloc,
                          unsigned int allowComments,
                          unsigned int validateUTF8);

void yajl_lex_free(yajl_lexer lexer);

/**
 * run/continue a lex. "offset" is an input/output parameter.
 * It should be initialized to zero for a
 * new chunk of target text, and upon subsetquent calls with the same
 * target text should passed with the value of the previous invocation.
 *
 * the client may be interested in the value of offset when an error is
 * returned from the lexer.  This allows the client to render useful
 * error messages.
 *
 * When you pass the next chunk of data, context should be reinitialized
 * to zero.
 *
 * Finally, the output buffer is usually just a pointer into the jsonText,
 * however in cases where the entity being lexed spans multiple chunks,
 * the lexer will buffer the entity and the data returned will be
 * a pointer into that buffer.
 *
 * This behavior is abstracted from client code except for the performance
 * implications which require that the client choose a reasonable chunk
 * size to get adequate performance.
 */
yajl_tok yajl_lex_lex(yajl_lexer lexer, const unsigned char * jsonText,
                      size_t jsonTextLen, size_t * offset,
                      const unsigned char ** outBuf, size_t * outLen);

/** have a peek at the next token, but don't move the lexer forward */
yajl_tok yajl_lex_peek(yajl_lexer lexer, const unsigned char * jsonText,
                       size_t jsonTextLen, size_t offset);


typedef enum {
    yajl_lex_e_ok = 0,
    yajl_lex_string_invalid_utf8,
    yajl_lex_string_invalid_escaped_char,
    yajl_lex_string_invalid_json_char,
    yajl_lex_string_invalid_hex_char,
    yajl_lex_invalid_char,
    yajl_lex_invalid_string,
    yajl_lex_missing_integer_after_decimal,
    yajl_lex_missing_integer_after_exponent,
    yajl_lex_missing_integer_after_minus,
    yajl_lex_unallowed_comment
} yajl_lex_error;

const char * yajl_lex_error_to_string(yajl_lex_error error)
	BSON_GNUC_CONST;

/** allows access to more specific information about the lexical
 *  error when yajl_lex_lex returns yajl_tok_error. */
yajl_lex_error yajl_lex_get_error(yajl_lexer lexer)
	BSON_GNUC_PURE;

/** get the current offset into the most recently lexed json string. */
size_t yajl_lex_current_offset(yajl_lexer lexer);

/** get the number of lines lexed by this lexer instance */
size_t yajl_lex_current_line(yajl_lexer lexer)
	BSON_GNUC_PURE;

/** get the number of chars lexed by this lexer instance since the last
 *  \n or \r */
size_t yajl_lex_current_char(yajl_lexer lexer);

/*==============================================================*/
/* --- yajl_tree.h */

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * Parses JSON data and returns the data in tree form.
 *
 * \author Florian Forster
 * \date August 2010
 *
 * This interface makes quick parsing and extraction of
 * smallish JSON docs trivial:
 *
 * include example/parse_config.c
 */

/** possible data types that a yajl_val_s can hold */
typedef enum {
    yajl_t_string = 1,
    yajl_t_number = 2,
    yajl_t_object = 3,
    yajl_t_array = 4,
    yajl_t_true = 5,
    yajl_t_false = 6,
    yajl_t_null = 7,
    /** The any type isn't valid for yajl_val_s.type, but can be
     *  used as an argument to routines like yajl_tree_get().
     */
    yajl_t_any = 8
} yajl_type;

#define YAJL_NUMBER_INT_VALID    0x01
#define YAJL_NUMBER_DOUBLE_VALID 0x02

/** A pointer to a node in the parse tree */
typedef struct yajl_val_s * yajl_val;

/**
 * A JSON value representation capable of holding one of the seven
 * types above. For "string", "number", "object", and "array"
 * additional data is available in the union.  The "YAJL_IS_*"
 * and "YAJL_GET_*" macros below allow type checking and convenient
 * value extraction.
 */
struct yajl_val_s
{
    /** Type of the value contained. Use the "YAJL_IS_*" macros to check for a
     * specific type. */
    yajl_type type;
    /** Type-specific data. You may use the "YAJL_GET_*" macros to access these
     * members. */
    union
    {
        char * string;
        struct {
            int64_t i; /*< integer value, if representable. */
            double  d;   /*< double value, if representable. */
            char   *r;   /*< unparsed number in string form. */
            /** Signals whether the \em i and \em d members are
             * valid. See \c YAJL_NUMBER_INT_VALID and
             * \c YAJL_NUMBER_DOUBLE_VALID. */
            unsigned int flags;
        } number;
        struct {
            const char **keys; /*< Array of keys */
            yajl_val *values; /*< Array of values. */
            size_t len; /*< Number of key-value-pairs. */
        } object;
        struct {
            yajl_val *values; /*< Array of elements. */
            size_t len; /*< Number of elements. */
        } array;
    } u;
};

/**
 * Parse a string.
 *
 * Parses an null-terminated string containing JSON data and returns a pointer
 * to the top-level value (root of the parse tree).
 *
 * \param input              Pointer to a null-terminated utf8 string containing
 *                           JSON data.
 * \param error_buffer       Pointer to a buffer in which an error message will
 *                           be stored if \em yajl_tree_parse fails, or
 *                           \c NULL. The buffer will be initialized before
 *                           parsing, so its content will be destroyed even if
 *                           \em yajl_tree_parse succeeds.
 * \param error_buffer_size  Size of the memory area pointed to by
 *                           \em error_buffer_size. If \em error_buffer_size is
 *                           \c NULL, this argument is ignored.
 *
 * \returns Pointer to the top-level value or \c NULL on error. The memory
 * pointed to must be freed using \em yajl_tree_free. In case of an error, a
 * null terminated message describing the error in more detail is stored in
 * \em error_buffer if it is not \c NULL.
 */
YAJL_API yajl_val yajl_tree_parse (const char *input,
                                   char *error_buffer, size_t error_buffer_size);


/**
 * Free a parse tree returned by "yajl_tree_parse".
 *
 * \param v Pointer to a JSON value returned by "yajl_tree_parse". Passing NULL
 * is valid and results in a no-op.
 */
YAJL_API void yajl_tree_free (yajl_val v);

/**
 * Access a nested value inside a tree.
 *
 * \param parent the node under which you'd like to extract values.
 * \param path A null terminated array of strings, each the name of an object key
 * \param type the yajl_type of the object you seek, or yajl_t_any if any will do.
 *
 * \returns a pointer to the found value, or NULL if we came up empty.
 *
 * Future Ideas:  it'd be nice to move path to a string and implement support for
 * a teeny tiny micro language here, so you can extract array elements, do things
 * like .first and .last, even .length.  Inspiration from JSONPath and css selectors?
 * No it wouldn't be fast, but that's not what this API is about.
 */
YAJL_API yajl_val yajl_tree_get(yajl_val parent, const char ** path, yajl_type type)
	BSON_GNUC_PURE;

/* Various convenience macros to check the type of a `yajl_val` */
#define YAJL_IS_STRING(v) (((v) != NULL) && ((v)->type == yajl_t_string))
#define YAJL_IS_NUMBER(v) (((v) != NULL) && ((v)->type == yajl_t_number))
#define YAJL_IS_INTEGER(v) (YAJL_IS_NUMBER(v) && ((v)->u.number.flags & YAJL_NUMBER_INT_VALID))
#define YAJL_IS_DOUBLE(v) (YAJL_IS_NUMBER(v) && ((v)->u.number.flags & YAJL_NUMBER_DOUBLE_VALID))
#define YAJL_IS_OBJECT(v) (((v) != NULL) && ((v)->type == yajl_t_object))
#define YAJL_IS_ARRAY(v)  (((v) != NULL) && ((v)->type == yajl_t_array ))
#define YAJL_IS_TRUE(v)   (((v) != NULL) && ((v)->type == yajl_t_true  ))
#define YAJL_IS_FALSE(v)  (((v) != NULL) && ((v)->type == yajl_t_false ))
#define YAJL_IS_NULL(v)   (((v) != NULL) && ((v)->type == yajl_t_null  ))

/** Given a yajl_val_string return a ptr to the bare string it contains,
 *  or NULL if the value is not a string. */
#define YAJL_GET_STRING(v) (YAJL_IS_STRING(v) ? (v)->u.string : NULL)

/** Get the string representation of a number.  You should check type first,
 *  perhaps using YAJL_IS_NUMBER */
#define YAJL_GET_NUMBER(v) ((v)->u.number.r)

/** Get the double representation of a number.  You should check type first,
 *  perhaps using YAJL_IS_DOUBLE */
#define YAJL_GET_DOUBLE(v) ((v)->u.number.d)

/** Get the 64bit (int64_t) integer representation of a number.  You should
 *  check type first, perhaps using YAJL_IS_INTEGER */
#define YAJL_GET_INTEGER(v) ((v)->u.number.i)

/** Get a pointer to a yajl_val_object or NULL if the value is not an object. */
#define YAJL_GET_OBJECT(v) (YAJL_IS_OBJECT(v) ? &(v)->u.object : NULL)

/** Get a pointer to a yajl_val_array or NULL if the value is not an object. */
#define YAJL_GET_ARRAY(v)  (YAJL_IS_ARRAY(v)  ? &(v)->u.array  : NULL)

#ifdef __cplusplus
}
#endif

/*==============================================================*/
/* --- yajl_parse.h */

/*
 * Interface to YAJL's JSON stream parsing facilities.
 */

#ifdef __cplusplus
extern "C" {
#endif
    /** error codes returned from this interface */
    typedef enum {
        /** no error was encountered */
        yajl_status_ok,
        /** a client callback returned zero, stopping the parse */
        yajl_status_client_canceled,
        /** An error occured during the parse.  Call yajl_get_error for
         *  more information about the encountered error */
        yajl_status_error
    } yajl_status;

    /** attain a human readable, english, string for an error */
    YAJL_API const char * yajl_status_to_string(yajl_status code);

    /** an opaque handle to a parser */
    typedef struct yajl_handle_t * yajl_handle;

    /** yajl is an event driven parser.  this means as json elements are
     *  parsed, you are called back to do something with the data.  The
     *  functions in this table indicate the various events for which
     *  you will be called back.  Each callback accepts a "context"
     *  pointer, this is a void * that is passed into the yajl_parse
     *  function which the client code may use to pass around context.
     *
     *  All callbacks return an integer.  If non-zero, the parse will
     *  continue.  If zero, the parse will be canceled and
     *  yajl_status_client_canceled will be returned from the parse.
     *
     *  \attention {
     *    A note about the handling of numbers:
     *
     *    yajl will only convert numbers that can be represented in a
     *    double or a 64 bit (int64_t) int.  All other numbers will
     *    be passed to the client in string form using the yajl_number
     *    callback.  Furthermore, if yajl_number is not NULL, it will
     *    always be used to return numbers, that is yajl_integer and
     *    yajl_double will be ignored.  If yajl_number is NULL but one
     *    of yajl_integer or yajl_double are defined, parsing of a
     *    number larger than is representable in a double or 64 bit
     *    integer will result in a parse error.
     *  }
     */
    typedef struct {
        int (* yajl_null)(void * ctx);
        int (* yajl_boolean)(void * ctx, int boolVal);
        int (* yajl_integer)(void * ctx, int64_t integerVal);
        int (* yajl_double)(void * ctx, double doubleVal);
        /** A callback which passes the string representation of the number
         *  back to the client.  Will be used for all numbers when present */
        int (* yajl_number)(void * ctx, const char * numberVal,
                            size_t numberLen);

        /** strings are returned as pointers into the JSON text when,
         * possible, as a result, they are _not_ null padded */
        int (* yajl_string)(void * ctx, const unsigned char * stringVal,
                            size_t stringLen);

        int (* yajl_start_map)(void * ctx);
        int (* yajl_map_key)(void * ctx, const unsigned char * key,
                             size_t stringLen);
        int (* yajl_end_map)(void * ctx);

        int (* yajl_start_array)(void * ctx);
        int (* yajl_end_array)(void * ctx);
    } yajl_callbacks;

    /** allocate a parser handle
     *  \param callbacks  a yajl callbacks structure specifying the
     *                    functions to call when different JSON entities
     *                    are encountered in the input text.  May be NULL,
     *                    which is only useful for validation.
     *  \param afs        memory allocation functions, may be NULL for to use
     *                    C runtime library routines (malloc and friends) 
     *  \param ctx        a context pointer that will be passed to callbacks.
     */
    YAJL_API yajl_handle yajl_alloc(const yajl_callbacks * callbacks,
                                    yajl_alloc_funcs * afs,
                                    void * ctx);


    /** configuration parameters for the parser, these may be passed to
     *  yajl_config() along with option specific argument(s).  In general,
     *  all configuration parameters default to *off*. */
    typedef enum {
        /** Ignore javascript style comments present in
         *  JSON input.  Non-standard, but rather fun
         *  arguments: toggled off with integer zero, on otherwise.
         *
         *  example:
         *    yajl_config(h, yajl_allow_comments, 1); // turn comment support on
         */
        yajl_allow_comments = 0x01,
        /**
         * When set the parser will verify that all strings in JSON input are
         * valid UTF8 and will emit a parse error if this is not so.  When set,
         * this option makes parsing slightly more expensive (~7% depending
         * on processor and compiler in use)
         *
         * example:
         *   yajl_config(h, yajl_dont_validate_strings, 1); // disable utf8 checking
         */
        yajl_dont_validate_strings     = 0x02,
        /**
         * By default, upon calls to yajl_complete_parse(), yajl will
         * ensure the entire input text was consumed and will raise an error
         * otherwise.  Enabling this flag will cause yajl to disable this
         * check.  This can be useful when parsing json out of a that contains more
         * than a single JSON document.
         */
        yajl_allow_trailing_garbage = 0x04,
        /**
         * Allow multiple values to be parsed by a single handle.  The
         * entire text must be valid JSON, and values can be seperated
         * by any kind of whitespace.  This flag will change the
         * behavior of the parser, and cause it continue parsing after
         * a value is parsed, rather than transitioning into a
         * complete state.  This option can be useful when parsing multiple
         * values from an input stream.
         */
        yajl_allow_multiple_values = 0x08,
        /**
         * When yajl_complete_parse() is called the parser will
         * check that the top level value was completely consumed.  I.E.,
         * if called whilst in the middle of parsing a value
         * yajl will enter an error state (premature EOF).  Setting this
         * flag suppresses that check and the corresponding error.
         */
        yajl_allow_partial_values = 0x10
    } yajl_option;

    /** allow the modification of parser options subsequent to handle
     *  allocation (via yajl_alloc)
     *  \returns zero in case of errors, non-zero otherwise
     */
    YAJL_API int yajl_config(yajl_handle h, yajl_option opt, ...);

    /** free a parser handle */
    YAJL_API void yajl_free(yajl_handle handle);

    /** Parse some json!
     *  \param hand - a handle to the json parser allocated with yajl_alloc
     *  \param jsonText - a pointer to the UTF8 json text to be parsed
     *  \param jsonTextLength - the length, in bytes, of input text
     */
    YAJL_API yajl_status yajl_parse(yajl_handle hand,
                                    const unsigned char * jsonText,
                                    size_t jsonTextLength);

    /** Parse any remaining buffered json.
     *  Since yajl is a stream-based parser, without an explicit end of
     *  input, yajl sometimes can't decide if content at the end of the
     *  stream is valid or not.  For example, if "1" has been fed in,
     *  yajl can't know whether another digit is next or some character
     *  that would terminate the integer token.
     *
     *  \param hand - a handle to the json parser allocated with yajl_alloc
     */
    YAJL_API yajl_status yajl_complete_parse(yajl_handle hand);

    /** get an error string describing the state of the
     *  parse.
     *
     *  If verbose is non-zero, the message will include the JSON
     *  text where the error occured, along with an arrow pointing to
     *  the specific char.
     *
     *  \returns A dynamically allocated string will be returned which should
     *  be freed with yajl_free_error
     */
    YAJL_API unsigned char * yajl_get_error(yajl_handle hand, int verbose,
                                            const unsigned char * jsonText,
                                            size_t jsonTextLength);

    /**
     * get the amount of data consumed from the last chunk passed to YAJL.
     *
     * In the case of a successful parse this can help you understand if
     * the entire buffer was consumed (which will allow you to handle
     * "junk at end of input").
     *
     * In the event an error is encountered during parsing, this function
     * affords the client a way to get the offset into the most recent
     * chunk where the error occured.  0 will be returned if no error
     * was encountered.
     */
    YAJL_API size_t yajl_get_bytes_consumed(yajl_handle hand);

    /** free an error returned from yajl_get_error */
    YAJL_API void yajl_free_error(yajl_handle hand, unsigned char * str);

#ifdef __cplusplus
}
#endif

/*==============================================================*/
/* --- yajl_parser.h */

typedef enum {
    yajl_state_start = 0,
    yajl_state_parse_complete,
    yajl_state_parse_error,
    yajl_state_lexical_error,
    yajl_state_map_start,
    yajl_state_map_sep,
    yajl_state_map_need_val,
    yajl_state_map_got_val,
    yajl_state_map_need_key,
    yajl_state_array_start,
    yajl_state_array_got_val,
    yajl_state_array_need_val,
    yajl_state_got_value,
} yajl_state;

struct yajl_handle_t {
    const yajl_callbacks * callbacks;
    void * ctx;
    yajl_lexer lexer;
    const char * parseError;
    /* the number of bytes consumed from the last client buffer,
     * in the case of an error this will be an error offset, in the
     * case of an error this can be used as the error offset */
    size_t bytesConsumed;
    /* temporary storage for decoded strings */
    yajl_buf decodeBuf;
    /* a stack of states.  access with yajl_state_XXX routines */
    yajl_bytestack stateStack;
    /* memory allocation routines */
    yajl_alloc_funcs alloc;
    /* bitfield */
    unsigned int flags;
};

yajl_status
yajl_do_parse(yajl_handle handle, const unsigned char * jsonText,
              size_t jsonTextLen);

yajl_status
yajl_do_finish(yajl_handle handle);

unsigned char *
yajl_render_error_string(yajl_handle hand, const unsigned char * jsonText,
                         size_t jsonTextLen, int verbose);

/* A little built in integer parsing routine with the same semantics as strtol
 * that's unaffected by LOCALE. */
int64_t
yajl_parse_integer(const unsigned char *number, unsigned int length);

/*==============================================================*/
/* --- yajl_alloc.c */

/*
 * default memory allocation routines for yajl which use malloc/realloc and
 * free
 */

static void * yajl_internal_malloc(void *ctx, size_t sz)
{
    (void)ctx;
    return malloc(sz);
}

static void * yajl_internal_realloc(void *ctx, void * previous,
                                    size_t sz)
{
    (void)ctx;
    return realloc(previous, sz);
}

static void yajl_internal_free(void *ctx, void * ptr)
{
    (void)ctx;
    free(ptr);
}

void yajl_set_default_alloc_funcs(yajl_alloc_funcs * yaf)
{
    yaf->malloc = yajl_internal_malloc;
    yaf->free = yajl_internal_free;
    yaf->realloc = yajl_internal_realloc;
    yaf->ctx = NULL;
}

/*==============================================================*/
/* --- yajl_buf.c */

#define YAJL_BUF_INIT_SIZE 2048

struct yajl_buf_t {
    size_t len;
    size_t used;
    unsigned char * data;
    yajl_alloc_funcs * alloc;
};

static
void yajl_buf_ensure_available(yajl_buf buf, size_t want)
{
    size_t need;
    
    assert(buf != NULL);

    /* first call */
    if (buf->data == NULL) {
        buf->len = YAJL_BUF_INIT_SIZE;
        buf->data = (unsigned char *) YA_MALLOC(buf->alloc, buf->len);
        buf->data[0] = 0;
    }

    need = buf->len;

    while (want >= (need - buf->used)) need <<= 1;

    if (need != buf->len) {
        buf->data = (unsigned char *) YA_REALLOC(buf->alloc, buf->data, need);
        buf->len = need;
    }
}

yajl_buf yajl_buf_alloc(yajl_alloc_funcs * alloc)
{
    yajl_buf b = YA_MALLOC(alloc, sizeof(struct yajl_buf_t));
    memset((void *) b, 0, sizeof(struct yajl_buf_t));
    b->alloc = alloc;
    return b;
}

void yajl_buf_free(yajl_buf buf)
{
    assert(buf != NULL);
    if (buf->data) YA_FREE(buf->alloc, buf->data);
    YA_FREE(buf->alloc, buf);
}

void yajl_buf_append(yajl_buf buf, const void * data, size_t len)
{
    yajl_buf_ensure_available(buf, len);
    if (len > 0) {
        assert(data != NULL);
        memcpy(buf->data + buf->used, data, len);
        buf->used += len;
        buf->data[buf->used] = 0;
    }
}

void yajl_buf_clear(yajl_buf buf)
{
    buf->used = 0;
    if (buf->data) buf->data[buf->used] = 0;
}

const unsigned char * yajl_buf_data(yajl_buf buf)
{
    return buf->data;
}

size_t yajl_buf_len(yajl_buf buf)
{
    return buf->used;
}

void
yajl_buf_truncate(yajl_buf buf, size_t len)
{
    assert(len <= buf->used);
    buf->used = len;
}

/*==============================================================*/
/* --- yajl.c */

BSON_GNUC_CONST
const char *
yajl_status_to_string(yajl_status stat)
{
    const char * statStr = "unknown";
    switch (stat) {
        case yajl_status_ok:
            statStr = "ok, no error";
            break;
        case yajl_status_client_canceled:
            statStr = "client canceled parse";
            break;
        case yajl_status_error:
            statStr = "parse error";
            break;
    }
    return statStr;
}

yajl_handle
yajl_alloc(const yajl_callbacks * callbacks,
           yajl_alloc_funcs * afs,
           void * ctx)
{
    yajl_handle hand = NULL;
    yajl_alloc_funcs afsBuffer;

    /* first order of business is to set up memory allocation routines */
    if (afs != NULL) {
        if (afs->malloc == NULL || afs->realloc == NULL || afs->free == NULL)
        {
            return NULL;
        }
    } else {
        yajl_set_default_alloc_funcs(&afsBuffer);
        afs = &afsBuffer;
    }

    hand = (yajl_handle) YA_MALLOC(afs, sizeof(struct yajl_handle_t));

    /* copy in pointers to allocation routines */
    memcpy((void *) &(hand->alloc), (void *) afs, sizeof(yajl_alloc_funcs));

    hand->callbacks = callbacks;
    hand->ctx = ctx;
    hand->lexer = NULL; 
    hand->bytesConsumed = 0;
    hand->decodeBuf = yajl_buf_alloc(&(hand->alloc));
    hand->flags	    = 0;
    yajl_bs_init(hand->stateStack, &(hand->alloc));
    yajl_bs_push(hand->stateStack, yajl_state_start);

    return hand;
}

int
yajl_config(yajl_handle h, yajl_option opt, ...)
{
    int rv = 1;
    va_list ap;
    va_start(ap, opt);

    switch(opt) {
        case yajl_allow_comments:
        case yajl_dont_validate_strings:
        case yajl_allow_trailing_garbage:
        case yajl_allow_multiple_values:
        case yajl_allow_partial_values:
            if (va_arg(ap, int)) h->flags |= opt;
            else h->flags &= ~opt;
            break;
        default:
            rv = 0;
    }
    va_end(ap);

    return rv;
}

void
yajl_free(yajl_handle handle)
{
    yajl_bs_free(handle->stateStack);
    yajl_buf_free(handle->decodeBuf);
    if (handle->lexer) {
        yajl_lex_free(handle->lexer);
        handle->lexer = NULL;
    }
    YA_FREE(&(handle->alloc), handle);
}

yajl_status
yajl_parse(yajl_handle hand, const unsigned char * jsonText,
           size_t jsonTextLen)
{
    yajl_status status;

    /* lazy allocation of the lexer */
    if (hand->lexer == NULL) {
        hand->lexer = yajl_lex_alloc(&(hand->alloc),
                                     hand->flags & yajl_allow_comments,
                                     !(hand->flags & yajl_dont_validate_strings));
    }

    status = yajl_do_parse(hand, jsonText, jsonTextLen);
    return status;
}


yajl_status
yajl_complete_parse(yajl_handle hand)
{
    /* The lexer is lazy allocated in the first call to parse.  if parse is
     * never called, then no data was provided to parse at all.  This is a
     * "premature EOF" error unless yajl_allow_partial_values is specified.
     * allocating the lexer now is the simplest possible way to handle this
     * case while preserving all the other semantics of the parser
     * (multiple values, partial values, etc). */
    if (hand->lexer == NULL) {
        hand->lexer = yajl_lex_alloc(&(hand->alloc),
                                     hand->flags & yajl_allow_comments,
                                     !(hand->flags & yajl_dont_validate_strings));
    }

    return yajl_do_finish(hand);
}

unsigned char *
yajl_get_error(yajl_handle hand, int verbose,
               const unsigned char * jsonText, size_t jsonTextLen)
{
    return yajl_render_error_string(hand, jsonText, jsonTextLen, verbose);
}

BSON_GNUC_PURE
size_t
yajl_get_bytes_consumed(yajl_handle hand)
{
    if (!hand) return 0;
    else return hand->bytesConsumed;
}


void
yajl_free_error(yajl_handle hand, unsigned char * str)
{
    /* use memory allocation functions if set */
    YA_FREE(&(hand->alloc), str);
}

/* XXX: add utility routines to parse from file */
/*==============================================================*/
/* --- yajl_encode.c */

static void CharToHex(unsigned char c, char * hexBuf)
{
    const char * hexchar = "0123456789ABCDEF";
    hexBuf[0] = hexchar[c >> 4];
    hexBuf[1] = hexchar[c & 0x0F];
}

void
yajl_string_encode(const yajl_print_t print,
                   void * ctx,
                   const unsigned char * str,
                   size_t len,
                   int escape_solidus)
{
    size_t beg = 0;
    size_t end = 0;
    char hexBuf[7];
    hexBuf[0] = '\\'; hexBuf[1] = 'u'; hexBuf[2] = '0'; hexBuf[3] = '0';
    hexBuf[6] = 0;

    while (end < len) {
        const char * escaped = NULL;
        switch (str[end]) {
            case '\r': escaped = "\\r"; break;
            case '\n': escaped = "\\n"; break;
            case '\\': escaped = "\\\\"; break;
            /* it is not required to escape a solidus in JSON:
             * read sec. 2.5: http://www.ietf.org/rfc/rfc4627.txt
             * specifically, this production from the grammar:
             *   unescaped = %x20-21 / %x23-5B / %x5D-10FFFF
             */
            case '/': if (escape_solidus) escaped = "\\/"; break;
            case '"': escaped = "\\\""; break;
            case '\f': escaped = "\\f"; break;
            case '\b': escaped = "\\b"; break;
            case '\t': escaped = "\\t"; break;
            default:
                if ((unsigned char) str[end] < 32) {
                    CharToHex(str[end], hexBuf + 4);
                    escaped = hexBuf;
                }
                break;
        }
        if (escaped != NULL) {
            print(ctx, (const char *) (str + beg), end - beg);
            print(ctx, escaped, (unsigned int)strlen(escaped));
            beg = ++end;
        } else {
            ++end;
        }
    }
    print(ctx, (const char *) (str + beg), end - beg);
}

static void hexToDigit(unsigned int * val, const unsigned char * hex)
{
    unsigned int i;
    for (i=0;i<4;i++) {
        unsigned char c = hex[i];
        if (c >= 'A') c = (c & ~0x20) - 7;
        c -= '0';
        assert(!(c & 0xF0));
        *val = (*val << 4) | c;
    }
}

static void Utf32toUtf8(unsigned int codepoint, char * utf8Buf) 
{
    if (codepoint < 0x80) {
        utf8Buf[0] = (char) codepoint;
        utf8Buf[1] = 0;
    } else if (codepoint < 0x0800) {
        utf8Buf[0] = (char) ((codepoint >> 6) | 0xC0);
        utf8Buf[1] = (char) ((codepoint & 0x3F) | 0x80);
        utf8Buf[2] = 0;
    } else if (codepoint < 0x10000) {
        utf8Buf[0] = (char) ((codepoint >> 12) | 0xE0);
        utf8Buf[1] = (char) (((codepoint >> 6) & 0x3F) | 0x80);
        utf8Buf[2] = (char) ((codepoint & 0x3F) | 0x80);
        utf8Buf[3] = 0;
    } else if (codepoint < 0x200000) {
        utf8Buf[0] =(char)((codepoint >> 18) | 0xF0);
        utf8Buf[1] =(char)(((codepoint >> 12) & 0x3F) | 0x80);
        utf8Buf[2] =(char)(((codepoint >> 6) & 0x3F) | 0x80);
        utf8Buf[3] =(char)((codepoint & 0x3F) | 0x80);
        utf8Buf[4] = 0;
    } else {
        utf8Buf[0] = '?';
        utf8Buf[1] = 0;
    }
}

void yajl_string_decode(yajl_buf buf, const unsigned char * str,
                        size_t len)
{
    size_t beg = 0;
    size_t end = 0;    

    while (end < len) {
        if (str[end] == '\\') {
            char utf8Buf[5];
            const char * unescaped = "?";
            yajl_buf_append(buf, str + beg, end - beg);
            switch (str[++end]) {
                case 'r': unescaped = "\r"; break;
                case 'n': unescaped = "\n"; break;
                case '\\': unescaped = "\\"; break;
                case '/': unescaped = "/"; break;
                case '"': unescaped = "\""; break;
                case 'f': unescaped = "\f"; break;
                case 'b': unescaped = "\b"; break;
                case 't': unescaped = "\t"; break;
                case 'u': {
                    unsigned int codepoint = 0;
                    hexToDigit(&codepoint, str + ++end);
                    end+=3;
                    /* check if this is a surrogate */
                    if ((codepoint & 0xFC00) == 0xD800) {
                        end++;
                        if (str[end] == '\\' && str[end + 1] == 'u') {
                            unsigned int surrogate = 0;
                            hexToDigit(&surrogate, str + end + 2);
                            codepoint =
                                (((codepoint & 0x3F) << 10) | 
                                 ((((codepoint >> 6) & 0xF) + 1) << 16) | 
                                 (surrogate & 0x3FF));
                            end += 5;
                        } else {
                            unescaped = "?";
                            break;
                        }
                    }
                    
                    Utf32toUtf8(codepoint, utf8Buf);
                    unescaped = utf8Buf;

                    if (codepoint == 0) {
                        yajl_buf_append(buf, unescaped, 1);
                        beg = ++end;
                        continue;
                    }

                    break;
                }
                default:
                    assert("this should never happen" == NULL);
            }
            yajl_buf_append(buf, unescaped, (unsigned int)strlen(unescaped));
            beg = ++end;
        } else {
            end++;
        }
    }
    yajl_buf_append(buf, str + beg, end - beg);
}

#define ADV_PTR s++; if (!(len--)) return 0;

int yajl_string_validate_utf8(const unsigned char * s, size_t len)
{
    if (!len) return 1;
    if (!s) return 0;
    
    while (len--) {
        /* single byte */
        if (*s <= 0x7f) {
            /* noop */
        }
        /* two byte */ 
        else if ((*s >> 5) == 0x6) {
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
        }
        /* three byte */
        else if ((*s >> 4) == 0x0e) {
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
        }
        /* four byte */        
        else if ((*s >> 3) == 0x1e) {
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
            ADV_PTR;
            if (!((*s >> 6) == 0x2)) return 0;
        } else {
            return 0;
        }
        
        s++;
    }
    
    return 1;
}

/*==============================================================*/
/* --- yajl_gen.c */

typedef enum {
    yajl_gen_start,
    yajl_gen_map_start,
    yajl_gen_map_key,
    yajl_gen_map_val,
    yajl_gen_array_start,
    yajl_gen_in_array,
    yajl_gen_complete,
    yajl_gen_error
} yajl_gen_state;

struct yajl_gen_t
{
    unsigned int flags;
    unsigned int depth;
    const char * indentString;
    yajl_gen_state state[YAJL_MAX_DEPTH];
    yajl_print_t print;
    void * ctx; /* yajl_buf */
    /* memory allocation routines */
    yajl_alloc_funcs alloc;
};

int
yajl_gen_config(yajl_gen g, yajl_gen_option opt, ...)
{
    int rv = 1;
    va_list ap;
    va_start(ap, opt);

    switch(opt) {
        case yajl_gen_beautify:
        case yajl_gen_validate_utf8:
        case yajl_gen_escape_solidus:
            if (va_arg(ap, int)) g->flags |= opt;
            else g->flags &= ~opt;
            break;
        case yajl_gen_indent_string: {
            const char *indent = va_arg(ap, const char *);
            g->indentString = indent;
            for (; *indent; indent++) {
                if (*indent != '\n'
                    && *indent != '\v'
                    && *indent != '\f'
                    && *indent != '\t'
                    && *indent != '\r'
                    && *indent != ' ')
                {
                    g->indentString = NULL;
                    rv = 0;
                }
            }
            break;
        }
        case yajl_gen_print_callback:
            yajl_buf_free(g->ctx);
            g->print = va_arg(ap, const yajl_print_t);
            g->ctx = va_arg(ap, void *);
            break;
        default:
            rv = 0;
    }

    va_end(ap);

    return rv;
}



yajl_gen
yajl_gen_alloc(const yajl_alloc_funcs * afs)
{
    yajl_gen g = NULL;
    yajl_alloc_funcs afsBuffer;

    /* first order of business is to set up memory allocation routines */
    if (afs != NULL) {
        if (afs->malloc == NULL || afs->realloc == NULL || afs->free == NULL)
        {
            return NULL;
        }
    } else {
        yajl_set_default_alloc_funcs(&afsBuffer);
        afs = &afsBuffer;
    }

    g = (yajl_gen) YA_MALLOC(afs, sizeof(struct yajl_gen_t));
    if (!g) return NULL;

    memset((void *) g, 0, sizeof(struct yajl_gen_t));
    /* copy in pointers to allocation routines */
    memcpy((void *) &(g->alloc), (void *) afs, sizeof(yajl_alloc_funcs));

    g->print = (yajl_print_t)&yajl_buf_append;
    g->ctx = yajl_buf_alloc(&(g->alloc));
    g->indentString = "    ";

    return g;
}

void
yajl_gen_reset(yajl_gen g, const char * sep)
{
    g->depth = 0;
    memset((void *) &(g->state), 0, sizeof(g->state));
    if (sep != NULL) g->print(g->ctx, sep, strlen(sep));
}

void
yajl_gen_free(yajl_gen g)
{
    if (g->print == (yajl_print_t)&yajl_buf_append) yajl_buf_free((yajl_buf)g->ctx);
    YA_FREE(&(g->alloc), g);
}

#define INSERT_SEP \
    if (g->state[g->depth] == yajl_gen_map_key ||               \
        g->state[g->depth] == yajl_gen_in_array) {              \
        g->print(g->ctx, ",", 1);                               \
        if ((g->flags & yajl_gen_beautify)) g->print(g->ctx, "\n", 1);               \
    } else if (g->state[g->depth] == yajl_gen_map_val) {        \
        g->print(g->ctx, ":", 1);                               \
        if ((g->flags & yajl_gen_beautify)) g->print(g->ctx, " ", 1);                \
   }

#define INSERT_WHITESPACE                                               \
    if ((g->flags & yajl_gen_beautify)) {                                                    \
        if (g->state[g->depth] != yajl_gen_map_val) {                   \
            unsigned int _i;                                            \
            for (_i=0;_i<g->depth;_i++)                                 \
                g->print(g->ctx,                                        \
                         g->indentString,                               \
                         (unsigned int)strlen(g->indentString));        \
        }                                                               \
    }

#define ENSURE_NOT_KEY \
    if (g->state[g->depth] == yajl_gen_map_key ||       \
        g->state[g->depth] == yajl_gen_map_start)  {    \
        return yajl_gen_keys_must_be_strings;           \
    }                                                   \

/* check that we're not complete, or in error state.  in a valid state
 * to be generating */
#define ENSURE_VALID_STATE \
    if (g->state[g->depth] == yajl_gen_error) {   \
        return yajl_gen_in_error_state;\
    } else if (g->state[g->depth] == yajl_gen_complete) {   \
        return yajl_gen_generation_complete;                \
    }

#define INCREMENT_DEPTH \
    if (++(g->depth) >= YAJL_MAX_DEPTH) return yajl_max_depth_exceeded;

#define DECREMENT_DEPTH \
  if (--(g->depth) >= YAJL_MAX_DEPTH) return yajl_gen_generation_complete;

#define APPENDED_ATOM \
    switch (g->state[g->depth]) {                   \
        case yajl_gen_start:                        \
            g->state[g->depth] = yajl_gen_complete; \
            break;                                  \
        case yajl_gen_map_start:                    \
        case yajl_gen_map_key:                      \
            g->state[g->depth] = yajl_gen_map_val;  \
            break;                                  \
        case yajl_gen_array_start:                  \
            g->state[g->depth] = yajl_gen_in_array; \
            break;                                  \
        case yajl_gen_map_val:                      \
            g->state[g->depth] = yajl_gen_map_key;  \
            break;                                  \
        default:                                    \
            break;                                  \
    }                                               \

#define FINAL_NEWLINE                                        \
    if ((g->flags & yajl_gen_beautify) && g->state[g->depth] == yajl_gen_complete) \
        g->print(g->ctx, "\n", 1);

yajl_gen_status
yajl_gen_integer(yajl_gen g, int64_t number)
{
    char i[32];
    ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    sprintf(i, "%lld", (long long)number);
    g->print(g->ctx, i, (unsigned int)strlen(i));
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return yajl_gen_status_ok;
}

#if defined(_WIN32) || defined(WIN32)
#define isnan _isnan
#define isinf !_finite
#endif

yajl_gen_status
yajl_gen_double(yajl_gen g, double number)
{
    char i[32];
    ENSURE_VALID_STATE; ENSURE_NOT_KEY;
    if (isnan(number) || isinf(number)) return yajl_gen_invalid_number;
    INSERT_SEP; INSERT_WHITESPACE;
    sprintf(i, "%.20g", number);
    if (strspn(i, "0123456789-") == strlen(i)) {
        strcat(i, ".0");
    }
    g->print(g->ctx, i, (unsigned int)strlen(i));
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return yajl_gen_status_ok;
}

yajl_gen_status
yajl_gen_number(yajl_gen g, const char * s, size_t l)
{
    ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    g->print(g->ctx, s, l);
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return yajl_gen_status_ok;
}

yajl_gen_status
yajl_gen_string(yajl_gen g, const unsigned char * str,
                size_t len)
{
    // if validation is enabled, check that the string is valid utf8
    // XXX: This checking could be done a little faster, in the same pass as
    // the string encoding
    if (g->flags & yajl_gen_validate_utf8) {
        if (!yajl_string_validate_utf8(str, len)) {
            return yajl_gen_invalid_string;
        }
    }
    ENSURE_VALID_STATE; INSERT_SEP; INSERT_WHITESPACE;
    g->print(g->ctx, "\"", 1);
    yajl_string_encode(g->print, g->ctx, str, len, g->flags & yajl_gen_escape_solidus);
    g->print(g->ctx, "\"", 1);
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return yajl_gen_status_ok;
}

yajl_gen_status
yajl_gen_null(yajl_gen g)
{
    ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    g->print(g->ctx, "null", strlen("null"));
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return yajl_gen_status_ok;
}

yajl_gen_status
yajl_gen_bool(yajl_gen g, int boolean)
{
    const char * val = boolean ? "true" : "false";

	ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    g->print(g->ctx, val, (unsigned int)strlen(val));
    APPENDED_ATOM;
    FINAL_NEWLINE;
    return yajl_gen_status_ok;
}

yajl_gen_status
yajl_gen_map_open(yajl_gen g)
{
    ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    INCREMENT_DEPTH;

    g->state[g->depth] = yajl_gen_map_start;
    g->print(g->ctx, "{", 1);
    if ((g->flags & yajl_gen_beautify)) g->print(g->ctx, "\n", 1);
    FINAL_NEWLINE;
    return yajl_gen_status_ok;
}

yajl_gen_status
yajl_gen_map_close(yajl_gen g)
{
    ENSURE_VALID_STATE;
    DECREMENT_DEPTH;

    if ((g->flags & yajl_gen_beautify)) g->print(g->ctx, "\n", 1);
    APPENDED_ATOM;
    INSERT_WHITESPACE;
    g->print(g->ctx, "}", 1);
    FINAL_NEWLINE;
    return yajl_gen_status_ok;
}

yajl_gen_status
yajl_gen_array_open(yajl_gen g)
{
    ENSURE_VALID_STATE; ENSURE_NOT_KEY; INSERT_SEP; INSERT_WHITESPACE;
    INCREMENT_DEPTH;
    g->state[g->depth] = yajl_gen_array_start;
    g->print(g->ctx, "[", 1);
    if ((g->flags & yajl_gen_beautify)) g->print(g->ctx, "\n", 1);
    FINAL_NEWLINE;
    return yajl_gen_status_ok;
}

yajl_gen_status
yajl_gen_array_close(yajl_gen g)
{
    ENSURE_VALID_STATE;
    DECREMENT_DEPTH;
    if ((g->flags & yajl_gen_beautify)) g->print(g->ctx, "\n", 1);
    APPENDED_ATOM;
    INSERT_WHITESPACE;
    g->print(g->ctx, "]", 1);
    FINAL_NEWLINE;
    return yajl_gen_status_ok;
}

yajl_gen_status
yajl_gen_get_buf(yajl_gen g, const unsigned char ** buf,
                 size_t * len)
{
    if (g->print != (yajl_print_t)&yajl_buf_append) return yajl_gen_no_buf;
    *buf = yajl_buf_data((yajl_buf)g->ctx);
    *len = yajl_buf_len((yajl_buf)g->ctx);
    return yajl_gen_status_ok;
}

void
yajl_gen_clear(yajl_gen g)
{
    if (g->print == (yajl_print_t)&yajl_buf_append) yajl_buf_clear((yajl_buf)g->ctx);
}

/*==============================================================*/
/* --- yajl_lex.c */

#ifdef YAJL_LEXER_DEBUG
static const char *
tokToStr(yajl_tok tok)
{
    switch (tok) {
        case yajl_tok_bool: return "bool";
        case yajl_tok_colon: return "colon";
        case yajl_tok_comma: return "comma";
        case yajl_tok_eof: return "eof";
        case yajl_tok_error: return "error";
        case yajl_tok_left_brace: return "brace";
        case yajl_tok_left_bracket: return "bracket";
        case yajl_tok_null: return "null";
        case yajl_tok_integer: return "integer";
        case yajl_tok_double: return "double";
        case yajl_tok_right_brace: return "brace";
        case yajl_tok_right_bracket: return "bracket";
        case yajl_tok_string: return "string";
        case yajl_tok_string_with_escapes: return "string_with_escapes";
    }
    return "unknown";
}
#endif

/* Impact of the stream parsing feature on the lexer:
 *
 * YAJL support stream parsing.  That is, the ability to parse the first
 * bits of a chunk of JSON before the last bits are available (still on
 * the network or disk).  This makes the lexer more complex.  The
 * responsibility of the lexer is to handle transparently the case where
 * a chunk boundary falls in the middle of a token.  This is
 * accomplished is via a buffer and a character reading abstraction.
 *
 * Overview of implementation
 *
 * When we lex to end of input string before end of token is hit, we
 * copy all of the input text composing the token into our lexBuf.
 *
 * Every time we read a character, we do so through the readChar function.
 * readChar's responsibility is to handle pulling all chars from the buffer
 * before pulling chars from input text
 */

struct yajl_lexer_t {
    /* the overal line and char offset into the data */
    size_t lineOff;
    size_t charOff;

    /* error */
    yajl_lex_error error;

    /* a input buffer to handle the case where a token is spread over
     * multiple chunks */
    yajl_buf buf;

    /* in the case where we have data in the lexBuf, bufOff holds
     * the current offset into the lexBuf. */
    size_t bufOff;

    /* are we using the lex buf? */
    unsigned int bufInUse;

    /* shall we allow comments? */
    unsigned int allowComments;

    /* shall we validate utf8 inside strings? */
    unsigned int validateUTF8;

    yajl_alloc_funcs * alloc;
};

#define readChar(lxr, txt, off)                      \
    (((lxr)->bufInUse && yajl_buf_len((lxr)->buf) && lxr->bufOff < yajl_buf_len((lxr)->buf)) ? \
     (*((const unsigned char *) yajl_buf_data((lxr)->buf) + ((lxr)->bufOff)++)) : \
     ((txt)[(*(off))++]))

#define unreadChar(lxr, off) ((*(off) > 0) ? (*(off))-- : ((lxr)->bufOff--))

yajl_lexer
yajl_lex_alloc(yajl_alloc_funcs * alloc,
               unsigned int allowComments, unsigned int validateUTF8)
{
    yajl_lexer lxr = (yajl_lexer) YA_MALLOC(alloc, sizeof(struct yajl_lexer_t));
    memset((void *) lxr, 0, sizeof(struct yajl_lexer_t));
    lxr->buf = yajl_buf_alloc(alloc);
    lxr->allowComments = allowComments;
    lxr->validateUTF8 = validateUTF8;
    lxr->alloc = alloc;
    return lxr;
}

void
yajl_lex_free(yajl_lexer lxr)
{
    yajl_buf_free(lxr->buf);
    YA_FREE(lxr->alloc, lxr);
    return;
}

/* a lookup table which lets us quickly determine three things:
 * VEC - valid escaped control char
 * note.  the solidus '/' may be escaped or not.
 * IJC - invalid json char
 * VHC - valid hex char
 * NFP - needs further processing (from a string scanning perspective)
 * NUC - needs utf8 checking when enabled (from a string scanning perspective)
 */
#define VEC 0x01
#define IJC 0x02
#define VHC 0x04
#define NFP 0x08
#define NUC 0x10

static const char charLookupTable[256] =
{
/*00*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,
/*08*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,
/*10*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,
/*18*/ IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    , IJC    ,

/*20*/ 0      , 0      , NFP|VEC|IJC, 0      , 0      , 0      , 0      , 0      ,
/*28*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , VEC    ,
/*30*/ VHC    , VHC    , VHC    , VHC    , VHC    , VHC    , VHC    , VHC    ,
/*38*/ VHC    , VHC    , 0      , 0      , 0      , 0      , 0      , 0      ,

/*40*/ 0      , VHC    , VHC    , VHC    , VHC    , VHC    , VHC    , 0      ,
/*48*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0      ,
/*50*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0      ,
/*58*/ 0      , 0      , 0      , 0      , NFP|VEC|IJC, 0      , 0      , 0      ,

/*60*/ 0      , VHC    , VEC|VHC, VHC    , VHC    , VHC    , VEC|VHC, 0      ,
/*68*/ 0      , 0      , 0      , 0      , 0      , 0      , VEC    , 0      ,
/*70*/ 0      , 0      , VEC    , 0      , VEC    , 0      , 0      , 0      ,
/*78*/ 0      , 0      , 0      , 0      , 0      , 0      , 0      , 0      ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,

       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    ,
       NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC    , NUC
};

/** process a variable length utf8 encoded codepoint.
 *
 *  returns:
 *    yajl_tok_string - if valid utf8 char was parsed and offset was
 *                      advanced
 *    yajl_tok_eof - if end of input was hit before validation could
 *                   complete
 *    yajl_tok_error - if invalid utf8 was encountered
 *
 *  NOTE: on error the offset will point to the first char of the
 *  invalid utf8 */
#define UTF8_CHECK_EOF if (*offset >= jsonTextLen) { return yajl_tok_eof; }

static yajl_tok
yajl_lex_utf8_char(yajl_lexer lexer, const unsigned char * jsonText,
                   size_t jsonTextLen, size_t * offset,
                   unsigned char curChar)
{
    if (curChar <= 0x7f) {
        /* single byte */
        return yajl_tok_string;
    } else if ((curChar >> 5) == 0x6) {
        /* two byte */
        UTF8_CHECK_EOF;
        curChar = readChar(lexer, jsonText, offset);
        if ((curChar >> 6) == 0x2) return yajl_tok_string;
    } else if ((curChar >> 4) == 0x0e) {
        /* three byte */
        UTF8_CHECK_EOF;
        curChar = readChar(lexer, jsonText, offset);
        if ((curChar >> 6) == 0x2) {
            UTF8_CHECK_EOF;
            curChar = readChar(lexer, jsonText, offset);
            if ((curChar >> 6) == 0x2) return yajl_tok_string;
        }
    } else if ((curChar >> 3) == 0x1e) {
        /* four byte */
        UTF8_CHECK_EOF;
        curChar = readChar(lexer, jsonText, offset);
        if ((curChar >> 6) == 0x2) {
            UTF8_CHECK_EOF;
            curChar = readChar(lexer, jsonText, offset);
            if ((curChar >> 6) == 0x2) {
                UTF8_CHECK_EOF;
                curChar = readChar(lexer, jsonText, offset);
                if ((curChar >> 6) == 0x2) return yajl_tok_string;
            }
        }
    }

    return yajl_tok_error;
}

/* lex a string.  input is the lexer, pointer to beginning of
 * json text, and start of string (offset).
 * a token is returned which has the following meanings:
 * yajl_tok_string: lex of string was successful.  offset points to
 *                  terminating '"'.
 * yajl_tok_eof: end of text was encountered before we could complete
 *               the lex.
 * yajl_tok_error: embedded in the string were unallowable chars.  offset
 *               points to the offending char
 */
#define STR_CHECK_EOF \
if (*offset >= jsonTextLen) { \
   tok = yajl_tok_eof; \
   goto finish_string_lex; \
}

/** scan a string for interesting characters that might need further
 *  review.  return the number of chars that are uninteresting and can
 *  be skipped.
 * (lth) hi world, any thoughts on how to make this routine faster? */
static size_t
yajl_string_scan(const unsigned char * buf, size_t len, int utf8check)
{
    unsigned char mask = IJC|NFP|(utf8check ? NUC : 0);
    size_t skip = 0;
    while (skip < len && !(charLookupTable[*buf] & mask))
    {
        skip++;
        buf++;
    }
    return skip;
}

static yajl_tok
yajl_lex_string(yajl_lexer lexer, const unsigned char * jsonText,
                size_t jsonTextLen, size_t * offset)
{
    yajl_tok tok = yajl_tok_error;
    int hasEscapes = 0;

    for (;;) {
        unsigned char curChar;

        /* now jump into a faster scanning routine to skip as much
         * of the buffers as possible */
        {
            const unsigned char * p;
            size_t len;

            if ((lexer->bufInUse && yajl_buf_len(lexer->buf) &&
                 lexer->bufOff < yajl_buf_len(lexer->buf)))
            {
                p = ((const unsigned char *) yajl_buf_data(lexer->buf) +
                     (lexer->bufOff));
                len = yajl_buf_len(lexer->buf) - lexer->bufOff;
                lexer->bufOff += yajl_string_scan(p, len, lexer->validateUTF8);
            }
            else if (*offset < jsonTextLen)
            {
                p = jsonText + *offset;
                len = jsonTextLen - *offset;
                *offset += yajl_string_scan(p, len, lexer->validateUTF8);
            }
        }

        STR_CHECK_EOF;

        curChar = readChar(lexer, jsonText, offset);

        /* quote terminates */
        if (curChar == '"') {
            tok = yajl_tok_string;
            break;
        }
        /* backslash escapes a set of control chars, */
        else if (curChar == '\\') {
            hasEscapes = 1;
            STR_CHECK_EOF;

            /* special case \u */
            curChar = readChar(lexer, jsonText, offset);
            if (curChar == 'u') {
                unsigned int i = 0;

                for (i=0;i<4;i++) {
                    STR_CHECK_EOF;
                    curChar = readChar(lexer, jsonText, offset);
                    if (!(charLookupTable[curChar] & VHC)) {
                        /* back up to offending char */
                        unreadChar(lexer, offset);
                        lexer->error = yajl_lex_string_invalid_hex_char;
                        goto finish_string_lex;
                    }
                }
            } else if (!(charLookupTable[curChar] & VEC)) {
                /* back up to offending char */
                unreadChar(lexer, offset);
                lexer->error = yajl_lex_string_invalid_escaped_char;
                goto finish_string_lex;
            }
        }
        /* when not validating UTF8 it's a simple table lookup to determine
         * if the present character is invalid */
        else if(charLookupTable[curChar] & IJC) {
            /* back up to offending char */
            unreadChar(lexer, offset);
            lexer->error = yajl_lex_string_invalid_json_char;
            goto finish_string_lex;
        }
        /* when in validate UTF8 mode we need to do some extra work */
        else if (lexer->validateUTF8) {
            yajl_tok t = yajl_lex_utf8_char(lexer, jsonText, jsonTextLen,
                                            offset, curChar);

            if (t == yajl_tok_eof) {
                tok = yajl_tok_eof;
                goto finish_string_lex;
            } else if (t == yajl_tok_error) {
                lexer->error = yajl_lex_string_invalid_utf8;
                goto finish_string_lex;
            }
        }
        /* accept it, and move on */
    }
  finish_string_lex:
    /* tell our buddy, the parser, wether he needs to process this string
     * again */
    if (hasEscapes && tok == yajl_tok_string) {
        tok = yajl_tok_string_with_escapes;
    }

    return tok;
}

#define RETURN_IF_EOF if (*offset >= jsonTextLen) return yajl_tok_eof;

static yajl_tok
yajl_lex_number(yajl_lexer lexer, const unsigned char * jsonText,
                size_t jsonTextLen, size_t * offset)
{
    /** XXX: numbers are the only entities in json that we must lex
     *       _beyond_ in order to know that they are complete.  There
     *       is an ambiguous case for integers at EOF. */

    unsigned char c;

    yajl_tok tok = yajl_tok_integer;

    RETURN_IF_EOF;
    c = readChar(lexer, jsonText, offset);

    /* optional leading minus */
    if (c == '-') {
        RETURN_IF_EOF;
        c = readChar(lexer, jsonText, offset);
    }

    /* a single zero, or a series of integers */
    if (c == '0') {
        RETURN_IF_EOF;
        c = readChar(lexer, jsonText, offset);
    } else if (c >= '1' && c <= '9') {
        do {
            RETURN_IF_EOF;
            c = readChar(lexer, jsonText, offset);
        } while (c >= '0' && c <= '9');
    } else {
        unreadChar(lexer, offset);
        lexer->error = yajl_lex_missing_integer_after_minus;
        return yajl_tok_error;
    }

    /* optional fraction (indicates this is floating point) */
    if (c == '.') {
        int numRd = 0;

        RETURN_IF_EOF;
        c = readChar(lexer, jsonText, offset);

        while (c >= '0' && c <= '9') {
            numRd++;
            RETURN_IF_EOF;
            c = readChar(lexer, jsonText, offset);
        }

        if (!numRd) {
            unreadChar(lexer, offset);
            lexer->error = yajl_lex_missing_integer_after_decimal;
            return yajl_tok_error;
        }
        tok = yajl_tok_double;
    }

    /* optional exponent (indicates this is floating point) */
    if (c == 'e' || c == 'E') {
        RETURN_IF_EOF;
        c = readChar(lexer, jsonText, offset);

        /* optional sign */
        if (c == '+' || c == '-') {
            RETURN_IF_EOF;
            c = readChar(lexer, jsonText, offset);
        }

        if (c >= '0' && c <= '9') {
            do {
                RETURN_IF_EOF;
                c = readChar(lexer, jsonText, offset);
            } while (c >= '0' && c <= '9');
        } else {
            unreadChar(lexer, offset);
            lexer->error = yajl_lex_missing_integer_after_exponent;
            return yajl_tok_error;
        }
        tok = yajl_tok_double;
    }

    /* we always go "one too far" */
    unreadChar(lexer, offset);

    return tok;
}

static yajl_tok
yajl_lex_comment(yajl_lexer lexer, const unsigned char * jsonText,
                 size_t jsonTextLen, size_t * offset)
{
    unsigned char c;

    yajl_tok tok = yajl_tok_comment;

    RETURN_IF_EOF;
    c = readChar(lexer, jsonText, offset);

    /* either slash or star expected */
    if (c == '/') {
        /* now we throw away until end of line */
        do {
            RETURN_IF_EOF;
            c = readChar(lexer, jsonText, offset);
        } while (c != '\n');
    } else if (c == '*') {
        /* now we throw away until end of comment */
        for (;;) {
            RETURN_IF_EOF;
            c = readChar(lexer, jsonText, offset);
            if (c == '*') {
                RETURN_IF_EOF;
                c = readChar(lexer, jsonText, offset);
                if (c == '/') {
                    break;
                } else {
                    unreadChar(lexer, offset);
                }
            }
        }
    } else {
        lexer->error = yajl_lex_invalid_char;
        tok = yajl_tok_error;
    }

    return tok;
}

yajl_tok
yajl_lex_lex(yajl_lexer lexer, const unsigned char * jsonText,
             size_t jsonTextLen, size_t * offset,
             const unsigned char ** outBuf, size_t * outLen)
{
    yajl_tok tok = yajl_tok_error;
    unsigned char c;
    size_t startOffset = *offset;

    *outBuf = NULL;
    *outLen = 0;

    for (;;) {
        assert(*offset <= jsonTextLen);

        if (*offset >= jsonTextLen) {
            tok = yajl_tok_eof;
            goto lexed;
        }

        c = readChar(lexer, jsonText, offset);

        switch (c) {
            case '{':
                tok = yajl_tok_left_bracket;
                goto lexed;
            case '}':
                tok = yajl_tok_right_bracket;
                goto lexed;
            case '[':
                tok = yajl_tok_left_brace;
                goto lexed;
            case ']':
                tok = yajl_tok_right_brace;
                goto lexed;
            case ',':
                tok = yajl_tok_comma;
                goto lexed;
            case ':':
                tok = yajl_tok_colon;
                goto lexed;
            case '\t': case '\n': case '\v': case '\f': case '\r': case ' ':
                startOffset++;
                break;
            case 't': {
                const char * want = "rue";
                do {
                    if (*offset >= jsonTextLen) {
                        tok = yajl_tok_eof;
                        goto lexed;
                    }
                    c = readChar(lexer, jsonText, offset);
                    if (c != *want) {
                        unreadChar(lexer, offset);
                        lexer->error = yajl_lex_invalid_string;
                        tok = yajl_tok_error;
                        goto lexed;
                    }
                } while (*(++want));
                tok = yajl_tok_bool;
                goto lexed;
            }
            case 'f': {
                const char * want = "alse";
                do {
                    if (*offset >= jsonTextLen) {
                        tok = yajl_tok_eof;
                        goto lexed;
                    }
                    c = readChar(lexer, jsonText, offset);
                    if (c != *want) {
                        unreadChar(lexer, offset);
                        lexer->error = yajl_lex_invalid_string;
                        tok = yajl_tok_error;
                        goto lexed;
                    }
                } while (*(++want));
                tok = yajl_tok_bool;
                goto lexed;
            }
            case 'n': {
                const char * want = "ull";
                do {
                    if (*offset >= jsonTextLen) {
                        tok = yajl_tok_eof;
                        goto lexed;
                    }
                    c = readChar(lexer, jsonText, offset);
                    if (c != *want) {
                        unreadChar(lexer, offset);
                        lexer->error = yajl_lex_invalid_string;
                        tok = yajl_tok_error;
                        goto lexed;
                    }
                } while (*(++want));
                tok = yajl_tok_null;
                goto lexed;
            }
            case '"': {
                tok = yajl_lex_string(lexer, (const unsigned char *) jsonText,
                                      jsonTextLen, offset);
                goto lexed;
            }
            case '-':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9': {
                /* integer parsing wants to start from the beginning */
                unreadChar(lexer, offset);
                tok = yajl_lex_number(lexer, (const unsigned char *) jsonText,
                                      jsonTextLen, offset);
                goto lexed;
            }
            case '/':
                /* hey, look, a probable comment!  If comments are disabled
                 * it's an error. */
                if (!lexer->allowComments) {
                    unreadChar(lexer, offset);
                    lexer->error = yajl_lex_unallowed_comment;
                    tok = yajl_tok_error;
                    goto lexed;
                }
                /* if comments are enabled, then we should try to lex
                 * the thing.  possible outcomes are
                 * - successful lex (tok_comment, which means continue),
                 * - malformed comment opening (slash not followed by
                 *   '*' or '/') (tok_error)
                 * - eof hit. (tok_eof) */
                tok = yajl_lex_comment(lexer, (const unsigned char *) jsonText,
                                       jsonTextLen, offset);
                if (tok == yajl_tok_comment) {
                    /* "error" is silly, but that's the initial
                     * state of tok.  guilty until proven innocent. */
                    tok = yajl_tok_error;
                    yajl_buf_clear(lexer->buf);
                    lexer->bufInUse = 0;
                    startOffset = *offset;
                    break;
                }
                /* hit error or eof, bail */
                goto lexed;
            default:
                lexer->error = yajl_lex_invalid_char;
                tok = yajl_tok_error;
                goto lexed;
        }
    }


  lexed:
    /* need to append to buffer if the buffer is in use or
     * if it's an EOF token */
    if (tok == yajl_tok_eof || lexer->bufInUse) {
        if (!lexer->bufInUse) yajl_buf_clear(lexer->buf);
        lexer->bufInUse = 1;
        yajl_buf_append(lexer->buf, jsonText + startOffset, *offset - startOffset);
        lexer->bufOff = 0;

        if (tok != yajl_tok_eof) {
            *outBuf = yajl_buf_data(lexer->buf);
            *outLen = yajl_buf_len(lexer->buf);
            lexer->bufInUse = 0;
        }
    } else if (tok != yajl_tok_error) {
        *outBuf = jsonText + startOffset;
        *outLen = *offset - startOffset;
    }

    /* special case for strings. skip the quotes. */
    if (tok == yajl_tok_string || tok == yajl_tok_string_with_escapes)
    {
        assert(*outLen >= 2);
        (*outBuf)++;
        *outLen -= 2;
    }


#ifdef YAJL_LEXER_DEBUG
    if (tok == yajl_tok_error) {
        printf("lexical error: %s\n",
               yajl_lex_error_to_string(yajl_lex_get_error(lexer)));
    } else if (tok == yajl_tok_eof) {
        printf("EOF hit\n");
    } else {
        printf("lexed %s: '", tokToStr(tok));
        fwrite(*outBuf, 1, *outLen, stdout);
        printf("'\n");
    }
#endif

    return tok;
}

const char *
yajl_lex_error_to_string(yajl_lex_error error)
{
    switch (error) {
        case yajl_lex_e_ok:
            return "ok, no error";
        case yajl_lex_string_invalid_utf8:
            return "invalid bytes in UTF8 string.";
        case yajl_lex_string_invalid_escaped_char:
            return "inside a string, '\\' occurs before a character "
                   "which it may not.";
        case yajl_lex_string_invalid_json_char:
            return "invalid character inside string.";
        case yajl_lex_string_invalid_hex_char:
            return "invalid (non-hex) character occurs after '\\u' inside "
                   "string.";
        case yajl_lex_invalid_char:
            return "invalid char in json text.";
        case yajl_lex_invalid_string:
            return "invalid string in json text.";
        case yajl_lex_missing_integer_after_exponent:
            return "malformed number, a digit is required after the exponent.";
        case yajl_lex_missing_integer_after_decimal:
            return "malformed number, a digit is required after the "
                   "decimal point.";
        case yajl_lex_missing_integer_after_minus:
            return "malformed number, a digit is required after the "
                   "minus sign.";
        case yajl_lex_unallowed_comment:
            return "probable comment found in input text, comments are "
                   "not enabled.";
    }
    return "unknown error code";
}


/** allows access to more specific information about the lexical
 *  error when yajl_lex_lex returns yajl_tok_error. */
yajl_lex_error
yajl_lex_get_error(yajl_lexer lexer)
{
    if (lexer == NULL) return (yajl_lex_error) -1;
    return lexer->error;
}

size_t yajl_lex_current_line(yajl_lexer lexer)
{
    return lexer->lineOff;
}

BSON_GNUC_PURE
size_t yajl_lex_current_char(yajl_lexer lexer)
{
    return lexer->charOff;
}

yajl_tok yajl_lex_peek(yajl_lexer lexer, const unsigned char * jsonText,
                       size_t jsonTextLen, size_t offset)
{
    const unsigned char * outBuf;
    size_t outLen;
    size_t bufLen = yajl_buf_len(lexer->buf);
    size_t bufOff = lexer->bufOff;
    unsigned int bufInUse = lexer->bufInUse;
    yajl_tok tok;

    tok = yajl_lex_lex(lexer, jsonText, jsonTextLen, &offset,
                       &outBuf, &outLen);

    lexer->bufOff = bufOff;
    lexer->bufInUse = bufInUse;
    yajl_buf_truncate(lexer->buf, bufLen);

    return tok;
}

/*==============================================================*/
/* --- yajl_parser.c */

#define MAX_VALUE_TO_MULTIPLY ((INT64_MAX / 10) + (INT64_MAX % 10))

 /* same semantics as strtol */
int64_t
yajl_parse_integer(const unsigned char *number, unsigned int length)
{
    int64_t ret  = 0;
    long sign = 1;
    const unsigned char *pos = number;
    if (*pos == '-') { pos++; sign = -1; }
    if (*pos == '+') { pos++; }

    while (pos < number + length) {
        if ( ret > MAX_VALUE_TO_MULTIPLY ) {
            errno = ERANGE;
            return sign == 1 ? INT64_MAX : INT64_MIN;
        }
        ret *= 10;
        if (INT64_MAX - ret < (*pos - '0')) {
            errno = ERANGE;
            return sign == 1 ? INT64_MAX : INT64_MIN;
        }
        if (*pos < '0' || *pos > '9') {
            errno = ERANGE;
            return sign == 1 ? INT64_MAX : INT64_MIN;
        }
        ret += (*pos++ - '0');
    }

    return sign * ret;
}

unsigned char *
yajl_render_error_string(yajl_handle hand, const unsigned char * jsonText,
                         size_t jsonTextLen, int verbose)
{
    size_t offset = hand->bytesConsumed;
    unsigned char * str;
    const char * errorType = NULL;
    const char * errorText = NULL;
    char text[72];
    const char * arrow = "                     (right here) ------^\n";

    if (yajl_bs_current(hand->stateStack) == yajl_state_parse_error) {
        errorType = "parse";
        errorText = hand->parseError;
    } else if (yajl_bs_current(hand->stateStack) == yajl_state_lexical_error) {
        errorType = "lexical";
        errorText = yajl_lex_error_to_string(yajl_lex_get_error(hand->lexer));
    } else {
        errorType = "unknown";
    }

    {
        size_t memneeded = 0;
        memneeded += strlen(errorType);
        memneeded += strlen(" error");
        if (errorText != NULL) {
            memneeded += strlen(": ");
            memneeded += strlen(errorText);
        }
        str = (unsigned char *) YA_MALLOC(&(hand->alloc), memneeded + 2);
        if (!str) return NULL;
        str[0] = 0;
        strcat((char *) str, errorType);
        strcat((char *) str, " error");
        if (errorText != NULL) {
            strcat((char *) str, ": ");
            strcat((char *) str, errorText);
        }
        strcat((char *) str, "\n");
    }

    /* now we append as many spaces as needed to make sure the error
     * falls at char 41, if verbose was specified */
    if (verbose) {
        size_t start, end, i;
        size_t spacesNeeded;

        spacesNeeded = (offset < 30 ? 40 - offset : 10);
        start = (offset >= 30 ? offset - 30 : 0);
        end = (offset + 30 > jsonTextLen ? jsonTextLen : offset + 30);

        for (i=0;i<spacesNeeded;i++) text[i] = ' ';

        for (;start < end;start++, i++) {
            if (jsonText[start] != '\n' && jsonText[start] != '\r')
            {
                text[i] = jsonText[start];
            }
            else
            {
                text[i] = ' ';
            }
        }
        assert(i <= 71);
        text[i++] = '\n';
        text[i] = 0;
        {
            char * newStr = (char *)
                YA_MALLOC(&(hand->alloc), (unsigned int)(strlen((char *) str) +
                                                         strlen((char *) text) +
                                                         strlen(arrow) + 1));
            if (newStr) {
                newStr[0] = 0;
                strcat((char *) newStr, (char *) str);
                strcat((char *) newStr, text);
                strcat((char *) newStr, arrow);
            }
            YA_FREE(&(hand->alloc), str);
            str = (unsigned char *) newStr;
        }
    }
    return str;
}

/* check for client cancelation */
#define _CC_CHK(x)                                                \
    if (!(x)) {                                                   \
        yajl_bs_set(hand->stateStack, yajl_state_parse_error);    \
        hand->parseError =                                        \
            "client cancelled parse via callback return value";   \
        return yajl_status_client_canceled;                       \
    }


yajl_status
yajl_do_finish(yajl_handle hand)
{
    yajl_status stat;
    stat = yajl_do_parse(hand,(const unsigned char *) " ",1);

    if (stat != yajl_status_ok) return stat;

    switch(yajl_bs_current(hand->stateStack))
    {
        case yajl_state_parse_error:
        case yajl_state_lexical_error:
            return yajl_status_error;
        case yajl_state_got_value:
        case yajl_state_parse_complete:
            return yajl_status_ok;
        default:
            if (!(hand->flags & yajl_allow_partial_values))
            {
                yajl_bs_set(hand->stateStack, yajl_state_parse_error);
                hand->parseError = "premature EOF";
                return yajl_status_error;
            }
            return yajl_status_ok;
    }
}

yajl_status
yajl_do_parse(yajl_handle hand, const unsigned char * jsonText,
              size_t jsonTextLen)
{
    yajl_tok tok;
    const unsigned char * buf;
    size_t bufLen;
    size_t * offset = &(hand->bytesConsumed);

    *offset = 0;

  around_again:
    switch (yajl_bs_current(hand->stateStack)) {
        case yajl_state_parse_complete:
            if (hand->flags & yajl_allow_multiple_values) {
                yajl_bs_set(hand->stateStack, yajl_state_got_value);
                goto around_again;
            }
            if (!(hand->flags & yajl_allow_trailing_garbage)) {
                if (*offset != jsonTextLen) {
                    tok = yajl_lex_lex(hand->lexer, jsonText, jsonTextLen,
                                       offset, &buf, &bufLen);
                    if (tok != yajl_tok_eof) {
                        yajl_bs_set(hand->stateStack, yajl_state_parse_error);
                        hand->parseError = "trailing garbage";
                    }
                    goto around_again;
                }
            }
            return yajl_status_ok;
        case yajl_state_lexical_error:
        case yajl_state_parse_error:
            return yajl_status_error;
        case yajl_state_start:
        case yajl_state_got_value:
        case yajl_state_map_need_val:
        case yajl_state_array_need_val:
        case yajl_state_array_start:  {
            /* for arrays and maps, we advance the state for this
             * depth, then push the state of the next depth.
             * If an error occurs during the parsing of the nesting
             * enitity, the state at this level will not matter.
             * a state that needs pushing will be anything other
             * than state_start */

            yajl_state stateToPush = yajl_state_start;

            tok = yajl_lex_lex(hand->lexer, jsonText, jsonTextLen,
                               offset, &buf, &bufLen);

            switch (tok) {
                case yajl_tok_eof:
                    return yajl_status_ok;
                case yajl_tok_error:
                    yajl_bs_set(hand->stateStack, yajl_state_lexical_error);
                    goto around_again;
                case yajl_tok_string:
                    if (hand->callbacks && hand->callbacks->yajl_string) {
                        _CC_CHK(hand->callbacks->yajl_string(hand->ctx,
                                                             buf, bufLen));
                    }
                    break;
                case yajl_tok_string_with_escapes:
                    if (hand->callbacks && hand->callbacks->yajl_string) {
                        yajl_buf_clear(hand->decodeBuf);
                        yajl_string_decode(hand->decodeBuf, buf, bufLen);
                        _CC_CHK(hand->callbacks->yajl_string(
                                    hand->ctx, yajl_buf_data(hand->decodeBuf),
                                    yajl_buf_len(hand->decodeBuf)));
                    }
                    break;
                case yajl_tok_bool:
                    if (hand->callbacks && hand->callbacks->yajl_boolean) {
                        _CC_CHK(hand->callbacks->yajl_boolean(hand->ctx,
                                                              *buf == 't'));
                    }
                    break;
                case yajl_tok_null:
                    if (hand->callbacks && hand->callbacks->yajl_null) {
                        _CC_CHK(hand->callbacks->yajl_null(hand->ctx));
                    }
                    break;
                case yajl_tok_left_bracket:
                    if (hand->callbacks && hand->callbacks->yajl_start_map) {
                        _CC_CHK(hand->callbacks->yajl_start_map(hand->ctx));
                    }
                    stateToPush = yajl_state_map_start;
                    break;
                case yajl_tok_left_brace:
                    if (hand->callbacks && hand->callbacks->yajl_start_array) {
                        _CC_CHK(hand->callbacks->yajl_start_array(hand->ctx));
                    }
                    stateToPush = yajl_state_array_start;
                    break;
                case yajl_tok_integer:
                    if (hand->callbacks) {
                        if (hand->callbacks->yajl_number) {
                            _CC_CHK(hand->callbacks->yajl_number(
                                        hand->ctx,(const char *) buf, bufLen));
                        } else if (hand->callbacks->yajl_integer) {
                            int64_t i = 0;
                            errno = 0;
                            i = yajl_parse_integer(buf, bufLen);
                            if ((i == INT64_MIN || i == INT64_MAX) &&
                                errno == ERANGE)
                            {
                                yajl_bs_set(hand->stateStack,
                                            yajl_state_parse_error);
                                hand->parseError = "integer overflow" ;
                                /* try to restore error offset */
                                if (*offset >= bufLen) *offset -= bufLen;
                                else *offset = 0;
                                goto around_again;
                            }
                            _CC_CHK(hand->callbacks->yajl_integer(hand->ctx,
                                                                  i));
                        }
                    }
                    break;
                case yajl_tok_double:
                    if (hand->callbacks) {
                        if (hand->callbacks->yajl_number) {
                            _CC_CHK(hand->callbacks->yajl_number(
                                        hand->ctx, (const char *) buf, bufLen));
                        } else if (hand->callbacks->yajl_double) {
                            double d = 0.0;
                            yajl_buf_clear(hand->decodeBuf);
                            yajl_buf_append(hand->decodeBuf, buf, bufLen);
                            buf = yajl_buf_data(hand->decodeBuf);
                            errno = 0;
                            d = strtod((char *) buf, NULL);
                            if ((d == HUGE_VAL || d == -HUGE_VAL) &&
                                errno == ERANGE)
                            {
                                yajl_bs_set(hand->stateStack,
                                            yajl_state_parse_error);
                                hand->parseError = "numeric (floating point) "
                                    "overflow";
                                /* try to restore error offset */
                                if (*offset >= bufLen) *offset -= bufLen;
                                else *offset = 0;
                                goto around_again;
                            }
                            _CC_CHK(hand->callbacks->yajl_double(hand->ctx,
                                                                 d));
                        }
                    }
                    break;
                case yajl_tok_right_brace: {
                    if (yajl_bs_current(hand->stateStack) ==
                        yajl_state_array_start)
                    {
                        if (hand->callbacks &&
                            hand->callbacks->yajl_end_array)
                        {
                            _CC_CHK(hand->callbacks->yajl_end_array(hand->ctx));
                        }
                        yajl_bs_pop(hand->stateStack);
                        goto around_again;
                    }
                    /* intentional fall-through */
                }
                case yajl_tok_colon:
                case yajl_tok_comma:
                case yajl_tok_right_bracket:
                    yajl_bs_set(hand->stateStack, yajl_state_parse_error);
                    hand->parseError =
                        "unallowed token at this point in JSON text";
                    goto around_again;
                default:
                    yajl_bs_set(hand->stateStack, yajl_state_parse_error);
                    hand->parseError = "invalid token, internal error";
                    goto around_again;
            }
            /* got a value.  transition depends on the state we're in. */
            {
                yajl_state s = yajl_bs_current(hand->stateStack);
                if (s == yajl_state_start || s == yajl_state_got_value) {
                    yajl_bs_set(hand->stateStack, yajl_state_parse_complete);
                } else if (s == yajl_state_map_need_val) {
                    yajl_bs_set(hand->stateStack, yajl_state_map_got_val);
                } else {
                    yajl_bs_set(hand->stateStack, yajl_state_array_got_val);
                }
            }
            if (stateToPush != yajl_state_start) {
                yajl_bs_push(hand->stateStack, stateToPush);
            }

            goto around_again;
        }
        case yajl_state_map_start:
        case yajl_state_map_need_key: {
            /* only difference between these two states is that in
             * start '}' is valid, whereas in need_key, we've parsed
             * a comma, and a string key _must_ follow */
            tok = yajl_lex_lex(hand->lexer, jsonText, jsonTextLen,
                               offset, &buf, &bufLen);
            switch (tok) {
                case yajl_tok_eof:
                    return yajl_status_ok;
                case yajl_tok_error:
                    yajl_bs_set(hand->stateStack, yajl_state_lexical_error);
                    goto around_again;
                case yajl_tok_string_with_escapes:
                    if (hand->callbacks && hand->callbacks->yajl_map_key) {
                        yajl_buf_clear(hand->decodeBuf);
                        yajl_string_decode(hand->decodeBuf, buf, bufLen);
                        buf = yajl_buf_data(hand->decodeBuf);
                        bufLen = yajl_buf_len(hand->decodeBuf);
                    }
                    /* intentional fall-through */
                case yajl_tok_string:
                    if (hand->callbacks && hand->callbacks->yajl_map_key) {
                        _CC_CHK(hand->callbacks->yajl_map_key(hand->ctx, buf,
                                                              bufLen));
                    }
                    yajl_bs_set(hand->stateStack, yajl_state_map_sep);
                    goto around_again;
                case yajl_tok_right_bracket:
                    if (yajl_bs_current(hand->stateStack) ==
                        yajl_state_map_start)
                    {
                        if (hand->callbacks && hand->callbacks->yajl_end_map) {
                            _CC_CHK(hand->callbacks->yajl_end_map(hand->ctx));
                        }
                        yajl_bs_pop(hand->stateStack);
                        goto around_again;
                    }
                default:
                    yajl_bs_set(hand->stateStack, yajl_state_parse_error);
                    hand->parseError =
                        "invalid object key (must be a string)"; 
                    goto around_again;
            }
        }
        case yajl_state_map_sep: {
            tok = yajl_lex_lex(hand->lexer, jsonText, jsonTextLen,
                               offset, &buf, &bufLen);
            switch (tok) {
                case yajl_tok_colon:
                    yajl_bs_set(hand->stateStack, yajl_state_map_need_val);
                    goto around_again;
                case yajl_tok_eof:
                    return yajl_status_ok;
                case yajl_tok_error:
                    yajl_bs_set(hand->stateStack, yajl_state_lexical_error);
                    goto around_again;
                default:
                    yajl_bs_set(hand->stateStack, yajl_state_parse_error);
                    hand->parseError = "object key and value must "
                        "be separated by a colon (':')";
                    goto around_again;
            }
        }
        case yajl_state_map_got_val: {
            tok = yajl_lex_lex(hand->lexer, jsonText, jsonTextLen,
                               offset, &buf, &bufLen);
            switch (tok) {
                case yajl_tok_right_bracket:
                    if (hand->callbacks && hand->callbacks->yajl_end_map) {
                        _CC_CHK(hand->callbacks->yajl_end_map(hand->ctx));
                    }
                    yajl_bs_pop(hand->stateStack);
                    goto around_again;
                case yajl_tok_comma:
                    yajl_bs_set(hand->stateStack, yajl_state_map_need_key);
                    goto around_again;
                case yajl_tok_eof:
                    return yajl_status_ok;
                case yajl_tok_error:
                    yajl_bs_set(hand->stateStack, yajl_state_lexical_error);
                    goto around_again;
                default:
                    yajl_bs_set(hand->stateStack, yajl_state_parse_error);
                    hand->parseError = "after key and value, inside map, "
                                       "I expect ',' or '}'";
                    /* try to restore error offset */
                    if (*offset >= bufLen) *offset -= bufLen;
                    else *offset = 0;
                    goto around_again;
            }
        }
        case yajl_state_array_got_val: {
            tok = yajl_lex_lex(hand->lexer, jsonText, jsonTextLen,
                               offset, &buf, &bufLen);
            switch (tok) {
                case yajl_tok_right_brace:
                    if (hand->callbacks && hand->callbacks->yajl_end_array) {
                        _CC_CHK(hand->callbacks->yajl_end_array(hand->ctx));
                    }
                    yajl_bs_pop(hand->stateStack);
                    goto around_again;
                case yajl_tok_comma:
                    yajl_bs_set(hand->stateStack, yajl_state_array_need_val);
                    goto around_again;
                case yajl_tok_eof:
                    return yajl_status_ok;
                case yajl_tok_error:
                    yajl_bs_set(hand->stateStack, yajl_state_lexical_error);
                    goto around_again;
                default:
                    yajl_bs_set(hand->stateStack, yajl_state_parse_error);
                    hand->parseError =
                        "after array element, I expect ',' or ']'";
                    goto around_again;
            }
        }
    }

    abort();
    return yajl_status_error;
}

/*==============================================================*/
/* --- yajl_tree.c */

#if defined(_WIN32) || defined(WIN32)
#define snprintf sprintf_s
#endif

#define STATUS_CONTINUE 1
#define STATUS_ABORT    0

struct stack_elem_s;
typedef struct stack_elem_s stack_elem_t;
struct stack_elem_s
{
    char * key;
    yajl_val value;
    stack_elem_t *next;
};

struct context_s
{
    stack_elem_t *stack;
    yajl_val root;
    char *errbuf;
    size_t errbuf_size;
};
typedef struct context_s context_t;

#define RETURN_ERROR(ctx,retval,...) {                                  \
        if ((ctx)->errbuf != NULL)                                      \
            snprintf ((ctx)->errbuf, (ctx)->errbuf_size, __VA_ARGS__);  \
        return (retval);                                                \
    }

static yajl_val value_alloc (yajl_type type)
{
    yajl_val v;

    v = malloc (sizeof (*v));
    if (v == NULL) return (NULL);
    memset (v, 0, sizeof (*v));
    v->type = type;

    return (v);
}

static void yajl_object_free (yajl_val v)
{
    size_t i;

    if (!YAJL_IS_OBJECT(v)) return;

    for (i = 0; i < v->u.object.len; i++)
    {
        free((char *) v->u.object.keys[i]);
        v->u.object.keys[i] = NULL;
        yajl_tree_free (v->u.object.values[i]);
        v->u.object.values[i] = NULL;
    }

    free((void*) v->u.object.keys);
    free(v->u.object.values);
    free(v);
}

static void yajl_array_free (yajl_val v)
{
    size_t i;

    if (!YAJL_IS_ARRAY(v)) return;

    for (i = 0; i < v->u.array.len; i++)
    {
        yajl_tree_free (v->u.array.values[i]);
        v->u.array.values[i] = NULL;
    }

    free(v->u.array.values);
    free(v);
}

/*
 * Parsing nested objects and arrays is implemented using a stack. When a new
 * object or array starts (a curly or a square opening bracket is read), an
 * appropriate value is pushed on the stack. When the end of the object is
 * reached (an appropriate closing bracket has been read), the value is popped
 * off the stack and added to the enclosing object using "context_add_value".
 */
static int context_push(context_t *ctx, yajl_val v)
{
    stack_elem_t *stack;

    stack = malloc (sizeof (*stack));
    if (stack == NULL)
        RETURN_ERROR (ctx, ENOMEM, "Out of memory");
    memset (stack, 0, sizeof (*stack));

    assert ((ctx->stack == NULL)
            || YAJL_IS_OBJECT (v)
            || YAJL_IS_ARRAY (v));

    stack->value = v;
    stack->next = ctx->stack;
    ctx->stack = stack;

    return (0);
}

static yajl_val context_pop(context_t *ctx)
{
    stack_elem_t *stack;
    yajl_val v;

    if (ctx->stack == NULL)
        RETURN_ERROR (ctx, NULL, "context_pop: "
                      "Bottom of stack reached prematurely");

    stack = ctx->stack;
    ctx->stack = stack->next;

    v = stack->value;

    free (stack);

    return (v);
}

static int object_add_keyval(context_t *ctx,
                             yajl_val obj, char *key, yajl_val value)
{
    const char **tmpk;
    yajl_val *tmpv;

    /* We're checking for NULL in "context_add_value" or its callers. */
    assert (ctx != NULL);
    assert (obj != NULL);
    assert (key != NULL);
    assert (value != NULL);

    /* We're assuring that "obj" is an object in "context_add_value". */
    assert(YAJL_IS_OBJECT(obj));

    tmpk = realloc((void *) obj->u.object.keys, sizeof(*(obj->u.object.keys)) * (obj->u.object.len + 1));
    if (tmpk == NULL)
        RETURN_ERROR(ctx, ENOMEM, "Out of memory");
    obj->u.object.keys = tmpk;

    tmpv = realloc(obj->u.object.values, sizeof (*obj->u.object.values) * (obj->u.object.len + 1));
    if (tmpv == NULL)
        RETURN_ERROR(ctx, ENOMEM, "Out of memory");
    obj->u.object.values = tmpv;

    obj->u.object.keys[obj->u.object.len] = key;
    obj->u.object.values[obj->u.object.len] = value;
    obj->u.object.len++;

    return (0);
}

static int array_add_value (context_t *ctx,
                            yajl_val array, yajl_val value)
{
    yajl_val *tmp;

    /* We're checking for NULL pointers in "context_add_value" or its
     * callers. */
    assert (ctx != NULL);
    assert (array != NULL);
    assert (value != NULL);

    /* "context_add_value" will only call us with array values. */
    assert(YAJL_IS_ARRAY(array));

    tmp = realloc(array->u.array.values,
                  sizeof(*(array->u.array.values)) * (array->u.array.len + 1));
    if (tmp == NULL)
        RETURN_ERROR(ctx, ENOMEM, "Out of memory");
    array->u.array.values = tmp;
    array->u.array.values[array->u.array.len] = value;
    array->u.array.len++;

    return 0;
}

/*
 * Add a value to the value on top of the stack or the "root" member in the
 * context if the end of the parsing process is reached.
 */
static int context_add_value (context_t *ctx, yajl_val v)
{
    /* We're checking for NULL values in all the calling functions. */
    assert (ctx != NULL);
    assert (v != NULL);

    /*
     * There are three valid states in which this function may be called:
     *   - There is no value on the stack => This is the only value. This is the
     *     last step done when parsing a document. We assign the value to the
     *     "root" member and return.
     *   - The value on the stack is an object. In this case store the key on the
     *     stack or, if the key has already been read, add key and value to the
     *     object.
     *   - The value on the stack is an array. In this case simply add the value
     *     and return.
     */
    if (ctx->stack == NULL)
    {
        assert (ctx->root == NULL);
        ctx->root = v;
        return (0);
    }
    else if (YAJL_IS_OBJECT (ctx->stack->value))
    {
        if (ctx->stack->key == NULL)
        {
            if (!YAJL_IS_STRING (v))
                RETURN_ERROR (ctx, EINVAL, "context_add_value: "
                              "Object key is not a string (%#04x)",
                              v->type);

            ctx->stack->key = v->u.string;
            v->u.string = NULL;
            free(v);
            return (0);
        }
        else /* if (ctx->key != NULL) */
        {
            char * key;

            key = ctx->stack->key;
            ctx->stack->key = NULL;
            return (object_add_keyval (ctx, ctx->stack->value, key, v));
        }
    }
    else if (YAJL_IS_ARRAY (ctx->stack->value))
    {
        return (array_add_value (ctx, ctx->stack->value, v));
    }
    else
    {
        RETURN_ERROR (ctx, EINVAL, "context_add_value: Cannot add value to "
                      "a value of type %#04x (not a composite type)",
                      ctx->stack->value->type);
    }
}

static int handle_string (void *ctx,
                          const unsigned char *string, size_t string_length)
{
    yajl_val v;

    v = value_alloc (yajl_t_string);
    if (v == NULL)
        RETURN_ERROR ((context_t *) ctx, STATUS_ABORT, "Out of memory");

    v->u.string = malloc (string_length + 1);
    if (v->u.string == NULL)
    {
        free (v);
        RETURN_ERROR ((context_t *) ctx, STATUS_ABORT, "Out of memory");
    }
    memcpy(v->u.string, string, string_length);
    v->u.string[string_length] = 0;

    return ((context_add_value (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_number (void *ctx, const char *string, size_t string_length)
{
    yajl_val v;
    char *endptr;

    v = value_alloc(yajl_t_number);
    if (v == NULL)
        RETURN_ERROR((context_t *) ctx, STATUS_ABORT, "Out of memory");

    v->u.number.r = malloc(string_length + 1);
    if (v->u.number.r == NULL)
    {
        free(v);
        RETURN_ERROR((context_t *) ctx, STATUS_ABORT, "Out of memory");
    }
    memcpy(v->u.number.r, string, string_length);
    v->u.number.r[string_length] = 0;

    v->u.number.flags = 0;

    errno = 0;
    v->u.number.i = yajl_parse_integer((const unsigned char *) v->u.number.r,
                                       strlen(v->u.number.r));
    if (errno == 0)
        v->u.number.flags |= YAJL_NUMBER_INT_VALID;

    endptr = NULL;
    errno = 0;
    v->u.number.d = strtod(v->u.number.r, &endptr);
    if ((errno == 0) && (endptr != NULL) && (*endptr == 0))
        v->u.number.flags |= YAJL_NUMBER_DOUBLE_VALID;

    return ((context_add_value(ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_start_map (void *ctx)
{
    yajl_val v;

    v = value_alloc(yajl_t_object);
    if (v == NULL)
        RETURN_ERROR ((context_t *) ctx, STATUS_ABORT, "Out of memory");

    v->u.object.keys = NULL;
    v->u.object.values = NULL;
    v->u.object.len = 0;

    return ((context_push (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_end_map (void *ctx)
{
    yajl_val v;

    v = context_pop (ctx);
    if (v == NULL)
        return (STATUS_ABORT);

    return ((context_add_value (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_start_array (void *ctx)
{
    yajl_val v;

    v = value_alloc(yajl_t_array);
    if (v == NULL)
        RETURN_ERROR ((context_t *) ctx, STATUS_ABORT, "Out of memory");

    v->u.array.values = NULL;
    v->u.array.len = 0;

    return ((context_push (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_end_array (void *ctx)
{
    yajl_val v;

    v = context_pop (ctx);
    if (v == NULL)
        return (STATUS_ABORT);

    return ((context_add_value (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_boolean (void *ctx, int boolean_value)
{
    yajl_val v;

    v = value_alloc (boolean_value ? yajl_t_true : yajl_t_false);
    if (v == NULL)
        RETURN_ERROR ((context_t *) ctx, STATUS_ABORT, "Out of memory");

    return ((context_add_value (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

static int handle_null (void *ctx)
{
    yajl_val v;

    v = value_alloc (yajl_t_null);
    if (v == NULL)
        RETURN_ERROR ((context_t *) ctx, STATUS_ABORT, "Out of memory");

    return ((context_add_value (ctx, v) == 0) ? STATUS_CONTINUE : STATUS_ABORT);
}

/*
 * Public functions
 */
yajl_val yajl_tree_parse (const char *input,
                          char *error_buffer, size_t error_buffer_size)
{
    static const yajl_callbacks callbacks =
        {
            /* null        = */ handle_null,
            /* boolean     = */ handle_boolean,
            /* integer     = */ NULL,
            /* double      = */ NULL,
            /* number      = */ handle_number,
            /* string      = */ handle_string,
            /* start map   = */ handle_start_map,
            /* map key     = */ handle_string,
            /* end map     = */ handle_end_map,
            /* start array = */ handle_start_array,
            /* end array   = */ handle_end_array
        };

    yajl_handle handle;
    yajl_status status;
    char * internal_err_str;
	context_t ctx = { NULL, NULL, NULL, 0 };

	ctx.errbuf = error_buffer;
	ctx.errbuf_size = error_buffer_size;

    if (error_buffer != NULL)
        memset (error_buffer, 0, error_buffer_size);

    handle = yajl_alloc (&callbacks, NULL, &ctx);
    yajl_config(handle, yajl_allow_comments, 1);

    status = yajl_parse(handle,
                        (unsigned char *) input,
                        strlen (input));
    status = yajl_complete_parse (handle);
    if (status != yajl_status_ok) {
        if (error_buffer != NULL && error_buffer_size > 0) {
               internal_err_str = (char *) yajl_get_error(handle, 1,
                     (const unsigned char *) input,
                     strlen(input));
             snprintf(error_buffer, error_buffer_size, "%s", internal_err_str);
             YA_FREE(&(handle->alloc), internal_err_str);
        }
        yajl_free (handle);
        return NULL;
    }

    yajl_free (handle);
    return (ctx.root);
}

yajl_val yajl_tree_get(yajl_val n, const char ** path, yajl_type type)
{
    if (!path) return NULL;
    while (n && *path) {
        size_t i;
        size_t len;

        if (n->type != yajl_t_object) return NULL;
        len = n->u.object.len;
        for (i = 0; i < len; i++) {
            if (!strcmp(*path, n->u.object.keys[i])) {
                n = n->u.object.values[i];
                break;
            }
        }
        if (i == len) return NULL;
        path++;
    }
    if (n && type != yajl_t_any && type != n->type) n = NULL;
    return n;
}

void yajl_tree_free (yajl_val v)
{
    if (v == NULL) return;

    if (YAJL_IS_STRING(v))
    {
        free(v->u.string);
        free(v);
    }
    else if (YAJL_IS_NUMBER(v))
    {
        free(v->u.number.r);
        free(v);
    }
    else if (YAJL_GET_OBJECT(v))
    {
        yajl_object_free(v);
    }
    else if (YAJL_GET_ARRAY(v))
    {
        yajl_array_free(v);
    }
    else /* if (yajl_t_true or yajl_t_false or yajl_t_null) */
    {
        free(v);
    }
}

/*==============================================================*/
/* --- yajl_version.c */

BSON_GNUC_CONST
int yajl_version(void)
{
	return YAJL_VERSION;
}

