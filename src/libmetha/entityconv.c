/*-
 * entityconv.c
 * This file is part of libmetha
 *
 * Copyright (c) 2009, Emil Romanus <emil.romanus@gmail.com>
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
 * 
 * http://bithack.se/projects/methabot/
 */

#include <string.h>
#include <iconv.h>
#include <ctype.h>

#include "errors.h"
#include "worker.h"
#include "urlengine.h"
#include "io.h"

static int entity_hash(const char *s, int size);
static int unicode_to_utf8(uint16_t v, char *out);

typedef
struct html_entity {
    const char *ident;
    uint16_t    unicode;
} entity_t;

entity_t e_tbl[128][8] = {
    /** 
     * this is a static hash table on every entity, to add an entry,
     * run entity_hash() on the string and put it in the corresponding
     * entry in the table;
     **/
    {
        {"sup2", 0x00B2},
        {0}
    },
    {
        {"sup3", 0x00B3},
        {"Pi", 0x03A0},
        {"hArr", 0x21D4},
        {0}
    },
    {
        {0}
    },
    {
        {"Auml", 0x00C4},
        {"szlig", 0x00DF},
        {0}
    },
    {
        {"Tau", 0x03A4},
        {0}
    },
    {
        {"Ouml", 0x00D6},
        {0}
    },
    {
        {"lang", 0x2329},
        {0}
    },
    {
        {"THORN", 0x00DE},
        {"gamma", 0x03B3},
        {0}
    },
    {
        {"uml", 0x00A8},
        {"Acirc", 0x00C2},
        {"Beta", 0x0392},
        {0}
    },
    {
        {"rho", 0x03C1},
        {"phi", 0x03C6},
        {0}
    },
    {
        {"Epsilon", 0x0395},
        {0}
    },
    {
        {"rlm", 0x200F},
        {0}
    },
    {
        {"Ucirc", 0x00DB},
        {0}
    },
    {
        {"lArr", 0x21D0},
        {0}
    },
    {
        {"ntilde", 0x00F1},
        {"rsquo", 0x2019},
        {0}
    },
    {
        {0}
    },
    {
        {"igrave", 0x00EC},
        {"scaron", 0x0161},
        {0}
    },
    {
        {"Otilde", 0x00D5},
        {0}
    },
    {
        {0}
    },
    {
        {"Lambda", 0x039B},
        {0}
    },
    {
        {"shy", 0x00AD},
        {"Chi", 0x03A7},
        {"delta", 0x03B4},
        {0}
    },
    {
        {"piv", 0x03D6},
        {0}
    },
    {
        {"Oslash", 0x00D8},
        {"equiv", 0x2261},
        {0}
    },
    {
        {0}
    },
    {
        {"amp", 0x0026},
        {"raquo", 0x00BB},
        {"sbquo", 0x201A},
        {0}
    },
    {
        {"sigmaf", 0x03C2},
        {0}
    },
    {
        {"upsilon", 0x03C5},
        {0}
    },
    {
        {"iuml", 0x00EF},
        {"bull", 0x2022},
        {0}
    },
    {
        {"Ecirc", 0x00CA},
        {0}
    },
    {
        {"frac14", 0x00BC},
        {"iquest", 0x00BF},
        {"thetasym", 0x03D1},
        {0}
    },
    {
        {0}
    },
    {
        {"frac12", 0x00BD},
        {"radic", 0x221A},
        {0}
    },
    {
        {"OElig", 0x0152},
        {"oelig", 0x0153},
        {0}
    },
    {
        {"harr", 0x2194},
        {"empty", 0x2205},
        {0}
    },
    {
        {"Eta", 0x0397},
        {"ang", 0x2220},
        {"rfloor", 0x230B},
        {0}
    },
    {
        {"iota", 0x03B9},
        {"and", 0x2227},
        {"sim", 0x223C},
        {0}
    },
    {
        {"tau", 0x03C4},
        {0}
    },
    {
        {"oacute", 0x00F3},
        {"zwj", 0x200D},
        {"rdquo", 0x201D},
        {0}
    },
    {
        {"Ocirc", 0x00D4},
        {"Alpha", 0x0391},
        {"cap", 0x2229},
        {0}
    },
    {
        {"thorn", 0x00FE},
        {"alefsym", 0x2135},
        {0}
    },
    {
        {"acirc", 0x00E2},
        {"notin", 0x2209},
        {0}
    },
    {
        {"hearts", 0x2665},
        {0}
    },
    {
        {"aelig", 0x00E6},
        {"Psi", 0x03A8},
        {"epsilon", 0x03B5},
        {0}
    },
    {
        {"frac34", 0x00BE},
        {"yuml", 0x00FF},
        {"mdash", 0x2014},
        {"ni", 0x220B},
        {0}
    },
    {
        {"reg", 0x00AE},
        {"ucirc", 0x00FB},
        {0}
    },
    {
        {"Omicron", 0x039F},
        {"larr", 0x2190},
        {0}
    },
    {
        {"deg", 0x00B0},
        {"Ntilde", 0x00D1},
        {"nsub", 0x2284},
        {0}
    },
    {
        {"apos", 0x0027},
        {"euml", 0x00EB},
        {"Kappa", 0x039A},
        {"rArr", 0x21D2},
        {"ne", 0x2260},
        {0}
    },
    {
        {"Igrave", 0x00CC},
        {"Icirc", 0x00CE},
        {"Scaron", 0x0160},
        {"zeta", 0x03B6},
        {0}
    },
    {
        {"middot", 0x00B7},
        {"le", 0x2264},
        {0}
    },
    {
        {"times", 0x00D7},
        {0}
    },
    {
        {"aacute", 0x00E1},
        {"prod", 0x220F},
        {0}
    },
    {
        {"cent", 0x00A2},
        {"chi", 0x03C7},
        {"trade", 0x2122},
        {0}
    },
    {
        {"bdquo", 0x201E},
        {0}
    },
    {
        {0}
    },
    {
        {"atilde", 0x00E3},
        {"upsih", 0x03D2},
        {0}
    },
    {
        {"agrave", 0x00E0},
        {"lsquo", 0x2018},
        {0}
    },
    {
        {"xi", 0x03BE},
        {"euro", 0x20AC},
        {0}
    },
    {
        {"ograve", 0x00F2},
        {"real", 0x211C},
        {"sup", 0x2283},
        {0}
    },
    {
        {"Iuml", 0x00CF},
        {"Prime", 0x2033},
        {0}
    },
    {
        {"ecirc", 0x00EA},
        {0}
    },
    {
        {"nbsp", 0x00A0},
        {"oplus", 0x2295},
        {0}
    },
    {
        {"Theta", 0x0398},
        {"diams", 0x2666},
        {0}
    },
    {
        {"eacute", 0x00E9},
        {"uuml", 0x00FC},
        {"nu", 0x03BD},
        {"image", 0x2111},
        {"sum", 0x2211},
        {"prop", 0x221D},
        {0}
    },
    {
        /*{"lt", 0x003C},*/
        {0}
    },
    {
        {"lsaquo", 0x2039},
        {0}
    },
    {
        {"eta", 0x03B7},
        {"sdot", 0x22C5},
        {0}
    },
    {
        {"circ", 0x02C6},
        {"Iota", 0x0399},
        {"or", 0x2228},
        {0}
    },
    {
        {"egrave", 0x00E8},
        {"tilde", 0x02DC},
        {"mu", 0x03BC},
        {0}
    },
    {
        {"Oacute", 0x00D3},
        {"divide", 0x00F7},
        {"Omega", 0x03A9},
        {0}
    },
    {
        {"ocirc", 0x00F4},
        {"alpha", 0x03B1},
        {"lowast", 0x2217},
        {0}
    },
    {
        {"macr", 0x00AF},
        {0}
    },
    {
        {"pound", 0x00A3},
        {"hellip", 0x2026},
        {"weierp", 0x2118},
        {0}
    },
    {
        /*{"gt", 0x003E},*/
        {"eth", 0x00F0},
        {"sube", 0x2286},
        {"lceil", 0x2308},
        {0}
    },
    {
        {"psi", 0x03C8},
        {"infin", 0x221E},
        {"cup", 0x222A},
        {0}
    },
    {
        {"sect", 0x00A7},
        {"iacute", 0x00ED},
        {"Yuml", 0x0178},
        {"supe", 0x2287},
        {0}
    },
    {
        {"acute", 0x00B4},
        {"ordm", 0x00BA},
        {"nabla", 0x2207},
        {"sub", 0x2282},
        {0}
    },
    {
        {"copy", 0x00A9},
        {"Aring", 0x00C5},
        {"omicron", 0x03BF},
        {"clubs", 0x2663},
        {0}
    },
    {
        {"dagger", 0x2020},
        {"frasl", 0x2044},
        {"asymp", 0x2248},
        {0}
    },
    {
        {"curren", 0x00A4},
        {"not", 0x00AC},
        {"Euml", 0x00CB},
        {"uacute", 0x00FA},
        {"kappa", 0x03BA},
        {"lrm", 0x200E},
        {"rarr", 0x2192},
        {"isin", 0x2208},
        {0}
    },
    {
        {"icirc", 0x00EE},
        {"Zeta", 0x0396},
        {"uArr", 0x21D1},
        {0}
    },
    {
        {"ordf", 0x00AA},
        {0}
    },
    {
        {0}
    },
    {
        {"Aacute", 0x00C1},
        {"permil", 0x2030},
        {"exist", 0x2203},
        {0}
    },
    {
        {"ugrave", 0x00F9},
        {0}
    },
    {
        {"brvbar", 0x00A6},
        {"darr", 0x2193},
        {0}
    },
    {
        {"yen", 0x00A5},
        {"ensp", 0x2002},
        {"crarr", 0x21B5},
        {0}
    },
    {
        {"Atilde", 0x00C3},
        {"ldquo", 0x201C},
        {"loz", 0x25CA},
        {0}
    },
    {
        {"Agrave", 0x00C0},
        {"ndash", 0x2013},
        {0}
    },
    {
        {"Xi", 0x039E},
        {0}
    },
    {
        {"Ograve", 0x00D2},
        {"ccedil", 0x00E7},
        {"ge", 0x2265},
        {0}
    },
    {
        {"yacute", 0x00FD},
        {"prime", 0x2032},
        {"int", 0x222B},
        {0}
    },
    {
        {0}
    },
    {
        {"iexcl", 0x00A1},
        {"Sigma", 0x03A3},
        {"emsp", 0x2003},
        {0}
    },
    {
        {"theta", 0x03B8},
        {0}
    },
    {
        {"Eacute", 0x00C9},
        {"Uuml", 0x00DC},
        {"Nu", 0x039D},
        {"part", 0x2202},
        {"perp", 0x22A5},
        {0}
    },
    {
        {"lfloor", 0x230A},
        {0}
    },
    {
        {"pi", 0x03C0},
        {"zwnj", 0x200C},
        {0}
    },
    {
        {0}
    },
    {
        {"auml", 0x00E4},
        {0}
    },
    {
        {"Egrave", 0x00C8},
        {"Mu", 0x039C},
        {"forall", 0x2200},
        {"there4", 0x2234},
        {0}
    },
    {
        {"cedil", 0x00B8},
        {"ouml", 0x00F6},
        {"omega", 0x03C9},
        {0}
    },
    {
        {"thinsp", 0x2009},
        {0}
    },
    {
        {"Gamma", 0x0393},
        {0}
    },
    {
        {"beta", 0x03B2},
        {"rang", 0x232A},
        {0}
    },
    {
        {"quot", 0x0022},
        {"ETH", 0x00D0},
        {"Rho", 0x03A1},
        {"Phi", 0x03A6},
        {0}
    },
    {
        {"AElig", 0x00C6},
        {0}
    },
    {
        {"Iacute", 0x00CD},
        {"otimes", 0x2297},
        {0}
    },
    {
        {"para", 0x00B6},
        {"minus", 0x2212},
        {0}
    },
    {
        {"aring", 0x00E5},
        {"cong", 0x2245},
        {0}
    },
    {
        {"Dagger", 0x2021},
        {0}
    },
    {
        {"Uacute", 0x00DA},
        {"rceil", 0x2309},
        {0}
    },
    {
        {"micro", 0x00B5},
        {"uarr", 0x2191},
        {0}
    },
    {
        {"otilde", 0x00F5},
        {0}
    },
    {
        {"laquo", 0x00AB},
        {0}
    },
    {
        {"plusmn", 0x00B1},
        {"lambda", 0x03BB},
        {"rsaquo", 0x203A},
        {0}
    },
    {
        {"Ugrave", 0x00D9},
        {"Delta", 0x0394},
        {0}
    },
    {
        {"dArr", 0x21D3},
        {0}
    },
    {
        {"oslash", 0x00F8},
        {0}
    },
    {
        {"fnof", 0x0192},
        {0}
    },
    {
        {0}
    },
    {
        {0}
    },
    {
        {"Ccedil", 0x00C7},
        {"Upsilon", 0x03A5},
        {0}
    },
    {
        {"Yacute", 0x00DD},
        {"oline", 0x203E},
        {0}
    },
    {
        {0}
    },
    {
        {"sigma", 0x03C3},
        {0}
    },
    {
        {"spades", 0x2660},
        {0}
    },
    {
        {"sup1", 0x00B9},
        {0}
    }
};

/* create a hash from a given string, very simple
 * hashing algorithm to avoid complexity */
static int
entity_hash(const char *s, int size)
{
    int   h = 0xc8;
    char *e;

    for (e=s+size; s<e; s++)
        h += (h<<1) ^ *s;
    h &= 0x7f;
    return h;
}

/** 
 * convert the given 16-bit unicode value to an UTF-8 
 * char, write the result to the buffer pointed to by
 * 'out' and return how many bytes that were 
 * written.
 **/
static int
unicode_to_utf8(uint16_t v, char *out)
{
    if (v < 0x0080) {
        *out = v;
        return 1;
    } else if (v < 0x0800) {
        *out     = 0xc0 | (v >> 6);
        *(out+1) = 0x80 | (v & 0x3f);
        return 2;
    }

    *out = 0xe0 | (v >> 12);
    *(out+1) = 0x80 | ((v >> 6) & 0x3f);
    *(out+2) = 0x80 | (v & 0x3f);

    return 3;
}

/** 
 * convert html entities to their corresponding UTF-8 
 * character, utf8conv
 **/
M_CODE
lm_parser_entityconv(worker_t *w, iobuf_t *buf,
                     uehandle_t *ue_h, url_t *url,
                     attr_list_t *al)
{
    char *p;
    char *s;
    char *n;
    char *e;
    char *last;
    int h;
    entity_t *ent;
    e = (last = p = n = buf->ptr) + buf->sz;

    while (n = memchr(n, '&', e-n)) {
        n++;
        s = n;
        if (*s == '#') {
            /* convert numeric entities */
            n++;
        } else {
            while (isalnum(*n))
                n++;
            if (*n != ';')
                continue;
            *n = '\0';
            h = entity_hash(s, n-s);
            for (ent = e_tbl[h]; ; ent++) {
                if (!ent->ident) {
                    *n = ';';
                    break;
                }
                if (strcmp(ent->ident, s) == 0) {
                    memcpy(p, last, s-1-last);
                    p += s-1-last;
                    p += unicode_to_utf8(ent->unicode, p);
                    last = n+1;
                    break;
                }
            }
        }
    }
    memcpy(p, last, e-last);
    buf->sz = (p+(e-last))-buf->ptr;

    return M_OK;
}

