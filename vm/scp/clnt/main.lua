function load_player(player)
--	if not account.player[0] then
--		print("create new player")
--		account.player[0] = pfm.new
--		account.player[0].name = "iyatomi"
--	else
--		print("player is already created. name : " .. account.player[0].name)
--	end

--      rediculous test
	print("login success!")
	player:set_name("takehiro")
	player.name = "iyatomi"
	print("set player name!!")
	local retval = player:chat("hehehe")
	assert(retval == 666)

--	real test: single value roundtrip
	assert(10011 == player:echo(10011))
	assert(3.14159265358979 == player:echo(3.14159265358979))
	assert(0x1234123412341234 == player:echo(0x1234123412341234))
	assert(nil == player:echo(nil))
	assert(true == player:echo(true))
	assert(false == player:echo(false))
	assert("strstrstr" == player:echo("strstrstr"))
	function compare_table(t1, t2) 
		local k1,v1 = next(t1)
		local k2,v2 = next(t2)
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
	local vfunc3_remote = player:echo(vfunc3)
	assert(vfunc3_remote(100, 200, 300) == 600)
	assert(vfunc3_remote(200, 400, 600) == vfunc3(200, 400, 600))

--	real test: send function to remote and use here
	retval = player:call_vfunc3(111, 222, 333, vfunc3)
	assert(retval == 666)

--	real test: calculate all table element sum
	local list = { 123, 456, 789, "length 16 string" }
        retval = player:calc_table_element_sum(list)
	print("calc_table_element retval = " .. retval)
	assert(retval == 1384)

--	real test: pfm.new
	player.item = player:new_item(100)
	print("create object success!");
	assert(pfm.typeof(player.item) == "ITEM")
	print("pfm.typeof test success!!");
	assert(player:get_item_data("attack_point") == 1000)
	print("pfm.new test success!!");

--	real test: enter world
	player:enter_world()
	local pos = player:get_pos()
	assert(compare_table(pos, {10000, 20000, 5000}))

-- 	real test: name convention
	function player:open_ui(url, p1, p2)
		print("open_ui called")
		local r = url .. "|" .. p1 .. "|" .. p2 
		print("result = " .. r)
		return r;
	end
	player:notify_chat("fuhehe")
	local retval = player:client_open_ui("index.html", "h=20", "w=100")
	print("client_open_ui : retval = " .. retval)
	assert(retval == "index.html|h=20|w=100")
	print("name convention test 1 pass")
	assert(player:notify_client_open_ui("index.html", "h=20", "w=100") == nil)
	player:_protected_call()
end

function init_object(obj, t)
	assert(false)
end

function remote_error(from, msg)
	print("remote error at [" .. from .. "] >> " .. msg)
end
