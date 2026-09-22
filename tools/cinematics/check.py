"""Run the actual callback mod in Lua 5.1 (requires lupa.lua51)."""
from pathlib import Path
from lupa.lua51 import LuaRuntime

source = Path(__file__).with_name('amalur_vr_cinematics.lua').read_text()
lua = LuaRuntime(unpack_returned_tuples=True)
lua.globals().mod_source = source
lua.execute('''
messages, writes, calls, windows = {}, {}, 0, {}
print = function(message) table.insert(messages, message) end
WINDOW = {
    find_window = function(root, id, index, recursive)
        assert(id == 4591092 and index == -1 and recursive == true,
               'native find_window requires numeric StringID and four arguments')
        assert(windows[root], 'stale root')
        return windows[root].box
    end,
    is_visible = function(box) return box.visible end,
    set_visible = function(box, value)
        assert(box.kind == 'letterbox', 'touched subtitles/choices/pause UI')
        box.visible = value
        table.insert(writes, box)
    end
}
function make_host(root)
    local box = {kind='letterbox',visible=true}
    windows[root] = {box=box,subtitle=true,choice=true,pause=true}
    local host = {m_window=root,m_inited=false}
    host.init = function(a,b)
        assert(a == 12 and b == 34)
        calls = calls + 1
        host.m_inited = true
        windows[host.m_window].box.visible = true
        return 91,nil,73,nil
    end
    host.on_update_event = function(a,b)
        assert(a == 12 and b == 34)
        calls = calls + 1
        return 91,nil,73,nil
    end
    host.on_window_event = function(a,b)
        calls = calls + 1
        if a == 'destroy' then
            windows[host.m_window] = nil
            host.m_window = nil
            host.m_inited = false
        else
            assert(a == 'show')
            windows[host.m_window].box.visible = true
        end
    end
    host.show_letterbox = function()
        calls = calls + 1
        windows[host.m_window].box.visible = true
    end
    return host,box
end
conversation_menu,dialogue_box = make_host(101)
''')
lua.execute(source)
lua.execute('''
assert(#writes == 0 and calls == 0) -- install does not touch UI
conversation_menu.on_update_event(12,34)
assert(#writes == 0) -- not initialized
conversation_menu.m_inited = 0
conversation_menu.on_update_event(12,34)
assert(#writes == 0) -- Lua numeric false must be handled explicitly
function check_returns(...)
    assert(select('#', ...) == 4)
    local a,b,c,d = ...
    assert(a == 91 and b == nil and c == 73 and d == nil)
end
check_returns(conversation_menu.init(12,34))
assert(#writes == 1 and not dialogue_box.visible)
check_returns(conversation_menu.on_update_event(12,34))
assert(#writes == 1) -- no redundant visibility writes
assert(windows[101].subtitle and windows[101].choice and windows[101].pause)
conversation_menu.on_window_event('show')
assert(#writes == 2 and not dialogue_box.visible)
saved = conversation_menu.on_update_event
''')
lua.execute(source)
lua.execute('''
assert(saved == conversation_menu.on_update_event) -- no wrapper stacking
local before = calls
conversation_menu.on_update_event(12,34)
assert(calls == before + 1)
conversation_menu.on_window_event('destroy')
assert(conversation_menu.m_window == nil)
-- Same module, new window: resolve the current handle, not the destroyed one.
windows[102] = {box={kind='letterbox',visible=true}}
conversation_menu.m_window = 102
conversation_menu.init(12,34)
assert(not windows[102].box.visible)
assert(not dialogue_box.visible)
cinematic_paused_win,cinematic_box = make_host(201)
''')
lua.execute(source)
lua.execute('''
cinematic_paused_win.init(12,34)
assert(not cinematic_box.visible)
cinematic_paused_win.show_letterbox()
assert(not cinematic_box.visible)
assert(windows[201].subtitle and windows[201].choice and windows[201].pause)
-- A Bink scene with no UI letterbox doesn't lose video/subtitle content.
windows[201].box = nil
cinematic_paused_win.on_update_event(12,34)
assert(windows[201].subtitle)
-- Native lifecycle failures retain native behavior; only cosmetic errors caught.
local saved_set = WINDOW.set_visible
local prior_messages = #messages
WINDOW.set_visible = function() error('UI failure') end
conversation_menu.on_window_event('show')
assert(#messages == prior_messages + 1 and windows[102].box.visible)
local before = calls
conversation_menu.on_update_event(12,34)
assert(calls == before + 1 and #messages == prior_messages + 1)
WINDOW.set_visible = saved_set
-- Reloaded module object must be wrapped independently of the old one.
conversation_menu,new_box = make_host(301)
assert(loadstring(mod_source))()
conversation_menu.init(12,34)
assert(not new_box.visible)
''')
print('PASS: real Lua 5.1 callbacks; dialogue/cinematic letterboxes; native calls and '
      'return values; no duplicate wraps; destroyed/recreated windows; numeric flags; '
      'subtitles/choices/pause preserved; absent video bars; cosmetic error isolation.')

gaze_source = Path(__file__).with_name('amalur_vr_dialogue_gaze.lua').read_text()
gaze = LuaRuntime(unpack_returned_tuples=True)
gaze.globals().bar_source = source
gaze.globals().gaze_source = gaze_source
gaze.globals().entry_source = Path(__file__).with_name('amalur_vr_cinematics_entry.lua').read_text()
gaze.execute('''
messages,calls,assignments = {},0,0
visible,disabled,has_target = true,false,false
npc,player = 42,7
print = function(message) table.insert(messages,message) end
WINDOW = {is_visible=function(w) assert(w==100); return visible end,
          find_window=function() return nil end,set_visible=function() error('unexpected') end}
ACTOR_STATE_ID = function(name) assert(name=='NPC_DisableHeadTracking');return 3 end
get_player = function() return player end
ACTOR = {
    get_active_dialog_target_npc=function() return npc end,
    has_actor_state=function(n,s) assert(n==npc and s==3);return disabled end,
    look_at_has_target=function(n) assert(n==npc);return has_target end,
    look_at_set_target_object=function(n,controller,p,node)
        assert(n==npc and p==player and controller==1985713 and node==701554)
        assignments=assignments+1;has_target=true
    end
}
conversation_menu={m_window=100,m_inited=true,on_update_event=function(a,b)
    assert(a==12 and b==34);calls=calls+1;return 1,nil,3,nil
end}
loadfile = function(path)
    if path == '.\\\\mods\\\\amalur_vr_cinematics.lua' then return loadstring(bar_source) end
    assert(path == '.\\\\mods\\\\amalur_vr_dialogue_gaze.lua')
    return loadstring(gaze_source)
end
assert(loadstring(entry_source))()
assert(assignments==0 and calls==0)
function tick()
    local function returns(...)
        assert(select('#',...)==4)
        local a,b,c,d=...;assert(a==1 and b==nil and c==3 and d==nil)
    end
    returns(conversation_menu.on_update_event(12,34))
end
tick();assert(assignments==1 and has_target)
tick();assert(assignments==1) -- never replace an existing target
has_target=false;disabled=true;tick();assert(assignments==1)
disabled=false;visible=false;tick();assert(assignments==1)
visible=true;npc=nil;tick();assert(assignments==1)
npc=player;tick();assert(assignments==1)
npc=43;has_target=nil;tick();assert(assignments==1) -- unknown state fails closed
has_target=false
assert(loadstring(entry_source))()
local before=calls;tick();assert(calls==before+1 and assignments==2)
ACTOR.look_at_has_target=function() error('unavailable') end
local prior_messages=#messages
tick();assert(#messages==prior_messages+1)
before=calls;tick();assert(calls==before+1 and #messages==prior_messages+1)
''')
print('PASS: NPC gaze only fills missing targets in visible dialogue; scripted '
      'disables/existing targets win; native arguments/returns and mod-chain reload preserved.')
