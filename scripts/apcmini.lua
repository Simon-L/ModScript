-- APC Mini script, see https://synthe.tiseur.fr/userfiles/videos/lune/lune2_3.mp4 for the process

config.frameDivider = 1
config.bufferSize = 8

gbe = NewGrooveBoxExpander(0x7af60bb00f317)
gb = NewGroovebox(0x13478f103b29dd)

trig1 = BooleanTrigger.new()

led = 0

mutes = { 0, 0, 0, 0, 0, 0, 0, 0 }
solos = { 0, 0, 0, 0, 0, 0, 0, 0 }

function handleMute(channel, on)
	display("Toggle mute on channel " .. channel)
	if on == true then gbe:setParam(channel, 1.0) else gbe:setParam(channel, 0.0) end
	if on == false then
		if mutes[channel + 1] == 0 then
			display("toggle mute on " .. channel)
			mutes[channel + 1] = 1
			sendMidiMessage(NOTE_ON, 64 + channel, 1)
		else
			mutes[channel + 1] = 0
			sendMidiMessage(NOTE_ON, 64 + channel, 0)
		end
	end
end

function handleSolo(channel, on)
	display("Toggle solo on channel " .. channel)
	if on == true then gbe:setParam(channel + 8, 1.0) else gbe:setParam(channel + 8, 0.0) end
	if on == false then
		if solos[channel + 1] == 0 then
			display("toggle solo on " .. channel)
			solos[channel + 1] = 1
			sendMidiMessage(NOTE_ON, channel, 1)
		else
			solos[channel + 1] = 0
			sendMidiMessage(NOTE_ON, channel, 0)
		end
	end
end

function handleVolume(channel, value)
	gbe:setParam(16 + channel, value/127)
end

shift = false

memNote = -1

function handleMemory(note, on)
	if note < 52 then par = 80 + (note - 48)
	elseif note < 56 then par = 84 + (note - 52)
	elseif note < 60 then par = 72 + (note - 56)
	else par = 76 + (note - 60) end
	display(note .. " -> " .. par)
	if on then gb:setParam(par, 1.0) else gb:setParam(par, 0.0) end
	sendMidiMessage(NOTE_ON, memNote, 0)
	memNote = note
	sendMidiMessage(NOTE_ON, memNote, 1)

end

function process(block)
	for i=1,block.midiInputSize do
		msg = Message(block.midiInput[i])
		display('type ' .. string.format("0x%x", msg.type) .. ' channel ' .. msg.channel .. ' note ' .. msg.note .. ' value ' .. msg.value)
		if msg.type == NOTE_ON or msg.type == NOTE_OFF then
			if msg.note == 98 then
				if msg.type == NOTE_ON then shift = true else shift = false end
				display(shift)
			end
			if msg.note >= 48 and msg.note <= 63 then
				handleMemory(msg.note, msg.type == NOTE_ON)
			end
			if msg.note >= 0 and msg.note <= 7 then
				handleSolo(msg.note, msg.type == NOTE_ON)
			end
			if msg.note >= 64 and msg.note <= 71 then
				if shift == true then
				else
					handleMute(msg.note - 64, msg.type == NOTE_ON)
				end
			end
		end
		if msg.type == CC then
			if msg.note >= 48 and msg.note <= 55 then
				handleVolume(msg.note - 48, msg.value)
			end
		end
	end

	if trig1:process(block.button == true) then
		if led == 0 then
			led = 1
			sendMidiMessage(NOTE_ON, 82, 0)
		else
			led = 0
			sendMidiMessage(NOTE_ON, 82, 0)
		end
		display('Button!')
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
