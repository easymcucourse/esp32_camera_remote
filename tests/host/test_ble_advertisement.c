#include "ble_advertisement.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    const uint8_t hid[]={3,3,0x12,0x18};
    const uint8_t appearance[]={3,0x19,0xc4,3};
    const uint8_t name[]={5,9,'X','b','o','x'};
    ble_advertisement_t s={0};
    assert(ble_advertisement_parse(&s,hid,sizeof(hid)) && s.hid && !s.gamepad);
    assert(ble_advertisement_parse(&s,appearance,sizeof(appearance)) && s.gamepad);
    assert(ble_advertisement_parse(&s,name,sizeof(name)) && !strcmp(s.name,"Xbox"));
    const uint8_t ultimate[]={18,9,'8','B','i','t','D','o',' ','U','l','t','i','m','a','t','e',' ','2'};
    ble_advertisement_t model={0};
    assert(ble_advertisement_parse(&model,ultimate,sizeof(ultimate)) && model.gamepad && !model.hid);
    for (size_t n=1;n<sizeof(hid);++n) {
        ble_advertisement_t before=s;
        assert(!ble_advertisement_parse(&s,hid,n));assert(!memcmp(&s,&before,sizeof(s)));
    }
    const uint8_t malformed[]={2,3,0x12};
    assert(!ble_advertisement_parse(&s,malformed,sizeof(malformed)));
    const uint8_t keyboard[]={3,0x19,0xc1,3,4,9,'k','e','y'};
    s=(ble_advertisement_t){0};
    assert(ble_advertisement_parse(&s,keyboard,sizeof(keyboard)) && !s.gamepad);
    const uint8_t padding[]={0,0xff};
    assert(ble_advertisement_parse(&s,padding,sizeof(padding)));
    assert(ble_advertisement_parse(&s,NULL,0));
    assert(!ble_advertisement_parse(&s,NULL,1));
    assert(!ble_advertisement_parse(NULL,hid,sizeof(hid)));
    return 0;
}
