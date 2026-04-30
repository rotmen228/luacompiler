
local rot = nil
function make_multiplier(factor)

    return function(x)
        return x * factor
    end
end

local doubler = make_multiplier(2)
local tripler = make_multiplier(3)

local results_table = {}


for i = 1, 5 do
    results_table[i] = doubler(i) + tripler(i)
end

local total = 0
for index = 1, 5 do
    total = total + results_table[index]
end

return total