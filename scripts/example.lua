config.frameDivider = 1
config.bufferSize = 32

Seq = Module(0x16c051fe00a32f)

function process(block)
	for i=1,block.midiInputSize do
		msg = Message(block.midiInput[i])
		display('type ' .. msg.type .. ' channel ' .. msg.channel .. ' note ' .. msg.note .. ' value ' .. msg.value)
		if msg.type == 11 and msg.note == 74 then
			val = (msg.value/127.0) * 2.0 - 1.0
			display('setting to ' .. val)
			-- setParamValue(Osc, 0, val)
		end
	end

	if block._switch then
		setParamValue(Seq, 5, -1.0)
	else
		setParamValue(Seq, 5, 1.0)
	end

	for j=1,block.bufferSize do
		block.outputs[1][j] = 10.0 * block.knobs[1]
	end

	-- Cardinal red led!
	block.light[1] = 0.76
	block.light[2] = 0.11
	block.light[3] = 0.22
end
