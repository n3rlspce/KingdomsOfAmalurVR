-- Override the game's script-visible save availability check. The native
-- serializer and save-slot routines remain the game's own implementations.
local state = rawget(_G, 'amalur_save_anywhere_state')
if not state then
    state = {}
    _G.amalur_save_anywhere_state = state
end
if not state.override then
    state.override = function(...) return true end
end

local service = _G.SAVE_RESTORE
if type(service) == 'table' and type(service.is_save_allowed) == 'function' and
   service.is_save_allowed ~= state.override then
    state.original_service_check = service.is_save_allowed
    service.is_save_allowed = state.override
    print('AMALUR_SAVE_ANYWHERE|SERVICE_ENABLED')
end

-- The save menu has its own availability function. It can keep the button
-- disabled even when SAVE_RESTORE.is_save_allowed reports true.
local window = _G.save_restore_win
if type(window) == 'table' and type(window.is_save_allowed) == 'function' and
   window.is_save_allowed ~= state.override then
    state.original_window_check = window.is_save_allowed
    window.is_save_allowed = state.override
    print('AMALUR_SAVE_ANYWHERE|MENU_ENABLED')
end
