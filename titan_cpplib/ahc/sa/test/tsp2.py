import sys
import time
import math
from math import hypot
import matplotlib.pyplot as plt
import matplotlib.animation as animation


class TSP:

    def __init__(self, n, distances, inf):
        self.n = n
        self.distances = distances
        self.inf = inf
        self.score = 0
        self.index2city = [i for i in range(n)]
        self.best_score = 0
        self.best_index2city = [i for i in range(n)]

    def solve(self, TIME_LIMIT):
        self.greedy(self.n, self.inf, self.distances)
        self.guided_local_search(self.n, self.inf, self.distances, TIME_LIMIT)
        best_index2city = self.best_index2city
        for i in range(n):
            if best_index2city[i] == 0:
                best_index2city = best_index2city[i:] + best_index2city[:i]
                break
        path = best_index2city[:]
        path.append(0)
        return path

    def greedy(self, n, inf, distances):
        with open("./out.txt", "r", encoding="utf-8") as f:
            tour = f.readline().split()
            tour = list(map(int, tour))
            tour.pop(0)
        # used = [False for i in range(n)]
        # used[0] = True
        # for i in range(1, n):
        #     best = inf
        #     best_city = -1
        #     for city in range(n):
        #         if used[city]:
        #             continue
        #         if distances[self.index2city[i - 1]][city] < best:
        #             best = distances[self.index2city[i - 1]][city]
        #             best_city = city
        #     used[best_city] = True
        #     self.index2city[i] = best_city
        self.index2city = tour
        self.score = 0
        for i in range(n):
            self.score += distances[self.index2city[i]][self.index2city[(i + 1) % n]]
        self.best_score = self.score
        for k in range(n):
            self.best_index2city[k] = self.index2city[k]

    def guided_local_search(self, n, inf, distances, time_limit, K=100):
        neighbors = []
        for i in range(n):
            nearest = sorted([j for j in range(n) if j != i], key=lambda x: distances[i][x])[:K]
            neighbors.append(nearest)
        penalties = [[1 for _ in range(n)] for _ in range(n)]
        coefficient = 0.3 * self.best_score / n
        iterations = 0
        index2city = self.index2city
        city2index = [0] * n
        for idx, city in enumerate(index2city):
            city2index[city] = idx
        START_TIME = time.time()
        while True:
            current_time = time.time() - START_TIME
            if current_time > time_limit:
                sys.stderr.write(
                    f"score: {self.best_score}, iterations: {iterations} \n"
                )
                break
            index1 = -1
            best = -inf
            for i in range(n):
                city1_tmp, city2_tmp = index2city[i], index2city[(i + 1) % n]
                v = distances[city1_tmp][city2_tmp] / penalties[city1_tmp][city2_tmp]
                if v > best:
                    best = v
                    index1 = i
            index2 = (index1 + 1) % n
            city1 = index2city[index1]
            city2 = index2city[index2]
            penalties[city1][city2] += 1
            penalties[city2][city1] += 1
            for city3 in neighbors[city1]:
                index3 = city2index[city3]
                index4 = (index3 + 1) % n
                if index2 == index3:
                    continue
                if index1 == index4:
                    continue
                city4 = index2city[index4]
                iterations += 1
                before = distances[city1][city2] + distances[city3][city4]
                after = distances[city1][city3] + distances[city2][city4]
                diff = after - before
                before_penalty = penalties[city1][city2] + penalties[city3][city4]
                after_penalty = penalties[city1][city3] + penalties[city2][city4]
                delta_penalty = coefficient * (after_penalty - before_penalty)
                if diff < -delta_penalty:
                    self.score += diff
                    l = index2
                    r = index3
                    if l > r:
                        l = index3 + 1
                        r = index2 - 1
                    while l < r:
                        c_l = index2city[l]
                        c_r = index2city[r]
                        index2city[l], index2city[r] = c_r, c_l
                        city2index[c_l] = r
                        city2index[c_r] = l
                        l += 1
                        r -= 1
                    if self.score < self.best_score:
                        self.best_score = self.score
                        print(self.best_score)
                        self.best_index2city = index2city[:]
                        coefficient = 0.1 * self.best_score / n
                    break

n = 4461
x = []
y = []
citys = []
for _ in range(6):
    input()
for _ in range(n):
    _, a, b = map(float, input().split())
    x.append(a)
    y.append(b)
    citys.append((a, b))
distances = [[0] * n for _ in range(n)]
for i in range(n):
    for j in range(i + 1, n):
        distances[i][j] = distances[j][i] = int(
            ((x[i] - x[j]) ** 2 + (y[i] - y[j]) ** 2) ** 0.5 + 0.5
        )

TIME_LIMIT = 60
tsp = TSP(n, distances, 1e9)
path = tsp.solve(TIME_LIMIT)
print(path)


def show(path):
    fig, ax = plt.subplots()
    ax.scatter(x, y, color="black")
    for i in range(len(path) - 1):
        ax.plot(
            [citys[path[i]][0], citys[path[i + 1]][0]],
            [citys[path[i]][1], citys[path[i + 1]][1]],
            linewidth=1,
            color="darkblue",
        )
    plt.show()


show(path)
# score: 188815, iterations: 83000600
