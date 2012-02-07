
def s_to_hms(s):
    m = int(s / 60)
    s -= (m * 60)
    h = int(m / 60)
    m -= (h * 60)
    return (h, m, s)
