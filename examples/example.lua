config.frameDivider = 1
config.bufferSize = 32

function process(block)
	if block.midiInputSize > 0 then
		display('type ' .. block.midiInput[1][1] .. ' chan ' .. block.midiInput[1][2] .. ' number ' .. block.midiInput[1][3] .. ' value ' .. block.midiInput[1][4])
		if block.midiInput[1][1] == 11 and block.midiInput[1][3] == 74 then
			val = block.midiInput[1][4]/127.0
			display('setting to ' .. val)
			setParamValue(0x1dabeeb5eb9196, 0, val)
		end
	end

	if block._switch then
		val = getParamValue(0x19ff151eef816a, 5)
		display('value is ' .. val)
	end

	for j=1,block.bufferSize do
		block.outputs[1][j] = 15.0 * block.knobs[1]
	end

	-- Cardinal red led!
	block.light[1] = 0.76
	block.light[2] = 0.11
	block.light[3] = 0.22
end
