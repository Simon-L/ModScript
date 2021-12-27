config.frameDivider = 1
config.bufferSize = 32

_8v1 = Module(0x1bbf041a298792)
_8v2 = Module(0x81d3a00ed7db4)

cable1 = 0
cable2 = 0
function process(block)

	for i=1,block.midiInputSize do
		msg = Message(block.midiInput[i])
		display('type ' .. string.format("0x%x", msg.type) .. ' channel ' .. msg.channel .. ' note ' .. msg.note .. ' value ' .. msg.value)
		if msg.type == NOTE_ON then
			if msg.note == 60 then
				cable1 = addCable(_8v1, 0, _8v2, 0)
			elseif msg.note == 61 then
				cable2 = addCable(_8v1, 1, _8v2, 1)
			elseif msg.note == 62 then
				removeCable(cable1)
			elseif msg.note == 63 then
				removeCable(cable2)
			else
			end
		end
	end
end
