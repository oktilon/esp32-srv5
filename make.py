#!/usr/bin/python3

f = open("main/index.html", "r")
t = f.read()
f.close()
e = t.replace("    ", "")
e = e.replace("\n", "")

f = open("main/index.htm", "w")
f.write(e)
f.close()

print(e)