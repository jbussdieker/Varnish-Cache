//
// http://tools.ietf.org/html/draft-fielding-http-key-01
//

#include "config.h"

#include "cache.h"

#include "vct.h"
#include "vend.h"

#define M_WORD		    1
#define M_SUBSTRING	    2
#define M_BEGINNING	    3
#define M_CASE		    4
#define M_NOT		    5
#define M_PARAMETER	    6
#define M_PARAMETER_RANGE   7

int enum_matchers(const char *matcher, int *type, const char **param, int *s) {
	const char *p = matcher;
	const char *e;

	if (*p != ';')
		return 0;

	p++;

	if (strncmp(p, "w=\"", 3) == 0) {
		p += 3;
		for (e = p; *e && (*e != '\"' || e[-1] == '\\'); e++)
			continue;
		if (!*e)
			return -1;
		*param = p;
		*s = (int)(e-p);
		*type = M_WORD;
		p = e + 1;
	} else if (strncmp(p, "s=\"", 3) == 0) {
		p += 3;
		for (e = p; *e && (*e != '\"' || e[-1] == '\\'); e++)
			continue;
		if (!*e)
			return -1;
		*param = p;
		*s = (int)(e-p);
		*type = M_SUBSTRING;
		p = e + 1;
	} else if (strncmp(p, "b=\"", 3) == 0) {
		p += 3;
		for (e = p; *e && (*e != '\"' || e[-1] == '\\'); e++)
			continue;
		if (!*e)
			return -1;
		*param = p;
		*s = (int)(e-p);
		*type = M_BEGINNING;
		p = e + 1;
	} else if (strncmp(p, "p=\"", 3) == 0) {
		p += 3;
		for (e = p; *e && (*e != '\"' || e[-1] == '\\'); e++)
			continue;
		if (!*e)
			return -1;
		*param = p;
		*s = (int)(e-p);
		*type = M_PARAMETER;
		p = e + 1;
	} else if (strncmp(p, "pr=", 3) == 0) {
		p += 3;
		for (e = p; *e && *e != '[' && *e != ';'; e++)
			continue;
		if (*e != '[')
			return -1;
		for (e = p; *e && *e != ']' && *e != ';'; e++)
			continue;
		if (*e != ']')
			return -1;
		*param = p;
		*s = (int)(e-p) + 1;
		*type = M_PARAMETER_RANGE;
		p = e + 1;
	} else if (*p == 'c') {
		*s = 0;
		*type = M_CASE;
		p++;
	} else if (*p == 'n') {
		*s = 0;
		*type = M_NOT;
		p++;
	} else {
		// Invalid matcher
		return -1;
	}
	return p - matcher;
}

int enum_fields(const char *fv, const char **m, int *s) {
	const char *f, *b;
	enum state {
		start,
		skip_space,
		scan,
		scan_quote,
		scan_back,
		done,
		end
	} cur_state = start;

	do {
		switch (cur_state) {
			case start:
				f = fv;
				cur_state = skip_space;
				break;
			case skip_space:
				if (*f == '\0')
					cur_state = end;
				else if (*f == ' ' || *f == ',')
					f++;
				else if (*f == '"') {
					b = f;
					cur_state = scan_quote;
				} else {
					b = f;
					cur_state = scan;
				}
				break;
			case scan:
				b++;
				if (*b == '\0')
					cur_state = scan_back;
				else if (*b == '"')
					cur_state = scan_quote;
				else if (*b == ',')
					cur_state = scan_back;
				break;
			case scan_quote:
				b++;
				if (*b == '\0')
					cur_state = scan_back;
				else if (*b == '\\' && b[1] != '\0')
					b++;
				else if (*b == '"')
					cur_state = scan;
				break;
			case scan_back:
				b--;
				if (*b != ' ')
					cur_state = done;
				break;
			case done:
				*m = f;
				*s = (int)(b-f)+1;
				return b - fv + 1;
				break;
		}
	} while (cur_state != end);

	return 0;
}

int cmp_func(const char *str1, const char *str2, int size, int case_sensitive) {
	if (case_sensitive == 1)
		return strncmp(str1, str2, size);
	else
		return strncasecmp(str1, str2, size);
}

int word_matcher(const char *p, const char *fv, int ps, int case_sensitive) {
	int read, size, offset = 0;
	const char *match;

	while (read = enum_fields(fv + offset, &match, &size)) {
		if (ps == size) {
			if (cmp_func(match, p, size, case_sensitive) == 0) {
				return 1;
			}
		}
		offset += read;
	}
	return 0;
}

int substring_matcher(const char *p, const char *fv, int ps, int case_sensitive) {
	int read, size, offset = 0;
	const char *match;

	while (read = enum_fields(fv + offset, &match, &size)) {
		if (strlen(p) <= size) {
			const char *s;
			for (s = match; s <= match + size - ps; s++) {
				if (cmp_func(s, p, ps, case_sensitive) == 0) {
					return 1;
				}
			}
		}
		offset += read;
	}
	return 0;
}

int beginning_substring_matcher(const char *p, const char *fv, int ps, int case_sensitive) {
	int read, size, offset = 0;
	const char *match;

	while (read = enum_fields(fv + offset, &match, &size)) {
		if (ps <= size) {
			if (cmp_func(match, p, ps, case_sensitive) == 0) {
				return 1;
			}
		}
		offset += read;
	}
	return 0;
}

int parameter_prefix_matcher(const char *p, const char *fv, int ps, int case_sensitive) {
	int read, size, offset = 0;
	const char *match;

	while (read = enum_fields(fv + offset, &match, &size)) {
		if (ps <= size) {
			if (cmp_func(match, p, ps, case_sensitive) == 0) {
				const char *r;
				for (r = match + ps; r < match + size && *r == ' '; r++)
					continue;
				if (*r == ';' || r == match + size)
					return 1;
			}
		}
		offset += read;
	}
	return 0;
}

int parameter_range_matcher(const char *p, const char *fv, int ps, int case_sensitive) {
	case_sensitive = 0; // Always case insensitive
	int read, size, offset = 0;
	const char *match;

	int min = 99, max = 0;
	int has_min = 1, has_max = 1;
	char *x = p;
	for (x = p; *x && *x != '['; x++)
		continue;
	char *prefix = x - 1;
	ps = (int)(prefix - p) + 1;

	if (strncmp(x, "[:", 2)) {
		if (!sscanf(x, "[%d:", &min)) {
			return 0;
		} else
			has_min = 0;
	}
	for (; *x && *x != ':'; x++)
		continue;
	if (strncmp(x, ":]", 2)) {
		if (!sscanf(x, ":%d]", &max)) {
			return 0;
		} else
			has_max = 0;
	}

	while (read = enum_fields(fv + offset, &match, &size)) {
		if (ps <= size) {
			if (cmp_func(match, p, ps, case_sensitive) == 0) {
				if (match[ps] == '=') {
					int value;
					if (sscanf(match + ps + 1, "%d", &value)) {
					    if ((has_min || (value >= min)) && (has_max || (value <= max)))
						    return 1;
					}
				}
			}
		}
		offset += read;
	}
	return 0;
}

int unescape(char *dst, const char *src, int len) {
	int i, j;
	i = j = 0;
	while (i < len) {
		switch (src[i]) {
			case '\\':
				switch(src[++i]) {
					case '\"':
						dst[j++] = '\"';
						i++;
						break;
					default:
						dst[j++] = '\\';
						dst[j] = src[i];
						break;
				}
				break;
			default:
				dst[j] = src[i];
				i++;
				j++;
				break;
		}
	}
	return j;
}

/*
 * Find length of a key entry
 */
static unsigned
key_len(const uint8_t *p)
{
	unsigned l = vbe16dec(p);

	return (3 + p[3] + 2 + (l == 0xffff ? 0 : l));
}

int
KEY_Create(const struct sess *sp, const struct http *hp, struct vsb **psb)
{
	char *v, *h, *e, *p, *q;
	struct vsb *sb, *sbh, *sbm;
	unsigned l;
	int matcher = 0;
	int error = 0;

	if (!http_GetHdr(hp, "\4Key:", &v))
		return (0);

	sb = VSB_new_auto();
	AN(sb);

	/* For header matching strings */
	sbh = VSB_new_auto();
	AN(sbh);

	/* For matcher strings */
	sbm = VSB_new_auto();
	AN(sbm);

	for (p = v; *p; p++) {
		/* Find next header-name */
		if (vct_issp(*p))
			continue;
		for (q = p; *q && !vct_issp(*q) && *q != ',' && *q != ';'; q++)
			continue;

		if (q - p > INT8_MAX) {
			WSP(sp, SLT_Error, "Key header name length exceeded");
			error = 1;
			break;
		}

		/* Build header matching string */
		VSB_clear(sbh);
		VSB_printf(sbh, "%c%.*s:%c",
		    (char)(1 + (q - p)), (int)(q - p), p, 0);
		AZ(VSB_finish(sbh));

		if (*q == ';') {
			VSB_clear(sbm);
			int read;
			int type;
			const char *match;
			int size;
			while (read = enum_matchers(q, &type, &match, &size)) {
				if (read < 0) {
					WSP(sp, SLT_Error, "Malformed Key header modifier");
					error = 1;
					break;
				} else {
					// Crazy complicated unescape
					struct vsb *sbtmp = VSB_new_auto();
					AN(sbtmp);
					VSB_printf(sbtmp, "%.*s", size, match);
					AZ(VSB_finish(sbtmp));
					int ms = unescape(VSB_data(sbtmp), VSB_data(sbtmp), size);
					VSB_printf(sbm, "%c%.*s%c", type, ms, VSB_data(sbtmp), 0);
					VSB_delete(sbtmp);

					q += read;
				}
			}

			if (error == 1)
				break;

			AZ(VSB_finish(sbm));
			l = VSB_len(sbm);
			e = h;
			matcher = 1;
		} else {
			matcher = 0;
			if (http_GetHdr(hp, VSB_data(sbh), &h)) {
				AZ(vct_issp(*h));
				/* Trim trailing space */
				e = strchr(h, '\0');
				while (e > h && vct_issp(e[-1]))
					e--;
				/* Encode two byte length and contents */
				l = e - h;
				if (l > 0xffff - 1) {
					WSP(sp, SLT_Error, "Key header maximum length exceeded");
					error = 1;
					break;
				}
			} else {
				e = h;
				l = 0xffff;
			}
		}

		VSB_printf(sb, "%c%c", (int)(l >> 8), (int)(l & 0xff));
		VSB_printf(sb, "%c", matcher);
		VSB_bcat(sb, VSB_data(sbh), VSB_len(sbh));
		if (e != h)
			VSB_bcat(sb, h, e - h);
		if (matcher == 1)
			VSB_bcat(sb, VSB_data(sbm), VSB_len(sbm));

		if (*q == '\0')
			break;
		if (*q != ',') {
			WSP(sp, SLT_Error, "Malformed Key header");
			error = 1;
			break;
		}

		p = q;
	}

	if (error) {
		VSB_delete(sbh);
		VSB_delete(sbm);
		VSB_delete(sb);
		return (-1);
	}

	/* Terminate key matching string */
	VSB_printf(sb, "%c%c%c%c", 0xff, 0xff, 0, 0);

	VSB_delete(sbh);
	VSB_delete(sbm);
	AZ(VSB_finish(sb));

	if (KEY_Match(sp->http, VSB_data(sb)) == 0) {
		WSP(sp, SLT_Error, "Cache key doesn't match request");
		VSB_delete(sb);
		return -1;
	}

        *psb = sb;
	return (VSB_len(sb));
}

void
KEY_Validate(const uint8_t *key)
{
	while (key[3] != 0) {
		assert(strlen((const char*)key+4) == key[3]);
		key += key_len(key);
	}
}

int
KEY_Match(struct http *http, const uint8_t *key)
{
	char *h;
	int i;
	int result = 1;

	while (key[3]) {
		// Exception for gzip
		if (params->http_gzip_support &&
		    !strcasecmp(H_Accept_Encoding, (const char*) key + 3)) {
		// Exact match
		} else if (key[2] == 0) {
			char *e;
			unsigned l = vbe16dec(key);

			i = http_GetHdr(http, (const char*)(key+3), &h);

			if (l == 0xFFFF) {
				if (i != 0)
					result = 0;
			} else {
				const char *value = key + 4 + key[3] + 1;

				if (i == 0) {
					result = 0;
				} else {
					AZ(vct_issp(*h));
					/* Trim trailing space */
					e = strchr(h, '\0');
					while (e > h && vct_issp(e[-1]))
						e--;

					if (l != (int)(e - h))
						result = 0;
					else
						if (memcmp(h, value, l) != 0)
							result = 0;
				}
			}

		// Matcher match
		} else if (key[2] == 1) {
			char *e;
			unsigned l = vbe16dec(key);
			int not_flag = 0;
			int case_flag = 0;
			const char *matcher = key + 4 + key[3] + 1;

			while (*matcher != 0 && *matcher != -1) {
				char *m = matcher + 1;
				unsigned u = 0;
				int subresult = 0;
				switch (matcher[0]) {
					case M_WORD:
						while (u = http_EnumHdr(http, u, (const char*)(key+3), &h)) {
							subresult |= word_matcher(m, h, strlen(m), case_flag);
						}
						if (!(subresult ^ not_flag))
							result = 0;
						break;
					case M_SUBSTRING:
						while (u = http_EnumHdr(http, u, (const char*)(key+3), &h)) {
							subresult |= substring_matcher(m, h, strlen(m), case_flag);
						}
						if (!(subresult ^ not_flag))
							result = 0;
						break;
					case M_BEGINNING:
						while (u = http_EnumHdr(http, u, (const char*)(key+3), &h)) {
							subresult |= beginning_substring_matcher(m, h, strlen(m), case_flag);
						}
						if (!(subresult ^ not_flag))
							result = 0;
						break;
					case M_PARAMETER:
						while (u = http_EnumHdr(http, u, (const char*)(key+3), &h)) {
							subresult |= parameter_prefix_matcher(m, h, strlen(m), case_flag);
						}
						if (!(subresult ^ not_flag))
							result = 0;
						break;
					case M_PARAMETER_RANGE:
						while (u = http_EnumHdr(http, u, (const char*)(key+3), &h)) {
							subresult |= parameter_range_matcher(m, h, strlen(m), case_flag);
						}
						if (!(subresult ^ not_flag))
							result = 0;
						break;
					case M_CASE:
						case_flag = 1;
						break;
					case M_NOT:
						not_flag = 1;
						break;
					default:
						result = 0;
						break;
				}
				matcher += strlen(m) + 2;
			}
		}

		key += key_len(key);
	}

	return result;
}
