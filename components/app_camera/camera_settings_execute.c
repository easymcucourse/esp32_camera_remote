#include "camera_settings_execute.h"
static bool healthy(camera_backend_result_t result)
{ return result == CAMERA_BACKEND_OK || result == CAMERA_BACKEND_REFUSED; }
camera_backend_result_t camera_settings_execute(camera_backend_t *backend,
    const camera_properties_t *properties, setting_control_t *mode,
    camera_menu_t *menu, bool *fresh, uint32_t now, uint32_t timeout, bool *issued)
{
    if (issued) *issued=false;
    if (!backend || !properties || !mode || !menu || !fresh || !issued || !timeout)
        return CAMERA_BACKEND_INVALID;
    if (backend->api_version!=CAMERA_BACKEND_API_VERSION || !backend->ops ||
        !(backend->capabilities & CAMERA_BACKEND_CAP_PROPERTIES)) return CAMERA_BACKEND_UNSUPPORTED;
    if (!*fresh) return CAMERA_BACKEND_OK;
    *fresh=false;
    if (!properties->valid) {
        camera_properties_apply(properties,mode,menu,now); return CAMERA_BACKEND_STATE;
    }
    uint32_t value;
    if (setting_control_next(mode,now,&value)) {
        camera_backend_result_t result;
        if (!properties->seen[CAMERA_SETTING_MODE]) result=CAMERA_BACKEND_PROTOCOL;
        else if (!backend->ops->set) result=CAMERA_BACKEND_UNSUPPORTED;
        else {
            *issued=true;
            result=backend->ops->set(backend->context,CAMERA_SETTING_MODE,
                (camera_value_t){properties->values[CAMERA_SETTING_MODE].type,value},timeout);
        }
        setting_control_response(mode,result==CAMERA_BACKEND_OK);
        return healthy(result) ? CAMERA_BACKEND_OK : result;
    }
    if (mode->awaiting) return CAMERA_BACKEND_OK;
    for (unsigned i=0;i<CAMERA_MENU_COUNT;++i) {
        camera_menu_write_t write;
        if (!camera_menu_next(menu,i,now,&write)) continue;
        camera_setting_t setting=camera_menu_settings[i];
        camera_backend_result_t result;
        if (!properties->seen[setting] || write.type!=(unsigned)properties->values[setting].type+1)
            result=CAMERA_BACKEND_PROTOCOL;
        else if (write.relative) {
            if (!backend->ops->step) result=CAMERA_BACKEND_UNSUPPORTED;
            else {
                *issued=true;
                result=backend->ops->step(backend->context,setting,write.value==1 ? 1 : -1,timeout);
            }
        } else if (!backend->ops->set) result=CAMERA_BACKEND_UNSUPPORTED;
        else {
            *issued=true;
            result=backend->ops->set(backend->context,setting,
                (camera_value_t){properties->values[setting].type,write.value},timeout);
        }
        camera_menu_response(menu,i,result==CAMERA_BACKEND_OK);
        return healthy(result) ? CAMERA_BACKEND_OK : result;
    }
    return CAMERA_BACKEND_OK;
}
