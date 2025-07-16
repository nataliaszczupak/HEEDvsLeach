import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import random
import math

# --- PARAMETERS
N = 100
FIELD_SIZE = 200
P = 0.05
ROUNDS = 100
ENERGY_INIT = 0.5

E_TX = 50e-9
E_RX = 50e-9
E_DA = 5e-9
E_FS = 10e-12
E_MP = 0.0013e-12
L = 4000
D0 = math.sqrt(E_FS / E_MP)
ENERGY_DEAD = 0.0

random.seed(42)

# --- HELPERS
def distance(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])

def energy_tx(d):
    if d < D0:
        return (E_TX * L) + (E_FS * L * d**2)
    else:
        return (E_TX * L) + (E_MP * L * d**4)

def energy_rx():
    return (E_RX + E_DA) * L

def threshold_T(round_idx):
    r_mod = round_idx % int(1 / P)
    denom = 1 - P * r_mod
    return P / denom if denom > 0 else P / 1e-9

# --- INITIALIZE NODES
nodes = [{
    'x': random.uniform(-FIELD_SIZE, FIELD_SIZE),
    'y': random.uniform(-FIELD_SIZE, FIELD_SIZE),
    'energy': ENERGY_INIT,
    'is_dead': False,
    'is_CH': False
} for _ in range(N)]

bs = (0, 0)
frames = []

# --- SIMULATION
for r in range(ROUNDS):
    # Reset cluster heads
    for n in nodes:
        n['is_CH'] = False

    # Cluster head selection
    Tn = threshold_T(r)
    for n in nodes:
        if not n['is_dead'] and random.random() < Tn:
            n['is_CH'] = True

    connections = []

    for i, n in enumerate(nodes):
        if n['is_dead']:
            continue

        if n['is_CH']:
            d = distance((n['x'], n['y']), bs)
            n['energy'] -= energy_tx(d)
            if n['energy'] <= ENERGY_DEAD:
                n['is_dead'] = True
        else:
            min_d = float('inf')
            ch_index = -1
            for j, ch in enumerate(nodes):
                if ch['is_CH'] and not ch['is_dead']:
                    d = distance((n['x'], n['y']), (ch['x'], ch['y']))
                    if d < min_d:
                        min_d = d
                        ch_index = j
            if ch_index != -1:
                d = min_d
                n['energy'] -= energy_tx(d)
                if n['energy'] <= ENERGY_DEAD:
                    n['is_dead'] = True

                ch = nodes[ch_index]
                ch['energy'] -= energy_rx()
                if ch['energy'] <= ENERGY_DEAD:
                    ch['is_dead'] = True

                connections.append(((n['x'], n['y']), (ch['x'], ch['y'])))
            else:
                d = distance((n['x'], n['y']), bs)
                n['energy'] -= energy_tx(d)
                if n['energy'] <= ENERGY_DEAD:
                    n['is_dead'] = True
                connections.append(((n['x'], n['y']), bs))

    frames.append({
        'nodes': [{**n} for n in nodes],
        'connections': connections
    })

# --- ANIMATION
fig, ax = plt.subplots(figsize=(8, 8))
sc = ax.scatter([], [], s=20)
bs_marker, = ax.plot(bs[0], bs[1], 'kx', markersize=12, label="Base Station")
ax.set_xlim(-FIELD_SIZE, FIELD_SIZE)
ax.set_ylim(-FIELD_SIZE, FIELD_SIZE)

def init():
    return sc,

def update(frame_idx):
    ax.clear()
    frame = frames[frame_idx]
    ax.set_title(f"Step: {frame_idx + 1} / {len(frames)}")

    x = [n['x'] for n in frame['nodes'] if not n['is_dead']]
    y = [n['y'] for n in frame['nodes'] if not n['is_dead']]
    colors = ['red' if n['is_CH'] else 'blue' for n in frame['nodes'] if not n['is_dead']]

    ax.scatter(x, y, s=20, c=colors)

    for n in frame['nodes']:
        if not n['is_dead']:
            ax.text(n['x'], n['y'] + 2, f"{n['energy']:.1f}", fontsize=6, color='green', ha='center')

    for src, dst in frame['connections']:
        ax.plot([src[0], dst[0]], [src[1], dst[1]], color='gray', linewidth=0.5)

    ax.plot(bs[0], bs[1], 'kx', markersize=10, label='Base Station')
    ax.legend(loc='lower right')
    ax.set_xlim(-FIELD_SIZE, FIELD_SIZE)
    ax.set_ylim(-FIELD_SIZE, FIELD_SIZE)
    return sc,

ani = FuncAnimation(fig, update, frames=len(frames), init_func=init, interval=1000, blit=False)
plt.show()
