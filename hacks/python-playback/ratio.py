# Class to describe a Ratio, and a collection of common
# (and not-so-common) film ratios collected from Wikipedia.

class Ratio:
    def __init__(self, ratio, nickname = None):
        self.nickname = nickname
        self.ratio = ratio

    # @return presentation name of this ratio
    def name(self):
        if self.nickname is not None:
            return "%.2f (%s)" % (self.ratio, self.nickname)
        
        return "%.2f" % self.ratio

ratios = []
ratios.append(Ratio(1.33, '4:3'))
ratios.append(Ratio(1.37, 'Academy'))
ratios.append(Ratio(1.78, '16:9'))
ratios.append(Ratio(1.85, 'Flat / widescreen'))
ratios.append(Ratio(2.39, 'CinemaScope / Panavision'))
ratios.append(Ratio(1.15, 'Movietone'))
ratios.append(Ratio(1.43, 'IMAX'))
ratios.append(Ratio(1.5))
ratios.append(Ratio(1.56, '14:9'))
ratios.append(Ratio(1.6, '16:10'))
ratios.append(Ratio(1.67))
ratios.append(Ratio(2, 'SuperScope'))
ratios.append(Ratio(2.2, 'Todd-AO'))
ratios.append(Ratio(2.35, 'Early CinemaScope / Panavision'))
ratios.append(Ratio(2.37, '21:9'))
ratios.append(Ratio(2.55, 'CinemaScope 55'))
ratios.append(Ratio(2.59, 'Cinerama'))
ratios.append(Ratio(2.76, 'Ultra Panavision'))
ratios.append(Ratio(2.93, 'MGM Camera 65'))
ratios.append(Ratio(4, 'Polyvision'))

# Find a Ratio object from a fractional ratio
def find(ratio):
    for r in ratios:
        if r.ratio == ratio:
            return r

    return None

# @return the ith ratio
def index_to_ratio(i):
    return ratios[i]

# @return the index within the ratios list of a given fractional ratio
def ratio_to_index(r):
    for i in range(0, len(ratios)):
        if ratios[i].ratio == r:
            return i

    return None
