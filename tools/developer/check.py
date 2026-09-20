"""Run the real developer Lua under Lua 5.1 with a fake engine; never opens a game."""
from pathlib import Path

from lupa.lua51 import LuaRuntime


def main():
    lua = LuaRuntime(unpack_returned_tuples=True)
    lua.execute('''
        mutations = {}
        math.floor = nil -- The game's math table does not expose standard floor.
        function record(kind, ...)
            table.insert(mutations, {kind, ...})
        end
        function get_player() return 42 end
        function SIMTYPE_ID(name)
            if name == 'wolf_forest' then return 101 end
            if name == 'sword2h_unique12f' then return 202 end
        end
        ACTOR = {
            get_angle = function(player) assert(player == 42); return 90 end,
            get_point_near_object = function(player, angle, distance)
                assert(player == 42 and angle == 90)
                return {distance, 20, 30}
            end
        }
        PROTO = {create_actor = function(...) record('spawn', ...) end}
        PLAYER = {
            cheat_add_item = function(...) record('grant', ...) end,
            get_item_index = function(...) record('find', ...) end,
            equip = function(...) record('equip', ...) end
        }
        interfaceLibrary = {ftp_notify = function(...) record('notify', ...) end}
    ''')
    source = Path(__file__).with_name('amalur_dev.lua').read_text(encoding='utf-8')
    lua.execute(source)
    lua.execute('assert(#mutations == 0)')
    lua.execute('''
        local original_player = get_player
        get_player = function() error('probe must not call engine functions') end
        assert(amalur_dev.probe() == 'developer APIs present; gameplay operations untested')
        assert(#mutations == 0)
        get_player = original_player
        local original_equip = PLAYER.equip
        PLAYER.equip = nil
        assert(not pcall(amalur_dev.probe))
        assert(#mutations == 0)
        PLAYER.equip = original_equip
        mutations = {}
        amalur_dev.wolf()
        local call = mutations[1]
        assert(#mutations == 1 and #call == 6)
        assert(call[1] == 'spawn' and call[2] == 101 and call[3] == 500)
        assert(call[4] == 20 and call[5] == 30 and call[6] == 42)
        mutations = {}
        amalur_dev.sword()
        assert(#mutations == 1 and mutations[1][1] == 'grant')
        assert(mutations[1][2] == 202 and mutations[1][3] == 1)
        mutations = {}
        amalur_dev.spawn('wolf_forest', 100)
        amalur_dev.spawn('wolf_forest', 2000)
        amalur_dev.give('sword2h_unique12f', 20)
        assert(#mutations == 3 and mutations[1][3] == 100)
        assert(mutations[2][3] == 2000 and mutations[3][3] == 20)
        mutations = {}
    ''')
    bad_calls = [
        "amalur_dev.spawn('missing')",
        "amalur_dev.give('missing')",
        "amalur_dev.spawn(123)",
        "amalur_dev.give(\"wolf_forest'); evil() --\")",
        "amalur_dev.give(string.rep('a',129))",
    ]
    for value in ['0', '-1', '21', '1.5', '0/0', 'math.huge', 'false', "'1'"]:
        bad_calls.append(f"amalur_dev.give('sword2h_unique12f', {value})")
    for value in ['99', '2001', '100.5', '0/0', 'math.huge', 'false', "'500'"]:
        bad_calls.append(f"amalur_dev.wolf({value})")
    for call in bad_calls:
        result = lua.eval(f'function() return pcall(function() {call} end) end')()
        ok = result[0] if isinstance(result, tuple) else result
        assert not ok, f'Unexpectedly accepted: {call}'
        lua.execute('assert(#mutations == 0)')
    lua.execute('''
        PLAYER.get_item_index = function(id) assert(id == 202); return 73 end
        PLAYER.equip = function(...) record('equip', ...) end
        for slot = 0, 1 do
            amalur_dev.equip_existing('sword2h_unique12f', slot)
            assert(#mutations == 1 and mutations[1][1] == 'equip')
            assert(mutations[1][2] == 73 and mutations[1][3] == slot)
            mutations = {}
        end
        assert(not pcall(amalur_dev.equip_existing, 'sword2h_unique12f', 2))
        assert(#mutations == 0)
        for slot = 0, 1 do
            amalur_dev.give_and_equip('sword2h_unique12f', slot)
            assert(#mutations == 2 and mutations[1][1] == 'grant')
            assert(mutations[2][1] == 'equip' and mutations[2][2] == 73)
            assert(mutations[2][3] == slot)
            mutations = {}
        end
        assert(not pcall(amalur_dev.give_and_equip, 'sword2h_unique12f', 2))
        assert(#mutations == 0)
        PLAYER.get_item_index = function() return -1 end
        assert(not pcall(amalur_dev.equip_existing, 'sword2h_unique12f', 0))
        assert(#mutations == 0)
        assert(not pcall(amalur_dev.give_and_equip, 'sword2h_unique12f', 0))
        assert(#mutations == 1 and mutations[1][1] == 'grant')
        mutations = {}
    ''')
    for setup, call in [
        ('get_player = function() return nil end', 'amalur_dev.wolf()'),
        ('get_player = function() return nil end', 'amalur_dev.sword()'),
        ('PROTO.create_actor = nil', 'amalur_dev.wolf()'),
        ('PLAYER.cheat_add_item = nil', 'amalur_dev.sword()'),
        ('SIMTYPE_ID = function() error("unknown") end', 'amalur_dev.wolf()'),
        ('SIMTYPE_ID = function() return 0 end', 'amalur_dev.sword()'),
        ('ACTOR.get_point_near_object = function() return {0/0,0,0} end',
         'amalur_dev.wolf()'),
        ('ACTOR.get_point_near_object = function() return nil end',
         'amalur_dev.wolf()'),
    ]:
        # Restore the fake engine between cases without reimplementing the module.
        lua.execute('''
            get_player = function() return 42 end
            SIMTYPE_ID = function() return 101 end
            PROTO.create_actor = function(...) record('spawn', ...) end
            PLAYER.cheat_add_item = function(...) record('grant', ...) end
            ACTOR.get_point_near_object = function() return {500,0,0} end
        ''')
        lua.execute(setup)
        result = lua.eval(f'function() return pcall(function() {call} end) end')()
        ok = result[0] if isinstance(result, tuple) else result
        assert not ok, f'Unexpectedly accepted after: {setup}'
        lua.execute('assert(#mutations == 0)')
    lua.execute(source)
    lua.execute('assert(#mutations == 0)')
    print('PASS: load/reload inert; exact spawn/grant dispatch; bounds, invalid IDs, '
          'missing engine/player and invalid positions reject without mutation.')


if __name__ == '__main__':
    main()
