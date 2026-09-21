-- One framework entry per module: do not depend on duplicate trigger handling.
for _, name in ipairs({'amalur_vr_cinematics', 'amalur_vr_dialogue_gaze'}) do
    local ok, message = pcall(function()
        local script, reason = loadfile('.\\mods\\' .. name .. '.lua')
        if not script then error(reason) end
        script()
    end)
    if not ok then
        print('AMALUR_VR_CINEMATICS_LOAD_ERROR|' .. name .. '|' .. tostring(message))
    end
end
