-- Run native startup transitions on window update callbacks, once per process.
-- Keep the original archive and the game's save compatibility/loading logic.
local state = _G.amalur_startup_state
if not state then
    state = {wrappers = {}}
    _G.amalur_startup_state = state
end

local function flag(value) return value == true or value == 1 end
local function require_api(value, name)
    assert(type(value) == 'function', name .. ' unavailable')
    return value
end
local function visible(window)
    if window == nil or window == false or window == 0 then return false end
    return flag(require_api(WINDOW and WINDOW.is_visible, 'WINDOW.is_visible')(window))
end

local function splash_step(host, window)
    if state.profileAttempted or not flag(host.m_inited) or not visible(window) then return end
    local progress = require_api(PROFILE and PROFILE.get_splash_win_progress_state,
                                'PROFILE.get_splash_win_progress_state')()
    if progress ~= -1 then return end
    if flag(require_api(PROFILE.is_system_ui_being_shown, 'PROFILE.is_system_ui_being_shown')()) then return end
    local acquire = require_api(PROFILE.start_splash_win_profile_acquisition,
                                'PROFILE.start_splash_win_profile_acquisition')
    local close = require_api(host.close_current_bink, 'splash_win.close_current_bink')
    state.profileAttempted = true
    close()
    acquire()
    print('AMALUR_STARTUP|PROFILE_REQUESTED')
end

local function menu_step(host)
    if state.loadAttempted or not flag(host.m_inited) or flag(host.m_restore_op_active) then return end
    if not visible(host.m_window) then return end
    if flag(require_api(PROFILE and PROFILE.is_system_ui_being_shown,
                        'PROFILE.is_system_ui_being_shown')()) then return end
    if flag(require_api(SAVE_RESTORE and SAVE_RESTORE.is_saving_disabled_for_user,
                        'SAVE_RESTORE.is_saving_disabled_for_user')()) then return end
    if flag(require_api(GAME and GAME.is_loading_fonts, 'GAME.is_loading_fonts')()) then return end
    local slot = require_api(SAVE_RESTORE.get_most_recent_save_slot,
                            'SAVE_RESTORE.get_most_recent_save_slot')()
    if type(slot) ~= 'number' or slot < 0 or slot % 1 ~= 0 then return end
    local continue = require_api(host.continue_last_save, 'main_menu.continue_last_save')
    state.loadAttempted = true -- Failed/declined loads never loop back into loading.
    print('AMALUR_STARTUP|CONTINUE_REQUESTED|' .. tostring(slot))
    continue()
end

local function wrap(name, step)
    local host = _G[name]
    if type(host) ~= 'table' or type(host.on_update_event) ~= 'function' then return end
    if host.on_update_event == state.wrappers[name] then return end
    local original = host.on_update_event
    local wrapper = function(event, window, ...)
        original(event, window, ...)
        if state.failed then return end
        local ok, message = pcall(step, host, window)
        if not ok then
            state.failed = true
            print('AMALUR_STARTUP|DISABLED|' .. tostring(message))
        end
    end
    state.wrappers[name] = wrapper
    host.on_update_event = wrapper
    print('AMALUR_STARTUP|HOOKED|' .. name)
end

wrap('splash_win', splash_step)
wrap('main_menu', menu_step)

-- Native startup may preload main_menu without passing through require(), so
-- the framework's main_menu trigger is not guaranteed to fire. UI_State_MGR
-- loads earlier; window creation gives us a callback boundary after the menu
-- functions exist. Only install wrappers here, never begin loading a save.
if not state.wrappers.main_menu and not state.creationWrapper and
   WINDOW and type(WINDOW.create_window) == 'function' then
    local original = WINDOW.create_window
    local observer
    observer = function(...)
        wrap('main_menu', menu_step)
        if state.wrappers.main_menu and WINDOW.create_window == observer then
            WINDOW.create_window = original
        end
        return original(...)
    end
    state.creationWrapper = observer
    WINDOW.create_window = observer
    print('AMALUR_STARTUP|WAITING_FOR_MENU')
end
