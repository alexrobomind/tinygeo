import tinygeo as tr
import numpy as np
import matplotlib.pyplot as plt

from matplotlib.patches import Rectangle
from scipy.spatial import Delaunay
from pytictoc import TicToc

# data = np.asarray(
# 	[[0, 0], [0, 1], [1, 0]],
# 	dtype = np.float32
# )

rng = np.random.default_rng(12345)

n_points = 2000000
data = rng.uniform(-1, 1, size=(n_points, 2))
# idx  = rng.integers(0, n_points, size = (n_points, 3))
print("Delaunay")
idx = Delaunay(data).simplices

r = 2 + data[:, 0]
phi = np.pi * data[:, 1]

x = r * np.cos(phi)
y = r * np.sin(phi)

data = np.stack([x, y], axis=-1)

print("Mesh")
mesh = tr.mesh(data, idx)

print(mesh.data)
print(mesh.indices)

with TicToc():
	mesh.pack(100)

nodes = []


def app(x):
    nodes.append(x)

    for c in x.children:
        app(c)


app(mesh.root)

tris = mesh.data[mesh.indices, :]

print(idx.shape)

if False:
	plt.figure()

	for i, node in enumerate(nodes):
		p1 = node.box.min
		p2 = node.box.max

		start, stop = node.range

		color = "C{}".format(i % 10) if not node.children else "k"

		rect = Rectangle(
			(p1[0], p1[1]),
			p2[0] - p1[0],
			p2[1] - p1[1],
			facecolor="none",
			edgecolor=color,
			linewidth=2,
		)
		plt.gca().add_patch(rect)

		for t in tris[start:stop]:
			plt.plot(*t[[0, 1, 2, 0], :].T, c=color, lw=1)

	plt.show()
