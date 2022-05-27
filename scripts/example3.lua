config.frameDivider = 1
config.bufferSize = 32

vinca1 = Module(0xb5dd1362802b6)
vinca2 = Module(0x1fb83dec3b11f5)

done = false
done2 = false
id1 = nil
id2 = nil
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

	if block.inputs[1][1] >= 1.0 and done and not done2 then
		print("yes above 1.0!")
		print(removeCable(id1))
		print(removeCable(id2))
		id3 = addCable(vinca2, 1, vinca1, 1)
		print("id3 is " .. id3)
		done2 = true
	end
end
