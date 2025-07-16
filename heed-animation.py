import random
import math
import copy
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.collections import LineCollection

# -----------------------------
# PARAMETERS
# -----------------------------
NODES = 100
ROUNDS = 100
FIELD_SIZE = 100
ENERGY_INIT = 0.5

E_TX = 50e-9
E_RX = 50e-9
E_DA = 5e-9
E_FS = 10e-12
E_MP = 0.0013e-12
L = 4000
D0 = math.sqrt(E_FS / E_MP)
ENERGY_DEAD = 0.0

# -----------------------------
# ENERGY FUNCTIONS
# -----------------------------
def calculate_distance(n1, n2):
    x1, y1 = n1['position']
    x2, y2 = n2['position']
    return math.sqrt((x2 - x1)**2 + (y2 - y1)**2)

def energy_tx(d):
    if d < D0:
        return E_TX * L + E_FS * L * d**2
    else:
        return E_TX * L + E_MP * L * d**4

def energy_rx():
    return (E_RX + E_DA) * L

# -----------------------------
# INITIALIZATION
# -----------------------------
base_station = {'id': 'BS', 'position': (0, 0)}

def create_nodes():
    return [{
        'id': i,
        'energy': ENERGY_INIT,
        'position': (random.uniform(-FIELD_SIZE, FIELD_SIZE), random.uniform(-FIELD_SIZE, FIELD_SIZE)),
        'is_dead': False
    } for i in range(NODES)]

# -----------------------------
# CLUSTER ASSIGNMENT ALGORITHM
# -----------------------------
def cluster_assignment(nodes, base_station):
    # Note: nodes is a copy – only local modifications
    for node in nodes:
        node.update({
            'is_head': False,
            'assigned_to_cluster': False,
            'head_id': None,
            'distance_to_head': None
        })

    unassigned_nodes = [n for n in nodes if not n['is_dead']]

    while unassigned_nodes:
        head_node = max(unassigned_nodes, key=lambda n: n['energy'])
        head_node['is_head'] = True
        head_node['assigned_to_cluster'] = True
        head_node['head_id'] = head_node['id']
        unassigned_nodes.remove(head_node)

        for node in nodes:
            if node['is_dead'] or node['is_head'] or node['assigned_to_cluster']:
                continue
            dist_to_head = calculate_distance(node, head_node)
            dist_to_base = calculate_distance(node, base_station)
            if dist_to_head < dist_to_base:
                node['distance_to_head'] = dist_to_head
                node['assigned_to_cluster'] = True
                node['head_id'] = head_node['id']

        unassigned_nodes = [n for n in nodes if not n['assigned_to_cluster'] and not n['is_dead']]

    # Final assignment to the closest head
    for node in nodes:
        if node['is_dead'] or node['is_head']:
            continue
        min_dist = float('inf')
        for head in nodes:
            if head['is_head'] and not head['is_dead']:
                d = calculate_distance(node, head)
                if d < min_dist:
                    min_dist = d
                    node['distance_to_head'] = d
                    node['head_id'] = head['id']
        node['assigned_to_cluster'] = True

    return nodes

# -----------------------------
# ROUND-BASED SIMULATION
# -----------------------------
original_nodes = create_nodes()
round_snapshots = []

for r in range(ROUNDS):
    if all(n['is_dead'] for n in original_nodes):
        break

    nodes = copy.deepcopy(original_nodes)

    # 1. Cluster head selection and assignment
    nodes = cluster_assignment(nodes, base_station)

    # 2. Transmission and energy consumption
    for node in nodes:
        if node['is_dead']:
            continue

        if node.get('is_head'):
            d_bs = calculate_distance(node, base_station)
            node['energy'] -= energy_tx(d_bs)
        else:
            head = next((h for h in nodes if h['id'] == node['head_id']), None)
            if head and not head['is_dead']:
                d = node['distance_to_head']
                node['energy'] -= energy_tx(d)
                head['energy'] -= energy_rx()

        if node['energy'] <= ENERGY_DEAD:
            node['is_dead'] = True

    # Update original nodes by ID
    for i, node in enumerate(nodes):
        original_nodes[i]['energy'] = node['energy']
        original_nodes[i]['is_dead'] = node['is_dead']

    round_snapshots.append(copy.deepcopy(nodes))

# -----------------------------
# ANIMATION
# -----------------------------
fig, ax = plt.subplots()
ax.set_xlim(-FIELD_SIZE, FIELD_SIZE)
ax.set_ylim(-FIELD_SIZE, FIELD_SIZE)
ax.set_aspect('equal')

def update(frame_idx):
    ax.clear()
    ax.set_xlim(-FIELD_SIZE, FIELD_SIZE)
    ax.set_ylim(-FIELD_SIZE, FIELD_SIZE)

    current = round_snapshots[frame_idx]
    alive = [n for n in current if not n['is_dead']]
    positions = np.array([n['position'] for n in alive])
    colors = ['red' if n['is_head'] else 'blue' for n in alive]

    ax.scatter(positions[:, 0], positions[:, 1], s=20, c=colors)
    ax.plot(0, 0, 'kx', markersize=10, label="Base Station")
    ax.legend(loc='lower right')

    for n in alive:
        x, y = n['position']
        ax.text(x, y + 1.5, f"{n['energy']:.2f}", fontsize=6, color='green', ha='center')

    segments = []
    heads = {n['id']: n['position'] for n in alive if n['is_head']}
    for n in alive:
        if not n['is_head'] and n['head_id'] in heads:
            segments.append([n['position'], heads[n['head_id']]])
    if segments:
        lc = LineCollection(segments, colors='gray', linewidths=0.6, alpha=0.5)
        ax.add_collection(lc)

    ax.set_title(f"Round: {frame_idx + 1} / {len(round_snapshots)}")

anim = FuncAnimation(fig, update, frames=len(round_snapshots), interval=1000, repeat=False)
plt.show()
