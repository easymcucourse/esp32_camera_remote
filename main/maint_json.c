#include "maint_json.h"
bool maint_json_flat(const char *text,size_t size)
{
    if (!text || !size) return false;
    bool string=false,escaped=false,object=false,closed=false;
    unsigned depth=0;
    for (size_t i=0;i<size;++i) {
        unsigned char c=(unsigned char)text[i];
        if (!c) return false;
        if (string) {
            if (escaped) {
                if (c=='u' && i+4<size && text[i+1]=='0' && text[i+2]=='0' && text[i+3]=='0' && text[i+4]=='0') return false;
                escaped=false;
            } else if (c=='\\') escaped=true;
            else if (c=='"') string=false;
            continue;
        }
        if (c==' ' || c=='\r' || c=='\n' || c=='\t') continue;
        if (closed) return false;
        if (c=='"') { if (!depth) return false;string=true; }
        else if (c=='[' || c==']') return false;
        else if (c=='{') { if (depth || object) return false;depth=1;object=true; }
        else if (c=='}') { if (!depth) return false;depth=0;closed=true; }
        else if (!depth) return false;
    }
    return object && closed && !depth && !string && !escaped;
}
