
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "GenerateFunctions.h"

SV * GF_escape_html(SV * str, int b_inplace, int b_lftobr, int b_sptonbsp) {
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
    else if (c == '&')
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
    else if (c == '&') {
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
  SV * attrstr, * val;

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

      /* For the value part, escape special html chars, then dispose of result */
      val = GF_escape_html(val, 0, 0, 0);
      sv_catsv(attrstr, val);
      SvREFCNT_dec(val);

      sv_catpvn(attrstr, "\"", 1);
    }
  }

  return attrstr;
}

SV * GF_generate_tag(SV * tag, HV * attrhv, SV * val, int b_escapeval, int b_addnewline) {
  int estimatedlen;
  SV * tagstr, * attrstr = 0;

  estimatedlen = SvCUR(tag) + 3 + (b_addnewline ? 1 : 0);

  /* Create attributes as string */
  if (attrhv) {
    attrstr = GF_generate_attributes(attrhv);
    estimatedlen += SvCUR(attrstr) + 1;
  }

  /* If asked to escape, escape the val */
  if (val) {
    if (b_escapeval)
      val = GF_escape_html(val, 0, 0, 0);
    estimatedlen += SvCUR(val) + SvCUR(tag) + 3;
  }

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

