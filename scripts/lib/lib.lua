local dsp = require('dsp')

-- MIDI Message
function Message(arg)
  return {type = arg[1], channel = arg[2], note = arg[3], value = arg[4]}
end

NOTE_OFF = 0x8
NOTE_ON = 0x9
CC = 0xb
PROG_CHANGE = 0xc
AFTERTOUCH = 0xd
PW = 0xe

function status(msg)
  if msg.type == NOTE_OFF then return "NOTE_OFF"
  elseif msg.type == NOTE_ON then return "NOTE_ON"
  elseif msg.type == CC then return "CC"
  elseif msg.type == PROG_CHANGE then return "PROG_CHANGE"
  elseif msg.type == AFTERTOUCH then return "AFTERTOUCH"
  elseif msg.type == PW then return "PW"
  else return ""
  end
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

function sendMidiMessage(status, note, value)
  __sendMidiMessage(status, 0, note, value)
end

function sendSysex(bytes)
  __sendSysex(bytes)
end

function setParamValue(module, param, value)
  id = module.id
  if type(param) == "string" then
    paramid = module.params[param]
  else
    paramid = param
  end
  if paramid ~= nil and id ~= nil then
    __setParamValue(id, paramid, value, true, false, false)
  end
end

function setParamValueRelative(module, param, value)
  id = module.id
  if type(param) == "string" then
    paramid = module.params[param]
  else
    paramid = param
  end
  if paramid ~= nil and id ~= nil then
    __setParamValue(id, paramid, value, true, false, true)
  end
end

function setParamValueRelativeRaw(module, param, value)
  id = module.id
  if type(param) == "string" then
    paramid = module.params[param]
  else
    paramid = param
  end
  if paramid ~= nil and id ~= nil then
    __setParamValue(id, paramid, value, false, false, true)
  end
end

function setParamValueNoIndicator(module, param, value)
  id = module.id
  if type(param) == "string" then
    paramid = module.params[param]
  else
    paramid = param
  end
  if paramid ~= nil and id ~= nil then
    __setParamValue(id, paramid, value, true, true, false)
  end
end

function getParamValue(module, param)
  id = module.id
  if type(param) == "string" then
    paramid = module.params[param]
  else
    paramid = param
  end
  if paramid ~= nil and id ~= nil then
    return __getParamValue(id, paramid)
  end
end

function addCable(outModule, outId, inModule, inId)
  if outModule.id ~= nil and outId ~= nil and inModule.id ~= nil and inId ~= nil then
    return __addCable(outModule.id, outId, inModule.id, inId)
  end
  return -1
end

function removeCable(cableId)
  if cableId ~= nil then
    return __removeCable(cableId)
  end
  return -1
end
--