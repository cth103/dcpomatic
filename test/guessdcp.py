import os

def path(f):
    for d in os.listdir(f):
        try:
            for s in os.listdir(os.path.join(f, d)):
                if s.endswith('.xml'):
                    return os.path.join(f, d)
        except:
            pass

    return None

if __name__ == '__main__':
    print path('/home/carl/Unsafe/DCP/Boon')
