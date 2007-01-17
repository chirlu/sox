-- skelform - Skeleton Format.  Use as a model for new formats.
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

-- A format script returns a table of methods
return
{
  -- Read method takes the file handle and input buffer.
  -- It may read up to #ibuf elements, and returns the number actually
  -- read.
  read =
    function (fh, ibuf)
      print "read"
      print(fh, ibuf)
      return 0
    end,

  -- Write method takes the file handle and output buffer.
  -- It should write up to #obuf elements, and returns the number
  -- actually written.
  write =
    function (fh, obuf)
      print "write"
      print(fh, obuf)
      return 0
    end,

  -- Seek method takes the file handle and offset to seek relative to
  -- the current position. It returns a boolean indicating success or
  -- failure.
  seek =
    function (fh, offset)
      print "seek"
      print(fh, offset)
      return false
    end,
}
