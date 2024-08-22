import random

rand = random.Random()

n = 10**3
q = 10**3
U = 10**9

A = [rand.randint(0, U) for _ in range(n)]
print(n)
print(*A)

print(q)
for _ in range(q):
    c = rand.randint(0, 4)
    key = rand.randint(0, U)
    if c == 0:
        print(c, key)
    elif c == 1:
        print(c, key)
    elif c == 2:
        print(c, key)
    elif c == 3:
        lower = rand.randint(0, U)
        upper = rand.randint(0, U)
        if lower > upper:
            lower, upper = upper, lower
        print(c, lower, upper)
    elif c == 4:
        print(c, key)
