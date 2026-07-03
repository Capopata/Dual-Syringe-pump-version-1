#include "domain.h"
#include <string.h>// using memset

// keyword "static" ensure that this variable can access in domain.c
// it must be call system_get() function to access this variable
static system_state_t g_system_state;

// Initialize all system state when booting
void system_state_init(void){
    // Clear trash in ram by memset (set all by 0)
    memset(&g_system_state, 0, sizeof(system_state_t));

    // Set default value for system state
    g_system_state.selected_channel = 0;

    // set default value for system
    g_system_state.op_mode = SYS_MODE_INDEPENDENT;
    g_system_state.is_system_running = false;

    // Set safety state for all channel
    for (uint8_t i = 0; i< MAX_CHANNEL; i++){
        system_reset_channel(i);
        g_system_state.channels[i].volume_target = 1.0f;

    }
}

//Return pointer manage system - Hàm truy xuất
system_state_t* system_get(void){
    return &g_system_state;
}

// Reset one channel (just reset runtime & hardware state, user config not change)
void system_reset_channel(uint8_t ch){
    if(ch>= MAX_CHANNEL){
        return;
    }

    // --- Reset Runtime Status ---
    g_system_state.channels[ch].volume_infused = 0.0f;
    g_system_state.channels[ch].flow_actual = 0.0f;
    g_system_state.channels[ch].velocity = 0.0f;
    g_system_state.channels[ch].time_run = 0.0f;

    // --- Reset Hardware Mapping ---
    g_system_state.channels[ch].current_steps = 0;
    // --- Reset States ---
    g_system_state.channels[ch].state = PUMP_IDLE;
}