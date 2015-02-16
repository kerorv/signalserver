local cjson = require "cjson"

sessions = {}

MSGTYPE_TIMER = 1

--function sendmsg(id, msg)
--function close(id)

--local function onlogin(session, msg)
--end

--local function onoffer(session, msg)
--end

local function ontimer(session, msg)
    local date = os.date("%Y-%m-%d %H:%M:%S", msg.now)
    print("message from session[" .. session.id .. "]: " .. date)
    local data = cjson.encode(msg)
    sendmsg(session.id, data)
--    if msg.now > session.start_time + 5 then
--        close(session.id)
--        print("close session[" .. session.id .. "].")
--    end
end

local function onmessage(id, msg)
    session = sessions[id]
    if session == nil then
        return
    end
    
    if math.tointeger(msg.type) == MSGTYPE_TIMER then
        ontimer(session, msg)
    end
end

-- session init
function oninit(id)
    session = {}
    session.id = id;
    sessions[id] = session
    session.start_time = os.time()
    print("session[" .. id .. "] init.")
end

-- session receive data packet
function onpacket(id, data)
    local msg = cjson.decode(data)
    if msg then
        onmessage(id, msg)
    else
        print("invalid message")
    end
end

-- session is broken
function onbreak(id)
    print("session[" .. id .. "] is broken")
    sessions[id] = nil
end
