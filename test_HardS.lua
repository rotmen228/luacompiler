-- test_hard_strings_no_len.lua

function check_status(val)
    if val == nil then
        return "EMPTY"
    end
    if val == "ERROR" then
        return "FAILED"
    end
    -- Implicit coercion: concatenating string with whatever 'val' is
    return "OK_" .. val
end

function build_sequence(start_str, max_iterations)
    local result = start_str
    local i = 0
    
    while i < max_iterations do
        -- Scope shadowing inside the loop
        local i = i + 1 
        local suffix = ""
        
        local rem = i % 2
        if rem == 0 then
            suffix = "_E"
        else
            suffix = "_O"
        end
        
        -- Concatenating strings and a number (i)
        result = result .. suffix .. i
        
        -- Short-circuit evaluation with nil
        local temp = nil
        if i > 5 and temp == nil then
            result = result .. "-MID"
        end
        
        -- Exact string matching to break flow instead of length
        if result == "START_O1_E2_O3_E4_O5_E6-MID" then
            result = result .. "-STOP"
            i = max_iterations -- Force exit loop
        end
    end
    
    return result
end

function mainLua()
    -- Variable declared but not initialized (implicitly nil)
    local uninitialized
    local status1 = check_status(uninitialized)
    local status2 = check_status(100)
    
    local sequence = build_sequence("START", 8)
    
    -- Heavy chaining
    local final_res = status1 .. "|" .. status2 .. "|" .. sequence
    
    return final_res
end

return mainLua()