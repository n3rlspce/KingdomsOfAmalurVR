"""Exercise the actual framework startup module under Lua 5.1."""
from pathlib import Path
from lupa.lua51 import LuaRuntime

source = Path(__file__).with_name('amalur_startup.lua').read_text()

def runtime():
    lua = LuaRuntime()
    lua.execute('''
        io=nil; bootTicket=true; skipEnabled=true
        loadfile=function(path)
            if path:find('amalur_boot_continue',1,true) then
                if bootTicket then return function() return true end end
                return nil
            end
            return function() return skipEnabled end
        end
        os.remove=function(path) assert(path:find('amalur_boot_continue',1,true)); bootTicket=false; return true end; calls = {}; shown = true; disabled = false; fonts = false; slot = 0
        progress = -1; system_ui = false; autosave = false; autosave_sets = 0; profile_applies = 0
        function record(name) calls[#calls+1] = name end
        WINDOW = {is_visible = function(window) assert(window == 42); return shown end}
        PROFILE = {
            set_enable_autosave = function(value) assert(value == true); autosave = value; autosave_sets = autosave_sets + 1 end,
            get_enable_autosave = function() return autosave end,
            apply_profile_settings = function() assert(autosave == true); profile_applies = profile_applies + 1 end,
            get_splash_win_progress_state = function() return progress end,
            is_system_ui_being_shown = function() return system_ui end,
            start_splash_win_profile_acquisition = function() record('profile') end
        }
        GAME = {is_loading_fonts = function() return fonts end}
        SAVE_RESTORE = {
            is_saving_disabled_for_user = function() return disabled end,
            get_most_recent_save_slot = function() return slot end
        }
        splash_win = {m_inited = true,
            close_current_bink = function() record('close') end,
            on_update_event = function(e,w,arg) assert(e == 7 and w == 42 and arg == 9); record('splash') end}
        main_menu = {m_inited = true, m_window = 42,
            on_update_event = function() record('menu') end,
            continue_last_save = function() assert(autosave == true and profile_applies == 1); record('continue') end}
    ''')
    lua.execute(source)
    lua.execute('assert(#calls == 0)')
    return lua

lua = runtime()
lua.execute('''
    system_ui = true; splash_win.on_update_event(7,42,9); assert(#calls == 1)
    system_ui = false; splash_win.on_update_event(7,42,9)
    assert(calls[3] == 'close' and calls[4] == 'profile')
    splash_win.on_update_event(7,42,9); assert(#calls == 5)
    main_menu.on_update_event(); assert(calls[7] == 'continue')
    main_menu.on_update_event(); assert(#calls == 8)
''')
lua.execute(source)
lua.execute('main_menu.on_update_event(); assert(#calls == 9)')
lua.execute('assert(autosave_sets == 1 and profile_applies == 1)')
for guard in ['shown = false', 'disabled = true', 'fonts = true', 'slot = -1',
              'system_ui = true', 'main_menu.m_inited = false', 'main_menu.m_restore_op_active = true']:
    lua = runtime(); lua.execute(guard)
    lua.execute('main_menu.on_update_event(); assert(#calls == 1)')
lua = runtime()
lua.execute('''
    main_menu.continue_last_save = function() record('attempt'); error('failed load') end
    main_menu.on_update_event(); main_menu.on_update_event()
    assert(#calls == 3 and calls[2] == 'attempt' and amalur_startup_state.failed)
''')
lua = runtime()
lua.execute('''
    PROFILE.start_splash_win_profile_acquisition = nil
    splash_win.on_update_event(7,42,9)
    assert(#calls == 1 and amalur_startup_state.failed)
''')
print('PASS: callback-only startup, native profile/continue once, slot zero, readiness guards, inert reload and no error retry.')

# Preloaded native main_menu never fires the framework's require trigger.
lua = runtime()
lua.execute('''
    saved_menu = main_menu; main_menu = nil; splash_win = nil
    saved_menu.on_update_event = function() record('menu') end
    amalur_startup_state = nil
    original_create = function(a,b) record('create'); return a,nil,b end
    WINDOW.create_window = original_create
''')
lua.execute(source)
lua.execute('''
    assert(#calls == 0)
    a,b,c = WINDOW.create_window(3,5)
    assert(a == 3 and b == nil and c == 5 and #calls == 1)
    main_menu = saved_menu
    WINDOW.create_window(3,5)
    assert(WINDOW.create_window == original_create and #calls == 2)
    main_menu.on_update_event()
    assert(calls[3] == 'menu' and calls[4] == 'continue')
    main_menu.on_update_event(); assert(#calls == 5)
''')
print('PASS: early window observer discovers preloaded menu; preserves return values; unhooks; loads once on update only.')
lua = runtime()
lua.execute('''
    PROFILE.set_enable_autosave = function() autosave_sets = autosave_sets + 1 end
    main_menu.on_update_event(); main_menu.on_update_event()
    assert(amalur_startup_state.failed and autosave_sets == 1 and #calls == 2)
''')
lua = runtime()
lua.execute('''slot = -1; main_menu.on_update_event()
    assert(autosave == true and profile_applies == 1 and #calls == 1)''')
print('PASS: autosave enabled and applied before Continue, no repeated profile writes, no load on failed verification, setting applied even with no save slot.')

lua = runtime()
lua.execute("""skipEnabled=false
main_menu.on_update_event();main_menu.on_update_event()
assert(#calls == 2 and autosave == true and profile_applies == 1)""")
print('PASS: Skip menu OFF leaves main menu visible while autosave is enabled.')

# Simulate returning to main menu with an entirely new Lua global state.
lua = runtime()
lua.execute("main_menu.on_update_event(); assert(not bootTicket)")
returned = runtime()
returned.execute("bootTicket=false;main_menu.on_update_event();main_menu.on_update_event();assert(#calls==2)")
# Starting with Skip OFF consumes the boot decision too; enabling it later
# must never pull the user out of the menu.
lua = runtime()
lua.execute("skipEnabled=false;main_menu.on_update_event();skipEnabled=true;main_menu.on_update_event();assert(not bootTicket and #calls==2)")
print('PASS: process boot ticket consumed once; fresh Lua state cannot auto-load; enabling Skip later does not load.')
