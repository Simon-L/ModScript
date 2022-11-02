-- Demo script

config.frameDivider = 1
config.bufferSize = 8

-- mymodule = Module(0xdeadbeef)

buttonTrig = BooleanTrigger.new()

-- uncomment below to help debug library paths
-- display(package.path)

function process(block)
	for i=1,block.midiInputSize do
		msg = Message(block.midiInput[i])
		display('type ' .. string.format("0x%x", msg.type) .. ' channel ' .. msg.channel .. ' note ' .. msg.note .. ' value ' .. msg.value)
	end

	if buttonTrig:process(block.button) then
		display('Button pressed!')
		-- sendMidiMessage(NOTE_ON, 61, 127)
	else
	end

	-- for j=1,block.bufferSize do
	-- 	block.outputs[1][j] = 1.0
	-- end

	-- Cardinal red led!
	intensity = block.knobs[1]
	block.light[1] = 0.76 * intensity
	block.light[2] = 0.11 * intensity
	block.light[3] = 0.22 * intensity
end
