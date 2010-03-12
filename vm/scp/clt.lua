function main(player)
--	if not account.player[0] then
--		print("create new player")
--		account.player[0] = pfm.new
--		account.player[0].name = "iyatomi"
--	else
--		print("player is already created. name : " .. account.player[0].name)
--	end
	print("login success!")
	player.name = "iyatomi"
	print("set player name!!")
	retval = player:chat("hehehe")
	print("retval = " .. retval)
end

function remote_error(from, msg)
	print("remote error at [" .. from .. "] >> " .. msg)
end
