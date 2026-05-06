
function factorial(n)
    if n <= 1 then
        return 1
    else
        return n * factorial(n - 1)
    end
end

local sum = 0
local i = 1

while i <= 5 do
    sum = sum + factorial(i)
    i = i + 1
end

print(sum)