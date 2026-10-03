#include "pad_cmd.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static bool eq(const char *a, const char *b)
{
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { ++a; ++b; }
    return !*a && !*b;
}
uint32_t pad_cmd_button_mask(const char *name)
{
    static const char *const names[18][2] = {
        {"share","select"},{"l3","l3"},{"r3","r3"},{"options","start"},
        {"up","up"},{"right","right"},{"down","down"},{"left","left"},
        {"l2","lt"},{"r2","rt"},{"l1","lb"},{"r1","rb"},
        {"triangle","y"},{"circle","b"},{"cross","a"},{"square","x"},
        {"ps","ps"},{"touch","touchpad"},
    };
    for (unsigned i = 0; i < 18; ++i) if (eq(name,names[i][0]) || eq(name,names[i][1])) return 1u << i;
    return 0;
}
static bool number(const char *s, int min, int max, int *value)
{
    if (!*s) return false;
    const char *p = s; if (*p == '-') ++p;
    if (!*p) return false;
    while (*p) if (!isdigit((unsigned char)*p++)) return false;
    if (strlen(s) > 6) return false;
    long n = strtol(s, NULL, 10);
    if (n < min || n > max) return false;
    *value = (int)n; return true;
}
static bool add(pad_sequence_t *s, pad_act_t action)
{
    if (s->count == PAD_ACT_MAX) return false;
    s->actions[s->count++] = action;
    if (action.kind == PAD_WAIT) s->duration_ms += action.value;
    return true;
}
static bool buttons(const char *text, uint32_t *out)
{
    uint32_t mask = 0;
    while (*text) {
        char name[16]; unsigned n = 0;
        while (*text && *text != '+') {
            if (n == sizeof(name)-1) return false;
            name[n++] = *text++;
        }
        name[n] = 0; uint32_t bit = pad_cmd_button_mask(name);
        if (!bit) return false;
        mask |= bit;
        if (*text == '+' && !*++text) return false;
    }
    *out = mask; return mask != 0;
}
static bool press(pad_sequence_t *s, uint32_t mask, bool down)
{
    uint32_t digital = mask & ~((1u<<8)|(1u<<9));
    if (digital && !add(s, (pad_act_t){.kind=down?PAD_PRESS:PAD_RELEASE,.mask=digital})) return false;
    for (unsigned side = 0; side < 2; ++side)
        if ((mask & (1u<<(8+side))) && !add(s,(pad_act_t){.kind=PAD_TRIGGER,.side=side,.value=down?PAD_HALF:0})) return false;
    return true;
}
static bool segment(int argc, char **a, pad_sequence_t *s)
{
    int n = 0, x = 0, y = 0; uint32_t mask = 0;
    if (!argc) return false;
    if (eq(a[0],"tap") || eq(a[0],"hold") || eq(a[0],"release")) {
        bool tap = eq(a[0],"tap"), release = eq(a[0],"release");
        if (release && argc == 2 && eq(a[1],"all"))
            return add(s,(pad_act_t){.kind=PAD_RELEASE,.mask=(1u<<18)-1}) &&
                add(s,(pad_act_t){.kind=PAD_STICK,.side=0}) && add(s,(pad_act_t){.kind=PAD_STICK,.side=1}) &&
                add(s,(pad_act_t){.kind=PAD_TRIGGER,.side=0}) && add(s,(pad_act_t){.kind=PAD_TRIGGER,.side=1});
        if (tap) {
            if (argc < 2 || argc > 3 || !buttons(a[1],&mask)) return false;
            n = 100; if (argc == 3 && !number(a[2],1,10000,&n)) return false;
            return press(s,mask,true) && add(s,(pad_act_t){.kind=PAD_WAIT,.value=n}) && press(s,mask,false);
        }
        if (argc < 2 || (!release && argc != 2)) return false;
        for (int i = 1; i < argc; ++i) { uint32_t bits; if (!buttons(a[i],&bits)) return false; mask |= bits; }
        return press(s,mask,!release);
    }
    if (eq(a[0],"wait")) return argc == 2 && number(a[1],1,10000,&n) && add(s,(pad_act_t){.kind=PAD_WAIT,.value=n});
    if (eq(a[0],"stick")) return argc == 4 && (eq(a[1],"l") || eq(a[1],"r")) &&
        number(a[2],-128,127,&x) && number(a[3],-128,127,&y) &&
        add(s,(pad_act_t){.kind=PAD_STICK,.side=eq(a[1],"r"),.x=x,.y=y});
    if (eq(a[0],"trigger")) {
        if (argc != 3 || !(eq(a[1],"lt") || eq(a[1],"l2") || eq(a[1],"rt") || eq(a[1],"r2"))) return false;
        if (eq(a[2],"half")) n = PAD_HALF;
        else if (eq(a[2],"full")) n = PAD_FULL;
        else if (eq(a[2],"off")) n = 0;
        else if (!number(a[2],0,255,&n)) return false;
        return add(s,(pad_act_t){.kind=PAD_TRIGGER,.side=eq(a[1],"rt")||eq(a[1],"r2"),.value=n});
    }
    if (argc == 1 && (eq(a[0],"shoot") || eq(a[0],"record"))) {
        bool shoot = eq(a[0],"shoot"); s->camera_warning = true;
        unsigned values[] = {PAD_HALF,PAD_FULL,PAD_HALF,0}; unsigned count = shoot?4:3;
        if (!shoot) values[2]=0;
        for (unsigned i=0;i<count;++i) {
            if (!add(s,(pad_act_t){.kind=PAD_TRIGGER,.side=shoot,.value=values[i]})) return false;
            if (i+1<count && !add(s,(pad_act_t){.kind=PAD_WAIT,.value=200})) return false;
        }
        return true;
    }
    return false;
}
bool pad_cmd_parse(int argc, char **argv, pad_sequence_t *out, const char **error)
{
    pad_sequence_t s = {0}; char line[256]; size_t used=0;
    *error = "invalid action, range or too many actions";
    if (argc <= 0) return false;
    bool seq=eq(argv[0],"seq");
    if (!seq) {
        if (!segment(argc,argv,&s)) return false;
    } else {
        for (int i=1;i<argc;++i) {
            size_t n=strlen(argv[i]); if (used+n+1 >= sizeof(line)) return false;
            memcpy(line+used,argv[i],n); used+=n; line[used++]=' ';
        }
        line[used]=0; char *start=line;
        for (;;) {
            char *end=strchr(start,';'); if (end) *end=0;
            char *args[20]; int count=0; char *p=start;
            while (*p) {
                while (*p==' ') ++p;
                if (!*p) break;
                if (count==20) return false;
                args[count++]=p;
                while (*p && *p!=' ') ++p;
                if (*p) *p++=0;
            }
            if (!segment(count,args,&s)) return false;
            if (!end) break;
            start=end+1;
        }
    }
    *out=s; *error=NULL; return true;
}
