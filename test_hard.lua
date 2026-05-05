local rot = nil
local man = "hello" .. " world"
function is_prime_helper(n, divisor)
    if n <= 1 then
        return 0
    end
    if divisor == 1 then
        return 1
    end
    
    local rem = n % divisor
    if rem == 0 then
        return 0
    end
    
    return is_prime_helper(n, divisor - 1)
end

function is_prime(n)
    return is_prime_helper(n, n - 1)
end

-- 2. Heavy control flow with loops and math
function collatz_steps(n)
    steps = 0
    while n > 1 do
        -- Local variable inside a while loop
        local rem = n % 2
        if rem == 0 then
            -- Note: In C, standard integer division truncates. 
            -- Your compiler needs to handle how Lua treats division.
            n = n / 2
        else
            n = (n * 3) + 1
        end
        steps = steps + 1
    end
    return steps
end

function complex_calculation(limit)
    local sum = 0
    local i = 1
    
    while i <= limit do
        -- 3. Scope Shadowing inside standard blocks
        local current_val = i
        local add_to_sum = 0
        
        if is_prime(current_val) == 1 then
            -- Shadowing the variable 'current_val' inside an 'if' block
            local current_val = current_val * 10
            add_to_sum = current_val
            
        -- 4. Short-circuit evaluation with function calls
        elseif current_val > 10 and collatz_steps(current_val) > 15 then
            add_to_sum = current_val * 2
        else
            add_to_sum = current_val
        end
        
        sum = sum + add_to_sum
        i = i + 1
    end
    
    return sum
end

function mainLua()
    local target = 20
    local result = complex_calculation(target)
    print(target)
    print(result)
    return result
end

return mainLua()