config.frameDivider = 1
config.bufferSize = 32

vinca1 = Module(0xb5dd1362802b6)
vinca2 = Module(0x1fb83dec3b11f5)

done = false
function process(block)
	if block.button and not done then
		print("hey yes " .. tostring(block.button))
		-- id1 = __addCable(, 0, , 0)
		id1 = addCable(vinca1, 0, vinca2, 0)
		print("id1 is " .. id1)
		id2 = addCable(vinca2, 0, vinca1, 0)
		print("id2 is " .. id2)
		-- id3 = __addCable(0x1fb83dec3b11f5, 1, 0xb5dd1362802b6, 3)
		-- print("id3 is " .. id3)
		-- id4 = __addCable(0x1fb83dec3b11f5, 1, 0xb5dd1362802b6, 3)
		-- print("id4 is " .. id4)
		done = true
	end
end
