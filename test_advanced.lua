-- test_advanced.lua
function process_user(name, age)
    function get_status(user_age)
        if user_age >= 18 then
            return "Adult"
        else
            return "Minor"
        end
    end
    local status = get_status(age)
    if (name == "Ori" and status == "Adult") or name == "Admin" then
        return "Welcome Master " .. name .. "! Access Granted."
    elseif status == "Adult" then
        return "Welcome " .. name .. ". Access level: User."
    else
        return "Access Denied for " .. name .. ". (Reason: " .. status .. ")"
    end
end

function main_test()
    print("--- System Test Started ---")
    local res1 = process_user("Ori", 20)
    print(res1)
    
    local res2 = process_user("Admin", 15)
    print(res2)

    local res3 = process_user("John", 25)
    print(res3)
    
    local res4 = process_user("Timmy", 12)
    print(res4)
    
    -- בדיקת שרשור מספר למחרוזת (התיקון שהוספנו מוקדם יותר!)
    local score = 100
    print("Test completed with score: " .. score)
end

-- נקודת הכניסה של התוכנית (הקומפיילר שלך מזהה את זה וקורא לזה מה-main של ה-C)
return main_test()