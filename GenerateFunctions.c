
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "GenerateFunctions.h"

SV * GF_escape_html(SV * str, int b_inplace, int b_lftobr, int b_sptonbsp, int b_leaveknown) {
  int i;
  STRLEN origlen, extrachars;
  char * sp, *newsp, c, lastc;
  SV * newstr;

  /* Get string pointer and length (in bytes) */
  if (b_inplace) {
    sp = SvPV_force(str, origlen);
  } else {
    sp = SvPV(str, origlen);
  }

  /* Calculate extra space required */
  extrachars = 0;
  lastc = '\0';
  for (i = 0; i < origlen; i++) {
    /* Need to keep track of previous char for '  ' => ' &nbsp;' expansion */
    lastc = c;
    c = sp[i];
    if (c == '<' || c == '>')
	    extrachars += 3;
    else if (c == '&' && (!b_leaveknown || !GF_is_known_entity(sp, i, origlen)))
	    extrachars += 4;
    else if (c == '"')
	    extrachars += 5;
    else if (b_lftobr && c == '\n')
	    extrachars += 3;
    else if (b_sptonbsp && c == ' ' && lastc == ' ') {
      extrachars += 5;
      /* don't pick up immediately again */
      c = '\0';
    }
  }

  /* Special single space case */
  if (b_sptonbsp && origlen == 1 && sp[0] == ' ') {
    extrachars += 5;
  }

  /* Create new SV, or grow existing SV */
  if (b_inplace) {
    newstr = str;
    SvGROW(newstr, origlen + extrachars + 1);
  } else {
    newstr = newSV(origlen + extrachars + 1);
    SvPOK_on(newstr);
    /* Make new string UTF-8 if input string was UTF-8 */
    if (SvUTF8(str))
      SvUTF8_on(newstr);
  }

  /* Set the length of the string */
  SvCUR_set(newstr, origlen + extrachars);

  /* Now do actual replacement (need to work
     backward for inplace change to work */

  /* Original string might have moved due to grow */
  sp = SvPV_nolen(str);

  /* Null terminate new string */
  newsp = SvPV_nolen(newstr) + origlen + extrachars;
  *newsp = '\0';

  c = '\0';
  for (i = origlen-1; i >= 0; i--) {
    lastc = c;
    c = sp[i];
    if (c == '<') {
      newsp -= 4;
      memcpy(newsp, "&lt;", 4);
    }
    else if (c == '>') {
      newsp -= 4;
      memcpy(newsp, "&gt;", 4);
    }
    else if (c == '&' && (!b_leaveknown || !GF_is_known_entity(sp, i, origlen))) {
      newsp -= 5;
      memcpy(newsp, "&amp;", 5);
    }
    else if (c == '"') {
      newsp -= 6;
      memcpy(newsp, "&quot;", 6);
    }
    else if (b_lftobr && c == '\n') {
      newsp -= 4;
      memcpy(newsp, "<br>", 4);
    }
    else if (b_sptonbsp && c == ' ' && lastc == ' ') {
      newsp -= 6;
      memcpy(newsp, "&nbsp; ", 7);
      /* don't pick up immediately again */
      c = '\0';
    }
    else
      *--newsp = c;
  }

  /* Special single space case */
  if (b_sptonbsp && origlen == 1 && sp[0] == ' ') {
    newsp -= 5;
    memcpy(newsp, "&nbsp;", 6);
  }

  if (SvPV_nolen(newstr) != newsp) {
    croak("Unexpected length mismatch");
    return 0;
  }

  return newstr;
}

SV * GF_generate_attributes(HV * attrhv) {
  int i, j, estimatedlen, vallen;
  I32 keylen;
  char * key, tmp[64];
  SV * attrstr, * val, **av_val;

  /* Iterate through keys to work out an estimated final length */
  estimatedlen = 1;
  while ((val = hv_iternextsv(attrhv, &key, &keylen))) {
    estimatedlen += keylen + 1;
    if (SvOK(val)) {
      if (SvPOK(val)) {
        estimatedlen += SvCUR(val) + 3;
      } else {
        SvPV(val, vallen);
        estimatedlen += vallen + 3;
      }
    }
  }

  attrstr = newSV(estimatedlen);
  SvPOK_on(attrstr);

  hv_iterinit(attrhv);
  while ((val = hv_iternextsv(attrhv, &key, &keylen))) {

    /* Add space to string if already something in it */
    if (SvCUR(attrstr))
      sv_catpvn(attrstr, " ", 1);

    /* For key, convert to lower case and add to attrstr */
    if (keylen < 64) {
      /* If key starts with - (eg -width => '10%'), skip - */
      j = 0;
      i = (keylen && key[0] == '-' ? 1 : 0);
      for (; i < keylen; i++)
        tmp[j++] = toLOWER(key[i]);
      sv_catpvn(attrstr, tmp, j);

    } else {
      sv_catpvn(attrstr, key, keylen);
    }

    /* Add '="value"' part if present*/
    if (SvOK(val)) {
      sv_catpvn(attrstr, "=\"", 2);

      /* If value is reference to array with one item, use that unescaped */
      if (SvOK(val) && SvROK(val) && SvTYPE(SvRV(val)) == SVt_PVAV) {
        if ((av_val = av_fetch((AV *)SvRV(val), 0, 0)) && SvOK(val = *av_val)) {
          sv_catsv(attrstr, val);
        }
      } else {
        /* For the value part, escape special html chars, then dispose of result */
        val = GF_escape_html(val, 0, 0, 0, 0);
        sv_catsv(attrstr, val);
        SvREFCNT_dec(val);
      }

      sv_catpvn(attrstr, "\"", 1);
    }
  }

  return attrstr;
}

SV * GF_generate_tag(SV * tag, HV * attrhv, SV * val, int b_escapeval, int b_addnewline, int b_closetag) {
  int estimatedlen;
  SV * tagstr, * attrstr = 0;

  estimatedlen = SvCUR(tag) + 3 + (b_addnewline ? 1 : 0);

  /* Create attributes as string */
  if (attrhv) {
    attrstr = GF_generate_attributes(attrhv);
    estimatedlen += SvCUR(attrstr) + 1;
  }

  if (val) {
    /* If asked to escape, escape the val */
    if (b_escapeval)
      val = GF_escape_html(val, 0, 0, 0, 0);
    estimatedlen += SvCUR(val) + SvCUR(tag) + 3;
  }

  /* If asked to close the tag, add ' /' */
  if (b_closetag)
    estimatedlen += 2;

  /* Create new string to put final result in */
  tagstr = newSV(estimatedlen);
  SvPOK_on(tagstr);

  sv_catpvn(tagstr, "<", 1);
  sv_catsv(tagstr, tag);
  if (attrstr) {
    sv_catpvn(tagstr, " ", 1);
    sv_catsv(tagstr, attrstr);
    SvREFCNT_dec(attrstr);
  }
  if (b_closetag)
    sv_catpvn(tagstr, " />", 3);
  else 
    sv_catpvn(tagstr, ">", 1);

  if (val) {
    sv_catsv(tagstr, val);
    if (b_escapeval)
      SvREFCNT_dec(val);
    sv_catpvn(tagstr, "</", 2);
    sv_catsv(tagstr, tag);
    sv_catpvn(tagstr, ">", 1);
  }

  if (b_addnewline)
    sv_catpvn(tagstr, "\n", 1);

  return tagstr;
}

int GF_is_known_entity(char * sp, int i, int origlen) {

  if (++i < origlen) {
    /* Check for unicode ref (eg &#1234;) */
    if (sp[i] == '#') {
      int is_hex = 0;

      /* Check for hex unicode ref (eg &#x12af;) */
      if (i+1 < origlen && (sp[i+1] == 'x' || sp[i+1] == 'X')) {
        is_hex = 1;
        i++;
      }

      /* Not quite right, says "&#" and "&#;" are ok */
      while (++i < origlen) {
        if (sp[i] >= '0' && sp[i] <= '9') continue;
        if (is_hex && ((sp[i] >= 'a' && sp[i] <= 'f') || (sp[i] >= 'A' && sp[i] <= 'F'))) continue;
        if (sp[i] == ';' || sp[i] == ' ') return 1;
        break;
      }

      return 0;

    /* Check for entity ref (eg &nbsp;) */
    } else if ((sp[i] >= 'a' && sp[i] <= 'z') || (sp[i] >= 'A' && sp[i] <= 'Z')) {
      while (++i < origlen) {
        if ((sp[i] >= 'a' && sp[i] <= 'z') || (sp[i] >= 'A' && sp[i] <= 'Z')) continue;
        if (sp[i] == ';' || sp[i] == ' ') {
          /* We should check to see if matched text string is known enity,
             but it's not that important */
          return 1;
        }
        break;
      }
      return 0;
    }
  }
  return 0;
}

