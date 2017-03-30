function jds2pri(code, jds) 
	local error_code = 0
	local cmd_length = #jds
	return error_code, cmd_length, jds
end

function pri2jds(code, length, bin) 
	local error_code 	= 0
	return error_code, bin, code
end
