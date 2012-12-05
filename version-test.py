#!/usr/bin/python

import version

a = version.Version("0.51")
assert(a.major == 0)
assert(a.minor == 51)
assert(a.pre == False)
assert(a.beta == None)
assert(str(a) == "0.51")

a.bump_and_to_pre()
assert(a.major == 0)
assert(a.minor == 52)
assert(a.pre == True)
assert(a.beta == None)
assert(str(a) == "0.52pre")

a.bump()
assert(a.major == 0)
assert(a.minor == 53)
assert(a.pre == False)
assert(a.beta == None)
assert(str(a) == "0.53")

a.to_pre()
a.bump_beta()
assert(a.major == 0)
assert(a.minor == 53)
assert(a.pre == False)
assert(a.beta == 1)
assert(str(a) == "0.53beta1")

a.bump_beta()
assert(a.major == 0)
assert(a.minor == 53)
assert(a.pre == False)
assert(a.beta == 2)
assert(str(a) == "0.53beta2")

a.to_release()
assert(a.major == 0)
assert(a.minor == 53)
assert(a.pre == False)
assert(a.beta == None)
assert(str(a) == "0.53")
