local mod_message = require('message')

config.frameDivider = 1
config.bufferSize = 8

filter = NewAcidStation(0x957ec284a84e5)
seq = NewPhraseSeq16(0xcf8c8db83b792)
quant = NewOctave(0x90cc0691ee6e9)
clocked = Module(0xde2e16dd1bb8)
osc = NewSlimeChildSubOscillator(0x1f89b120112cc6)

gateTrig = BooleanTrigger.new()
accTrig = BooleanTrigger.new()
tiedTrig = BooleanTrigger.new()
tiedTrig = BooleanTrigger.new()
slideTrig = BooleanTrigger.new()

stepLedsTrig = BooleanTrigger.new()

writeTrigger = PulseGenerator.new()

curStep = -1
curStepParam = -1
cv = -1
performCv = -6 -- cv range is [-5,+5], -6 means disabled
poly = 0

memCv = 0.0
memory = 0

forceIn = 0

mode = "perform" -- or "perform"

data = {0x47, 0x7f, 0x49, 0x64, 0x01, 0x76, 0x00, 
0x33, 0x30, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x09, 0x01, 0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 
0x00, 0x00, 0x03, 0x00, 0x78, 0x00, 0x02, 0x04, 
0x03, 0x02, 0x02, 0x01, 0x2c, 0x00, 0x10, 0x2d, 
0x01, 0x11, 0x2e, 0x02, 0x12, 0x2f, 0x03, 0x13, 
0x28, 0x04, 0x14, 0x29, 0x05, 0x15, 0x2a, 0x06, 
0x16, 0x2b, 0x07, 0x17, 0x34, 0x08, 0x18, 0x35, 
0x09, 0x19, 0x36, 0x0a, 0x1a, 0x37, 0x0b, 0x1b, 
0x30, 0x0c, 0x1c, 0x31, 0x0d, 0x1d, 0x32, 0x0e, 
0x1e, 0x33, 0x0f, 0x1f, 0x01, 0x46, 0x00, 0x7f, 
0x43, 0x75, 0x74, 0x6f, 0x66, 0x66, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x01, 0x47, 0x00, 0x7f, 0x52, 0x65, 0x73, 0x6f, 
0x6e, 0x61, 0x6e, 0x63, 0x65, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x01, 0x48, 0x00, 0x7f, 
0x45, 0x6e, 0x76, 0x6d, 0x6f, 0x64, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x01, 0x49, 0x00, 0x7f, 0x56, 0x43, 0x46, 0x20, 
0x64, 0x65, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x01, 0x4a, 0x00, 0x7f, 
0x41, 0x63, 0x63, 0x65, 0x6e, 0x74, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x01, 0x4b, 0x00, 0x7f, 0x44, 0x72, 0x69, 0x76, 
0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x01, 0x4c, 0x00, 0x7f, 
0x56, 0x43, 0x41, 0x20, 0x64, 0x65, 0x63, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x01, 0x4d, 0x00, 0x7f, 0x48, 0x6f, 0x6c, 0x64, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x00, 0x00, 0x0c}

-- 70 -> 77
params = {0, 1, 5, 4, 6, 8, 3}

octaves = { [18] = -1.0, [19] = 0.0, [22] = 1.0, [23] = 2.0 }

noteNames = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"}

function curNote()
	for i=1,12 do
		if seq:getLight(54 + i*2) == 1 then
			return noteNames[i]
		end
	end
end

function handleOctSelect(note)
	quant:setParamRaw("Shift", octaves[note])
	forceDisplay()
end

function forceDisplay()
	if mode == "write" then
		writeString(data, OFF_PGM_NAME, makeWriteModeString(), OFF_PGM_NAME+16)
		sendSysex(data)
	elseif mode == "perform" then
		writeString(data, OFF_PGM_NAME, makeModeString(), OFF_PGM_NAME+16)
		sendSysex(data)
	end
end

function makeWriteModeString()
	if seq:getLight(82) == 1 then gate = "g" else gate =  "-" end
	if seq:getLight(84) == 1 then accent = "a" else accent =  "-" end
	if seq:getLight("Tie") == 1 then tied = "t" else tied =  "-" end
	if seq:getLight("Slide") == 1 then slide = "s" else slide =  "-" end
	return tostring(curStep) .. "|" .. gate .. accent .. tied .. slide .. "|" .. curNote()
end

function makeModeString()
	if mode == "perform" then
		if osc:getParam("Waveform") == 0.0 then wfm = "sq" else wfm = "sa" end
		return("PERF|" .. tostring(memory + 1) .. "|" .. tostring(quant:getParam("Shift")) .. "|" .. wfm)
	elseif mode == "write" then
		return("WRITE")
	end
end

function handleStepSelect(msg)
	if mode ~= "write" then
		display("auto switch!")
		mode = "write"
	end
	display('step = ' .. msg.note - 39)
	curStep = msg.note - 39
	curStepParam = msg.note + 7
	if msg.type == NOTE_ON then writeString(data, OFF_PGM_NAME, makeWriteModeString(), OFF_PGM_NAME+16); sendSysex(data) end
	if msg.type == NOTE_ON then setParamValue(seq, curStepParam, 1.0) else setParamValue(seq, curStepParam, 0.0) end
end

function handleModeSelect(msg)
	-- run
	-- bank B upper left toggle run
	if msg.note == 28 then if msg.value > 0 then setParamValue(clocked, 13, 1.0) else setParamValue(clocked, 13, 0.0) end end
	-- bank A upper left switch to write mode
	if msg.note == 20 then
		if msg.value > 0 then
			mode = "write"
			display(mode .. "!!!!")
			writeString(data, OFF_PGM_NAME, makeModeString(), OFF_PGM_NAME+16)
			sendSysex(data)
		end
	end
	-- bank A second upper switch to perform mode
	if msg.note == 21 then
		if msg.value > 0 then
			mode = "perform"
			display(mode .. "!!!!")
			writeString(data, OFF_PGM_NAME, makeModeString(), OFF_PGM_NAME+16)
			sendSysex(data)
			poly = 0
		end
	end
end

function handleWriteCv(msg)
	if msg.type == NOTE_ON then
		-- set cv on noteon
		cv = (msg.note - 60.0) / 12.0
		display('cv = ' .. cv)
	end
	if msg.type == NOTE_OFF then
		-- write on noteoff
		writeTrigger:trigger()
		forceIn = 64
	end
end

function handlePerform(msg)
	if msg.type == NOTE_ON then
		-- set perform cv on noteon
		poly = poly + 1
		performCv = (msg.note - 60.0) / 12.0
		display('performCv = ' .. performCv)
	end
	if msg.type == NOTE_OFF then
		-- disable perform cv on noteoff
		poly = poly - 1
		if poly <= 0 then performCv = -6; poly = 0 end
	end
end

function handleKnobs(msg)
	if msg.note >= 70 and msg.note < 77 then -- 7 knobs
		id = params[msg.note - 70 + 1]
		if msg.value < 64 then factor = msg.value else factor = (128 - msg.value) * -1.0 end -- inc or dec
		setParamValueRelative(filter, id, factor * 0.01)
	elseif msg.note == 77 then -- 8th knob
		if msg.value < 64 then factor = msg.value else factor = (128 - msg.value) * -1.0 end -- inc or dec
		if factor >= 3 then setParamValue(filter, 7, 1.0) elseif factor <= -3 then setParamValue(filter, 7, 0.0) end
	end
end

function processGate(msg)
	gateTrig:process(msg.value > 115)
	if gateTrig:isRising() then
		display('gate >')
		setParamValue(seq, 16, 1.0)
	elseif gateTrig:isFalling() then
		display('gate <')
		setParamValue(seq, 16, 0.0)
	end
end

function processAccent(msg)
	accTrig:process(msg.value > 115)
	if accTrig:isRising() then
		display('accent >')
		setParamValue(seq, 17, 1.0)
	elseif accTrig:isFalling() then
		display('accent <')
		setParamValue(seq, 17, 0.0)
	end
end


function processTied(msg)
	tiedTrig:process(msg.value > 115)
	if tiedTrig:isRising() then
		display('tied >')
		setParamValue(seq, 45, 1.0)
	elseif tiedTrig:isFalling() then
		display('tied <')
		setParamValue(seq, 45, 0.0)
	end
end

function processSlide(msg)
	slideTrig:process(msg.value > 115)
	if slideTrig:isRising() then
		display('slide >')
		setParamValue(seq, 18, 1.0)
	elseif slideTrig:isFalling() then
		display('slide <')
		setParamValue(seq, 18, 0.0)
	end
end

function processStepLeds(bool)
	stepLedsTrig:process(bool)
	if stepLedsTrig:isRising() then
		display("gate " .. seq:getLight(82))
		display("accent " .. seq:getLight(84))
		display("tied " .. seq:getLight("Tie"))
		display("slide " .. seq:getLight("Slide"))
		writeString(data, OFF_PGM_NAME, makeWriteModeString(), OFF_PGM_NAME+16)
		sendSysex(data)
	end
end

function handleMemMode(toggle)
	if toggle then
		memSel = true
		forceDisplay()
		display("Mem mode! ON")
	else
		memSel = false
		display("Mem mode! OFF")
	end
end

function handleMemSel(number)
	if number >= 48 and number <= 72 then
		memory = math.min(number - 48, 16)
		memCv = memory * 0.625 + 0.3
		display("Selecting memory " .. " " .. number .. " " .. memory + 1 .. " cv is: " .. memCv)
		forceDisplay()
	end
end

function handleWfm()
	if osc:getParam("Waveform") == 2.0 then
		osc:setParamRaw("Waveform", 0.0) -- set to square
	else 
		osc:setParamRaw("Waveform", 2.0) -- set to saw
	end
	forceDisplay()
end

function process(block)
	if forceIn > 0 then
		forceIn = forceIn - 1
		if forceIn == 0 then
			display("forced! " .. forceIn .. " " .. curNote())
			forceDisplay()
		end
	end

	for i=1,block.midiInputSize do
		msg = Message(block.midiInput[i])
		-- Pads / step select
		if (msg.type == NOTE_ON or msg.type == NOTE_OFF) and msg.channel == 9 then
			handleStepSelect(msg)
			if msg.type == NOTE_OFF then
				forceDisplay()
			end
			goto done
		end
		if msg.type == CC and msg.channel == 9 then
			if msg.note == 16 then
				handleMemMode(msg.value > 0)
			end
			if msg.note == 17 and msg.value > 0 then
				handleWfm()
			end
			if octaves[msg.note] ~= nil then
				handleOctSelect(msg.note)
			else
				handleModeSelect(msg)
			end
			goto done
		end
		-- Keys
		if msg.channel == 0 and msg.type ~= CC then
			if mode == "write" then
				handleWriteCv(msg)
			elseif mode == "perform" then
				if memSel == true then
					handleMemSel(msg.note)
				else
					handlePerform(msg)
				end
			end
			goto done
		end
		-- Knobs and joystick
		if msg.channel == 0 and msg.type == CC then
			-- Knobs
			if msg.note >= 70 then
				handleKnobs(msg)
			end
			-- Joystick / gate, accent, tied, slide
			if msg.note >= 1 and msg.note <=4 then
				if msg.note == 2 then
					processGate(msg)
				end
				if msg.note == 1 then
					processAccent(msg)
				end
				if msg.note == 4 then
					processTied(msg)
				end
				if msg.note == 3 then
					processSlide(msg)
				end
				processStepLeds(msg.value < 5)
			end
			goto done
		end
		display('type ' .. string.format("0x%x", msg.type) .. ' channel ' .. msg.channel .. ' note ' .. msg.note .. ' value ' .. msg.value)
		::done::
	end

	for j=1,block.bufferSize do
		block.outputs[1][j] = cv
		block.outputs[4][j] = memCv
		if writeTrigger:process(block.sampleTime) then block.outputs[2][j] = 10.0 else block.outputs[2][j] = 0.0 end
		if mode == "write" then
			block.outputs[3][j] = block.inputs[3][j]
		elseif mode == "perform" then
			if performCv > -6 then
				block.outputs[3][j] = performCv
			else
				block.outputs[3][j] = block.inputs[3][j]
			end
		end
	end

-- 0x1abc2589cacea0
end
