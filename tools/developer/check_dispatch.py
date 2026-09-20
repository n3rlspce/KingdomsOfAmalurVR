"""Exercise the real game-callback dispatcher with an isolated Lua 5.1 runtime."""
from pathlib import Path
from lupa.lua51 import LuaRuntime

root = Path(__file__).parent
lua = LuaRuntime(unpack_returned_tuples=True)
lua.globals().module_source = root.joinpath('amalur_dev.lua').read_text()
lua.execute(r'''
    messages, mutations = {}, {}
    print = function(s) table.insert(messages, s) end
    calls, paused = 0, false
    minimap_win = {on_update_event = function(a,b)
        assert(a == 12 and b == 34); calls = calls + 1
    end}
    GAME = {is_game_paused = function() return paused end}
    get_player = function() return 42 end
    SIMTYPE_ID = function() return 100 end
    PROTO = {create_actor = function() table.insert(mutations, 'spawn') end}
    ACTOR = {get_angle = function() return 0 end,
             get_point_near_object = function() return {500,0,0} end}
    PLAYER = {cheat_add_item = function() table.insert(mutations, 'grant') end,
              get_item_index = function() return 73 end,
              equip = function(item,slot)
                  assert(item == 73 and slot == 0)
                  table.insert(mutations, 'equip')
              end}
    loadfile = function(path)
        if path == '.\\mods\\amalur_dev.lua' then return assert(loadstring(module_source)) end
        assert(path == '.\\mods\\amalur_request.lua')
        if request then return function() return request end end
    end
    function tick()
        for i=1,15 do minimap_win.on_update_event(12,34) end
    end
''')
dispatcher = root.joinpath('amalur_dispatch.lua').read_text()
lua.execute(dispatcher)
lua.execute('''
    assert(#mutations == 0 and #messages == 1)
    request = {nonce='old',session='old',run=function() amalur_dev.sword() end}
    tick(); assert(#mutations == 0)
    assert(string.find(messages[#messages], 'stale session'))
    request = {nonce='session_1',connect=true}
    tick(); assert(#mutations == 0 and amalur_dispatch_state.session == 'session_1')
    request = {nonce='equip1',session='session_1',run=function()
        return amalur_dev.equip_existing('sword2h_unique12f',0)
    end}
    tick(); assert(#mutations == 1 and mutations[1] == 'equip')
    tick(); assert(#mutations == 1) -- repeated file cannot repeat action
    paused = true
    request = {nonce='paused1',session='session_1',run=function() amalur_dev.sword() end}
    tick(); assert(#mutations == 1)
    paused = false
    tick(); assert(#mutations == 1) -- rejected request does not execute on unpause
    request = {nonce='error1',session='session_1',run=function() error('test failure') end}
    tick(); assert(amalur_dispatch_state.executing == false)
    assert(string.find(messages[#messages], 'ERROR|'))
    assert(not pcall(amalur_dev.sword)) -- console calls stay disabled
    request = {nonce='grant1',session='session_1',run=function() amalur_dev.sword() end}
    tick(); assert(#mutations == 2 and mutations[2] == 'grant')
    saved_wrapper = minimap_win.on_update_event
''')
lua.execute(dispatcher)
lua.execute('''
    assert(saved_wrapper == minimap_win.on_update_event)
    tick(); assert(#mutations == 2)
    assert(calls == 135)
''')
print('PASS: game callback only; connect/session isolation; exactly-once grants/equip; '
      'paused rejection; no delayed retry; reload and exception cleanup.')
