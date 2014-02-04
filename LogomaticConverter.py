
import struct
import array
import Tkinter, tkFileDialog
import re




#Function to take the 2 accelerometer bytes and convert to a signed int
#(Conversion to G's not yet in here!  But 2G = 12 bits for now, so it's easy to add?
def convert_accel_data(data):
	#mask out the first two axis-identification bits
	data = data & 0x3FFF
	#print(hex(data))
	#Check the sign bits; see if it's negative (non-zero sign bits = negative)
	if ((data & 0x3000) != 0):
		#Mask out the sign bit if it's negative (the actual value is just the last 12 bits)
		data = (data & 0x0FFF)
		#print hex(data)
		#And convert it to a "negative" number (but keep the top two bits away!)
		data = (~data + 1)&0x0FFF
		#Tag on the "negative" :)
		data = data*-1
		#Now scale to G's!
	data = float(data)*2
	data = float(data)/2048
	data = round(data, 4)
	#print(data)
	return data


print "Running Data Parser"
#opens the file as read only, binary file (the b is only needed for windows)
#These file names are for testing; will be overwritten by the TKinter stuff
infile_name = '/Users/nancyhd/Documents/Logomatic/Outputs/LOG01.bin'
outfile_name = '/Users/nancyhd/Documents/Logomatic/Outputs/LOGtestout.txt'

root = Tkinter.Tk()
root.withdraw()

infile_name = tkFileDialog.askopenfilename()
print "Input file: " + infile_name
outfile_name = tkFileDialog.asksaveasfilename()
#Searches for a 3 character file extension at the end
has_extension = re.search('\....\Z', outfile_name)
#If there is no file extension, makes it a .txt file
if (has_extension == None):
	outfile_name += ".txt"
print "Output file: " + outfile_name

# regex: /\.txt\Z/i (\Z means at the end of thr string)

#Opens the file in binary format
infile = open(infile_name, 'rb')

#Creates a file; will overwrite a file of the same name
outfile = open(outfile_name, 'w')

#bindata holds all the binary data from the file
bindata = infile.read()


i = 0
worddata = []

#print len(bindata)
#print type(bindata)

#Goes through the binary data and combines each two bytes into 
#one unsigned short of data (I think :) )
#This is because all data (at the moment) is in a two-byte format
while (i < len(bindata)):
	worddata.append(struct.unpack('H',bindata[i+1]+bindata[i]))
	i+=2

#Arrays to hold the data
ADC = []
#accelx = array.array('h')
#accely = array.array('h')
#accelz = array.array('h')
accelx = []
accely = []
accelz = []

#Boolean that is true when reading the parts of the file that are SPI accelerometer data
#The accelerometer data has a 0xFFFF flag to begin and a #0xFF00 flag to finish
#This works (and only if) the ADC data is 15 or less bits (so can't start with 0xF)
#and the accel data is labelled as x, y, and z with the bits 00, 01 and 10 for [15:14]

inaccel = 0


for subword in worddata:
	#The result of the binary read is a tuple that looks like (data, (nothing))
	#So here we extract the "data" part of that
	word = subword[0]

	#Looks for the flags that signal the beginning or end of the 
	#accelerometer data.  If one of these is found, loop can end
	#(the "continue" skips the rest of the loop and goes on to the next iteration)
	if (word == 0xFFFF):
		#print "accel start\n\r"
		inaccel = 1
		continue
	if (word == 0xFF00):
		#print "accel end\n\r"
		inaccel = 0
		continue
	
	#If you're not in an accelerometer section, the data appends to the ADC array
	#It's all unsigned positive; a translation to voltage is probably needed here
	if inaccel == 0:
		ADC.append(word)

	#Otherwise you ARE in accel land, and need to identify whether it's X, Y, or Z by 
	#masking off the first two bits and comparing to the expected value
	#X is signaled by 00
	elif ((word & 0xC000) == 0x0000):
		accelx.append(convert_accel_data(word))
	elif ((word & 0xC000) == 0x4000):
		accely.append(convert_accel_data(word))
	elif ((word & 0xC000) == 0x8000):
		accelz.append(convert_accel_data(word))

#The data is in arrys; now print it out to the file.  
#Header:
outfile.write("Source data: " + infile_name + "\n\r")
outfile.write("ADC, Accel X, Accel Y, Accel Z\n\r")

#Maps through the arrays and prints each row with the corresponding data from each array in CSV format
for a, x, y, z in map(None, ADC, accelx, accely, accelz):
	outfile.write(str(a) + "," + str(x) + "," + str(y) + "," + str(z) + "\n\r")

#All done!  Clean it up!
infile.close()
outfile.close()
print "Parse Complete!  Give yourself a high-5!"



