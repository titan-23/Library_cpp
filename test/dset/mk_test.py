from random import Random
rand = Random(0)

q = 1000
U = 20
print(q)

a = []
for _ in range(q):
    com = rand.randint(0, 2)
    if com == 0:
        v = rand.randint(0, U)
        print(com, v)
        a.append(v)
    elif com == 1:
        v = rand.randint(0, U)
        print(com, v)
        if v in a:
            a.remove(v)
    elif com == 2:
        l = rand.randint(0, len(a))
        r = rand.randint(0, len(a))
        if l > r:
            l, r = r, l
        print(com, l, r)
