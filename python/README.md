# HypercubeHopfield: Modern Hopfield Associative Memory

Pattern storage and recall on Boolean hypercube graphs, with exponential
capacity and softmax attention over sparse Hamming-ball neighborhoods.

## Installation

```bash
pip install hypercube-hopfield
```

## Quick Start

```python
import numpy as np
import hypercube_hopfield as hh

# Create a network with 256 neurons (dim=8, N=2^8)
net = hh.HopfieldNetwork(dim=8, seed=42)

# Store some patterns
patterns = np.random.randn(10, net.num_vertices).astype(np.float32)
net.store_patterns(patterns)

# Recall from a noisy cue
cue = patterns[0] + np.random.randn(net.num_vertices).astype(np.float32) * 0.3
result = net.recall(cue)
print(f"Converged: {result.converged}, steps: {result.steps}")
```

## Features

- **Explicit pattern storage** with exponential capacity
- **Softmax attention** over sparse Hamming-ball neighborhoods
- **Two update modes**: Sync (deterministic, GPU-portable) and Async (guaranteed energy descent)
- **Pickle support** for saving/loading networks
- **NumPy integration** with automatic float32 conversion

## Documentation

See [Python SDK Reference](https://github.com/dliptak001/HypercubeHopfield/blob/main/docs/Python_SDK.md) for the full API.
