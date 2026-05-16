

global_counter = 0
local program_name = "ShowcaseTest"
local version = 3
local pi_approx = 3.14
local flag = true
local nothing = nil


function square(x)
    return x * x
end

function is_even(n)
    local rem = n % 2
    if rem == 0 then
        return 1
    else
        return 0
    end
end

function factorial(n)
    if n <= 1 then
        return 1
    end
    return n * factorial(n - 1)
end


function clamp(val, lo, hi)
    if val < lo then
        return lo
    elseif val > hi then
        return hi
    else
        return val
    end
end


function count_to(limit)
    local i = 1
    while i <= limit do
        global_counter = global_counter + 1
        i = i + 1
    end
    return global_counter
end


function sum_until(target)
    local total = 0
    local step = 1
    repeat
        total = total + step
        step = step + 1
    until total >= target
    return total
end


function sum_squares(n)
    local acc = 0
    for k = 1, n do
        acc = acc + square(k)
    end
    return acc
end


function shadow_demo(x)
    local result = x * 2
    if x > 5 then
        local result = x * 10
        global_counter = result
    end
    return result
end


function in_range(val, lo, hi)
    if val >= lo and val <= hi then
        return 1
    end
    return 0
end

function either_flag(a, b)
    if a == 1 or b == 1 then
        return 1
    end
    return 0
end


function classify(n)
    local s = square(n)
    if s > 100 then
        return 3
    elseif s > 25 then
        return 2
    else
        return 1
    end
end

function mainLua()

    local f5 = factorial(5)
    print(f5)

    local sq7 = square(7)
    print(sq7)

    local c = clamp(42, 1, 10)
    print(c)

    local after_count = count_to(5)
    print(after_count)

    local su = sum_until(15)
    print(su)

    local ss = sum_squares(4)
    print(ss)

    local sd = shadow_demo(8)
    print(sd)
    print(global_counter)

    local ir = in_range(5, 1, 10)
    print(ir)
    local ef = either_flag(0, 1)
    print(ef)

    local cls = classify(6)
    print(cls)

    local ev = is_even(8)
    print(ev)

    return f5
end

return mainLua()