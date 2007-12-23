--
--  lua-dump.lua -- Simple Lua Object Dumper (language: Lua)
--  Copyright (c) 2007 Ralf S. Engelschall <rse@engelschall.com>
--
--  You can redistribute and/or modify this file under the terms of
--  the GNU General Public License as published by the Free Software
--  Foundation; either version 2 of the License, or (at your option) any
--  later version.
--
--  This file is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
--  General Public License for more details.
--
--  You should have received a copy of the GNU General Public License
--  along with this program; if not, write to the Free Software
--  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
--  USA, or contact Ralf S. Engelschall <rse@engelschall.com>.
--

--  WORK HORSE:
--  dump a single object recursively into a string
function lua_dump_object(obj)
    local dump = "<unknown>"
    if type(obj) == "nil" then
        dump = "nil"
    elseif type(obj) == "number" then
        dump = string.format("%d", obj)
    elseif type(obj) == "string" then
        local str = obj
        str = string.gsub(str, "\\\\", "\\\\")
        str = string.gsub(str, "\"", "\\\"")
        str = string.gsub(str, "\r", "\\r")
        str = string.gsub(str, "\n", "\\n")
        str = string.gsub(str, ".",
            function(c)
                local n = string.byte(c)
                if n < 32 or n >= 127 then
                    c = string.format("\\%03d", n)
                end
                return c
            end
        )
        dump = "\"" .. str .. "\""
    elseif type(obj) == "boolean" then
        if obj then
            dump = "true"
        else
            dump = "false"
        end
    elseif type(obj) == "table" then
        dump = "{"
        local first = true
        for k, v in pairs(obj) do
            if not first then
                dump = dump .. ","
            end
            dump = dump .. " " .. lua_dump_object(k) .. " = "
            dump = dump .. lua_dump_object(v)
            first = false
        end
        dump = dump .. " }"
    elseif type(obj) == "function" then
        dump = "<function>"
    elseif type(obj) == "thread" then
        dump = "<thread>"
    elseif type(obj) == "userdata" then
        dump = "<userdata>"
    end
    return dump
end

--  CONVENIENCE FRONTEND:
--  dump one or more objects recursively, one per line
function lua_dump(obj1, ...)
    local dump = lua_dump_object(obj1) .. "\n"
    for _, obj in ipairs(arg) do
        dump = dump .. lua_dump_object(obj) .. "\n"
    end
    return dump
end

--  CONVENIENCE FRONTEND:
--  dump one or more objects recursively, one per line, to stderr
function lua_dump_stderr(obj1, ...)
    io.stderr:write(lua_dump(obj1, unpack(arg)))
end

