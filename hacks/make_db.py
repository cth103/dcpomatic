#!/usr/bin/python3

import random
import sqlite3


SCREENS = 2622
AVERAGE_SCREENS_PER_CINEMA = 4


def name(min_len=12, max_len=20):
    return ''.join(random.choices("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ", k=random.randint(min_len, max_len)))

def email():
    return name(4, 8) + "@" + name(5, 10) + ".com"

def certificate():
    return """-----BEGIN CERTIFICATE-----
MIIEZDCCA0ygAwIBAgIBBTANBgkqhkiG9w0BAQsFADB9MRYwFAYDVQQKEw1kY3Bv
bWF0aWMuY29tMRYwFAYDVQQLEw1kY3BvbWF0aWMuY29tMSQwIgYDVQQDExsuZGNw
b21hdGljLnNtcHRlLTQzMC0yLlJPT1QxJTAjBgNVBC4THEhRTDR1ODV6OHFaSjBP
ZWJLbndIWkdSTndFUT0wHhcNMjQxMjIwMTQyMzM3WhcNMzQxMjE4MTQyMzM3WjB9
MRYwFAYDVQQKEw1kY3BvbWF0aWMuY29tMRYwFAYDVQQLEw1kY3BvbWF0aWMuY29t
MSQwIgYDVQQDExsuZGNwb21hdGljLnNtcHRlLTQzMC0yLlJPT1QxJTAjBgNVBC4T
HEhRTDR1ODV6OHFaSjBPZWJLbndIWkdSTndFUT0wggEiMA0GCSqGSIb3DQEBAQUA
A4IBDwAwggEKAoIBAQDGAvpPyIQ8BWBYZqqimNR8tpFMvPFlk+UkB0BlO+mt8rfi
tpkYVxZuvpsRCBqK9pEYvUnybp2bQHJCuG2lvAp1MTvn9t0TyON5w5BXbxyMKg8f
EvEKZvWsgCSupMw7TWvRebYidj7y51XmKF+p4vq9BOyV54MNPGwZWP1UVw6nJMr2
/jw1TR+iURdVYtQKMMc6jQp59EhE2FMpTGAe47LK/gSwZvsiXz9tJIf2xd6IELeA
CZ7D7eMe4TtApbYZofpIdEcOQye+O1Brl3UhsJJOAVrlAN8ya4P6IaSP20kMrFij
vZFfT8CuWXsg68JwiMLWJKFPvvixSPWImZVf8g3NAgMBAAGjge4wgeswEgYDVR0T
AQH/BAgwBgEB/wIBAzALBgNVHQ8EBAMCAQYwHQYDVR0OBBYEFB0C+LvOc/KmSdDn
myp8B2RkTcBEMIGoBgNVHSMEgaAwgZ2AFB0C+LvOc/KmSdDnmyp8B2RkTcBEoYGB
pH8wfTEWMBQGA1UEChMNZGNwb21hdGljLmNvbTEWMBQGA1UECxMNZGNwb21hdGlj
LmNvbTEkMCIGA1UEAxMbLmRjcG9tYXRpYy5zbXB0ZS00MzAtMi5ST09UMSUwIwYD
VQQuExxIUUw0dTg1ejhxWkowT2ViS253SFpHUk53RVE9ggEFMA0GCSqGSIb3DQEB
CwUAA4IBAQCrJuN+Bdzvj9m5MBNYjgCkmuvVQsGMvwQOIm38PN94hbrmxCNMWJTF
r9MYLKT7wqcdKTw9QvW0g5pjbywXdHCSObgDUsON8nOrIzTsX0bUomD+C6Ohga9P
ep/49aw8CH4yypW/0NUKnqz0+JVMOYsz5wOzIdoQpNggToJXfM08W0m2pIrTrR4+
8rMnp3g2WF8Td+NDIiP23M2FJB2/c0WvoaT++TAVTaBxL1mbCHcxeVN0nyWnBskA
WLqdsVYYC3fKvvcP3Kopbyz7qDN2tE4Ei82PtGQ8WwU+Pt7qI8YMEi6HqfCFmBlj
rb1igF89Id53udux1MaUBdeBnQjG+9Aj
-----END CERTIFICATE-----"""


con = sqlite3.connect("test.db")
cur = con.cursor()

cur.execute("CREATE TABLE cinemas (id INTEGER PRIMARY KEY, name TEXT, emails TEXT, notes TEXT, utc_offset_hour INTEGER, utc_offset_minute INTEGER)")
cur.execute("CREATE TABLE screens (id INTEGER PRIMARY KEY, cinema INTEGER, name TEXT, notes TEXT, recipient TEXT, recipient_file TEXT)")
cur.execute("CREATE TABLE trusted_devices (id INTEGER PRIMARY KEY, screen INTEGER, certificate_or_thumbprint TEXT)")

while SCREENS > 0:
    cur.execute(
        "INSERT INTO cinemas (name, emails, notes, utc_offset_hour, utc_offset_minute) VALUES (?, ?, ?, ?, ?)",
        (name(), email(), "", 0, 0)
    )

    cinema_id = cur.lastrowid

    for i in range(0, AVERAGE_SCREENS_PER_CINEMA + random.randint(-2, 2)):
        cur.execute(
            "INSERT INTO screens (cinema, name, notes, recipient, recipient_file) VALUES(?, ?, ?, ?, ?)",
            (cinema_id, name(), "", certificate(), "")
        )
        SCREENS -= 1

con.commit()
con.close()

