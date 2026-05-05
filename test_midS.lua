function get_valid_string(str_val)
    if str_val == nil then
        return "DEFAULT_EMPTY"
    end
    return str_val
end

function mainLua()
    local missing_data = nil
    local actual_data = "REAL_DATA"
    
    local result1 = get_valid_string(missing_data)
    local result2 = get_valid_string(actual_data)
    
    -- Testing the outcomes
    if result1 == "DEFAULT_EMPTY" then
        if result2 == "REAL_DATA" then
            return 1 -- Success
        end
    end
    
    return 0 -- Failure
end

return mainLua()