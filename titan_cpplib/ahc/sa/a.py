import sys
import math
import matplotlib.pyplot as plt

def read_coords(filepath):
    coords = []
    is_coord_section = False
    with open(filepath, 'r') as f:
        for line in f:
            tokens = line.strip().split()
            if not tokens:
                continue
            if tokens[0] == "NODE_COORD_SECTION":
                is_coord_section = True
                continue
            if tokens[0] == "EOF":
                break
            if is_coord_section:
                coords.append((float(tokens[1]), float(tokens[2])))
    return coords

def calc_score(coords, tour):
    score = 0
    n = len(tour)
    for i in range(n):
        u = tour[i]
        v = tour[(i + 1) % n]
        dx = coords[u][0] - coords[v][0]
        dy = coords[u][1] - coords[v][1]
        score += int(math.sqrt(dx * dx + dy * dy) + 0.5)
    return score

def main():
    if len(sys.argv) < 2:
        print("Usage: python visualizer.py <tsp_filepath>")
        sys.exit(1)

    filepath = sys.argv[1]
    coords = read_coords(filepath)

    input_data = sys.stdin.read().split()
    if not input_data:
        sys.exit(1)

    tour = [int(x) for x in input_data]

    score = calc_score(coords, tour)

    tour_x = [coords[i][0] for i in tour]
    tour_y = [coords[i][1] for i in tour]

    tour_x.append(coords[tour[0]][0])
    tour_y.append(coords[tour[0]][1])

    plt.figure(figsize=(10, 10))
    plt.plot(tour_x, tour_y, marker='.', linestyle='-', color='blue')
    
    plt.title(f"TSP Tour (Score: {score})")
    plt.xlabel("X")
    plt.ylabel("Y")
    plt.grid(True)
    plt.savefig('a.png')
    plt.show()

if __name__ == "__main__":
    main()
