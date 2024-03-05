import random

n = 2*10**5
q = 2*10**5

print(n, q)
A = [random.randint(0, 10**18) for _ in range(n)]
print(*A)

for _ in range(q):
  com = random.randint(1, 2)
  if com == 1:
    p = random.randint(1, n)
    x = random.randint(0, 10**18)
    print(com, p, x)
  else:
    l = random.randint(1, n)
    r = random.randint(1, n)
    if l > r: l, r = r, l
    print(com, l, r)
