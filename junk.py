def dotProductSum(lines):
  dot = 0.0
  for line in lines:
    if "--" not in line and len(line.strip()) > 4:
      print "stripping line: "+line
      dot += float( line.strip().split("\t")[1] )
  return dot


ifile = open("./junk.txt","r")
lines = ifile.readlines()
ifile.close()

dasovich = []   # 1
kean = []       # 2
germany = []    # 3
jones = []      # 4
shackleton = [] # 5

ctr = 0
for line in lines:
  if "--dasovich" in line:
    ctr = 1
  if "--kean" in line:
    ctr = 2
  if "--germany" in line:
    ctr = 3
  if "--jones" in line:
    ctr = 4
  if "--shackleton" in line:
    ctr = 5

  if ctr == 1:
    dasovich.append(line)
  elif ctr == 2:
    kean.append(line)
  elif ctr == 3:
    germany.append(line)
  elif ctr == 4:
    jones.append(line)
  elif ctr == 5:
    shackleton.append(line)
  else:
    print "ERROR ctr=" + str(ctr) + " not found!"

dot = []
dot.append(dotProductSum(dasovich))
dot.append(dotProductSum(kean))
dot.append(dotProductSum(germany))
dot.append(dotProductSum(jones))
dot.append(dotProductSum(shackleton))

print "result:"
for dotprod in dot:
  print str(dotprod)



