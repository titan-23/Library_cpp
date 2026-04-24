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
        used = [False for i in range(n)]
        used[0] = True
        for i in range(1, n):
            best = inf
            best_city = -1
            for city in range(n):
                if used[city]:
                    continue
                if distances[self.index2city[i - 1]][city] < best:
                    best = distances[self.index2city[i - 1]][city]
                    best_city = city
            used[best_city] = True
            self.index2city[i] = best_city

        self.score = 0
        # self.index2city = [0, 63, 42, 160, 5, 50, 165, 132, 108, 183, 95, 189, 179, 136, 75, 146, 38, 56, 30, 88, 32, 110, 54, 60, 120, 114, 104, 178, 151, 105, 99, 28, 156, 109, 125, 175, 142, 58, 126, 124, 78, 123, 61, 131, 43, 90, 143, 7, 150, 80, 148, 84, 93, 73, 121, 158, 185, 41, 14, 187, 71, 116, 76, 118, 40, 190, 22, 68, 198, 74, 44, 135, 112, 2, 98, 111, 159, 62, 193, 39, 85, 31, 59, 8, 37, 48, 17, 47, 19, 137, 169, 82, 149, 10, 106, 180, 138, 100, 69, 89, 12, 177, 16, 46, 101, 129, 9, 65, 141, 134, 192, 184, 133, 197, 64, 91, 77, 87, 113, 199, 24, 182, 79, 67, 172, 83, 25, 140, 186, 45, 161, 176, 35, 195, 102, 97, 96, 33, 168, 15, 18, 181, 1, 52, 163, 4, 49, 115, 103, 34, 162, 107, 139, 57, 188, 191, 27, 194, 144, 26, 86, 171, 13, 153, 3, 72, 119, 94, 36, 70, 167, 128, 29, 55, 155, 196, 6, 53, 166, 130, 66, 164, 127, 173, 154, 20, 23, 145, 117, 157, 147, 51, 122, 174, 11, 21, 81, 152, 170, 92]
        for i in range(n):
            self.score += distances[self.index2city[i]][self.index2city[(i + 1) % n]]
        self.best_score = self.score
        for k in range(n):
            self.best_index2city[k] = self.index2city[k]

    def guided_local_search(self, n, inf, distances, time_limit):
        penalties = [[1 for _ in range(n)] for _ in range(n)]
        coefficient = 0.3 * self.best_score / n
        iterations = 0
        index2city = self.index2city
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
                city1, city2 = index2city[i], index2city[(i + 1) % n]
                v = distances[city1][city2] / penalties[city1][city2]
                if v > best:
                    best = v
                    index1 = i

            index2 = (index1 + 1) % n

            penalties[index2city[index1]][index2city[index2]] += 1
            penalties[index2city[index2]][index2city[index1]] += 1

            for i in range(n):
                if i == index1:
                    continue

                iterations += 1

                index3 = i
                index4 = (index3 + 1) % n

                if index2 == index3:
                    continue
                if index1 == index4:
                    continue
                city1 = index2city[index1]
                city2 = index2city[index2]
                city3 = index2city[index3]
                city4 = index2city[index4]
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
                        index2city[l], index2city[r] = index2city[r], index2city[l]
                        l += 1
                        r -= 1
                    if self.score < self.best_score:
                        self.best_score = self.score
                        print(self.best_score)
                        self.best_index2city = index2city[:]
                        coefficient = 0.3 * self.best_score / n


n = 442
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

TIME_LIMIT = 10
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
