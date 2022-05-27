config.frameDivider = 1
config.bufferSize = 32

osc1 = Module(0x9525b684e7612)
osc2 = Module(0xf873d65f5e0cf)
osc1_off = Module(0x162cb73ff4ff0d)
osc2_off = Module(0xfd96bd0e1e59)
osc1_filt = Module(0x15aa37842eb989)
osc2_filt = Module(0x10b913db57d03f)
hadsr = Module(0xb039e58a641e3)
slew = Module(0x10713113734f54)
mixer = Module(0xf512fa34f3f5d)
mastertune = Module(0x99800fb996144)
lfo = Module(0x11ff8ff2485541)
lfo_mod = Module(0x1300ea03ada5d9)

mappings = {
	{type = CC, note = 25, module = hadsr, id = 5},
	{type = CC, note = 73, module = hadsr, id = 1},
	{type = CC, note = 75, module = hadsr, id = 2},
	{type = CC, note = 70, module = hadsr, id = 3},
	{type = CC, note = 72, module = hadsr, id = 4},
	{type = CC, note = 5, module = slew, id = 0},
	{type = CC, note = 5, module = slew, id = 2},
	{type = CC, note = 91, module = mixer, id = 8},
	{type = CC, note = 7, module = mixer, id = 6}
}

function match_mapping(type, note, value)
	matched = false
	for i, m in ipairs(mappings) do
		if m.type == type and m.note == note then
			setParamValue(m.module, m.id, value/127)
			matched = true
		end
	end
	return matched
end

trig = PulseGenerator.new()

gate = 0.0
note = 0.0
function process(block)


	for i=1,block.midiInputSize do
		msg = Message(block.midiInput[i])
		display('type ' .. string.format("0x%x", msg.type) .. ' channel ' .. msg.channel .. ' note ' .. msg.note .. ' value ' .. msg.value)

		if msg.type == NOTE_ON then
			-- if gate == 0.0 then gate = 5.0 end
			trig:trigger()
			note = (msg.note - 60) / 12
		end

		if msg.type == NOTE_OFF then
			-- if gate == 5.0 then gate = 0.0 end
			trig:reset()
		end


		match = match_mapping(msg.type, msg.note, msg.value)
		if not match then
			if msg.type == CC then
				if msg.note == 82 then
					if msg.value == 127 then setParamValue(osc1, 0, 0.0)
					elseif msg.value == 85 then setParamValue(osc1, 0, 1.0)
					elseif msg.value == 43 then setParamValue(osc1, 0, 2.0)
					else setParamValue(osc1, 0, 3.0)
					end
				end
				if msg.note == 77 then
					if msg.value == 127 then setParamValue(osc2, 0, 0.0)
					elseif msg.value == 85 then setParamValue(osc2, 0, 1.0)
					elseif msg.value == 43 then setParamValue(osc2, 0, 2.0)
					else setParamValue(osc2, 0, 3.0)
					end
				end
				if msg.note == 22 then
					if msg.value == 127 then setParamValue(osc1_off, 0, 0.1)
					elseif msg.value == 85 then setParamValue(osc1_off, 0, 0.0)
					elseif msg.value == 43 then setParamValue(osc1_off, 0, -0.1)
					else setParamValue(osc1_off, 0, -0.2)
					end
				end
				if msg.note == 15 then
					if msg.value == 127 then setParamValue(osc2_off, 0, 0.0)
					elseif msg.value == 85 then setParamValue(osc2_off, 0, -0.1)
					elseif msg.value == 43 then setParamValue(osc2_off, 0, -0.2)
					else setParamValue(osc2_off, 0, -0.3)
					end
				end
				if msg.note == 19 then setParamValue(osc1, 3, (msg.value/127) * 2 - 1.00) end
				if msg.note == 14 then setParamValue(osc2, 4, (msg.value/127)) end
				if msg.note == 74 then setParamValue(osc1_filt, 0, (msg.value/127) * 9.97) end
				if msg.note == 71 then setParamValue(osc1_filt, 1, (msg.value/127) * 1.20) end
				if msg.note == 28 then setParamValue(osc2_filt, 1, (msg.value/127) * 100.0) end
				if msg.note == 29 then setParamValue(osc2_filt, 2, (msg.value/127) * 0.85) end
				if msg.note == 20 then setParamValue(mixer, 3, (msg.value/127)) end
				if msg.note == 21 then setParamValue(mixer, 4, (msg.value/127)) end
				if msg.note == 18 then setParamValue(mastertune, 0, (msg.value/127) * 0.2 - 0.1) end
				if msg.note == 27 then setParamValue(lfo, 2, (msg.value/127) * 96.0 - 48.0) end
				if msg.note == 76 then setParamValue(lfo, 5, (msg.value/127) * 2.0 - 1.0) end
				if msg.note == 12 then setParamValue(lfo_mod, 0, (msg.value/127)) end
				if msg.note == 30 then setParamValue(lfo_mod, 1, (msg.value/127)) end
				if msg.note == 85 then setParamValue(lfo_mod, 2, (msg.value/127)) end
			end
		end
	end

	if trig:process(block.sampleTime) then gate = 5.0 else gate = 0.0 end

	for j=1,block.bufferSize do
		block.outputs[1][j] = note
		block.outputs[2][j] = gate
	end

end