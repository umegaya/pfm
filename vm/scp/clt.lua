function main(player)
--	if not account.player[0] then
--		print("create new player")
--		account.player[0] = pfm.new
--		account.player[0].name = "iyatomi"
--	else
--		print("player is already created. name : " .. account.player[0].name)
--	end

--      rediculous test
	print("login success!")
	player.name = "iyatomi"
	print("set player name!!")
	retval = player:chat("hehehe")
	print("retval = " .. retval)
	assert(retval == 666)

--	real test: single value roundtrip
	assert(10011 == player:echo(10011))
	assert(0x1234123412341234 == player:echo(0x1234123412341234))
	assert(nil == player:echo(nil))
	assert(true == player:echo(true))
	assert(false == player:echo(false))
	assert("strstrstr" == player:echo("strstrstr"))
	function compare_table(t1, t2) 
		k1,v1 = next(t1)
		k2,v2 = next(t2)
		while k1 and k2 do
			print("t1[" .. k1 .. "]=" .. v1 .. ",t2[" .. k2 .. "]=" .. v2)
			if not k1 == k2 then return false end
			if not v1 == v2 then return false end
			k1,v1 = next(t1,k1)
			k2,v2 = next(t2,k2)
		end
		return k1 == k2 and k1 == nil
	end
	assert(compare_table({123, 456, 789}, player:echo({123, 456, 789})))
	function vfunc3(n1, n2, n3)
		return n1 + n2 + n3
	end
	vfunc3_remote = player:echo(vfunc3)
	assert(vfunc3_remote(100, 200, 300) == 600)
	assert(vfunc3_remote(200, 400, 600) == vfunc3(200, 400, 600))

--	real test: send function to remote and use here
	retval = player:call_vfunc3(111, 222, 333, vfunc3)
	assert(retval == 666)

--	real test: calculate all table element sum
	list = { 123, 456, 789, "length 16 string" }
        retval = player:calc_table_element_sum(list)
	print("calc_table_element retval = " .. retval)
	assert(retval == 1384)

--	real test: pfm.new
--	player.item = pfm.new("ITEM")
--	assert(not player.item.kind == "ITEM")
	
end

function remote_error(from, msg)
	print("remote error at [" .. from .. "] >> " .. msg)
end
