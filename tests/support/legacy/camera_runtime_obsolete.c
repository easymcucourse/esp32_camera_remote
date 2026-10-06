bool camera_maintenance_acquire(uint32_t timeout)
{
    portENTER_CRITICAL(&lifecycle_mux); bool blocked=maintenance_gate || !camera_controller_admitting();
    if (!blocked) { maintenance_gate=true; request_stop_locked(); } portEXIT_CRITICAL(&lifecycle_mux); if (blocked) return false;
    camera_focus_cancel(); uint32_t started=now_ms(NULL);
    while (atomic_load(&busy)) {
        if ((uint32_t)(now_ms(NULL)-started)>=timeout) { portENTER_CRITICAL(&lifecycle_mux); maintenance_gate=!camera_controller_admitting(); portEXIT_CRITICAL(&lifecycle_mux); return false; }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    portENTER_CRITICAL(&lifecycle_mux); atomic_store(&busy,true); portEXIT_CRITICAL(&lifecycle_mux); return true;
}
void camera_maintenance_release(void) { portENTER_CRITICAL(&lifecycle_mux); atomic_store(&busy,false); maintenance_gate=!camera_controller_admitting(); portEXIT_CRITICAL(&lifecycle_mux); }
/* Historical producer-fixture helpers only. No production link. */
bool camera_controller_forget(bool reserved)
{
    portENTER_CRITICAL(&lifecycle_mux);
    bool blocked=reserved ? (!maintenance_gate || !atomic_load(&busy) || atomic_load(&worker_active)) :
        (maintenance_gate || !camera_controller_admitting() || atomic_load(&busy));
    if (!blocked && !reserved) atomic_store(&busy,true);
    portEXIT_CRITICAL(&lifecycle_mux);
    if (blocked) return false;
    bool ok=camera_identity_run(NULL,CAMERA_IDENTITY_FORGET,NULL,NULL); if (ok) camera_network_select(camera_session_next_generation(),NULL);
    if (!reserved) { portENTER_CRITICAL(&lifecycle_mux); atomic_store(&busy,false); portEXIT_CRITICAL(&lifecycle_mux); }
    return ok;
}
bool camera_forget_pairing(void) { return camera_controller_forget(false); }
bool app_camera_quiesce(uint32_t timeout) { return initialized && camera_maintenance_acquire(timeout); }
void app_camera_quiesce_release(void) { camera_maintenance_release(); }
void app_camera_messages_stop(void)
{
    (void)app_camera_messages_quiesce(UINT32_MAX);
}
