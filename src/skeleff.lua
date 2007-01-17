-- skeleff - Skeleton Effect.  Use as a model for new effects.
-- 
-- Copyright 2007 Reuben Thomas
-- 
-- This library is free software; you can redistribute it and/or
-- modify it under the terms of the GNU Lesser General Public
-- License as published by the Free Software Foundation; either
-- version 2 of the License, or (at your option) any later version.
-- 
-- This library is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
-- Lesser General Public License for more details.
-- 
-- You should have received a copy of the GNU Lesser General Public
-- License along with this library. If not, write to the Free Software
-- Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
-- USA.


-- The effect is passed the entire input as an st_sample_t table
ibuf = ...

-- The output is a normal Lua table
obuf = {}

-- Compute the output
for i = 1, #ibuf do
  print(ibuf[i])
  obuf[i] = ibuf[i]
end

-- Return the output
return obuf
