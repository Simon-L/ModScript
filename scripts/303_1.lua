config.frameDivider = 1
config.bufferSize = 32

filter = Module(0x18c727813532be)
seq = Module(0xbcc970ebfd8bf)

gateTrig = BooleanTrigger.new()
accTrig = BooleanTrigger.new()
tiedTrig = BooleanTrigger.new()
slideTrig = BooleanTrigger.new()

writeTrigger = PulseGenerator.new()

curStep = -1
cv = -1

-- 70 -> 77
params = {0, 1, 5, 4, 6, 8, 3}

function process(block)
	for i=1,block.midiInputSize do
		msg = Message(block.midiInput[i])
		-- Pads / step select
		if (msg.type == NOTE_ON or msg.type == NOTE_OFF) and msg.channel == 9 then
			display('param = ' .. msg.note + 7)
			curStep = msg.note + 7
			if msg.type == NOTE_ON then setParamValue(seq, curStep, 1.0) else setParamValue(seq, curStep, 0.0) end
			goto done
		end
		if msg.type == CC and msg.channel == 9 then
			-- run
			if msg.note == 28 then if msg.value > 0 then setParamValue(seq, 5, 1.0) else setParamValue(seq, 5, 0.0) end end
			goto done
		end
		-- Keys
		if msg.channel == 0 and msg.type ~= CC then
			if msg.type == NOTE_ON then
				-- set cv on noteon
				cv = (msg.note - 60.0) / 12.0
				display('cv = ' .. cv)
			end
			if msg.type == NOTE_OFF then
				-- write on noteoff
				writeTrigger:trigger()
			end
			goto done
		end
		-- Knobs
		if msg.channel == 0 and msg.type == CC then
			if msg.note >= 70 and msg.note < 77 then -- 7 knobs
				id = params[msg.note - 70 + 1]
				if msg.value < 64 then factor = msg.value else factor = (128 - msg.value) * -1.0 end -- inc or dec
				setParamValueRelative(filter, id, factor * 0.01)
			elseif msg.note == 77 then -- 8th knob
				if msg.value < 64 then factor = msg.value else factor = (128 - msg.value) * -1.0 end -- inc or dec
				if factor >= 3 then setParamValue(filter, 7, 1.0) elseif factor <= -3 then setParamValue(filter, 7, 0.0) end
			end
			-- Joystick / gate, accent, tied, slide
			if msg.note >= 1 and msg.note <=4 then
				if msg.note == 2 then
					gateTrig:process(msg.value > 115)
					if gateTrig:isRising() then display('gate >'); setParamValue(seq, 16, 1.0) elseif gateTrig:isFalling() then display('gate <'); setParamValue(seq, 16, 0.0) end
				end
				if msg.note == 1 then
					accTrig:process(msg.value > 115)
					if accTrig:isRising() then display('accent >'); setParamValue(seq, 17, 1.0) elseif accTrig:isFalling() then display('accent <'); setParamValue(seq, 17, 0.0) end
				end
				if msg.note == 4 then
					tiedTrig:process(msg.value > 115)
					if tiedTrig:isRising() then display('tied >'); setParamValue(seq, 45, 1.0) elseif tiedTrig:isFalling() then display('tied <'); setParamValue(seq, 45, 0.0) end
				end
				if msg.note == 3 then
					slideTrig:process(msg.value > 115)
					if slideTrig:isRising() then display('slide >'); setParamValue(seq, 18, 1.0) elseif slideTrig:isFalling() then display('slide <'); setParamValue(seq, 18, 0.0) end
				end
			end
			goto done
		end
		display('type ' .. string.format("0x%x", msg.type) .. ' channel ' .. msg.channel .. ' note ' .. msg.note .. ' value ' .. msg.value)
		::done::
	end

	for j=1,block.bufferSize do
		block.outputs[1][j] = cv
		if writeTrigger:process(block.sampleTime) then block.outputs[2][j] = 10.0 else block.outputs[2][j] = 0.0 end
	end

end
