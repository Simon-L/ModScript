-- Demo script to test with the keyboard set in the midi expander

config.frameDivider = 1
config.bufferSize = 32

_8vert = Module(0xa1114e2bdb822)

function process(block)
	for i=1,block.midiInputSize do
		msg = Message(block.midiInput[i])
		display('type ' .. string.format("0x%x", msg.type) .. ' channel ' .. msg.channel .. ' note ' .. msg.note .. ' value ' .. msg.value)
		-- if msg.note >= 60 and msg.note <=67 then
		-- 	if msg.type == NOTE_ON then
		-- 		setParamValue(_8vert, msg.note - 60, 1.0)
		-- 	else
		-- 		setParamValue(_8vert, msg.note - 60, -1.0)
		-- 	end
		-- end
	end

	-- if block.button then
	-- else
	-- end

	-- for j=1,block.bufferSize do
	-- 	block.outputs[1][j] = 1.0
	-- end

	-- Cardinal red led!
	-- intensity = block.inputs[1][1]
	-- block.light[1] = 0.76 * intensity
	-- block.light[2] = 0.11 * intensity
	-- block.light[3] = 0.22 * intensity
end
