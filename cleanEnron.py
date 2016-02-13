"""
This pyscript does cleaning and outputting on an enron directory of emails. So
given a prolific emailer in the enron dataset, clean their sent emails, and write them to 
an output directory. We just want to build a large, clean dataset of someone's personal language
model; the added benefit of the enron emails is that they contain attributes such as time-of-day,
addressee(s), and so on. The downside is the dataset is so full of business language and a lot of
character and formatting noise; so the dataset is inherently difficult to parse well, with the additional
difficulty of the language complexity used by corporate executives in a legal, engineering, and business context.

Outputs file into a structured form:
  To: A list of recipients
  Subject: A topic
  DateTime: A datetime field (don't expect much from this attribute)
  Body: Body of email, including the subject line, so we get that context as well.
  
Essentially this script outputs each email as a vector of key:vals.

The filters here are ad hoc, and based on a lot of manual ajustment. But they are very good, best so far for the
noisy-as-heck enron set.


"""

import re
import os


#line validation: function crudely checks if there is a higher special/regular char ratio than some guesstimated valid ratio
# regular chars are normal grammar symbols and alphas. No numbers.
def ValidRegCharThreshold(testline):
  
  line = testline.strip().replace(" ","")
  regChars = 0.0
  for char in line:
    if char in "$0987654321ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz,.?!;' ":
      regChars += 1.0

  #nirreg = (len(line) - regChars)
  #if (nreg / nirreg) greater than some ratio, return true
  #if nirreg == 0 or (regChars / nirreg) > 0.66:
  if (regChars / float(len(line))) > 0.75:
    return True
  return False


def isValidLine(line):
  if len(line) > 10 and ">" != line[0] and "X-" not in line[0:2] and "----" not in line and "____" not in line:
    if "         " not in line and "\t\t" not in line[0:2] and "~~~~~" not in line[0:7] and "http" not in line:
      if "To:" not in line[0:5] and "Subject:" not in line[0:10] and "Cc:" not in line[0:5] and "cc:" not in line[0:5]:
        if "Sent: " not in line[0:6] and "ATTENTION" not in line[0:10]:
          if None == re.search(r'(\d+/\d+)',line[0:10]) and "@" not in line:
            if ValidRegCharThreshold(line):  #crudely checks if more special chars than regular chars  
              #print "valid body? -> "+line
              return True
  #print "invalid? -> "+line
  return False

#implements the (structural) regex of an enron email, discarding the header and trying its best to 
#extract only real email lines, not FORWARDED sections or other weird stuff.
#Removes empty lines.
def parseEmailFile(emailRawLines):
  bodyLines = []

  #advance to first line of email's user text section (if it exists), after the "X-Filename:" header line
  i = 0
  emailBegin = False
  while (i < len(emailRawLines)) and (not emailBegin):
    if "X-FileName:" in emailRawLines[i]:
      emailBegin = True
      i += 2
    else:
      i += 1
  
  #empty email check
  if i >= len(emailRawLines):
    return []

  prevBlank = False
  doubleBlankCtr = 0
  endOfBody = False
  while i < len(emailRawLines) and not endOfBody:
    line = emailRawLines[i].strip()

    if len(line) > 0 and isValidLine(line):
      bodyLines.append(line+"\n")

    #structural regex looks for two consecutive blank lines as end-of-email-body signal
    if len(line) == 0:
      doubleBlankCtr += 1
      prevBlank = True
    elif prevBlank: #reset prevBlank flag and counter for non-consecutive blank lines
      prevBlank = False
      doubleBlankCtr = 0

    if doubleBlankCtr >= 2 and prevBlank:  #break when we encounter two consecutive blank lines
      endOfBody = True

    i += 1

  return bodyLines

def parseFilesInDir(dirFullPath):  
  outputLines = []
  fileList = os.listdir(dirFullPath)
  for fname in fileList:
    ifile = open(dirFullPath+"/"+fname,"r")
    email = ifile.readlines()
    ifile.close()
    tmpLines = parseEmailFile(email)
    if len(tmpLines) > 0:
      outputLines += tmpLines
      outputLines += "~~~\n" #email separator signal

  return outputLines

lines = parseFilesInDir("./Personnel/dasovich/dasovich_In")
ofile = open("./Personnel/dasovich/parsedSent.txt","w+")
ofile.writelines(lines)
ofile.close()







'''
for fname in fileList:
  nfiles += 1

  ifile = open(inDir+fname,"r")

  parseBody = False
  body = []
  subject = ""
  cc = ""
  bcc = ""
  date = ""
  to = ""
  rawlines = ifile.readlines()
  for line in rawlines:
    #print line
    if line[0] != "\t":
      if subject == "" and "Subject: " in line[0:10]:
        subject = line.replace("Subject: ","").replace("Re: ","").strip()
        #print "subject === ", subject
      if to == "" and "To: " in line[0:6]:
        to = line.split("To: ")[1].strip()  #Note: To, Bcc, and Cc fields are lists, and could all contain multiple addressees
      if cc == "" and "Cc: " in line[0:6]:            
        cc = line.split("Cc: ")[1].strip()
      if bcc == "" and "Bcc: " in line[0:6]:            
        bcc = line.split("Bcc: ")[1].strip()
      if date == "" and "Date: " in line[0:7]:            
        date = line.split("Date: ")[1].strip()
      if line.strip() == "":
        parseBody = True  #change state once we see a blank line, indicating start of body
      if parseBody and isValidLine(line.strip()):
        #print "adding line: ",line.strip()
        body.append(line.strip().replace("\\x00",""))

    #print "body.size()="+str(len(body))
  #do stuff with vector of: <Targets,Date,Body>
  #print "vec>>  subject:"+subject+"  to:"+to+"  cc:"+cc+"  bcc:"+bcc+"  date:"+date
  #print "nbodylines="+str(len(body))
  #print "BODY!!",body
  if len(subject) > 0 and len(to) > 0 and len(date) > 0 and len(body) > 0:
    ofile = open(outDir+fname+suffix,"w+")
    docExamples += 1
    #output document as a vector: to,date,subject,body (osme fields are merged below)
    if len(subject) > 5:
      body.append(subject) #add subject line to body
    to = to.split(",")
    if len(cc) > 5:
      to += cc.split(",")
    if len(bcc) > 5:
      to += bcc.split(",")
    #print "to===",to
    ofile.write("TO:")
    for addressee in to:
      ofile.write("\t"+addressee)
    ofile.write("\nSUBJECT:\t"+subject.strip()+"\n")
    ofile.write("DATE:\t"+date.strip()+"\n")
    ofile.write("BODY:")
    for line in body:
      if len(line) > 0:
        ofile.write("\n"+line.strip())
    ofile.write("\nEOF\n")
    ofile.close()  

  #close files
  ifile.close()


print "nfiles: "+str(nfiles)
print "docExamples: "+str(docExamples)
'''














