-- MIDI Message
function Message(arg)
  return {type = arg[1], channel = arg[2], note = arg[3], value = arg[4]}
end
-- 

-- Module class
Module = {}
Module.__index = Module

setmetatable(Module, {
  __call = function (cls, ...)
    return cls.new(...)
  end,
})

function Module.new(id, ...)
  local self = setmetatable({}, Module)
  self.id = id
  if select('#', ...) == 1 then
    self.fullname = ...
    -- TODO: get parameters automatically for known modules
  end
  return self
end

function setParamValue(module, param, value)
  id = module.id
  if type(param) == "string" then
    paramid = module.params[param]
  else
    paramid = param
  end
  if paramid ~= nil and id ~= nil then
    __setParamValue(id, paramid, value)
  end
end
--