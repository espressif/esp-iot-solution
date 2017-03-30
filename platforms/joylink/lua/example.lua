function tableToString(cmd)
    local strcmd=""
    local i
    for i=1,#cmd do
        strcmd=strcmd..string.char(cmd[i])
    end
    return strcmd
end

function stringToTable(sta)
    local tablesta={}
    local i
    for i=1,#sta do
        tablesta[i]=sta:byte(i)
    end
    return tablesta
end

function decode( cmd )
    return cjson.decode(cmd)
end

function jds2pri( code, cmd )
	local json = decode(cmd)
	local streams = json["streams"]
	local bin = {0xaa, 0x18, 0xb4, 0xac, 0x00, 0x00, 0x20, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa7, 0xbe}

	if code == 1002 then
		for i=1, #streams do
			local key = streams[i]['stream_id']
			local val = streams[i]['current_value']
			if (key == "switch") then
				--print('current_value', val)
				if val == "1" then
					bin = {0xaa, 0x18, 0xb4, 0xac, 0x00, 0x00, 0x20, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa7, 0xbe}
				elseif val == "0" then
					bin = {0xaa, 0x18, 0xb4, 0xac, 0x00, 0x00, 0x91, 0x00, 0x00, 0x02, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5e, 0x90 }
				end
			end
		end

	elseif code == 1004 then
		bin = {0xaa, 0x0a, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x42}
	else

	end

	local ret = tableToString(bin)
	return 0, string.len(ret), ret
end

function pri2jds( code, length, bin )
	-- 控制: 102
	if code == 102 then
		return 0,'{"code":0,"streams":[{"current_value":1,"stream_id":"switch"}],"msg":"done"}', 102

	-- 获取设备快照: 104
	elseif code == 104 then
		return 0,'{"code":0,"streams":[{"current_value":1,"stream_id":"switch"}],"msg":"done"}', 104

	-- 数据上报: 103
	elseif code == 103 then
		return 0,'{"code":0,"streams":[{"current_value":1,"stream_id":"switch"}],"msg":"done"}', 103

	else
		return 0,'{"code":0,"streams":[{"current_value":1,"stream_id":"switch"}],"msg":"done"}', code
	end
	return 0,'{"code":0,"streams":[{"current_value":1,"stream_id":"switch"}],"msg":"done"}', code
end
