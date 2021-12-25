-- Demo script to test with the keyboard set in the midi expander

config.frameDivider = 1
config.bufferSize = 32

_8vert = Module(0xa894ee5452f45)

function process(block)
	for i=1,block.midiInputSize do
		msg = Message(block.midiInput[i])
		display('type ' .. string.format("0x%x", msg.type) .. ' channel ' .. msg.channel .. ' note ' .. msg.note .. ' value ' .. msg.value)
		if msg.note >= 60 and msg.note <=67 then
			if msg.type == NOTE_ON then
				setParamValue(_8vert, msg.note - 60, 1.0)
			else
				setParamValue(_8vert, msg.note - 60, -1.0)
			end
		end
	end

	-- if block._switch then
	-- else
	-- end

	-- for j=1,block.bufferSize do
	-- 	block.outputs[1][j] = 1.0
	-- end

	-- Cardinal red led!
	-- block.light[1] = 0.76
	-- block.light[2] = 0.11
	-- block.light[3] = 0.22
end
