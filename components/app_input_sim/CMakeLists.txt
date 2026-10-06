set(sim_sources "")
if(CONFIG_REMOTE_DBG_SIM)
    list(APPEND sim_sources "input_sim.c" "../../common/atom_sim.c"
        "../../common/pad_player.c" "../../m5_atom_matrix/main/ds4_events.c")
endif()
idf_component_register(SRCS ${sim_sources}
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "../../common"
    PRIV_REQUIRES app_input app_console atom_protocol esp_timer freertos)
