return
{
  rea =
    function (fh, ibuf)
      print "read"
      print(fh, ibuf)
      return 0
    end,

  writ =
    function (fh, obuf)
      print "write"
      print(fh, obuf)
      return 0
    end,
  
  see =
    function (fh, offset)
      print "seek"
      print(fh, offset)
      return false
    end,
}
