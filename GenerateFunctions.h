
SV * GF_escape_html(SV * str, int b_inplace, int b_lftobr, int b_sptonbsp, int b_leaveknown);
SV * GF_generate_attributes(HV * attrhv);
SV * GF_generate_tag(SV * tag, HV * attrhv, SV * val, int b_escapeval, int b_addnewline, int b_closetag);
int GF_is_known_entity(char * sp, int i, int origlen, int * maxlen);