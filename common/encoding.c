/*-
 * Copyright (c) 2011, 2012
 *	Zhihao Yuan.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#ifndef lint
static const char sccsid[] = "$Id: encoding.c,v 1.3 2011/12/03 02:22:20 zy Exp $";
#endif /* not lint */

#include <sys/types.h>

int looks_utf8 __P((const char *, size_t));
int looks_utf16 __P((const char *, size_t));

#define F 0   /* character never appears in text */
#define T 1   /* character appears in plain ASCII text */
#define I 2   /* character appears in ISO-8859 text */
#define X 3   /* character appears in non-ISO extended ASCII (Mac, IBM PC) */

static char text_chars[256] = {
	/*                  BEL BS HT LF    FF CR    */
	F, F, F, F, F, F, F, T, T, T, T, F, T, T, F, F,  /* 0x0X */
	/*                              ESC          */
	F, F, F, F, F, F, F, F, F, F, F, T, F, F, F, F,  /* 0x1X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x2X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x3X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x4X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x5X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T,  /* 0x6X */
	T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, F,  /* 0x7X */
	/*            NEL                            */
	X, X, X, X, X, T, X, X, X, X, X, X, X, X, X, X,  /* 0x8X */
	X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,  /* 0x9X */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xaX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xbX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xcX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xdX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,  /* 0xeX */
	I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I   /* 0xfX */
};

/*
 * looks_utf8 --
 *  Decide whether some text looks like UTF-8. Returns:
 *
 *     -1: invalid UTF-8
 *      0: uses odd control characters, so doesn't look like text
 *      1: 7-bit text
 *      2: definitely UTF-8 text (valid high-bit set bytes)
 *
 *  Based on RFC 3629. UTF-8 with BOM is not accepted.
 *
 * PUBLIC: int looks_utf8 __P((const char *, size_t));
 */
int
looks_utf8(const char *ibuf, size_t nbytes)
{
	const unsigned char *buf = ibuf;
	size_t i;
	int n;
	int gotone = 0, ctrl = 0;

	for (i = 0; i < nbytes; i++) {
		if ((buf[i] & 0x80) == 0) {	   /* 0xxxxxxx is plain ASCII */
			/*
			 * Even if the whole file is valid UTF-8 sequences,
			 * still reject it if it uses weird control characters.
			 */

			if (text_chars[buf[i]] != T)
				ctrl = 1;
		} else if ((buf[i] & 0x40) == 0) { /* 10xxxxxx never 1st byte */
			return -1;
		} else {			   /* 11xxxxxx begins UTF-8 */
			int following;

			if ((buf[i] & 0x20) == 0)	/* 110xxxxx */
				if (buf[i] > 0xC1)	/* C0, C1 */
					following = 1;
				else return -1;
			else if ((buf[i] & 0x10) == 0)	/* 1110xxxx */
				following = 2;
			else if ((buf[i] & 0x08) == 0)	/* 11110xxx */
				if (buf[i] < 0xF5)
					following = 3;
				else return -1;		/* F5, F6, F7 */
			else
				return -1;		/* F8~FF */

			for (n = 0; n < following; n++) {
				i++;
				if (i >= nbytes)
					goto done;

				if (buf[i] & 0x40)	/* 10xxxxxx */
					return -1;
			}

			gotone = 1;
		}
	}
done:
	return ctrl ? 0 : (gotone ? 2 : 1);
}

/*
 * looks_utf16 --
 *  Decide whether some text looks like UTF-16. Returns:
 *
 *      0: invalid UTF-16
 *      1: Little-endian UTF-16
 *      2: Big-endian UTF-16
 *
 * PUBLIC: int looks_utf16 __P((const char *, size_t));
 */
int
looks_utf16(const char *ibuf, size_t nbytes)
{
	const unsigned char *buf = ibuf;
	int bigend;
	size_t i;
	unsigned int c;
	int bom;
	int following = 0;

	if (nbytes < 2)
		return 0;

	bom = buf[0] << 8 ^ buf[1];
	if (bom == 0xFFFE)
		bigend = 0;
	else if (bom == 0xFEFF)
		bigend = 1;
	else
		return 0;

	for (i = 2; i + 1 < nbytes; i += 2) {
		if (bigend)
			c = buf[i] << 8 ^ buf[i + 1];
		else
			c = buf[i] ^ buf[i + 1] << 8;

		if (!following)
			if (c < 0xD800 || c > 0xDFFF)
				if (c < 128 && text_chars[c] != T)
					return 0;
				else
					following = 0;
			else if (c > 0xDBFF)
				return 0;
			else {
				following = 1;
				continue;
			}
		else if (c < 0xDC00 || c > 0xDFFF)
			return 0;
	}

	return 1 + bigend;
}

#undef F
#undef T
#undef I
#undef X
