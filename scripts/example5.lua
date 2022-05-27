config.frameDivider = 1
config.bufferSize = 32

done = false
buttonTrig = SchmittTrigger.new()

function process(block)
	if buttonTrig:process(block.button and 0 or 1) then
		sendMidiMessage(NOTE_ON, 60, 127)
	end
end
