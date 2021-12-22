-- MIDI Message
Message = {}
Message.__index = Message

setmetatable(Message, {
  __call = function (cls, ...)
    return cls.new(...)
  end,
})

function Message.new(array)
  local self = setmetatable({}, Message)
  self.type = array[1]
  self.channel = array[2]
  self.note = array[3]
  self.value = array[4]
  return self
end

-- 