config.frameDivider = 4
config.bufferSize = 8

config.osc = true
config.oscPort = 9000 -- receive port
config.oscAddress = "127.0.0.1:9001" -- send address

buttonTrig = BooleanTrigger.new()

function process(block)
	if buttonTrig:process(block.button) then
		display("Sending!")
		sendOscMessage({address = '/rack', types = 'if', 123, 0.12345})
	end

	for i=1,block.oscInputSize do
		local msg = getOscMessage()
		display(msg:address())
		display(msg:types())
		display(msg:args()[1])
	end
end
